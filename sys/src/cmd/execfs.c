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

		Not clear that we need anything else at the outset,
		will need to review notes.

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

char Enopid[] =	"process not initialized";
char Eoverflow[] = "ctl buffer overflow";
char Ebadctl[] = "bad ctl message";
char Eexec[] = "could not exec";

char 	defaultpath[] =	"/proc";
char *procpath;

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

	procexecl(pidc, cb->f[1], cb->f[1], nil);
}

static char *
myexec(Exec *e, Cmdbuf *cb)
{
	char *err = nil;

	/* TODO: check for at least one arg */
	/* TODO: this is where we'll setup the pipes */
	
	pidc = chancreate(sizeof(ulong), 1);

	proccreate(cmdproc, cb, STACK);
	e->pid = recvul(pidc);
	print("started a new proc pid=%d\n", e->pid);
	
	if(e->pid < 0)
		err = estrdup9p(Eexec);

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
		print("exec\n"); /* DEBUG */
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

Srv fs=
{
	.open=	fsopen,
	.write=	fswrite,
	.read=	fsread,
	.destroyfid=	fsclunk,
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
		procpath = defaultpath;;


	fs.tree = alloctree("execfs", "execfs", DMDIR|0555, nil);
	closefile(createfile(fs.tree->root, "clone", "execfs", 0666, nil));

	threadpostmountsrv(&fs, nil, procpath, MBEFORE);

	threadexits(0);
}
