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

	BUGS:
		* none yet
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

char 	defaultpath[] =	"/proc";
char *procpath;
char *tmpfiles;
Channel *binderchan;

static void
usage(void)
{
	fprint(2, "execfs [-D] [mtpt]\n");
	exits("usage");
}

enum
{
	STACK = (8 * 1024),
};

typedef struct Exec Exec;
struct Exec 
{
	int pid;	/* maybe this would be sufficient for above? */
	int ctlfd;	/* file descriptor of actual fid */
	/* do we need to keep pipes? */
};
	
static void
fsopen(Req *r)
{
	Fid *f = r->fid;
	if(f->aux != nil)
		threadexitsall("wtf");

	f->aux = emalloc9p(sizeof(Exec));
	memset(f->aux, 0, sizeof(Exec));
	respond(r, nil);
}

enum
{
	Cexec,
};

Cmdtab ctltab[]={
	Cexec,	"exec", 0,
};

/* global for now */
Channel *pidc;

static void
cmdproc(void *arg)
{
	Cmdbuf *cb = arg;
	
	if(sendul(binderchan, 1) < 0) { /* bind initial pipes into place */	
		fprint(2, "cmdproc: can't send to binderchan: %r\n");
		threadexits("binderchan send failed");
	}
	if(recvul(binderchan) != 1) {
		fprint(2, "cmdproc: can't recv from binderchan: %r\n");
		threadexits("binderchan response failed");
	}

	print("cmdproc pid=%d\n", getpid());
	/* TODO: do the dup thing */

	procexec(pidc, cb->f[1], &cb->f[1]);
}

static void
binderthread(void *arg)
{
	Channel *cmds = arg;
	unsigned long pid;
	char fname[255];

	while(1) {
		pid = recvul(cmds);
		switch(pid) {
		case -1:
		case 0:			/* interrupt */
			threadexits("exit requested");
			break;
		case 1:			/* prep initial bind */
			/* bind pipes in place assuming mntgen is running */
			if(bind("#|", "/n/stdin", MREPL) < 0)
				goto nomntgen;

			if(bind("#|", "/n/stdout", MREPL) < 0)
				goto nomntgen;

			if(bind("#|", "/n/stderr", MREPL) < 0)
				goto nomntgen;
			
			if(sendul(cmds, 1) < 0)
				threadexits("pipehangup");
			break;
		 default:			/* we got a pid */
			snprint(fname, 255, "%s/stdin", tmpfiles);
			if(bind("/n/stdin/data", fname, MREPL) < 0)
				goto bindissues;
			snprint(fname, 255, "%s/stdout", tmpfiles);
			if(bind("/n/stdout/data1", fname, MREPL) < 0)
				goto bindissues;
			if(bind("/n/stderr/data1", fname, MREPL) < 0)
				goto bindissues;
			snprint(fname, 255, "/proc/%lud", pid);
			print("about to bind %s %s\n", tmpfiles, fname);
			if(bind(tmpfiles, fname, MAFTER) < 0) {
				fprint(2, "binderthread: can't bind to proc: %r\n");
				threadexits("proc bind problems");	
			}
			if(sendul(cmds, 1) < 0)
				threadexits("pipehangup");
		}
	}

bindissues:
	fprint(2, "binderthread: can't bind to piddir: %r\n");
	threadexits("piddir bind problems");	
nomntgen:
	fprint(2, "binderthread: can't bind to /n: %r\n");
	threadexits("no mntgen");	
}

static char *
myexec(Exec *e, Cmdbuf *cb)
{
	char *err = nil;

	pidc = chancreate(sizeof(ulong), 1);

	proccreate(cmdproc, cb, STACK);
	e->pid = recvul(pidc);
	if(e->pid < 0)
		err = estrdup9p(Eexec);

	/* bugger, we need a way to get stdin, stdout, stderr names - this isn't going to work */
	if(sendul(binderchan, e->pid) < 0) { /* bind over proc */	
		fprint(2, "myexec: can't send to binderchan: %r\n");
		threadexits("binderchan send failed");
	}
	if(recvul(binderchan) != 1) {
		fprint(2, "myexec: can't recv from binderchan: %r\n");
		threadexits("binderchan response failed");
	}	

	return err;
}

static void
fswrite(Req *r)
{
	Fid *f = r->fid;
	Exec *e = f->aux;
	char *ctlbuf;
	char *err = nil;
	Cmdbuf *cb;
	Cmdtab *cmd;
	int ret;

	ctlbuf = emalloc9p(r->ifcall.count+1);
	strncpy(ctlbuf, r->ifcall.data, r->ifcall.count);
	ctlbuf[r->ifcall.count] = 0;

	cb = parsecmd(ctlbuf, strlen(ctlbuf));
	cmd = lookupcmd(cb, ctltab, nelem(ctltab));
	if(cmd == nil) {
		if(e->ctlfd == 0){
			err = Ebadctl;
			goto error;
		}
		/* pass through unknown commands to actual ctl file */
		ret = write(e->ctlfd, r->ifcall.data, r->ifcall.count);
		if(ret < 0) {
			free(cb);
			free(ctlbuf);
			responderror(r);
			return;	
		}	
		if(ret < r->ifcall.count) {
			r->ofcall.count = ret;
			goto error;
		}	
	}

	switch(cmd->index){
	case Cexec:
		if(cb->nf < 2) {
			err=Eusage;
			goto error;
		}
		err = myexec(e, cb);
		if(err == nil)
			r->ofcall.count = r->ifcall.count;
		else 
			r->ofcall.count = -1;
		break;
	/* room for more commands later */
	}

error:
	free(cb);
	free(ctlbuf);
	respond(r, err);
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
	if(f->aux)
		free(f->aux);
}

static void
createstdiofiles(void)
{
	char fname[255];
	int fd;

	tmpfiles = estrdup9p(tmpnam(nil));
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
}


static void
cleanup(Srv *)
{
	char fname[255];

	snprint(fname, 255, "%s/stdin", tmpfiles);
	remove(fname);
	snprint(fname, 255, "%s/stdout", tmpfiles);
	remove(fname);
	snprint(fname, 255, "%s/stderr", tmpfiles);
	remove(fname);
	remove(tmpfiles);
	free(tmpfiles);
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

	createstdiofiles();	/* assumes no one will mess with them */

	fs.tree = alloctree("execfs", "execfs", DMDIR|0555, nil);
	closefile(createfile(fs.tree->root, "clone", "execfs", 0666, nil));

	binderchan=chancreate(sizeof(unsigned long), 0);
	proccreate(binderthread, binderchan, STACK);

	threadpostmountsrv(&fs, nil, procpath, MBEFORE);

	threadexits(0);
}
