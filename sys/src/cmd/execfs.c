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
		* none
		
	BUGS:
		* none

*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>
#include <stdio.h>

char Enopid[] =	"process not initialized";
char Eoverflow[] = "ctl buffer overflow";
char Ebadctl[] = "bad ctl message";
char Eexec[] = "could not exec";
char Eusage[] = "exec: invalid number of arguments";
char Edispatch[] = "problems communicating with dispatcher";
char Ectlchan[] ="problems with command channel to wrapper";
char Ectlopen[] ="problems with opening command channel to wrapper";

char 	defaultpath[] =	"/proc";
char *procpath;
Channel *dispatchc;
char basetmp[] ="/tmp/execfs";
char *srvctl;

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

static void
unbind(void)
{
	unmount(0, "/n/stdin");
	unmount(0, "/n/stdout");
	unmount(0, "/n/stderr");
}

static char *
createstdiofiles(char *base, unsigned long pid)
{
	char fname[255];
	int fd;
	char *tmpfiles;
	int len = strlen(base)+10;

	tmpfiles = emalloc9p(len);
	snprint(tmpfiles, len, "%s/io-%lud", base, pid);

	if((fd = create(tmpfiles, OREAD, DMDIR|0777)) < 0) {
		fprint(2, "couldn't create temporary directory for I/O templates: %s: %r\n", tmpfiles);
		threadexitsall("no /tmp");
	}
	close(fd);

	snprint(fname, 255, "%s/stdin", tmpfiles);
	close(create(fname, OWRITE, 0222));
	snprint(fname, 255, "%s/stdout", tmpfiles);
	close(create(fname, OWRITE, 0444));
	snprint(fname, 255, "%s/stderr", tmpfiles);
	close(create(fname, OWRITE, 0444));

	return tmpfiles;
}
	
/* dispatcher thread - handle setup and tear down of children*/
static void
dispatcher(void *arg)
{
	Channel *c = arg;
	unsigned long cmd;
	unsigned long pid;
	Channel *pidc;
	char *tmpfiles;
	char fname[255];

	threadsetname("execfsdispatch");

	while(1) {
		cmd = recvul(c);
		switch(cmd) {
			case ~0:
				threadexits("exit requested");
				break;
			case 0:
				threadexits("abort");
				break;
			case 1:	/* request a clone */
				pidc = chancreate(sizeof(ulong), 1);

				if(bind("#|", "/n/stdin", MREPL) < 0)
					goto nomntgen;
				if(bind("#|", "/n/stdout", MREPL) < 0)
					goto nomntgen;
				if(bind("#|", "/n/stderr", MREPL) < 0)
					goto nomntgen;

				procrfork(cloneproc, pidc, STACK, RFNAMEG|RFCFDG); 

				pid = recvul(pidc);
				if(pid <= 0) {
					unbind();
					sendul(c, 0);
					continue;
				}

				assert(pid > 2);	/* assumption for our ctl channels */
				tmpfiles = createstdiofiles(basetmp, pid);	
fprint(2, "binding over %s", tmpfiles);
				snprint(fname, 255, "%s/stdin", tmpfiles);
				if(bind("/n/stdin/data", fname, MREPL) < 0)
					goto bindissues;
				snprint(fname, 255, "%s/stdout", tmpfiles);
				if(bind("/n/stdout/data1", fname, MREPL) < 0)
					goto bindissues;
				snprint(fname, 255, "%s/stderr", tmpfiles);
				if(bind("/n/stderr/data1", fname, MREPL) < 0)
					goto bindissues;

				snprint(fname, 255, "/proc/%lud", pid);
fprint(2, "binding %s AFTER %s", tmpfiles,  fname);
				if(bind(tmpfiles, fname, MAFTER) < 0) {
					fprint(2, "dispatch: can't bind to proc: %r\n");
					threadexits("proc bind problems");	
				}
				unbind();
				free(tmpfiles);

				/* success */
				if(sendul(c, pid) < 0)
					threadexits("pipehangup");
				break;
			default:	/* cleanup a pid */
				unbind();
			
				if(sendul(c, 1) < 0)
					threadexits("pipehangup");			
		}
	}

nomntgen:
	fprint(2, "couldn't bind to /n/stdios: %r\n");
	chanfree(pidc);
	threadexits("nomntgen");

bindissues:
	fprint(2, "bind: can't bind to piddr: %r\n");
	threadexits("procbind");	
}

static void
fsopen(Req *r)
{
	Fid *f = r->fid;
	Exec *e;
	char *err;
	int n;
	char ctlbuf[255];	/* error string from wrapper */
	int p[2];
	int fd;

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
	if(sendul(dispatchc, 1) < 0) { 
		err = estrdup9p(Edispatch);
		goto error;
	}

	e->pid = recvul(dispatchc);
	if(e->pid == 0) {
		err = estrdup9p(Edispatch);
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

			if(sendul(dispatchc, e->pid) > 0)
				recvul(dispatchc);

			snprint(fname, 255, "/proc/%lud", e->pid);
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
cleantmp(void *)
{
	threadsetname("cleantmp");

	procexecl(0, "/bin/rm", "/bin/rm", "-rf", basetmp, nil);
}

static void
cleanup(Srv *)
{
	proccreate(cleantmp, 0, STACK);
}

Srv fs=
{
	.open=	fsopen,
	.write=	fswrite,
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

	close(create(basetmp, OREAD, DMDIR|0777));	/* create base tmp */

	fs.tree = alloctree("execfs", "execfs", DMDIR|0555, nil);
	closefile(createfile(fs.tree->root, "clone", "execfs", 0666, (void *)Xclone));

	dispatchc=chancreate(sizeof(unsigned long), 0);
	proccreate(dispatcher, dispatchc, STACK);

	threadpostmountsrv(&fs, nil, procpath, MAFTER);

	threadexits(0);
}
