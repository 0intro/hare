/*
	execfs - proc file system execution wrapper

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

	Description:
		This provides an overlay file system which provides
		facilities for process creation using a clone mechanism.

		Along with that, we create stdio pipes for the process
		to use as I/O, and overlay those on top of the created pid
		in the proc table.

	TODO:
		* need a better way to clean up after fids
		* maybe autostart mpipe when we need it
		
*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>
#include <stdio.h>
#include <debug.h>

char Enopid[] =	"process not initialized";
char Eoverflow[] = "ctl buffer overflow";
char Ebadctl[] = "bad ctl message";
char Eexec[] = "could not exec";
char Eusage[] = "exec: invalid number of arguments";
char Ectlchan[] ="problems with command channel to wrapper";
char Ectlopen[] ="problems with opening command channel to wrapper";
char Empipe[] = "problems with mpipe";

char 	defaultpath[] =	"/proc";
char *procpath;
char *srvctl;
Channel *iochan;

static void
usage(void)
{
	fprint(2, "execfs [-D] [mtpt]\n");
	exits("usage");
}

enum
{
	STACK = (8 * 1024),
	Xclone = 1,
};

typedef struct Exec Exec;
struct Exec 
{
	int active;	/* setup=0, executing=1 */
	int pid;	/* maybe this would be sufficient for above? */
	int ctlfd;	/* command and control channel */
};

enum
{
	Cexec,
};

Cmdtab ctltab[]={
	Cexec,	"exec", 0,
};

/* call the execution wrapper */
static void
cloneproc(void *arg)
{
	Channel *pidc = arg;

	threadsetname("cloneproc");

	procexecl(pidc, "/bin/execcmd", "/bin/execcmd", srvctl, nil);
	fprint(2, "cloneproc: could not find execution wrapper\n");
	
	sendul(pidc, 0); /* failure */
	threadexits("no exection wrapper");
}

static int
mpipe(char *path, char *name)
{
	int fd, ret;
	fd = open("/srv/mpipe", ORDWR);
	if(fd<0) {
		fprint(2, "couldn't open /srv/mpipe: %r\n");
		return -1;
	}
	
	ret = mount(fd, -1, path, MAFTER, name);
	close(fd);
	return ret;
}

static void
fsopen(Req *r)
{
	Fid *f = r->fid;
	Exec *e;
	char *err;
	int n;
	char fname[255];	/* pathname buffer */
	char ctlbuf[255];	/* error string from wrapper */
	int p[2];
	int fd;
	Channel *pidc;

	if(f->file->aux != (void *)Xclone) {
		respond(r, nil);
		return;
	}

	e = emalloc9p(sizeof(Exec));
	memset(e, 0, sizeof(Exec));

	/* setup bootstrap ctlfd */
	pipe(p);
         fd = create(srvctl, OWRITE, 0666);
	if(fd < 0) {
		err = "couldn't create srv file";
		goto error;
	}

         fprint(fd, "%d", p[0]);
         close(fd);
         close(p[0]);
	e->ctlfd = p[1];
     
	/* ask for a new child process */
	pidc = chancreate(sizeof(ulong), 1);

	procrfork(cloneproc, pidc, STACK, RFCFDG); 

	e->pid = recvul(pidc);
	if(e->pid <= 0) {
		fprint(2, "problem during exec process: %r\n");
		err = Eexec;
		goto error;
	}

	assert(e->pid > 2);	/* assumption for our ctl channels */

	snprint(fname, 255, "/proc/%d", e->pid);
	/* bind things here boofhead */
	if(mpipe(fname, "stdin") < 0) {
		err = estrdup9p("stdin-pipe");
		goto error;
	}
	if(mpipe(fname, "stdout") < 0) {
		err = estrdup9p("stdout-pipe");
		goto error;
	}
	if(mpipe(fname, "stderr") < 0) {
		err = estrdup9p("stderr-pipe");
		goto error;
	}

	/* handshake to execcmd */
	n = write(e->ctlfd, "1", 1);
	if(n < 0) {
		err = estrdup9p(Ectlchan);
		goto error;	
	}	

	/* check that wrapper has open and dup'd stdio */
	n = read(e->ctlfd, ctlbuf, 255);
	if(n < 0) {
		err = estrdup9p(Ectlchan);
		goto error;	
	}
	if(n != 1) {
		ctlbuf[n] = 0;
		err = ctlbuf;
		goto error;
	}

	f->aux = e;
	respond(r, nil);
	return;

error:
	/* free channel */
	free(e);
	f->aux = nil;
	respond(r, err);
}

/* TODO: doesn't appear to do command relaying yet */
static void
fswrite(Req *r)
{
	Fid *f = r->fid;
	Exec *e = f->aux;
	char ctlbuf[256];
	int ret;
	
	if(e == 0) {
		respond(r, "crappy fid\n");
	}
	assert(e->ctlfd != -1);

	ret = write(e->ctlfd, r->ifcall.data, r->ifcall.count);
	if(ret < 0) {
		responderror(r);
		return;
	} else if(e->active) {	/* program already executing */
		goto success;
	}

	/* not active to read to get response */
	ret = read(e->ctlfd, ctlbuf, 255);
	if(ret < 0) {
		responderror(r);
		return;
	}

	if(ret == 0) {		/* other side exec'd - go active */
		close(e->ctlfd);
		e->active++;
		/* reopen ctlfd */
		snprint(ctlbuf, 255, "/proc/%d/ctl", e->pid);
		e->ctlfd = open(ctlbuf, OREAD);
		if(e->ctlfd < 0) {	/* may lead to a confusing error */
			fprint(2, "fswrite: couldn't open %s: %r\n", ctlbuf);
			responderror(r);
			return;
		}
		goto success;
	}

	/* convention: a single byte is sucess, multibyte is an error message */
	if(ret != 1) {
		ctlbuf[ret] = 0;
		respond(r, ctlbuf);
		return;
	}
success:
	r->ofcall.count = r->ifcall.count;
	respond(r, nil);
	return;
}

static void
fsread(Req *r)
{
	Fid *f = r->fid;
	Exec *e = f->aux;

	if(e->pid == 0)
		respond(r, Enopid);
	else {
		char buf[16];
		snprint(buf, 16, "%d", e->pid);
		readstr(r, buf);
		respond(r, nil);
	}	
}

static void
fsclunk(Fid *f)
{
	char fname[255];

	if(f->aux) {
		Exec *e = f->aux;

		if(!e->active) {
			if(e->ctlfd)
				close(e->ctlfd);

			snprint(fname, 255, "/proc/%d", e->pid);
			unmount(0, fname);

			/* if we have an outstanding inactive thread, kill it */
			postnote(PNPROC, e->pid, "kill");
			e->pid = -1;
			e->active = 0;
		}

		free(e);
		f->aux = nil;
	}
}

static void
cleanup(Srv *)
{
	threadexitsall("done");
}

/* handle certain ops in a separate thread in original namespace */
static void
iothread(void*)
{
	Req *r;

	threadsetname("execfs-iothread");
	for(;;) {
		r = recvp(iochan);
		if(r == nil)
			threadexits("interrupted");

		switch(r->ifcall.type){
		case Topen:
			fsopen(r);
			break;
		case Twrite:
			fswrite(r);
			break;
		default:
			fprint(2, "unrecognized io op %d\n", r->ifcall.type);
			break;
		}
	}
}

static void
ioproxy(Req *r)
{
	if(sendp(iochan, r) != 1) {
		fprint(2, "iochan hungup");
		threadexits("iochan hungup");
	}
}

Srv fs=
{
	.open=	ioproxy,
	.write=	ioproxy,
	.read=	fsread,
	.destroyfid=	fsclunk,
	.end=	cleanup,
};

void
threadmain(int argc, char **argv)
{
	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();

	if(argc)
		procpath = argv[0];
	else
		procpath = defaultpath;

	srvctl = smprint("/srv/execfs-%d", getpid());

	fs.tree = alloctree("execfs", "execfs", DMDIR|0555, nil);
	closefile(createfile(fs.tree->root, "clone", "execfs", 0666, (void *)Xclone));

	/* spawn off a io thread */
	iochan = chancreate(sizeof(void *), 0);
	proccreate(iothread, nil, STACK);

	threadpostmountsrv(&fs, nil, procpath, MAFTER);
	threadexits(0);
}
