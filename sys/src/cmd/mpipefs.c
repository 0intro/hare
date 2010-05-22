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

	Bugs:
		Flushes and interruptions are probably going
		to break everything right now.  Need to add
		code to handle such situations in a reasonable
		fashion.

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
char Eremain[] = "new header when bytes still remaining";
char Eother[] = "Problem with peer";


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

	Channel *rrchan;	/* ready to read */
};

typedef struct Fidaux Fidaux;
struct Fidaux {
	QLock l;
	Mpipe *mp;

	/* only for writer */
	Fid *other;		/* peer Fid for long writes */
	int remain;		/* bytes remaining in multi-block */
	
	/* only for reader */
	Req *r;			/* for partial reads */
	Channel *chan;
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
	chanclose(iochan);
	chanfree(iochan);
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

/* tiny helper, why don't we have this generically? */
static void *
emalloc9pz(int sz, int zero)
{
	void *v = emalloc9p(sz);
	if(zero)
		memset(v, 0, sz);
	return v;
}

static Mpipe *
setfidmp(Fid *fid) /* blech */
{
	if(fid->qid.type&QTDIR)
		return fid->aux;

	if(fid->omode == -1)
		return fid->aux;
	else
		return ((Fidaux *)fid->aux)->mp;
}

static Fidaux *
setfidaux(Fid *fid) /* blech */
{
	if(fid->qid.type&QTDIR)
		return nil;

	if(fid->omode == -1)
		return nil;
	else
		return fid->aux;
}

/* MAYBE: eventually pull other parameters from the spec/aname */
static void
fsattach(Req *r)
{
	Mpipe *mp = emalloc9pz(sizeof(Mpipe), 1);
	
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

	mp->rrchan = chancreate(sizeof(Fid *), 0);

	mp->ref++;

	respond(r, nil);
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	ulong path;
	Mpipe *mp = setfidmp(fid);

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
	Mpipe *mp = setfidmp(fid);

	newfid->aux = fid->aux;
	mp->ref++;

	return nil;
}

static void
fsopen(Req *r)
{
	Fid*	fid = r->fid;
	Qid q = fid->qid;
	Mpipe *mp = setfidmp(fid);
	Fidaux *aux;

	if (q.type&QTDIR){
		respond(r, nil);
		return;
	}

	if((r->ifcall.mode&OMASK) == ORDWR) {
		respond(r, Eoneorother);
		return;
	}

	aux = emalloc9pz(sizeof(Fidaux), 1);
	aux->mp = mp;
	fid->aux = aux;
	if((r->ifcall.mode&OMASK) ==  OWRITE)
		mp->writers++;
	
	/* MAYBE: unclear how to assign readers to subgroups */
	if((r->ifcall.mode&OMASK)==0) { /* READER */
		mp->readers++;
		aux->chan = chancreate(sizeof(Req *), 0);
	}

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

/* copy from transmitter to receiver */
static int
fscopy(Req *tr, Req *r, int offset)
{
	int count = tr->ifcall.count - offset;
	
	if(count > r->ifcall.count)
			count = r->ifcall.count;	/* truncate */

	memcpy(r->ofcall.data+offset, tr->ifcall.data, count);
	r->ofcall.count = count;
	
	return count;
}

static void
fsread(void *arg)
{
	Req *r = arg;
	Fid*	fid = r->fid;
	Fidaux *aux = setfidaux(fid);
	Fidaux *raux;
	Mpipe *mp = setfidmp(fid);
	int count;
	Req *tr;
	int offset;
	Qid	q = fid->qid;

	if (q.type&QTDIR) {
		dirread9p(r, getdirent, mp);
		respond(r, nil);
		threadexits(nil);
	}

	/* If remainder, serve that first */
	if(aux->r) {
		tr = aux->r;
		offset = (int) aux->r->aux;
	 } else {
		if(aux->other == nil) {
			if(sendp(mp->rrchan, fid) != 1) {
				respond(r, nil);
				threadexits(nil);
			}
		}
		tr = recvp(aux->chan);
		offset = 0;
		if(tr == nil) {
			respond(r, nil);
			threadexits(nil);
		}
	}
	raux = aux->other->aux;
	count = fscopy(tr, r, offset);
	raux->remain -= count;
	if(raux->remain == 0)
	mp->len -= count;
	offset += count;

	if(offset == tr->ifcall.count) {
		if(raux->remain == 0) { /* done for this pkt */
			raux->other = nil;
			aux->other = nil;
		}
		tr->aux = 0;
		aux->r = nil;
		tr->ofcall.count = tr->ifcall.count;
		respond(tr, nil);
		respond(r, nil);
	} else {
		tr->aux = (void *) offset;
		aux->r = tr;
		respond(r, nil);
	}

	threadexits(nil);
}

static void
fswrite(void *arg)
{
	Req *r = arg;
	Fid *fid = r->fid;
	Fidaux *raux, *aux = setfidaux(fid);
	Mpipe *mp = setfidmp(fid);
	ulong hdrtag = ~0;

	if(r->ifcall.offset == hdrtag) {
		if(aux->remain) {
			respond(r,Eremain);
			goto out;
		}
		aux->remain = atoi(r->ifcall.data);
		/* MAYBE: we don't pass this on do we? */
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		goto out;
	} else {
		mp->len += r->ifcall.count;
	}

	if(aux->other == nil) {
		aux->other = recvp(mp->rrchan);
		if(aux->other == nil) {
			respond(r, Ehangup);
			goto out;
		}
		raux = aux->other->aux;
		if(raux->other != nil) {
			respond(r, Eother);
			goto out;
		}
		raux->other = fid;
	} else {
		raux = aux->other->aux;
		if(raux->other != fid) {
			respond(r, Eother);
			goto out;
		}
	}
			
	assert(raux->chan != 0);
	if(sendp(raux->chan, r) != 1)
		respond(r, Ehangup);

out:
	threadexits(nil);
}

static void
fsstat(Req* r)
{
	Fid*	fid = r->fid;
	Qid	q = fid->qid;
	Mpipe *mp = setfidmp(fid);
	
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
	Mpipe *mp = setfidmp(fid);

	if(fid->aux) {
		Fidaux *aux = setfidaux(fid);	
		mp->ref--;
	
		if(mp->ref == 0) { 
			free(mp->name);
			free(mp->uid);
			free(mp->gid);
			if(mp->rrchan) {
				chanclose(mp->rrchan);
				chanfree(mp->rrchan);
			}
			free(mp);
		}

		if((fid->omode&OMASK) == OWRITE) {
			/* MAYBE: mark outstanding requests crap */

			mp->writers--;
			if(mp->writers == 0) {
				chanclose(mp->rrchan);
			}
			free(aux);
			fid->aux = nil;
		} 

		if(((fid->omode&OMASK) == 0)&&(fid->qid.type==QTFILE)) {
			mp->readers--;
			chanclose(aux->chan);
			chanfree(aux->chan);
			aux->chan = nil;
			if(aux->r) {/* crap! we still have data - return short write */
				int offset = (int) aux->r->aux;
				aux->r->ofcall.count = aux->r->ifcall.count - offset;
				respond(aux->r, "short write");
				aux->r = nil;
			}
			
			if((mp->writers == 0)&&(mp->readers == 0)) {
				chanfree(mp->rrchan);
				mp->rrchan = chancreate(sizeof(Fid *), 0);
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
		if(r == nil)
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
	if(sendp(iochan, r) != 1) {
		fprint(2, "iochan hungup");
		threadexits("iochan hungup");
	}
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
