/*
	devmpipe.c - multipipe device
        
 	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

 	Description: 
	 The multipipe device keeps track of multiple readers and
	 multiple writers, providing separate queues to isolate each.
	 Writers are paired with a reader for the duration of a record.
 	 When there are multiple readers, the record will be forwarded in
	 a fifo fashion.
	 More advanced modes will be implemented in the future, including
	 support for deterministic hash delivery, broadcast, multicast, typed
	 pipes, collective operations, etc.
 
	Based in part on devpipe.c

	TODO:
		* make sure you qlock openq when necessary
		* fix up debug prints
		* general code review
		* test
		* man page
		* standard regression tests

	BUGS:
		* crappy lock problem prevents anything from working
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

typedef struct Readerq Readerq;
typedef struct Writerq Writerq;
typedef struct Openq Openq;
typedef struct Mpipe	Mpipe;

enum 
{
	Oqinactive = 0,
	Oqreader,
	Oqwriter,
	Oqrdwr,
};
	
struct Openq
{
	QLock;

	int mode;			/* inactive, reader, writer, or read/write */

	Queue *q;			/* for readers */
	Rendez r;			/* for a blocked writer */

	ulong remain;		/* remaining bytes in record */
	int status;			/* -1 for error, 1 for finished, 0 for not finished */

	Openq *writer;		/* who is writing us */
	Openq *reader;		/* who is reading us */

	Openq *wnext;		/* for ready to write list */
	Openq *rnext;		/* for ready to read list */

	Mpipe *mp;		/* backpointer to multipipe */
};

struct Writerq			/* queue of ready to write */
{					/* protected by Mpipe QLock */
	Openq *first;
	Openq *last;
};

struct Readerq			/* queue of ready to read */
{
	Openq *first;
	Openq *last;
};

struct Mpipe
{
	QLock;
	Mpipe	*next;
	int	ref;
	ulong	path;

	Writerq writers[2];	/* queues ready to write */	
	Readerq readers[2]; /* queues ready to read */
};

struct
{
	Lock;
	ulong	path;
} mpipealloc;

enum
{
	Qdir,
	Qdata0,
	Qdata1,
};

Dirtab mpipedir[] =
{
	".",		{Qdir,0,QTDIR},	0,		DMDIR|0500,
	"data",		{Qdata0},		0,		0600,
	"data1",	{Qdata1},		0,		0600,
};
#define NPIPEDIR 3

#define PIPETYPE(x)	(((unsigned)x)&0x1f)
#define PIPEID(x)	((((unsigned)x))>>5)
#define PIPEQID(i, t)	((((unsigned)i)<<5)|(t))

static void
mpipeinit(void)
{
	if(conf.pipeqsize == 0){
		if(conf.nmach > 1)
			conf.pipeqsize = 256*1024;
		else
			conf.pipeqsize = 32*1024;
	}
}

/*
 *  create a mpipe, no streams are created until an open
 */
static Chan*
mpipeattach(char *spec)
{
	Mpipe *p;
	Chan *c;

	c = devattach('<', spec);
	p = mallocz(sizeof(Mpipe), 1);
	if(p == 0)
		exhausted("memory");
	p->ref = 1;

	lock(&mpipealloc);
	p->path = ++mpipealloc.path;
	unlock(&mpipealloc);

	mkqid(&c->qid, PIPEQID(2*p->path, Qdir), 0, QTDIR);
	c->aux = p;
	c->devno = 0;
	return c;
}

static int
mpipelen(Mpipe *p, int which)
{
	int len = 0;

	qlock(p);
	if(p->readers[which].first)
		len = qlen(p->readers[which].first->q);
	qunlock(p);
	return len;
}

static int
mpipegen(Chan *c, char*, Dirtab *tab, int ntab, int i, Dir *dp)
{
	Qid q;
	int len;
	Mpipe *p;

	if(i == DEVDOTDOT){
		devdir(c, c->qid, "#<", 0, eve, DMDIR|0555, dp);
		return 1;
	}
	i++;	/* skip . */
	if(tab==0 || i>=ntab)
		return -1;

	tab += i;
	p = c->aux;
	
	switch((ulong)tab->qid.path){
	case Qdata0:
		len = mpipelen(p, 0);
		break;
	case Qdata1:
		len = mpipelen(p, 1);
		break;
	default:
		len = tab->length;
		break;
	}

	mkqid(&q, PIPEQID(PIPEID(c->qid.path), tab->qid.path), 0, QTFILE);
	devdir(c, q, tab->name, len, eve, tab->perm, dp);
	return 1;
}


static Walkqid*
mpipewalk(Chan *c, Chan *nc, char **name, int nname)
{
	Walkqid *wq;
	Mpipe *p;

	wq = devwalk(c, nc, name, nname, mpipedir, NPIPEDIR, mpipegen);
	if(wq != nil && wq->clone != nil && wq->clone != c){
		if(c->flag & COPEN){
			print("mpipe: channel open in mpipewalk\n");
		} else {
			p = c->aux;
			qlock(p);
			p->ref++;
			qunlock(p);
		}
	}
	return wq;
}

static long
mpipestat(Chan *c, uchar *db, long n)
{
	Mpipe *p;
	Openq *openq;
	Dir dir;

	if(c->flag & COPEN){
		openq = c->aux;
		p = openq->mp;
	} else
		p = c->aux;

	switch(PIPETYPE(c->qid.path)){
	case Qdir:
		devdir(c, c->qid, ".", 0, eve, DMDIR|0555, &dir);
		break;
	case Qdata0:
		devdir(c, c->qid, "data", mpipelen(p,0), eve, 0600, &dir);
		break;
	case Qdata1:
		devdir(c, c->qid, "data1", mpipelen(p,1), eve, 0600, &dir);
		break;
	default:
		panic("mpipestat");
	}
	n = convD2M(&dir, db, n);
	if(n < BIT16SZ)
		error(Eshortstat);
	return n;
}

/* TODO: eventually will have to deal with enumeration & other modes*/
static void
mpipekick(Mpipe *p, int which, Openq *q)
{
	qlock(p);
	if(q->rnext) {
		print("mpipekick: already on read queue\n");
		goto error;
	}
	if(p->readers[which].first == nil) {
		p->readers[which].first = p->readers[which].last = q;
	
		/* kick first waiting writer */
		if(p->writers[which].first)
			wakeup(&p->writers[which].first->r);
	} else {
		p->readers[which].last->rnext = q;
		p->readers[which].last = q;
	}
error:
	qunlock(p);
}

/*
 *  if the stream doesn't exist, create it
 */
static Chan*
mpipeopen(Chan *c, int omode)
{
	Mpipe *p;
	int which = -1;
	Openq *openq;

	if(c->qid.type & QTDIR){
		if(omode != OREAD)
			error(Ebadarg);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	p = c->aux;

	openq = mallocz(sizeof(Openq), 1);
	if(openq == nil)
		error(Enomem);
	openq->mp = p;
	openq->mode = (omode&3)+1;
	if(openq->mode == 4)
		openq->mode = Oqrdwr;

	if(openq->mode & Oqreader) {
		openq->q = qopen(conf.pipeqsize, 0, 0, 0);
		if(openq->q)
			error(Enomem);
	}
	
	qlock(p);
	switch(PIPETYPE(c->qid.path)){
	case Qdata0:
		which = 0;
		break;
	case Qdata1:
		which = 1;
		break;
	}
	qunlock(p);

	if(openq->q)
		mpipekick(p, which, openq);

	c->aux = openq;	
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	c->iounit = qiomaxatomic;
	return c;
}

/* TODO: prints are debug prints and should be conditioned or removed */
static void
mpipeclose(Chan *c)
{
	Mpipe *p;

	if(c->flag & COPEN){
		Openq *openq;
		int dirty = 0;

		openq = c->aux;
		p = openq->mp;
		qlock(p);
		if(openq->mode & Oqreader) {
			if(openq->writer) {
				print("mpipe: close: closing an active reader\n");
				qhangup(openq->q, 0);
				dirty++;
				openq->writer->status = -1;
				openq->writer = nil;
				/* do we mark reader as having his q closed? */
			}
			if(openq->rnext)	/* if we are on the ready to read queue */
				dirty++;
		}
		if(openq->mode & Oqwriter) {
			if(openq->reader) {
				print("mpipe: close: closing an active writer\n");
				qhangup(openq->reader->q, 0);
				openq->reader = nil;
				dirty++;
			}
			if(openq->wnext) {   /* if we are on the ready to write queue */
				print("mpipe: close; closing a ready to write writer\n");
				wakeup(&openq->r);
				dirty++;
			}
		}

		/* if we aren't dirty we can free */
		c->aux = p;
		if(dirty==0) {
			free(openq);
			c->aux = p;
		} else 
			openq->mode = 0;
	} else {
		p = c->aux;
		qlock(p);
	}

	/*
	 *  free the structure on last close
	 */
	p->ref--;
	if(p->ref == 0){
		int count;
		for(count=0; count < 2; count++) {
			Openq *current = p->writers[count].first;
			while(current != nil) {
				print("mpipe: close: someone still waiting on write queue\n");
				wakeup(&current->r);
			}
			current = p->readers[count].first;
			while(current != nil) {
				Openq *next = current->rnext;
				free(current);
				current = next;
			}
		}
		qunlock(p);
		free(p);
	} else
		qunlock(p);
}

static long
mpiperead(Chan *c, void *va, long n, vlong)
{
	Openq *openq;
	Mpipe *p;
	int which = -1;

	switch(PIPETYPE(c->qid.path)){
	case Qdir:
		return devdirread(c, va, n, mpipedir, NPIPEDIR, mpipegen);
	case Qdata0:
		which = 0;
		break;
	case Qdata1:
		which =1;
		break;
	default:
		panic("mpiperead");
	}

	openq = c->aux;
	p = openq->mp;

	/* if we are empty kick writers */
	if((!openq->writer)&&(qlen(openq->q) == 0)&&(openq->rnext == nil))
		mpipekick(p, which, openq);

	if(n > openq->remain) { /* finished reading, wake final write */
		openq->writer->reader = nil;
		wakeup(&openq->writer->r);
		openq->writer = nil;
	}	

	return qread(openq->q, va, n);
}

static Block*
mpipebread(Chan *c, long n, vlong)
{
	Openq *openq;
	Mpipe *p;
	int which = -1;

	switch(PIPETYPE(c->qid.path)){
	case Qdata0:
		which = 0;
		break;
	case Qdata1:
		which =1;
		break;
	default:
		panic("mpipebread");
	}

	openq = c->aux;
	p = openq->mp;

	/* if we are empty kick waiting writers */
	if((!openq->writer)&&(qlen(openq->q) == 0)&&(openq->rnext == nil))
		mpipekick(p, which, openq);

	if(n >= openq->remain) { /* finished reading, wake final write*/
		openq->writer->reader = nil;
		wakeup(&openq->writer->r);
		openq->writer = nil;
	}
		
	return qbread(openq->q, n);
}

static int
checkfinished(void *arg)
{
	Openq *openq = arg;
	return openq->status;
}

/* no readers available, add us to the writer queue */
static void
writeradd(Mpipe *p, int which, Openq *q)
{
	qlock(p);
	if(q->wnext) {
		print("mpipe: writeradd: already on list\n");
		goto error;
	}
	if(p->writers[which].first == nil) 
		p->writers[which].first = p->writers[which].last = q;
	else {
		p->writers[which].last->wnext = q;
		p->writers[which].last = q;
	}
error:
	qunlock(p);
}

static int
readersavail(void *arg)
{
	return arg != nil;
}

static void
findreader(Mpipe *p, int which, Openq *openq)
{
	int added = 0;

	/* MAYBE: lock down openq? */
	while(openq->reader == nil) {   /* no readers available */
		if(!added) {
			writeradd(p, which, openq);
			added++;
		}

		sleep(&openq->r, readersavail, p->readers[which].first);

		qlock(p);
		if(p->readers[which].first == nil) {
			qunlock(p);
			continue;
		} 
		
		/* pull reader off queue */
		openq->reader = p->readers[which].first;
		p->readers[which].first = openq->reader->rnext;
		if(p->readers[which].first == nil)
			p->readers[which].last = nil;
		openq->reader->rnext = nil;

		/* pull us off the writer queue */
		if(p->writers[which].first == openq) {
			p->writers[which].first = openq->wnext;
			if(p->writers[which].first == nil)
				p->writers[which].last = nil;
		} else {
			Openq *cq;

			for(cq= p->writers[which].first; cq != nil; cq = cq->wnext) {
				if(cq->wnext == openq) {
					cq->wnext = openq->wnext;
					if(cq->wnext = nil)
						p->writers[which].last = cq;
					break;
				}
			}
		}
		openq->wnext = nil;

		qunlock(p);
	}
}

static int
adjustwrite(Openq *openq, int n)
{
	if(n > openq->remain) { 	/* shouldn't ever happen - print warning */
		print("mpipe: write: underflow\n");
		n = openq->remain;
	}
	openq->remain = openq->remain-n;
	if(openq->remain == 0) { 
		/* last record sent, wait for final read */
		sleep(&openq->r, checkfinished, openq);
		if(openq->status < 0) { /* reader closed before reading everything */
			n = n - qlen(openq->reader->q);
			openq->reader = nil;
		}	
	}
	return n;
}

static void
parseheader(Openq *openq, void *va, long n)
{
	char *len = malloc(n+1);
	memmove(len, va, n);
	len[n] = 0;
	openq->remain = strtoul(len, nil, 0);
	if(openq->remain == 0)
		print("mpipewrite: bad header string %s\n", len);
	free(len);
}

/*
 *  a write to a closed mpipe causes a note to be sent to
 *  the process.
 */
static long
mpipewrite(Chan *c, void *va, long n, vlong)
{
	Mpipe *p;
	Openq *openq;
	int which = -1;

	if(!islo())
		print("mpipewrite hi %#p\n", getcallerpc(&c));

	openq = c->aux;
	p = openq->mp;

	if(PIPETYPE(c->qid.path) == Qdata0) 
		which = 1;
	else if(PIPETYPE(c->qid.path) == Qdata1)
		which = 0;
	else
		panic("mpipewrite");

	findreader(p, which, openq);
	qlock(openq);
	if(openq->remain == 0) {	/* initial write of new record, read header */
		parseheader(openq, va, n);
	} else {
		if(n > openq->remain) {
			print("mpipewrite: write record overflow");
			n = openq->remain;
		}
		if(waserror()) {
			/* avoid notes when mpipe is a mounted queue */
			if((c->flag & CMSG) == 0)
				postnote(up, 1, "sys: write on closed mpipe", NUser);
			nexterror();
		}
		n = qwrite(openq->reader->q, va, n);
		poperror();
		if(n > 0)
			n = adjustwrite(openq, n);
	}
	qunlock(openq);
	return n;
}

static long
mpipebwrite(Chan *c, Block *bp, vlong)
{
	long n;
	Mpipe *p;
	Openq *openq;
	int which = -1;

	openq = c->aux;
	p = openq->mp;

	if(PIPETYPE(c->qid.path) == Qdata0) 
		which = 1;
	else if(PIPETYPE(c->qid.path) == Qdata1)
		which = 0;
	else
		panic("mpipewrite");

	n = blocklen(bp);

	findreader(p, which, openq);
	qlock(openq);
	if(openq->remain == 0) {	/* initial write of new record */ 
		/* assume its a single block */
		parseheader(openq, bp->base, n);
		freeb(bp); /* ?MAYBE? */
	} else {
		if(n > openq->remain) {
			print("mpipewrite: write record overflow, truncating");
			bp = trimblock(bp, 0, openq->remain);
		}
		if(waserror()) {
			/* avoid notes when mpipe is a mounted queue */
			if((c->flag & CMSG) == 0)
				postnote(up, 1, "sys: write on closed mpipe", NUser);
			nexterror();
		}
		n = qbwrite(openq->reader->q, bp);
		poperror();
		if(n>0)
			n = adjustwrite(openq, n);
	}
	qunlock(openq);
	
	return n;
}

Dev mpipedevtab = {
	'<',
	"mpipe",

	devreset,
	mpipeinit,
	devshutdown,
	mpipeattach,
	mpipewalk,
	mpipestat,
	mpipeopen,
	devcreate,
	mpipeclose,
	mpiperead,
	mpipebread,
	mpipewrite,
	mpipebwrite,
	devremove,
	devwstat,
};
