/*
	mpipefs - userspace multipipe implementation

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

	Todo:
		* explicit hangup modes
		* buffered mpipes
		* internal splice optimization
		* reads of ~(0) yielding ctl blocks
		* ability to forward headers
		* network tight coupling mode (distributed mpipes)
			* direct connects bypassing 9P
			* broadcast mpipe tight coupling
			* collective mpipe tight coupling on BG/P
			* MAYBE: RDMA support
		* filter builtins for common scenarios (whitespace, EOL, etc)
		
	See Also:
		9p(2)
		9pfid(2)
		9pfile(2)
*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>
#include <debug.h>

enum
{
	STACK = 8192,
	MSIZE = 8192,
	MAXARGS = 8,					/* maximum spec arguments */
	MAXHDRARGS = 4,					/* maximum header entries */
};

Channel *iochan;					/* dispatch channel */
typedef struct Fidaux Fidaux;
char defaultsrvpath[] = "mpipe";
int iothread_id;

char Enotdir[] = "walk in non-directory";
char Enotfound[] = "directory entry not found";
char Eoneorother[] = "open read or write, not both";
char Eshort[] = "short write";
char Ehangup[] = "pipe hang up";
char Eremain[] = "new header when bytes still remaining";
char Eother[] = "Problem with peer";
char Ebcastoverflow[] = "broadcast read underflow";
char Ebcast[] = "broadcast failed (perhaps partially)";
char Ebadspec[] = "bad mount specification arguments";
char Esplice[] = "splice couldnt open target";
char Ebadhdr[] = "unknown packet header";

enum {	/* DEBUG LEVELS */
	DERR = 0,	/* error */
	DCUR = 1,	/* current - temporary trace */
	DHDR = 2,	/* packet headers */
	DOPS = 3,	/* operation tracking */
	DWRT = 4,	/* writer */
	DRDR = 5,	/* reader */
	DBCA = 6,	/* broadcast */
	DSPT = 7,	/* splice to */
	DSPF = 8,	/* splice from */
	DFID = 9,	/* fid tracking */
	DARG = 10,	/* arguments */
};


/*
 * Right now there are two types of pipe:
 *	1) normal - writers match to readers
 *      2) broadcast - writers send to all readers
 * 
 */
enum {
	MPTnormal 	= 1,	/* strictly a normal pipe */
	MPTbcast	= 2,	/* broadcast pipe */

	MPThangingup,		/* terminating */
	MPThungup, 
	MPTcleanup,
};

/*
 * Mpipe - primary data structure
 *
 * There is one of these per pipe.  Mostly it contains
 * the information necessary for file system queries on the
 * pipe, but also maintains a number of reference count values
 * relating to accounting for readers and writers.
 *
 * for non-enumerated pipes, there is only a single slot
 *
 */
typedef struct Mpipe Mpipe;
struct Mpipe {
	QLock l;		/* protect pipe */
	int ref;		/* reference count */

	int mode;		/* pipe type */

	ulong path;		/* qid.path */
	char *name;		/* name in directory */
	char *uid;		/* owner id */
	char *gid;		/* owner group */
	ulong len;		/* current length in bytes of data */

	int writers;	/* number of writers */
	int readers;	/* number of readers */

	int slots;		/* number of enumerations */

	/* normal only, one entry per slot: */
	Channel **rrchan;	/* ready to read channels */
	int *numr;			/* number of readers on slot */

	/* broadcast only */
	Fidaux *bcastr;		/* list of readers */
};

/* states for Fidaux */
enum {
	FID_IDLE,
	FID_CTLONLY,
	FID_WRITEN,
	FID_FLUSHED,
};

/*
 * Fidaux - per particpant descriptor accounting structure
 * 
 * This is typically hung off the fid private value and contains
 * the accounting for a particular "participant" (reader or writer)
 *
 */
struct Fidaux {
	QLock l;			/* Protect it */
	int state;			/* if participant has been written yet */

	Mpipe *mp;			/* Multipipe we are dealing with */
	ulong which;		/* for enumerations */

	/* only for writer */
	Fid *other;			/* peer Fid for long writes */
	int remain;			/* bytes remaining in multi-block */
	
	/* only for reader */
	Req *r;				/* cached request for partial reads */
	Channel *chan;		/* Response channel */

	/* for bcast */
	Fidaux *next;		/* Linked list of broadcasters */
};

/*
 * pipealloc - qid.path accounting 
 *
 * we must guarentee unique qid.paths, this helps with that 
 *
 */
static struct
{
	Lock l;
	ulong	path;
} pipealloc;

/*
 * splicearg - parameter encapsulation
 *
 * since splices get spawned off as sub-procs, this allows
 * us to pass all the arguments in a structure
 */

typedef struct Splicearg Splicearg;
struct Splicearg {
	Mpipe *mp;		/* the pipe in question */
	int which;		/* enumeration, 0 for don't care */
	int fd;			/* file descriptor to read from/write to */
	char *path;		/* name of target of splice */
	Channel *chan;	/* error/sync channel */
};

/*
 * Bcastr - Broadcast argument
 *
 * since broadcasts get spawned off as sub-threads, this allows
 * us to pass all the arguments in a structure
 */
typedef struct Bcastr Bcastr;
struct Bcastr {
	Channel *chan;	/* channel to send on */
	Req *r;			/* request to send */
};

static void
usage(void)
{
	fprint(2, "mpipefs [-D] [-v debuglevel] [-s <srvpt>] [mtpt]\n");
	threadexits("usage");
}

static void
killall(Srv*)
{
	DPRINT(DFID, "killing iochan and all thread\n");
	chanfree(iochan);
	threadexitsall("killall");
}

/* FIXME: prepending/adding the PID with pipealloc's path will not fit
 * within the lower 32 bits
 *

 * This might not scale well.  Need to reexamine how the qid's are
 * generated and acounted for not only the local machine and the
 * synthetic file systems, but also for its scalability.
 */
#define MAGIC		((uvlong) 0x6163 << 32)
#define NETTYPE(x)	((ulong)(x)&0x1f)
#define NETID(x)	((((ulong)(x))>>5)&0xffff)
#define NETQID(i,t)	(MAGIC|((((i)&0xffff)<<5)|((getpid()&0x7ff)<<21)|(t)))

enum
{
	Qdir,
	Qdata,
};

static char MPdefault[] = "data";

/* tiny helper, why don't we have this generically? */
static void *
emalloc9pz(int sz, int zero)
{
	void *v = emalloc9p(sz);
	if(zero)
		memset(v, 0, sz);
	return v;
}

/*
 * We need to send our responses down a channel
 * if one exists so that errors are propagated properly --
 * particularly for broadcasts.
 */
static void
myrespond(Req *r, char *err)
{
	if(r->aux)
		sendp(r->aux, err);
	else
		respond(r, err);
}

/*
 * This only gets called from spliceto.
 */
static void
myresponderror(Req *r)
{
	if(r->aux) {
		sendp(r->aux, smprint("%r"));
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

/* MAYBE: allow splice-to to specify which by setting aux->which */
/* register a new reader with the infrastructure */
static void
addnewreader(Mpipe *mp, Fidaux *aux)
{
	int count;
	int min = mp->numr[0];
	aux->chan = chancreate(sizeof(Req *), 0);
	assert(aux->chan != nil);

	if(mp->mode == MPThangingup || mp->mode == MPThungup || mp->mode == MPTcleanup) {
		DPRINT(DBCA,"[%s](%p) WARNING: mp mode set to not running\n");
		return;
	}

	qlock(&mp->l);
	mp->readers++;
	DPRINT(DRDR, "[%s](%p) addnewreader total now: %d\n", mp->name, mp, mp->readers);
	
	/* find enumeration slot with fewest readers */
	aux->which = 0;
	for(count=0; count<mp->slots; count++)
		if(mp->numr[count] < min)
			aux->which = count;

	mp->numr[aux->which]++;

	if(mp->mode == MPTbcast) {
		DPRINT(DBCA,"[%s](%p) adding %p to broadcast list next: %p\n", mp->name, mp, aux, mp->bcastr);
		aux->next=mp->bcastr;
		mp->bcastr=aux;
	}
	qunlock(&mp->l);
}

/* safely remove a reader from the broadcast list */
static void
rmreader(Mpipe *mp, Fidaux *aux)
{
	Fidaux *c;
	
	qlock(&mp->l);
	if(mp->bcastr == aux)
		mp->bcastr = aux->next;
	else {
		for(c = mp->bcastr; c != nil; c=c->next) {
			if(c->next = aux) {
				c->next = aux->next;
				break;
			}
		}
	}
	aux->next = nil;
	qunlock(&mp->l);
}

/* cleanup broadcast pipes on last write close */
static void
closebcasts(Mpipe *mp)
{
	Fidaux *c;
	
	DPRINT(DFID, "closebcasts: pid=(%d)\n", getpid());
	for(c=mp->bcastr; c != nil; c=c->next) {
		chanclose(c->chan);
	}
}

/* close everyone the mpipe has slotted */
static void
closempipe(Mpipe *mp)
{
	int count;
	
	DPRINT(DFID, "closempipe: pid=(%d)\n", getpid());
	for(count=0;count <  mp->slots; count++)
		if(chanclosing(mp->rrchan[count]))
			chanclose(mp->rrchan[count]);
}

/* process mount options */
static char *
parsespec(Mpipe *mp, int argc, char **argv)
{
	char *f;
	char *err=nil;

	ARGBEGIN {
	case 'e':	/* enumerated mode */
		f = ARGF();
		if(f) {
			char *x = f;
			if(x)
				mp->slots = atoi(x);
			DPRINT(DARG, "(%p) enumerated mode (%d)\n", mp, mp->slots);	
		} else {
			DPRINT(DERR, "(%p) *ERROR*: usage: invalid enumeration argument\n", mp);
			err = Ebadspec;
		}
		break;
	case 'b':	/* broadcast mode */
		DPRINT(DARG, "(%p) parsespec: BROADCAST mode\n", mp);
		mp->mode = MPTbcast;
		break;
	default:
		DPRINT(DERR, "(%p) *ERROR*:  badflag('%c')", mp, ARGC());
		err = Ebadspec;		
	}ARGEND

	DPRINT(DARG, "(%p) argc: %d\n", mp, argc);
	
	if(argc>0) 
		mp->name = estrdup9p(*argv);

	return err;
}

static void
fsattach(Req *r)
{
	Mpipe *mp = emalloc9pz(sizeof(Mpipe), 1);
	char *argv[MAXARGS];
	int argc;
	int count;
	char *err;
	char *scratch;

	/* need to parse aname */
	DPRINT(DARG, "args: %s\n", r->ifcall.aname);
	/* add a stupid extra field so we can use args(2) */
	scratch = smprint("mount %s\n", r->ifcall.aname);
	argc = tokenize(scratch, argv, MAXARGS);
	DPRINT(DARG, "argc: %d\n", argc);
	err = parsespec(mp, argc, argv);
	free(scratch);
	if(err) {
		respond(r, err);
		return;
	}

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
fsclunk(Fid *fid)
{
	Mpipe *mp = setfidmp(fid);
	int count;


	DPRINT(DOPS, "[%s](%p) fsclunk: fid %p (%d) aux=%p\n", 
			mp==nil ? "nil" : mp->name, mp, fid, fid->fid, fid->aux);

	/* if there is an participant on this fid */
	if(fid->aux) {
		Fidaux *aux = setfidaux(fid);	
		qlock(&mp->l);
		mp->ref--;
		qunlock(&mp->l);

		DPRINT(DFID, ">>>> fid %d writers: %d readers: %d\n", fid->fid, mp->writers, mp->readers);

		/* writer accounting and cleanup */ 
		if((fid->omode&OMASK) == OWRITE) {
			/* TODO: what do we do with outstanding requests? */
			DPRINT(DWRT, "---- fid %d writers now %d\n", fid->fid, mp->writers-1);
			qlock(&mp->l);
			mp->writers--;
			qunlock(&mp->l);

			if((aux->state != FID_CTLONLY) && (mp->writers == 0)) {
				closempipe(mp);

				if(mp->mode == MPTbcast)
					closebcasts(mp);
			}

			free(aux);
			fid->aux = nil;
		} 

		/* reader accounting & cleanup */
		if(((fid->omode&OMASK) == 0)&&(fid->qid.type==QTFILE)) {
			qlock(&mp->l);
			mp->readers--;
			qunlock(&mp->l);

			chanclose(aux->chan);

			qlock(&mp->l);
			mp->numr[aux->which]--;
			qunlock(&mp->l);

			if(mp->mode == MPTbcast) {
				rmreader(mp, aux);
			}
			aux->chan = nil;
			if(aux->r) { /* crap! we still have data */
				respond(aux->r, "short write");
				aux->r = nil;
			}
			
			if((mp->writers == 0)&&(mp->readers == 0)) {
				closempipe(mp);

				// everyone is hungup. Make sure the channels are cleared
				for(count=0;count <  mp->slots; count++) {
					/* make sure that all any dynamically allocated data
					   is freed up AND that it is reinitialized */
					if(mp->rrchan[count]->closed == 2) {
						DPRINT(DOPS, "---- reinitialize the channel[%d]\n", count);
						chanfree(mp->rrchan[count]);
						mp->rrchan[count] = chancreate(sizeof(Fid *), 0);
					} else {
						DPRINT(DOPS, "[%s](%p) ERROR: fsclunk: channel [%d] still in use\n", 
						       mp==nil ? "nil" : mp->name, mp, count);
						DPRINT(DOPS, "       element size [%d] nentry [%d]\n",
						       mp->rrchan[count]->e, mp->rrchan[count]->nentry);
					}
				}
			}
		}

		/* no more references to pipe, cleanup */
		if(mp->ref == 0) { 
			free(mp->name);
			free(mp->uid);
			free(mp->gid);
			free(mp->rrchan);
			free(mp);
		}
	}

	fid->aux = nil;
}

static void
fsopen(Req *r)
{
	Fid* fid = r->fid;
	Qid q = fid->qid;
	Mpipe *mp = setfidmp(fid);
	Fidaux *aux;

	if (q.type&QTDIR){
		respond(r, nil);
		return;
	}

	/* FIXME: make sure that all the checks are necessary, and not just hungup */
	if(mp->mode == MPThangingup || mp->mode == MPThungup || mp->mode == MPTcleanup) {
		respond(r, Ehangup);
		return;
	}

	if((r->ifcall.mode&OMASK) == ORDWR) {
		respond(r, Eoneorother);
		return;
	}

	/* allocate a new participant structure */
	aux = emalloc9pz(sizeof(Fidaux), 1);
	aux->state = FID_IDLE;
	aux->mp = mp;
	fid->aux = aux;

	if((r->ifcall.mode&OMASK) == OWRITE) {
		/* TODO: grab the lock naybe */
		mp->writers++;
		DPRINT(DWRT, "[%s](%p) fsopen: add new writer (writers=%d)\n", mp->name, mp, mp->writers);	
	}
	
	if((r->ifcall.mode&OMASK) == 0) /* READER */
		addnewreader(mp, aux);

	/* TODO: detect bad open modes */

	respond(r, nil);
}

/* simple dirgen for file system directory listings */
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
/* TODO: really, no passing by reference? */
static int
fscopy(Req *tr, Req *r, int offset)
{
	int count = tr->ifcall.count - offset;

	if(count > r->ifcall.count)
			count = r->ifcall.count;	/* truncate */

	memcpy(r->ofcall.data, tr->ifcall.data+offset, count);
	
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

	/* if remainder, serve that first */
	if(aux->r) {	
		tr = aux->r;
		offset = tr->ofcall.count;
	} else {
		/* FIXME: make sure that all the checks are necessary, and not just hungup */
		if(mp->mode == MPThangingup || mp->mode == MPThungup || mp->mode == MPTcleanup) {
			myrespond(r, nil);
			threadexits(nil);
		}

		/* if pipe isn't a broadcast & we don't have a peer
		   then put us on the recvq rrchan */
		if((mp->mode != MPTbcast) && (aux->other == nil)) {
			if(sendp(mp->rrchan[aux->which], fid) != 1) {
				myrespond(r, nil);
				threadexits(nil);
			}
		}
		/* wait for a new packet */
		tr = recvp(aux->chan);
		offset = 0;
		if(tr == nil) {
			DPRINT(DRDR, "[%s](%p): fsread: got a nil packet, exiting\n", mp->name, mp);
			r->ofcall.count = 0;
			myrespond(r, nil);
			threadexits(nil);
		}
	}
	
	count = fscopy(tr, r, offset);

	if(mp->mode != MPTbcast) { /* non-broadcast case */
		raux = aux->other->aux;
		raux->remain -= count;

		mp->len -= count;
		offset += count;

		if(offset == tr->ifcall.count) { /* full packet */
			if(raux->remain == 0) { /* done for this pkt */
				raux->other = nil;
				raux->which = 0;
				aux->other = nil;
			}
			aux->r = nil;
			tr->ofcall.count = tr->ifcall.count;
			myrespond(tr, nil); /* respond to writer */
			myrespond(r, nil); 	/* respond to our reader */
		} else { 				/* handle partial packet */
			tr->ofcall.count = offset;
			aux->r = tr;
			myrespond(r, nil); 	/* respond to our reader */
		}
	} else { 					/* broadcast case */
		/* broadcast can't handle partial packet */
		if(count != tr->ifcall.count)
			myrespond(tr, Ebcastoverflow);
		else
			myrespond(tr, nil);
		
		r->ofcall.count = count;
		myrespond(r, nil);
	}
	threadexits(nil);
}

/* MAYBE: eliminate duplicate code with fsread */
/* proc to act like a reader and manage splicing data to an fd */
static void
spliceto(void *arg) {
	Splicearg *sa = arg;
	Mpipe *mp = sa->mp;
	Fid *dummy = emalloc9pz(sizeof(Fid), 1); /* fake fid */
	Fidaux *raux;	/* remote fid accounting pointer */
	Fidaux *aux = emalloc9pz(sizeof(Fidaux), 1); /* our accounting */
	Req *tr;

	sa->fd = open(sa->path, OWRITE);
	DPRINT(DHDR, "[%s](%p) spliceto open: %s returned %d\n", 
					mp->name, mp, sa->path, sa->fd);
	free(sa->path);
	if(sa->fd < 0) {
		sendp(sa->chan, smprint("spliceto: open returned %r\n"));
		threadexits(Esplice);
	}
	sendp(sa->chan, nil);

	/* setup a dummy reader */
	dummy->omode = OREAD;
	dummy->aux = aux;
	/* aux->which always gets set in addnewreader */
	/* addnewreader puts on bcast list if we are in bcast mode */
	/* TODO: are we ignore which argument? */
	addnewreader(mp, aux);
	aux->mp = mp;
	mp->ref++;

	while(1) {
		int offset;
		int count;

		/* if pipe isn't a broadcast & we don't have a peer
		   then put us on the recvq rrchan */
		if((mp->mode != MPTbcast) && (aux->other == nil)) {
			if(sendp(mp->rrchan[aux->which], dummy) != 1) {
				DPRINT(DSPF, "[%s](%p) spliceto: hungup rrchan\n", mp->name, mp);
				goto exit;
			}
		}

		/* block on incoming */
		tr = recvp(aux->chan);
		if(tr == nil) {
			DPRINT(DSPF, "[%s](%p) spliceto: mpipehungup aux.chan\n", mp->name, mp);
			goto exit;
		}

		offset = 0;
		count = 0;

		while(offset < tr->ifcall.count) {
			int n;
			DPRINT(DSPF,"[%s](%p) spliceto: fd: %d\n",  mp->name, mp, sa->fd);
			n = write(sa->fd, tr->ifcall.data+offset, tr->ifcall.count-offset);
			if(n < 0) {
				myresponderror(tr);
				goto exit;
			} else {
				offset += n;
				count += n;
			}
		}

		if(mp->mode != MPTbcast) { /* non-broadcast case */
			raux = aux->other->aux;
			raux->remain -= count;

			mp->len -= count;

			if(offset == tr->ifcall.count) { /* full packet */
				if(raux->remain == 0) { /* done for this pkt */
					raux->other = nil;
					raux->which = 0;
					aux->other = nil;
				}
				aux->r = nil;
				tr->ofcall.count = tr->ifcall.count;
				myrespond(tr, nil); /* respond to writer */
			} else { 				/* handle partial packet */
				tr->ofcall.count = offset;
				aux->r = tr;
			}
		} else { 					/* broadcast case */
			/* broadcast can't handle partial packet */
			if(count != tr->ifcall.count)
				myrespond(tr, Ebcastoverflow);
			else
				myrespond(tr, nil);
		}
	}
	/* on error do the right thing */
exit:
	close(sa->fd);
	if(mp->mode != MPTbcast)
		fsclunk(dummy);	
	free(sa);
	threadexits(nil);
}

/* thread to send a broadcast */
/* TODO: could this block indefinitely until reader becomes available? */
static void
bcastsend(void *arg)
{
	Bcastr *br = arg;
	
	DPRINT(DBCA, ">>>> bcastsend %p\n", arg);

	if(sendp(br->chan, br->r) != 1)
		sendp(br->r->aux, Ebcast);

	free(br);
}

/* broadcast a message */
static char *
fsbcast(Req *r, Mpipe *mp)
{
	Fidaux *c;
	Channel *reterr = chancreate(sizeof(char *), 0);
	char *err = nil;
	void *bakaux;

	assert(reterr != nil);

	/* backup r->aux for splicefrom case */
	bakaux = r->aux;
	r->aux = reterr;

	/* setup argument structures */
	qlock(&mp->l); /* we have to hold lock for entire broadcast right now */
	for(c = mp->bcastr; c != nil; c = c->next) {
		Bcastr *br = emalloc9p(sizeof(Bcastr));

		DPRINT(DBCA, "[%s](%p) broadcasting %p to %p\n", mp->name, mp, r, c);

		br->chan = c->chan;
		br->r = r;
		threadcreate(bcastsend, br, STACK);
		/* MAYBE: why do we create separate instances instead
				of just one and then free it ourselves?
		*/
	}

	/* gather responses */
	for(c = mp->bcastr; c != nil; c = c->next) {
		char *e;
		if(e = recvp(reterr)) {
			err = e;
			DPRINT(DERR, "*ERROR*: fsbcast: %s\n", err);
		}
	}
	qunlock(&mp->l);

	/* restore r->aux for splicefrom case */
	r->aux = bakaux;
	DPRINT(DBCA, "[%s](%p) fsbcast done: r: %p aux: %p r->srv: %p err: %p\n", 
					mp->name, mp, r, r->aux, r->srv, err);

	if(!err)
		r->ofcall.count = r->ifcall.count;

	chanfree(reterr);

	return err;
}


/* MAYBE: eliminate duplicate code with fswrite */
/* process to act like a writer, reading input form a file */ 
/* TODO: splicefrom won't know how to handle large packets */
static void
splicefrom(void *arg) {
	Splicearg *sa = arg;
	Mpipe *mp = sa->mp;
	Fid *dummy = emalloc9pz(sizeof(Fid),1);
	Fidaux *raux;		/* remote fid accounting pointer */ 
	Fidaux *aux = emalloc9pz(sizeof(Fidaux), 1);
	Req tr;
	char *err;
	Channel *reterr;

	sa->fd = open(sa->path, OREAD);
	DPRINT(DHDR, "[%s](%p) splicefrom open: %s returned %d\n", 
					mp->name, mp, sa->path, sa->fd);
	free(sa->path);
	if(sa->fd < 0) {
		sendp(sa->chan, smprint("splicefrom: open returned %r\n"));
		threadexits(Esplice);
	}
	sendp(sa->chan, nil);

	/* setup a dummy writer */
	memset(&tr, 0, sizeof(Req));
	aux->mp = mp;
	aux->state = FID_IDLE;
	mp->ref++;
	mp->writers++;
	DPRINT(DSPF, "[%s](%p) splicefrom: add new writer (writers=%d)\n", mp->name, mp, mp->writers);

	/* create a dummy fid */
	dummy->omode = OWRITE;
	dummy->aux = aux;

	/* setup initial packet */
	tr.fid = dummy;
	tr.ifcall.data = emalloc9pz(MSIZE, 1);
	reterr = chancreate(sizeof(char *), 0);
	tr.aux = reterr;

	while(1) {
		int n;

		tr.ifcall.count = MSIZE;
		DPRINT(DSPF, "[%s](%p) splicefrom: top of loop - reading\n", mp->name, mp);
		n = read(sa->fd, tr.ifcall.data, tr.ifcall.count);
		DPRINT(DSPF, "[%s](%p) splicefrom: read returned %d\n", mp->name, mp, n);
		if(n < 0) { /* Error */
			DPRINT(DERR, "*ERROR*: [%s](%p) splicefrom: read retuned error: %r\n", mp->name, mp);
			close(sa->fd);		
			goto exit;
		} else if (n == 0) {
			DPRINT(DSPF, "[%s](%p) splicefrom: got EOF\n", mp->name, mp);
 			goto exit;
		} else {
			tr.ifcall.count = n;
		}

		/* TODO: indiscriminately set - what if its 0 or not valid */
		aux->which = sa->which;
		aux->remain = n;
		aux->state = FID_WRITEN;
		
		if(mp->mode == MPTbcast) {
			err = fsbcast(&tr, mp);
		} else {
			/* acquire a reader */
			aux->other = recvp(mp->rrchan[aux->which]);
			if(aux->other == nil) {
				DPRINT(DERR, "*ERROR*: [%s](%p) splicefrom: %s\n", mp->name, mp, Ehangup);
				goto exit;
			}
			raux = aux->other->aux;
			if(raux->other != nil) {
				DPRINT(DERR, "*ERROR*: [%s](%p) splicefrom: %s\n", mp->name, mp, Eother);
				goto exit;
			}

			raux->other = dummy;
			assert(raux->chan != 0);
			if(sendp(raux->chan, &tr) != 1) {
				DPRINT(DERR, "*ERROR*: [%s](%p) splicefrom: %s\n", mp->name, mp, Ehangup);
				goto exit;
			}
			if(err = recvp(reterr)) {
				DPRINT(DERR, "*ERROR*: [%s](%p) splicefrom: reterr %s\n", mp->name, mp, Ehangup);
				goto exit;
			}
		}
		if(err) {
			DPRINT(DERR, "*ERROR*: [%s](%p) splicefrom: error: %s\n", mp->name, mp, err);
			goto exit;
		}
		/* wait for completion? */
		if(tr.ifcall.count == 0) {	/* EOF  - so we are done */
			close(sa->fd);
			goto exit;
		}
	}

exit:
	DPRINT(DSPF, "[%s](%p) SPLICEFROM (fid %d) (aux %p) EXITING\n", 
			mp->name, mp, sa->fd, dummy->aux);
	fsclunk(dummy);
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
	char *err;

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
		DPRINT(DHDR, "[%s](%p) PACKET size: %d which %d\n", 
				mp->name, mp, aux->remain, aux->which);
		break;
	case '>':	/* splice to */
	case '<':	/* splice from */
		if(n<4)
			return "problem parsing spliceto header";
		sa = emalloc9p(sizeof(Splicearg));
		sa->mp = mp;
		sa->which = atoi(argv[2]) % mp->slots;
		sa->path = estrdup9p(argv[3]);
		sa->chan = chancreate(sizeof(void *), 0);
		if(type=='>')
			proccreate(spliceto, sa, STACK);
		else
			proccreate(splicefrom, sa, STACK);
		err = recvp(sa->chan); /* sync */
		chanfree(sa->chan);
		return err;
		break;
	case '.': /* forced close */
		DPRINT(DHDR, "[%s](%p) FLUSHED\n", mp->name, mp);
		aux->state = FID_FLUSHED;
		break;
	default:
		return Ebadhdr;
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
	
	/* FIXME: make sure that all the checks are necessary, and not just hungup */
	if(mp->mode == MPThangingup || mp->mode == MPThungup || mp->mode == MPTcleanup) {
		respond(r, Ehangup);
		goto out;
	}

	if(r->ifcall.offset == hdrtag) {
		if(aux->state == FID_IDLE)
			aux->state = FID_CTLONLY;
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
	} else { /* MAYBE: this doesn't need to be an else */
		if(!aux->remain)
			aux->remain = r->ifcall.count;
		mp->len += r->ifcall.count;
	}

	aux->state = FID_WRITEN;

	if(mp->mode == MPTbcast) {
		err = fsbcast(r, mp);
		if(err)
			DPRINT(DERR, "*ERROR*: [%s](%p) fswrite: fsbcast returned %s\n", mp->name, mp, err);

		mp->len -= r->ofcall.count; /* bcast must do this in sender */
		respond(r, err);
		goto out;
	}

	DPRINT(DWRT, "[%s](%p)  fswrite: aux: %p aux->other: %p aux->remain: %d\n",
			mp->name, mp, aux, aux->other, aux->remain);

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

/* handle potentially blocking ops in their own proc */
static void
iothread(void*)
{
	Req *r;

	threadsetname("mpipefs-iothread");

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
			DPRINT(DERR, "*ERROR*: unrecognized io op %d\n", r->ifcall.type);
			break;
		}
	}
}

/* proxy operations to iothread */
static void
ioproxy(Req *r)
{
	if(sendp(iochan, r) != 1) {
		DPRINT(DERR, "*ERROR*: iochan hungup");
		threadexits("iochan hungup");
	}
}

Srv fs=
{
	.attach=		fsattach,
	.walk1=			fswalk1,
	.clone=			fsclone,
	.open=			fsopen,
	.read=			ioproxy,
	.write=			ioproxy,
	.stat=			fsstat,
	.destroyfid=	fsclunk,
	.end=			killall,
};

void
threadmain(int argc, char **argv)
{
	char *srvpath = defaultsrvpath;
	char *mntpath = nil;
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
	iothread_id = proccreate(iothread, nil, STACK);

	DPRINT(DFID, "Main: srvpath=(%s) pid=(%d)\n", srvpath, getpid());

	threadpostmountsrv(&fs, srvpath, mntpath, MBEFORE);

	threadexits(nil);
}
