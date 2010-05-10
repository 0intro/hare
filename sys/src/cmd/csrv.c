/*
	csrv - central services distributed registry infrastructure

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

	Description:
		This sets up the per-node mount hierarchy for aggregated
		distributed services.  Since name space operations are 
		performed in the context of the original application they
		are reflected in all subsequent mounts and exports.

	BUGS:
		* ctl operations are single threaded, hangs on errors
		* mount is particularly brittle
		* cleanup usually misses mntgen
*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>

char Eperm[] =		"permission denied";
char Ebadctl[] = "bad ctl command or arguments";
char Ebind[] = "bind failed";

ulong	mntgenpid;
char 	defaultcsrvpath[] =	"/csrv";
char *csrvpath;
Channel *ctlchan, *ctlrespchan;

static void
usage(void)
{
	fprint(2, "csrv [-D] [mtpt]\n");
	exits("usage");
}

enum
{
	STACK = 2048,
};

enum
{
	Cmount,
	Cbind,
	Cunmount,
	Cexit,
};

Cmdtab ctltab[]={
	Cmount,	"mount", 3,
	Cbind,	"bind", 3,
	Cunmount,"unmount", 2,
	Cexit,	"exit", 1,
};	

void
killall(Srv*)
{
	send(ctlchan, "exit");
	unmount(0, csrvpath);
	threadexitsall("killall");
}

static void
fswrite(Req *r)
{
	char buf[255];

	if(r->ifcall.count > 254)
		respond(r, "ctl buffer overflow");

	strncpy(buf, r->ifcall.data, r->ifcall.count);
	buf[r->ifcall.count] = 0;

	if(send(ctlchan, buf) < 0) {
		respond(r, "send ctl chan error");
		return;
	}

	if(recv(ctlrespchan, buf) < 0) {
		respond(r, "recv ctl chan error");
		return;
	}
	
	if(buf[0] != 0)
		respond(r, buf);
	else
		respond(r, nil);
}

Srv fs=
{
	.write=			fswrite,
	.end=			killall,
};

enum
{
	Xctl	= 1,
};

/* start up a mntgen for us to work with */
static void
mntgen(void *arg)
{
	char *path = (char *) arg;

	procexecl(0, "/bin/mntgen", "/bin/mntgen", path, 0);

	threadexits("exec");
}

/* If anything goes wrong here its a pain */
/* TODO: move to threads with timeouts or something */
static void
mountit(char *dest, char *old)
{
	char dir[1024];
	int fd;

	dest = netmkaddr(dest, 0, "9fs");
	fd = dial(dest, 0, dir, 0);
	if(fd < 0) {
		fprint(2, "csrv: dial %s: %r\n", dest);
		return;
	}
	if(mount(fd, -1, old, MREPL, "") < 0)
		fprint(2, "csrv: mount on %s: %r\n", old);
}

static void
ctlworker(void *)
{
	char ctlbuf[255];
	char newpath[255];
	Cmdbuf *cb;
	Cmdtab *cmd;

	while(1) {
		if(recv(ctlchan, ctlbuf) < 0)
			threadexits("ctlworker rcvchan error");
		cb = parsecmd(ctlbuf, strlen(ctlbuf));
		cmd = lookupcmd(cb, ctltab, nelem(ctltab));
		if(cmd == nil) {
			send(ctlrespchan, Ebadctl);
			continue;
		}
		send(ctlrespchan, "\0"); /* success */

		switch(cmd->index){
		case Cmount:
			snprint(newpath, 255, "%s/%s", csrvpath, cb->f[2]);
			mountit(cb->f[1], newpath);	
			break;
		case Cbind:
			snprint(newpath, 255, "%s/%s", csrvpath, cb->f[2]);
			if(bind(cb->f[1], newpath, MREPL) < 0)
				fprint(2, "csrv: bind %s %s: %r\n", cb->f[1], newpath);
			break;
		case Cunmount:
			snprint(newpath, 255, "%s/%s", csrvpath, cb->f[1]);
			unmount(0, newpath);
			break;
		case Cexit:
			unmount(0, csrvpath);
			threadexitsall("exitrequested");
		}	
	}

}

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
		csrvpath = argv[0];
	else
		csrvpath = defaultcsrvpath;


	fs.tree = alloctree("csrv", "csrv", DMDIR|0555, nil);
	closefile(createfile(fs.tree->root, "ctl", "csrv", 0222, (void *)Xctl));

	ctlchan = chancreate(255, 0);	/* ctl command channel */
	ctlrespchan = chancreate(255, 0); /* response channel */

	proccreate(mntgen, csrvpath, STACK);

	threadpostmountsrv(&fs, nil, csrvpath, MBEFORE);

	proccreate(ctlworker, nil, STACK);

	exits(0);
}
