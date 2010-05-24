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
	MSIZE = 8192,
	MAXARGS = 8,
	MAXHDRARGS = 4,
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

	int slots;		/* number of enumerations */
	Channel **rrchan;	/* ready to read */
	int *numr;
};

typedef struct Fidaux Fidaux;
struct Fidaux {
	QLock l;
	Mpipe *mp;
	ulong which;		/* for enumerations */

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

typedef struct Splicearg Splicearg;
struct Splicearg {
	Mpipe *mp;
	int which;
	int fd;
};

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

static void
myrespond(Req *r, char *err)
{
	if(r->aux)
		sendp(r->aux, err);
	else
		respond(r, err);
}

static void
myresponderror(Req *r)
{
	if(r->aux) {
		char *err = emalloc9p(ERRMAX);
		snprint(err, ERRMAX, "%r");
		sendp(r->aux, err);
	} else
		responderror(r);
}

static Mpipe *
setfidmp(Fid *fid)
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

static void
parsespec(Mpipe *mp, int argc, char **argv)
{
	if(argc >= 1) 
		mp->name = estrdup9p(argv[0]);
	ARGBEGIN {
	case 'e':
		mp->slots = atoi(ARGF());
		break;
	default:
		print(" badflag('%c')", ARGC());
	}ARGEND
}

static void
fsattach(Req *r)
{
	Mpipe *mp = emalloc9pz(sizeof(Mpipe), 1);
	char *argv[MAXARGS];
	int argc;
	int count;

	/* need to parse aname */
	argc = tokenize(r->ifcall.aname, argv, MAXARGS);
	parsespec(mp, argc, argv);

	if(mp->slots == 0)
		mp->slots = 1;
	if(mp->name == nil) {
		mp->name = estrdup9p(MPdefault);
	}

	lock(&pipealloc.l);
	mp->path = ++pipealloc.path;
	unlock(&pipealloc.l);

	r->fid->qid = (Qid){NETQID(2*mp->path, Qdir), 0, QTDIR};
	r->ofcall.qid = r->fid->qid;

	r->fid->aux = mp;
	mp->uid = estrdup9p(getuser());
	mp->gid = estrdup9p(mp->uid);

	mp->rrchan = emalloc9p(sizeof(Channel *)*mp->slots);
	mp->numr = emalloc9p(sizeof(int)*mp->slots);
	for(count = 0; count < mp->slots; count++) {
		mp->rrchan[count] = chancreate(sizeof(Fid *), 0);
		mp->numr[count] = 0;
	}

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
addnewreader(Mpipe *mp, Fidaux *aux)
{
	int count;
	int min = mp->numr[0];	

	mp->readers++;

	aux->which = 0;
	for(count=0; count<mp->slots; count++)
		if(mp->numr[count] < min)
			aux->which = count;

	mp->numr[aux->which]++;
	aux->chan = chancreate(sizeof(Req *), 0);
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
	
	if((r->ifcall.mode&OMASK)==0) /* READER */
		addnewreader(mp, aux);

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
		offset = tr->ofcall.count;
	 } else {
		if(aux->other == nil) {
			if(sendp(mp->rrchan[aux->which], fid) != 1) {
				myrespond(r, nil);
				threadexits(nil);
			}
		}
		tr = recvp(aux->chan);
		offset = 0;
		if(tr == nil) {
			myrespond(r, nil);
			threadexits(nil);
		}
	}
	raux = aux->other->aux;
	count = fscopy(tr, r, offset);
	raux->remain -= count;

	mp->len -= count;
	offset += count;

	if(offset == tr->ifcall.count) {
		if(raux->remain == 0) { /* done for this pkt */
			raux->other = nil;
			raux->which = 0;
			aux->other = nil;
		}
		aux->r = nil;
		tr->ofcall.count = tr->ifcall.count;
		myrespond(tr, nil);
		myrespond(r, nil);
	} else {
		tr->ofcall.count = offset;
		aux->r = tr;
		myrespond(r, nil);
	}

	threadexits(nil);
}

/* MAYBE: eliminate duplicate code with fsread */
static void
spliceto(void *arg) {
	Splicearg *sa = arg;
	Mpipe *mp = sa->mp;
	Fid dummy;
	Fidaux daux, *raux;
	Req *tr;

	/* setup a dummy reader */
	memset(&dummy, 0, sizeof(Fid));
	memset(&daux, 0, sizeof(Fidaux));

	dummy.aux = &daux;
	addnewreader(mp, &daux);

	while(1) {
		int offset;

		/* put ourselves on rrchan */
		if(daux.other == nil)
			if(sendp(mp->rrchan[daux.which], &dummy) != 1) {
				fprint(2, "spliceto: hungup rrchan\n");
				goto exit;
			}
	
		/* block on incoming */
		tr = recvp(daux.chan);
		if(tr == nil) {
			fprint(2, "spliceto: hungup daux.chan\n");
			goto exit;
		}

		raux = daux.other->aux;
		offset = 0;

		while(offset < tr->ifcall.count) {
			int n;

			n = write(sa->fd, tr->ifcall.data+offset, tr->ifcall.count-offset);
			if(n < 0) {
				tr->ofcall.count = offset;
				myresponderror(tr);
				goto exit;
			} else {
				offset += n;
				raux->remain -= n;
			}
		}
		
		if(raux->remain == 0) {
			raux->other = nil;
			raux->which = 0;
			daux.other = nil;
		}

		tr->ofcall.count = offset;
		myrespond(tr, nil);
	}
	/* on error do the right thing */
exit:
	free(sa);
	threadexits(nil);
}

/* MAYBE: eliminate duplicate code with fswrite */
static void
splicefrom(void *arg) {
	Splicearg *sa = arg;
	Mpipe *mp = sa->mp;
	Fid dummy;
	Fidaux daux, *raux;
	Req tr;
	char *err;
	Channel *reterr;

	/* setup a dummy reader */
	memset(&dummy, 0, sizeof(Fid));
	memset(&daux, 0, sizeof(Fidaux));
	memset(&tr, 0, sizeof(Req));

	dummy.aux = &daux;
	tr.fid = &dummy;
	tr.ifcall.data = emalloc9p(MSIZE);
	reterr = chancreate(sizeof(char *), 0);
	tr.aux = reterr;
	
	while(1) {
		tr.ifcall.count = MSIZE;
		daux.which = sa->which;
		tr.ifcall.count = read(sa->fd, tr.ifcall.data, tr.ifcall.count);
		
		/* acquire a reader */
		daux.other = recvp(mp->rrchan[daux.which]);
		if(daux.other == nil) {
			fprint(2, "splicefrom: %s\n", Ehangup);
			goto exit;
		}
		raux = daux.other->aux;
		if(raux->other != nil) {
			fprint(2, "splicefrom: %s\n", Eother);
			goto exit;
		}
		raux->other = &dummy;
		assert(raux->chan != 0);
		if(sendp(raux->chan, &tr) != 1) {
			fprint(2, "splicefrom: %s\n", Ehangup);
			goto exit;
		}
		/* wait for completion? */
		if(err = recvp(reterr)) {
			fprint(2, "splicefrom: reterr %s\n", Ehangup);
			goto exit;
		}
		if(err) {
			fprint(2, "spliceform: error: %s\n", err);
			goto exit;
		}
	}

exit:
	free(sa);
	threadexits(nil);
}

static char *
parseheader(Req *r, Mpipe *mp, Fidaux *aux)
{
	char *argv[MAXHDRARGS];
	char type;
	int n;
	Splicearg *sa;

	n = tokenize(r->ifcall.data, argv, MAXHDRARGS);
	if(n==0)
		return "problem parsing header";
	type = argv[0][0];

	switch(type) {
	case 'p':	/* packet */
		if(n<3)
			return "problem parsing pkt header";
		aux->remain = atoi(argv[1]);
		aux->which = atoi(argv[2]) % mp->slots;
		break;
	case '>':	/* splice to */
	case '<':	/* splice from */
		if(n<4)
			return "problem parsing spliceto header";
		sa = emalloc9p(sizeof(Splicearg));
		sa->mp = mp;
		sa->which = atoi(argv[2]) % mp->slots;

		if(type=='>')
			sa->fd = open(argv[3], OWRITE);
		else
			sa->fd = open(argv[3], OREAD);

		if(sa->fd < 0)
			return "splice couldnt open target";

		if(type=='>')
			proccreate(spliceto, sa, STACK);
		else
			proccreate(splicefrom, sa, STACK);
		break;
	default:
		return "unknown pkt header";
	}

	return nil;
}

static void
fswrite(void *arg)
{
	Req *r = arg;
	Fid *fid = r->fid;
	Fidaux *raux, *aux = setfidaux(fid);
	Mpipe *mp = setfidmp(fid);
	ulong hdrtag = ~0;
	char *err;
	
	if(r->ifcall.offset == hdrtag) {
		if(aux->remain) {
			respond(r,Eremain);
			goto out;
		}
		if((err = parseheader(r, mp, aux)) == nil) {
			r->ofcall.count = r->ifcall.count;
			respond(r, nil);
		} else {
			respond(r, err);
		}
		goto out;
	} else {
		aux->remain = r->ifcall.count;
		mp->len += r->ifcall.count;
	}

	if(aux->other == nil) {
		aux->other = recvp(mp->rrchan[aux->which]);
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
	int count;

	if(fid->aux) {
		Fidaux *aux = setfidaux(fid);	
		mp->ref--;
	
		if(mp->ref == 0) { 
			free(mp->name);
			free(mp->uid);
			free(mp->gid);
			if(mp->rrchan) {
				for(count=0;count <  mp->slots; count++) {
					if(mp->rrchan[count]) {
						chanclose(mp->rrchan[count]);
						chanfree(mp->rrchan[count]);
					}
				}
				free(mp->rrchan);
			}
			free(mp);
		}

		if((fid->omode&OMASK) == OWRITE) {
			/* MAYBE: mark outstanding requests crap */
			mp->writers--;
			if(mp->writers == 0) {
				for(count=0;count <  mp->slots; count++)
					if(mp->rrchan[count])
						chanclose(mp->rrchan[count]);
			}
			free(aux);
			fid->aux = nil;
		} 

		if(((fid->omode&OMASK) == 0)&&(fid->qid.type==QTFILE)) {
			mp->readers--;
			chanclose(aux->chan);
			chanfree(aux->chan);
			mp->numr[aux->which]--;
			aux->chan = nil;
			if(aux->r) {/* crap! we still have data - return short write */
				respond(aux->r, "short write");
				aux->r = nil;
			}
			
			if((mp->writers == 0)&&(mp->readers == 0)) {
				for(count=0;count <  mp->slots; count++) {
					if(mp->rrchan[count])
						chanfree(mp->rrchan[count]);
					mp->rrchan[count] = chancreate(sizeof(Fid *), 0);
				}
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
