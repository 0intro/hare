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

/*
 * Right now there are two types of pipe:
 *	1) normal - writers match to readers
 *  2) broadcast - writers send to all readers
 * 
 */
enum {
	MPTnormal = 1,	/* strictly a normal pipe */
	MPTbcast=2,		/* enumerated pipe */
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

/*
 * We need to send our responses down a channel
 * if one exists so that errors are propagated properly --
 * particularly for broadcasts.
 */
static void
myrespond(Req *r, char *err)
{
	DPRINT(2, "myrespond: r: %p aux: %p r->srv: %p err: %p\n", r, r->aux, r->srv, err);
	if(r->aux)
		sendp(r->aux, err);
	else
		respond(r, err);
	DPRINT(2, "done responding: aux: %p\n", r->aux);
}

/*
 * This only gets called from spliceto, which seems odd.
 */
static void
myresponderror(Req *r)
{
	DPRINT(2, "myresponderror: r: %p aux: %p r->srv: %p\n", r, r->aux, r->srv);
	if(r->aux) {/* only happens for spliceto? or only for bcast? */
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

	qlock(&mp->l);
	mp->readers++;
	DPRINT(2, "addnewreader %p total now: %d\n", mp, mp->readers);
	aux->which = 0;
	for(count=0; count<mp->slots; count++)
		if(mp->numr[count] < min)
			aux->which = count;

	mp->numr[aux->which]++;

	if(mp->mode == MPTbcast) {
		DPRINT(2,"\t adding %p to broadcast list next: %p\n", aux, mp->bcastr);
		aux->next=mp->bcastr;
		mp->bcastr=aux;
	}
	qunlock(&mp->l);
	aux->chan = chancreate(sizeof(Req *), 0);
}

/* safely remove a reader from a list */
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
	
	qlock(&mp->l);
	for(c=mp->bcastr; c != nil; c=c->next) {
		chanclose(c->chan);
	}
	qunlock(&mp->l);
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
			DPRINT(2, "\t enumerated mode (%d)\n", mp->slots);	
		} else {
			fprint(2, "usage: invalid enumeration argument\n");
			err = Ebadspec;
		}
		break;
	case 'b':	/* broadcast mode */
		DPRINT(2, "\t parsespec: BROADCAST mode\n");
		mp->mode = MPTbcast;
		break;
	default:
		fprint(2, "ERROR: badflag('%c')", ARGC());
		err = Ebadspec;		
	}ARGEND

	DPRINT(2, "\t argc: %d\n", argc);
	
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
	DPRINT(2, "\t args: %s\n", r->ifcall.aname);
	/* add a stupid extra field so we can use args(2) */
	scratch = smprint("mount %s\n", r->ifcall.aname);
	argc = tokenize(scratch, argv, MAXARGS);
	DPRINT(2, "\t argc: %d\n", argc);
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

	DPRINT(2, "fsclunk %p (%d) aux=%p\n", fid, fid->fid, fid->aux);
	/* if there is an participant on this fid */
	if(fid->aux) {
		Fidaux *aux = setfidaux(fid);	
		mp->ref--;
		
		DPRINT(2, ">>>> fid %d writers: %d readers: %d\n", fid->fid, mp->writers, mp->readers);
		if(aux)
			DPRINT(2, ">>>> state: %d\n", aux->state);

		/* writer accounting and cleanup */ 
		if((fid->omode&OMASK) == OWRITE) {
			/* TODO: what do we do with outstanding requests? */
			mp->writers--;
			DPRINT(2, "---- fid %d writers now %d\n", fid->fid, mp->writers);
			if((aux->state != FID_CTLONLY) && (mp->writers == 0)) {
				for(count=0;count <  mp->slots; count++)
					if(mp->rrchan[count]) {
						chanclose(mp->rrchan[count]);
					}

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

			chanclose(aux->chan);
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
			
			qlock(&mp->l);
			if((mp->writers == 0)&&(mp->readers == 0)) {
				for(count=0;count <  mp->slots; count++) {
					if(mp->rrchan[count])
						chanclose(mp->rrchan[count]);
					mp->rrchan[count] = chancreate(sizeof(Fid *), 0);
				}
			}
			qunlock(&mp->l);
		}

		/* no more references to pipe, cleanup */
		qlock(&mp->l);
		if(mp->ref == 0) { 
			free(mp->name);
			free(mp->uid);
			free(mp->gid);
			if(mp->rrchan) {
				for(count=0;count <  mp->slots; count++) {
					if(mp->rrchan[count]) {
						chanclose(mp->rrchan[count]);
					}
				}
				free(mp->rrchan);
			}
			free(mp);
		} else {
			qunlock(&mp->l);
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

	if((r->ifcall.mode&OMASK) == ORDWR) {
		respond(r, Eoneorother);
		return;
	}

	/* allocate a new participant structure */
	aux = emalloc9pz(sizeof(Fidaux), 1);
	aux->state = FID_IDLE;
	aux->mp = mp;
	fid->aux = aux;

	if((r->ifcall.mode&OMASK) ==  OWRITE) {
		DPRINT(2, "fsopen: add new writer\n");
		mp->writers++;
	}
	
	if((r->ifcall.mode&OMASK) == 0) /* READER */
		addnewreader(mp, aux);

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
			DPRINT(2, "fsread got a nil packet, exiting\n");
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
				DPRINT(2, "\n====>spliceto: mpipe %p (%s) hungup rrchan\n", mp, mp->name);
				goto exit;
			}
		}

		/* block on incoming */
		tr = recvp(aux->chan);
		if(tr == nil) {
			DPRINT(2, "\n====> spliceto: mpipe %p (%s) hungup aux.chan\n", mp, mp->name);
			goto exit;
		}

		offset = 0;
		count = 0;

		while(offset < tr->ifcall.count) {
			int n;
			DPRINT(2,"\tspliceto: fd: %d\n", sa->fd);
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
	//write(sa->fd, "", 0); /* null write */
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
	
	DPRINT(5, "bcastsend %p\n", arg);

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

		DPRINT(2, "\t broadcasting %p to %p\n", r, c);

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
			DPRINT(2, "fsbcast: %s\n", err);
		}
	}
	qunlock(&mp->l);

	/* restore r->aux for splicefrom case */
	r->aux = bakaux;
	DPRINT(2, "fsbcast respond: r: %p aux: %p r->srv: %p err: %p\n", 
					r, r->aux, r->srv, err);

	if(!err)
		r->ofcall.count = r->ifcall.count;

	DPRINT(2, "fsbcast closing return channels\n");	

	/* TODO: do we really need to do both of these? */
	chanclose(reterr);
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

	/* setup a dummy writer */
	memset(&tr, 0, sizeof(Req));
	aux->mp = mp;
	aux->state = FID_IDLE;
	mp->ref++;
	DPRINT(2, "splicefrom: add new writer\n");
	mp->writers++;
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
		DPRINT(2, "splicefrom: top of loop - reading\n");
		n = read(sa->fd, tr.ifcall.data, tr.ifcall.count);
		DPRINT(2, "splicefrom: read returned %d\n", n);
		if(n < 0) { /* Error */
			DPRINT(2, "splicefrom: read retuned error: %r\n");
			close(sa->fd);		
			goto exit;
		} else if (n == 0) {
			DPRINT(2, "mpipe %p (%s) splicefrom: got EOF\n", mp, mp->name);
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
			DPRINT(2, "back from fsbcast?\n");
		} else {
			/* acquire a reader */
			aux->other = recvp(mp->rrchan[aux->which]);
			if(aux->other == nil) {
				DPRINT(2, "splicefrom: %s\n", Ehangup);
				goto exit;
			}
			raux = aux->other->aux;
			if(raux->other != nil) {
				DPRINT(2, "splicefrom: %s\n", Eother);
				goto exit;
			}

			raux->other = dummy;
			assert(raux->chan != 0);
			if(sendp(raux->chan, &tr) != 1) {
				DPRINT(2, "splicefrom: %s\n", Ehangup);
				goto exit;
			}
			if(err = recvp(reterr)) {
				DPRINT(2, "splicefrom: reterr %s\n", Ehangup);
				goto exit;
			}
		}
		if(err) {
			DPRINT(2, "splicefrom: mp: %p (%s) error: %s\n", err, mp, mp->name);
			goto exit;
		}
		/* wait for completion? */
		if(tr.ifcall.count == 0) {	/* EOF  - so we are done */
			close(sa->fd);
			goto exit;
		}
	}

exit:
	DPRINT(2, "]]]]]]]]]]]]] SPLICEFROM (mp %p) (fid %d) (aux %p) EXITING [[[[[[[[[[[[\n", 
			mp, sa->fd, dummy->aux);
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
		DPRINT(2, "\t PACKET size: %d which %d\n", aux->remain, aux->which);
		break;
	case '>':	/* splice to */
	case '<':	/* splice from */
		if(n<4)
			return "problem parsing spliceto header";
		sa = emalloc9p(sizeof(Splicearg));
		sa->mp = mp;
		sa->which = atoi(argv[2]) % mp->slots;

		if(type=='>') {
			sa->fd = open(argv[3], OWRITE);
			DPRINT(2, "spliceto open: %s returned %d\n", argv[3], sa->fd);
		} else
			sa->fd = open(argv[3], OREAD);

		if(sa->fd < 0)
			return "splice couldnt open target";

		if(type=='>')
			proccreate(spliceto, sa, STACK);
		else
			proccreate(splicefrom, sa, STACK);
		break;
	case '.': /* forced close */
		aux->state = FID_FLUSHED;
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
	} else {
		if(!aux->remain)
			aux->remain = r->ifcall.count;
		mp->len += r->ifcall.count;
	}

	aux->state = FID_WRITEN;

	if(mp->mode == MPTbcast) {
		err = fsbcast(r, mp);
		if(err)
			DPRINT(2, "fsbcast returned %s\n", err);
		respond(r, err);
		goto out;
	}

	DPRINT(2, "fsread: aux: %p aux->other: %p aux->remain: %d\n", aux, aux->other, aux->remain);

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
			DPRINT(2, "unrecognized io op %d\n", r->ifcall.type);
			break;
		}
	}
}

/* proxy operations to iothread */
static void
ioproxy(Req *r)
{
	DPRINT(2, "ioproxy: forwarding %p %d\n", r, r->ifcall.type);
	if(sendp(iochan, r) != 1) {
		DPRINT(2, "iochan hungup");
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
	proccreate(iothread, nil, STACK);

	threadpostmountsrv(&fs, srvpath, mntpath, MBEFORE);

	threadexits(nil);
}
