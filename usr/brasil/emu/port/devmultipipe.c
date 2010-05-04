/*
 * devmultipipe.c - multipipe device
 *
 * Copyright (C) 2010, IBM Corporation, 
 *		Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: 
 *   The multipipe device keeps track of multiple readers and
 *   multiple writers, providing separate queues for each.
 *   When there are multiple writers, the multipipe will only forward
 *   a record when an end-of-record identifier (a write of length 0) is
 *   received.
 *   When there are multiple readers, the record will be forwarded in
 *   a round robin fashion to the first queue with available space for
 *   the entire record.
 *
 * Based in part on devpipe.c
 */
 
 /*
      TODO:
          * finish qqkick implementation
          * add qqkick to reader sections when we empty buffers
          * replace old qlen with code which reports bytes on top ready to read queue
          * add code to ensure that writers will never block
          * add corner cases on close (where data is still on buffers 
          * add some debug prints to help with verification/test
  */

#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	<interp.h>

#define NETTYPE(x)	((ulong)(x)&0x1f)
#define NETID(x)	(((ulong)(x))>>5)
#define NETQID(i,t)	(((i)<<5)|(t))

/* TODO: Fix me */
#define Maxatomic (32 * 1024)

typedef struct Queueqe Queueqe;
struct Queueqe			/* queue of queues element */
{
	Queue *q;
	Queueqe *prev;
	Queueqe *next;
};

typedef struct Queueq Queueq;
struct Queueq			/* queue of queues */
{
	Queueqe *head;
	Queueqe *tail;
};

typedef struct Multipipe	Multipipe;
struct Multipipe
{
	QLock	l;
	Multipipe*	next;
	int	ref;
	ulong	path;
	
	Queueq writers[2];	/* queues ready to write */
	Queueq readers[2]; /* queues ready to read */
	
	Dirtab*	pipedir;
	char*	user;
	int	pipeqsize;
};

typedef struct Openq Openq;
struct Openq
{
	Queueqe qein;
	Queueqe qeout;
	Multipipe *mp;
};

static struct
{
	Lock	l;
	ulong	path;	/* qid.path accounting */
} pipealloc;

enum
{
	Qdir,
	Qdata0,
	Qdata1
};

Dirtab multipipedir[] =
{
	".",		{Qdir,0,QTDIR},	0,		DMDIR|0500,
	"data",		{Qdata0},	0,			0660,
	"data1",	{Qdata1},	0,			0660,
};

/* stupid qq ops */

/* we always add to the tail of the list */
static void
qqadd(Queueq *q, Queueqe *qe) 
{
	qe->prev = q->tail;
	q->tail = qe;
	qe->next = nil;
	if(q->head == nil)
		q->head = q->tail;
}

/* remove us from the queue list -- this assumes we were on the list */
static void
qqdel(Queueq *q, Queueqe *qe)
{
	if(q->head == qe)
		q->head = qe->next;
	if(q->tail == qe)
		q->tail = qe->prev;
	if(qe->prev)
		qe->prev->next = qe->next;
	if(qe->next)
		qe->next->prev = qe->prev;
}

/* a writer is ready, check appropriate writer q and reader q */
/* we may need to lock the pipe */
static void
qqkick(Multipipe *p, int which)
{
	/* TODO: go through entire ready to write q and
	 * transfer to ready to readq */
	 Queueqe *current = p->writers[which].head;
	 Queueqe *next;
	 Block *b;
	 
	 while(current != nil) {
	 	next = current->next;
	 	qqdel(&p->writers[which], current);
	 	/* TODO: find reader & remove him from the ready to read list */
	 	/* TODO: empty writer queue into available reader queue */
	 	
	 	current = next;
	 }
}

static void
freepipe(Multipipe *p)
{
	if(p != nil){
		free(p->user);
		free(p->pipedir);

		free(p);
	}
}

/*
 *  create a multipipe, no streams are created until an open
 */
static Chan*
multipipeattach(char *spec)
{
	Multipipe *p;
	Chan *c;

	c = devattach('<', spec);
	p = malloc(sizeof(Multipipe));
	if(p == 0)
		error(Enomem);
	if(waserror()){
		freepipe(p);
		nexterror();
	}
	p->pipedir = malloc(sizeof(multipipedir));
	if (p->pipedir == 0)
		error(Enomem);
	memmove(p->pipedir, multipipedir, sizeof(multipipedir));
	kstrdup(&p->user, up->env->user);
	p->ref = 1;
	
	p->pipeqsize = atoi(spec);
	if(p->pipeqsize == 0)
		p->pipeqsize = 32 * 1024;
	
	p->writers[0].head = nil;
	p->writers[1].head = nil;
	p->readers[0].head = nil;
	p->readers[1].head = nil;
	
	poperror();

	lock(&pipealloc.l);
	p->path = ++pipealloc.path;
	unlock(&pipealloc.l);

	c->qid.path = NETQID(2*p->path, Qdir);
	c->qid.vers = 0;
	c->qid.type = QTDIR;
	c->aux = p;
	c->dev = 0;
	return c;
}

static int
multipipegen(Chan *c, char *name, Dirtab *tab, int ntab, int i, Dir *dp)
{
	int id, len;
	Qid qid;
	Multipipe *p;

	USED(name);
	if(i == DEVDOTDOT){
		devdir(c, c->qid, "#<", 0, eve, 0555, dp);
		return 1;
	}
	i++;	/* skip . */
	if(tab==0 || i>=ntab)
		return -1;
	tab += i;
	p = c->aux;
	switch(NETTYPE(tab->qid.path)){
	case Qdata0:
		/* TODO: oh poopy */
		len = Maxatomic; /*qlen(p->q[0])*/
		break;
	case Qdata1:
		len = Maxatomic;
		break;
	default:
		len = tab->length;
		break;
	}
	id = NETID(c->qid.path);
	qid.path = NETQID(id, tab->qid.path);
	qid.vers = 0;
	qid.type = QTFILE;
	devdir(c, qid, tab->name, len, eve, tab->perm, dp);
	return 1;
}

static Walkqid*
multipipewalk(Chan *c, Chan *nc, char **name, int nname)
{
	Walkqid *wq;
	Multipipe *p;

	p = c->aux;
	wq = devwalk(c, nc, name, nname, p->pipedir, nelem(multipipedir), multipipegen);
	if(wq != nil && wq->clone != nil && wq->clone != c){
		qlock(&p->l);
		p->ref++;
		qunlock(&p->l);
	}
	return wq;
}

static int
multipipestat(Chan *c, uchar *db, int n)
{
	Multipipe *p;
	Dir dir;
	Dirtab *tab;

	p = c->aux;
	tab = p->pipedir;

	switch(NETTYPE(c->qid.path)){
	case Qdir:
		devdir(c, c->qid, ".", 0, eve, DMDIR|0555, &dir);
		break;
	case Qdata0:
		devdir(c, c->qid, tab[1].name, Maxatomic, eve, tab[1].perm, &dir);
		break;
	case Qdata1:
		devdir(c, c->qid, tab[2].name, Maxatomic, eve, tab[2].perm, &dir);
		break;
	default:
		panic("pipestat");
	}
	n = convD2M(&dir, db, n);
	if(n < BIT16SZ)
		error(Eshortstat);
	return n;
}

/*
 *  if the stream doesn't exist, create it
 */
static Chan*
multipipeopen(Chan *c, int omode)
{
	Multipipe *p;
	Openq *openq;
	int which = -1;

	if(c->qid.type & QTDIR){
		if(omode != OREAD)
			error(Ebadarg);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	openmode(omode);	/* check it */
	
	if(c->flag == COPEN)	/* ALREADY OPEN? */
		error("multipipe already open, how can that happen");

	p = c->aux;
	
	qlock(&p->l);
	if(waserror()){
		qunlock(&p->l);
		nexterror();
	}
	switch(NETTYPE(c->qid.path)){
	case Qdata0:
		which = 0;
		devpermcheck(p->user, p->pipedir[1].perm, omode);
		break;
	case Qdata1:
		which=1;
		devpermcheck(p->user, p->pipedir[2].perm, omode);
		break;
	}

	openq = malloc(sizeof(Openq));
	if(openq == nil)
		error(Enomem);
	
	openq->mp = p;
	if((omode == OWRITE)||(omode == ORDWR))
		openq->qein.q = qopen(p->pipeqsize, 0,0,0);
	if((omode & OREAD)||(omode == ORDWR)) {
		openq->qeout.q = qopen(p->pipeqsize, 0,0,0);
		/* add qeout to readerq for this file */
		qqadd(&p->readers[which], &openq->qeout);
	}
	c->aux = openq;
	
	poperror();
	qunlock(&p->l);

	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	c->iounit = qiomaxatomic;
	return c;
}

static void
multipipeclose(Chan *c)
{
	Multipipe *p;
	Openq *openq = nil;
	int which = -1;

	if(c->qid.type & QTDIR) {
		p = c->aux;
	} else {
		openq = c->aux;
		p = openq->mp;
	}
		
	qlock(&p->l);

	if(c->flag & COPEN){
		/*
		 *  closing either side hangs up the stream
		 */
		switch(NETTYPE(c->qid.path)){
		case Qdata0:
			which = 0;
			break;
		case Qdata1:
			which = 1;
			break;
		}

		if(openq) {
			/* TODO: if there is data in our inpipe flush it */
			/* TODO:  if there is data in our out pipe? dunno */		
			if(openq->qein.q) {
				qhangup(openq->qein.q, 0);
				qclose(openq->qein.q);
				qqdel(&p->writers[which], &openq->qein);
				free(openq);
			}
			if(openq->qeout.q) {
				qhangup(openq->qeout.q, 0);
				qclose(openq->qeout.q);
				qqdel(&p->readers[which], &openq->qeout);
				free(openq);				
			}
		}
	}

	/*
	 *  free the structure on last close
	 */
	p->ref--;
	if(p->ref == 0){
		qunlock(&p->l);
		freepipe(p);
	} else
		qunlock(&p->l);
}

static long
multipiperead(Chan *c, void *va, long n, vlong junk)
{
	Multipipe *p;
	Openq *openq = nil;	
	
	USED(junk);

	if(c->qid.type & QTDIR) {
		p = c->aux;
	} else {
		openq = c->aux;
		p = openq->mp;
	}

	switch(NETTYPE(c->qid.path)){
	case Qdir:
		return devdirread(c, va, n, p->pipedir, nelem(multipipedir), multipipegen);
	case Qdata0:
	case Qdata1:
		if(openq) /* TODO: if queue is empty when we are done kick writers */
			return qread(openq->qeout.q, va, n);
	default:
		panic("piperead");
	}
	return -1;	/* not reached */
}

static Block*
multipipebread(Chan *c, long n, ulong offset)
{
	Multipipe *p;
	Openq *openq = nil;	

	if(c->qid.type & QTDIR) {
		p = c->aux;
	} else {
		openq = c->aux;
		p = openq->mp;
	}

	if(openq) {
		switch(NETTYPE(c->qid.path)){
		case Qdata0:
		case Qdata1:
			/* TODO: if queue is empty when we are done kick writers */
			return qbread(openq->qeout.q, n);
		}
	}

	return devbread(c, n, offset);
}

/*
 *  a write to a closed pipe causes an exception to be sent to
 *  the prog.
 */
static long
multipipewrite(Chan *c, void *va, long n, vlong junk)
{
	Multipipe *p;
	Prog *r;
	Openq *openq = nil;
	long ret;
	int which = -1;

	USED(junk);
	if(waserror()) {
		/* avoid exceptions when pipe is a mounted queue */
		if((c->flag & CMSG) == 0) {
			r = up->iprog;
			if(r != nil && r->kill == nil)
				r->kill = "write on closed pipe";
		}
		nexterror();
	}

	if(c->qid.type & QTDIR) {
		p = c->aux;
	} else {
		openq = c->aux;
		p = openq->mp;
	}

	if(openq) {
		switch(NETTYPE(c->qid.path)){
		case Qdata0:
			which = 0;
			break;
		case Qdata1:
			which = 1;
			break;
		default:
			panic("pipewrite");
		}
		if(n == 0) {
			ret = 0;
			/* add queue to ready to write queue and kick it */
			qqadd(&p->writers[which], &openq->qeout);
			qqkick(p, which);
		} else {
			/* TODO: Need to check that we won't block */
			ret = qwrite(openq->qein.q, va, n);
		}
	}

	poperror();
	return n;
}

static long
multipipebwrite(Chan *c, Block *bp, ulong junk)
{
	long n = 0;
	long ret = 0;
	Multipipe *p;
	Prog *r;
	Openq *openq = nil;
	int which = -1;

	USED(junk);
	if(waserror()) {
		/* avoid exceptions when pipe is a mounted queue */
		if((c->flag & CMSG) == 0) {
			r = up->iprog;
			if(r != nil && r->kill == nil)
				r->kill = "write on closed pipe";
		}
		nexterror();
	}

	if(c->qid.type & QTDIR) {
		p = c->aux;
	} else {
		openq = c->aux;
		p = openq->mp;
	}
	
	if(openq) {
		switch(NETTYPE(c->qid.path)){
		case Qdata0:
			which = 0;
			break;
		case Qdata1:
			which = 1;
			break;	
		default:
			n = 0;
			panic("pipebwrite");
		}
		
		n = BLEN(bp);
		if(n == 0) {
			ret = 0;
			/* add queue to ready to write queue and kick it */
			qqadd(&p->writers [which], &openq->qeout);
			qqkick(p, which);		
		} else {
			/* TODO: Need to check that we won't block */
			ret = qbwrite(openq->qein.q, bp);
		}	
	}

	poperror();
	return n;
}

/* TODO: maybe add some bits to remark on the readers/writers
 * someplace in the stat (version?  size?  uid?  gid? muid? 
 * or maybe that's just an abuse */
static int
multipipewstat(Chan *c, uchar *dp, int n)
{
	Dir *d;
	Multipipe *p;
	int d1;

	if (c->qid.type&QTDIR)
		error(Eperm);
	p = c->aux;
	if(strcmp(up->env->user, p->user) != 0)
		error(Eperm);
	d = smalloc(sizeof(*d)+n);
	if(waserror()){
		free(d);
		nexterror();
	}
	n = convM2D(dp, n, d, (char*)&d[1]);
	if(n == 0)
		error(Eshortstat);
	d1 = NETTYPE(c->qid.path) == Qdata1;
	if(!emptystr(d->name)){
		validwstatname(d->name);
		if(strlen(d->name) >= KNAMELEN)
			error(Efilename);
		if(strcmp(p->pipedir[1+!d1].name, d->name) == 0)
			error(Eexist);
		kstrcpy(p->pipedir[1+d1].name, d->name, KNAMELEN);
	}
	if(d->mode != ~0UL)
		p->pipedir[d1 + 1].perm = d->mode & 0777;
	poperror();
	free(d);
	return n;
}

Dev multipipedevtab = {
	'<',
	"multipipe",

	devinit,
	multipipeattach,
	multipipewalk,
	multipipestat,
	multipipeopen,
	devcreate,
	multipipeclose,
	multipiperead,
	multipipebread,
	multipipewrite,
	multipipebwrite,
	devremove,
	multipipewstat,
};
