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
		* maybe autostart mpipe when we need it (on-demand pipes)
		
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

static char Enopid[] =	"process not initialized";
static char Eoverflow[] = "ctl buffer overflow";
static char Ebadctl[] = "bad ctl message";
static char Ebadfid[] = "bad fid";
static char Eexec[] = "could not exec";
static char Eusage[] = "exec: invalid number of arguments";
static char Ectlchan[] ="problems with command channel to wrapper";
static char Ectlopen[] ="problems with opening command channel to wrapper";
static char Empipe[] = "problems with mpipe";
static char Epid[] = "stupid pid problem";
static char Esrv[] = "couldn't create srv file";
static char Ewtf[] = "I have no idea what could be going wrong here";

enum {	/* DEBUG LEVELS */
	DERR = 0,	/* error */
	DCUR = 1,	/* current - temporary trace */
	DFID = 9,	/* fid tracking */
	DARG = 10,	/* arguments */
};

static char defaultpath[] =	"/proc";
static char *procpath;
static char *srvctl;
static Channel *iochan;
static Channel *clunkchan;

static void
usage(void)
{
	fprint(2, "execfs [-D] [-v debuglevel] [mtpt]\n");
	exits("usage");
}

enum
{
	STACK = (8 * 1024),
	STRMAX = 255,
	Xclone = 1,
};

typedef struct Exec Exec;
struct Exec 
{
	int active;	/* setup=0, executing=1 */
	int pid;	/* maybe this would be sufficient for above? */
	int ctlfd;	/* command and control channel */
	int rctlfd;	/* actual control channel */
};

enum
{
	Cexec,
};

Cmdtab ctltab[]={
	Cexec,	"exec", 0,
};

static int
mpipe(char *path, char *name)
{
	int fd, ret;
	fd = open("/srv/mpipe", ORDWR);
	if(fd<0) {
		DPRINT(DERR, "*ERROR*: couldn't open /srv/mpipe: %r\n");
		return -1;
	}
	
	ret = mount(fd, -1, path, MAFTER, name);
	close(fd);
	return ret;
}

/* call the execution wrapper */
static void
cloneproc(void *arg)
{
	Channel *pidc = arg;

	threadsetname("execfs-cloneproc");

	DPRINT(DFID, "cloneproc (%p) (%s) (%s) (%s) (%s) (%s) (%s): %r\n",
		pidc, "/bin/execcmd", "execcmd", "-s", srvctl,
		  "-v", smprint("%d", vflag));
	procexecl(pidc, "/bin/execcmd", "execcmd", "-s", srvctl, "-v", 
		  smprint("%d", vflag), nil);
	
	sendul(pidc, 0); /* failure */
	threadexits(nil);
}

/* separated process creation to a function to help with debug */
static ulong
kickit(void)
{
	Channel *pidc = chancreate(sizeof(ulong), 0);
	ulong pid;

	procrfork(cloneproc, (void *) pidc, STACK, RFFDG);
	pid = recvul(pidc);
	if(pid==0) {
		DPRINT(DERR, "*ERROR*: Problem with cloneproc\n");
	}

	chanfree(pidc);

	return pid;
}

static void
fsopen(Req *r)
{
	int n;
	int p[2];
	int fd;
	Exec *e;
	char *err;
	Fid *f = r->fid;
	char *fname = (char *) emalloc9p(STRMAX);	/* pathname buffer */
	char *ctlbuf = (char *) emalloc9p(STRMAX);	/* error string from wrapper */

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
		err = estrdup9p(Esrv);
		goto error;
	}

	fprint(fd, "%d", p[0]);
	close(fd);
	close(p[0]);
	e->ctlfd = p[1];
     
	/* ask for a new child process */
	e->pid = (int) kickit();
	assert(e->pid > 2);	/* assumption for our ctl channels */

	/* grab actual reference to real control channel */
	n = snprint(fname, STRMAX, "/proc/%d/ctl", e->pid);
	assert(n > 0);
	
	/* grab ahold of the ctl file */
	if((e->rctlfd = open(fname, OWRITE)) < 0) {
		DPRINT(DERR, "*ERROR*: opening %s failed: %r\n", fname);
		err = smprint("execfs: fsopen: opening [%s] failed: %r", fname);
		goto error;
	}

	n = snprint(fname, STRMAX, "/proc/%d", e->pid);
	assert(n > 0);

	/* asserts are heavy handed, but help with debug */
	n = mpipe(fname, "stdin");
	assert(n > 0);
	n = mpipe(fname, "stdout");
	assert(n > 0);
	n = mpipe(fname, "stderr");
	assert(n > 0);

	/* handshake to execcmd */
	n = write(e->ctlfd, "1", 1);
	if(n < 0) {
		err = estrdup9p(Ectlchan);
		goto error;	
	}

	/* check for partial success: execcmd has opened stdio */
	n = read(e->ctlfd, ctlbuf, STRMAX);
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
	free(fname);
	free(ctlbuf);
	respond(r, nil);
	return;

error:
	free(fname);
	if(err != ctlbuf)
		free(ctlbuf);
	/* free channel */
	free(e);
	f->aux = nil;
	respond(r, err);
	free(err);
}

static void
fswrite(Req *r)
{
	Fid *f = r->fid;
	Exec *e = f->aux;
	char ctlbuf[256];
	int ret;
	
	if(e == 0) {
		respond(r, Ebadfid);
	}
	assert(e->ctlfd != -1);

	ret = write(e->ctlfd, r->ifcall.data, r->ifcall.count);
	if(ret < 0) {
		responderror(r);
		return;
	} else if(e->active) {	/* program already executing */
		goto success;
	}

	/* not active yet: read to get response */

	/* this assumes that the only thing expected to be written is
	 * the exec cmd if the session is not yet active. */
	ret = read(e->ctlfd, ctlbuf, STRMAX);
	if(ret < 0) {
		responderror(r);
		return;
	}
	
	if(ret == 0) {		/* other side exec'd - go active */
		close(e->ctlfd);
		/* replace our channel with the real control channel */
		e->ctlfd = e->rctlfd;
		e->active++;

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
cleanupsession(void *arg)
{
	int pid = (int) arg;
	char *path = (char *) emalloc9p(STRMAX);

	/* BUG: what if we can't get to them because proc is already closed? */
	snprint(path, STRMAX, "/proc/%d", pid);
	unmount(0, path);
	postnote(PNPROC, pid, "kill");

	free(path);
}

static void
fsclunk(Fid *f)
{
	DPRINT(DFID, "fsclunk: %p fid=%d aux=%p\n", f, f->fid, f->aux);
	if(f->aux) {
		Exec *e = f->aux;

		if(e->ctlfd)
			close(e->ctlfd);

		proccreate(cleanupsession, (void *)e->pid, STACK);

		e->pid = -1;
		e->active = 0;

		free(e);
		f->aux = nil;
	}
}

static void
cleanup(Srv *)
{
	chanfree(iochan);
	chanfree(clunkchan);
	free(srvctl);
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
		case Tclunk:
			fsclunk(r->fid);
			sendp(clunkchan, r);
			break;
		default:
			DPRINT(DERR, "*ERROR*: unrecognized io op %d\n", r->ifcall.type);
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

static void
clunkproxy(Fid *f)
{
	/* really freaking clunky, but not sure what else to do */
	Req *r = emalloc9p(sizeof(Req));

	DPRINT(DFID, "clunkproxy: %p fidnum=%d aux=%p\n", f, f->fid, f->aux);

	r->ifcall.type = Tclunk;
	r->fid = f;
	if(sendp(iochan, r) != 1) {
		fprint(2, "iochan hungup");
		threadexits("iochan hungup");
	}
	recvp(clunkchan);

	free(r);
}

Srv fs=
{
	.open=		ioproxy,
	.write=		ioproxy,
	.read=		fsread,
	.destroyfid=clunkproxy,
	.end=		cleanup,
};

void
threadmain(int argc, char **argv)
{
	char *x;

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'v':
		x = ARGF();
		if(x)
			vflag = atoi(x);
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
	clunkchan = chancreate(sizeof(void *), 0);
	proccreate(iothread, nil, STACK);

	threadpostmountsrv(&fs, nil, procpath, MAFTER);
	threadexits(0);
}
