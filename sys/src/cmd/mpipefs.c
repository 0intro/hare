/*
	pipefs - userspace multipipe implementation

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

	Description:
		This is a user space implementation of our
		multipipe concept intended to more easily
		experiment with ideas prior to attempting a
		more efficient kernel version.

	Notes:
		more common use case for this is to just
		post at a srv point versus to actually
		mount itself due to nature of instance per
		attach.

*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>

enum
{
	STACK = 8192,
};

Channel *iochan;		/* dispatch channel */

char defaultsrvpath[] = "mpipe";

char Enotdir[] = "walk in non-directory";
char Enotfound[] = "directory entry not found";
char Eoneorother[] = "open read or write, not both";
char Eshort[] = "short write";
char Ehangup[] = "pipe hang up";

typedef struct Mpipe Mpipe;
struct Mpipe {
	QLock l;
	int ref;

	ulong path;
	char *name;
	char *uid;
	char *gid;
	ulong len;

	int writers;	/* number of writers */
	int readers;	/* number of readers */
	Channel *chan;	/* of Reqs */

	/* rendez, qlock? */
	/* TODO: add channel queues */
};

static struct
{
	Lock l;
	ulong	path;
} pipealloc;

static void
usage(void)
{
	fprint(2, "mpipefs [-D] [-s <srvpt>] [mtpt]\n");
	threadexits("usage");
}

void
killall(Srv*)
{
	threadexitsall("killall");
}

#define NETTYPE(x)	((ulong)(x)&0x1f)
#define NETID(x)	(((ulong)(x))>>5)
#define NETQID(i,t)	(((i)<<5)|(t))

enum
{
	Qdir,
	Qdata,
};

char MPdefault[] = "data";

/* TODO: eventually pull other parameters from the spec/aname */
static void
fsattach(Req *r)
{
	Mpipe *mp = emalloc9p(sizeof(Mpipe));
	
	memset(mp, 0, sizeof(Mpipe));
	if(strlen(r->ifcall.aname) == 0)
		mp->name = estrdup9p(MPdefault);
	else
		mp->name = estrdup9p(r->ifcall.aname);

	lock(&pipealloc.l);
	mp->path = ++pipealloc.path;
	unlock(&pipealloc.l);

	r->fid->qid = (Qid){NETQID(2*mp->path, Qdir), 0, QTDIR};
	r->ofcall.qid = r->fid->qid;

	r->fid->aux = mp;
	mp->uid = estrdup9p(getuser());
	mp->gid = estrdup9p(mp->uid);

	mp->chan = chancreate(sizeof(Req *), 0); /* no buffering for now */

	mp->ref++;

	respond(r, nil);
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	ulong path;
	Mpipe *mp = fid->aux;

	path = fid->qid.path;
	if(!(fid->qid.type&QTDIR))
		return Enotdir;

	/* there's only one dir, so just compare names */
	if(strcmp(name, mp->name) == 0) {
		qid->path = NETQID(path, Qdata);
		qid->type = QTFILE;
		return nil;
	}

	return Enotfound;
}

static char*
fsclone(Fid* fid, Fid* newfid)
{
	Mpipe *mp = fid->aux;

	newfid->aux = fid->aux;
	mp->ref++;

	return nil;
}

static void
fsopen(Req *r)
{
	Fid*	fid = r->fid;
	Qid	q = fid->qid;
	Mpipe *mp = fid->aux;

	if (q.type&QTDIR){
		respond(r, nil);
		return;
	}

	if((r->ifcall.mode&3) == ORDWR) {
		respond(r, Eoneorother);
		return;
	}

	if((r->ifcall.mode&0x3) ==  OWRITE)
		mp->writers++;
	if(r->ifcall.mode==0)
		mp->readers++;

	respond(r, nil);
}

static int
getdirent(int n, Dir* d, void *aux)
{
	Mpipe *mp = aux;

	if(aux == nil)
		return -1;

	d->atime= time(nil);
	d->mtime= d->atime;
	d->uid = estrdup9p(mp->uid);
	d->gid = estrdup9p(mp->gid);
	d->muid= estrdup9p(d->uid);

	if(n == -1) {
		d->qid = (Qid){NETQID(mp->path, Qdir), 0, QTDIR};
		d->mode = 0775;
		d->name = estrdup9p("/");
		d->length = 0;
	} else if (n==0) {
		d->qid = (Qid){NETQID(mp->path, Qdata), 0, QTFILE};
		d->mode = 0666;
		d->name = estrdup9p(mp->name);
		d->length = mp->len;
	} else {
		return -1;
	}
	return 0;
}

/*
 * This is oversimplified and won't work if
 * read len isn't greater than or equal to write len
 *
 */
static void
fsread(void *arg)
{
	Req *r = arg;
	Fid*	fid = r->fid;
	Mpipe *mp = fid->aux;
	int count;
	Req *tr;
	Qid	q = fid->qid;

	if (q.type&QTDIR){
		dirread9p(r, getdirent, mp);
		respond(r, nil);
		threadexits(nil);
	}

	tr = recvp(mp->chan);
	if(tr == 0) { /* return with 0 */
		respond(r, nil);
		threadexits(nil);
	}
	if(tr->ifcall.count > r->ifcall.count) {
		/* temporary */
		respond(tr, Eshort);
		respond(r, Eshort);
		threadexits(Eshort);
	}
	if(r->ifcall.count > tr->ifcall.count)
		count = tr->ifcall.count;
	else
		count = r->ifcall.count; 

	memcpy(r->ofcall.data, tr->ifcall.data, count);
	tr->ofcall.count = count;
	r->ofcall.count = count;
	mp->len -= count;

	respond(tr, nil);
	respond(r, nil);
	threadexits(nil);
}

/*
 * This is oversimplified and won't work if
 * read len isn't greater than or equal to write len
 *
 */
static void
fswrite(void *arg)
{
	Req *r = arg;
	Fid*	fid = r->fid;
	Mpipe *mp = fid->aux;
	
	mp->len += r->ifcall.count;

	sendp(mp->chan, arg);
	threadexits(nil);
}

static void
fsstat(Req* r)
{
	Fid*	fid = r->fid;
	Qid	q = fid->qid;
	Mpipe *mp = fid->aux;
	
	fid = r->fid;
	q = fid->qid;
	if (q.type&QTDIR)
		getdirent(-1, &r->d, mp);
	else
		getdirent(q.path, &r->d, mp);

	respond(r, nil);
}

static void
fsclunk(Fid *fid)
{
	if(fid->aux) {
		Mpipe *mp = fid->aux;
		mp->ref--;
	
		if(mp->ref == 0) { /* TODO: probably want to do more here */
			free(mp->name);
			free(mp->uid);
			free(mp->gid);
			chanfree(mp->chan);
			free(mp);
		}

		if((fid->omode&0x3) == OWRITE) {
			mp->writers--;
			if(mp->writers == 0) {
				chanclose(mp->chan);
			}
		} 

		if((fid->omode == 0)&&(fid->qid.type==QTFILE)) {
			mp->readers--;
			if((mp->writers == 0)&&(mp->readers == 0)) {
				/* reset pipe */
				chanfree(mp->chan);
				mp->chan = chancreate(sizeof(Req *), 0); /* no buffering for now */
			}
		}
	}

	fid->aux = nil;
}

/* handle potentially blocking ops in their own proc */

static void
iothread(void*)
{
	Req *r;

	threadsetname("mpipfs-iothread");
	for(;;) {
		r = recvp(iochan);
		if(r == 0)
			threadexits("interrupted");

		switch(r->ifcall.type){
		case Tread:
			threadcreate(fsread, r, STACK);
			break;
		case Twrite:
			threadcreate(fswrite, r, STACK);
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
	sendp(iochan, r);
}

Srv fs=
{
	.attach=			fsattach,
	.walk1=			fswalk1,
	.clone=			fsclone,
	.open=			fsopen,
	.read=			ioproxy,
	.write=			ioproxy,
	.stat	=			fsstat,
	.destroyfid=		fsclunk,
	.end=			killall,
};

void
threadmain(int argc, char **argv)
{
	char *srvpath = defaultsrvpath;
	char *mntpath = nil;

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 's':
		srvpath = ARGF();
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();

	if(argc)
		mntpath = argv[0];

	/* spawn off a dispatcher */
	iochan = chancreate(sizeof(void *), 0);
	proccreate(iothread, nil, STACK);

	threadpostmountsrv(&fs, srvpath, mntpath, MBEFORE);

	threadexits(nil);
}
