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
	int mode;			/* inactive, reader, writer, or read/write */

	Queue *q;			/* for readers */
	Rendez r;			/* for a blocked writer */

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
	
	qlock(p);
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
	qunlock(p);

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
		p = c->aux;
		qlock(p);
		p->ref++;
		if(c->flag & COPEN){
			print("channel open in mpipewalk\n");
		}
		qunlock(p);
	}
	return wq;
}

static long
mpipestat(Chan *c, uchar *db, long n)
{
	Mpipe *p;
	Dir dir;

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
	if(p->readers[which].first) {
		p->readers[which].first = p->readers[which].last = q;
	
		/* kick first waiting writer */
		if(p->writers[which].first)
			wakeup(&p->writers[which].first->r);
	} else {
		p->readers[which].last->rnext = q;
		p->readers[which].last = q;
	}
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

/* TODO: prints are debug prints and should be */
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

/* TODO: Implement Read & Write */

static long
mpiperead(Chan *c, void *va, long n, vlong)
{
	Mpipe *p;

	p = c->aux;

	switch(PIPETYPE(c->qid.path)){
	case Qdir:
		return devdirread(c, va, n, mpipedir, NPIPEDIR, mpipegen);
	case Qdata0:
		return qread(p->q[0], va, n);
	case Qdata1:
		return qread(p->q[1], va, n);
	default:
		panic("mpiperead");
	}
	return -1;	/* not reached */
}

static Block*
mpipebread(Chan *c, long n, vlong offset)
{
	Mpipe *p;

	p = c->aux;

	switch(PIPETYPE(c->qid.path)){
	case Qdata0:
		return qbread(p->q[0], n);
	case Qdata1:
		return qbread(p->q[1], n);
	}

	return devbread(c, n, offset);
}

/*
 *  a write to a closed mpipe causes a note to be sent to
 *  the process.
 */
static long
mpipewrite(Chan *c, void *va, long n, vlong)
{
	Mpipe *p;

	if(!islo())
		print("mpipewrite hi %#p\n", getcallerpc(&c));
	if(waserror()) {
		/* avoid notes when mpipe is a mounted queue */
		if((c->flag & CMSG) == 0)
			postnote(up, 1, "sys: write on closed mpipe", NUser);
		nexterror();
	}

	p = c->aux;

	switch(PIPETYPE(c->qid.path)){
	case Qdata0:
		n = qwrite(p->q[1], va, n);
		break;

	case Qdata1:
		n = qwrite(p->q[0], va, n);
		break;

	default:
		panic("mpipewrite");
	}

	poperror();
	return n;
}

static long
mpipebwrite(Chan *c, Block *bp, vlong)
{
	long n;
	Mpipe *p;

	if(waserror()) {
		/* avoid notes when mpipe is a mounted queue */
		if((c->flag & CMSG) == 0)
			postnote(up, 1, "sys: write on closed mpipe", NUser);
		nexterror();
	}

	p = c->aux;
	switch(PIPETYPE(c->qid.path)){
	case Qdata0:
		n = qbwrite(p->q[1], bp);
		break;

	case Qdata1:
		n = qbwrite(p->q[0], bp);
		break;

	default:
		n = 0;
		panic("mpipebwrite");
	}

	poperror();
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
