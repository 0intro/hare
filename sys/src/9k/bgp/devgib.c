/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: global interrupts and barriers
 *
 * All rights reserved
 *
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#include	"io.h"

enum{
	/* shift for 4-bit fields in `read data' */
	Rsticky_s=	28,		/* mode register (1=sticky) */
	Rstatus_s=	24,		/* sticky interrupt status (1=true) */
	Rarmed_s=	20,		/* ARM active (1=armed) */
	Rand_s=		16,		/* ARM type (0=OR, 1=AND) */
	Rdiag=	1<<15,		/* 0=normal mode */
	Rpu0err=	1<<14,		/* gi_pu0_add_err */
	Rpu1err=	1<<13,		/* gi_pu1_add_err */
	Rparity=	1<<12,		/* gi_parity_err */
	Rglobin_s=	8,		/* gi_ch_glob_int_in_r[0:3] */
	Ruser_s=		4,		/* gi_user_enables[0:3] when gi_read_mode=0, otherwise out[0:3] */
	Rgloben_s=	0,		/* gi_ch_glob_int_out_en[0:3] */
};

enum{
	GibDCR=	0x660,		/* base DCR number */
	Wstatus=		0x0,		/* read for status register (in fact any offset will do) */
						/* offsets to writable registers */
	Wenable0=	0x0,		/* set global in/out enable */
	Wenable1=	0x1,
	Wenable2=	0x2,
	Wenable3=	0x3,
	Wsetmode=	0x4,		/* 1=diagnostic read mode; 0=normal */
	Wreset=		0x5,		/* low order 4 bits are resets for 0 to 3 (L to R) */
	Wuser=		0x6,		/* low order 4 bits enable user access for 0 to 3 (L to R) */
	/* 7 is reserved */
	Wset0=		0x8,		/* 1= send interrupt using ARM type; 0=clear sticky int, clear armed */
	Wset1=		0x9,
	Wset2=		0xA,
	Wset3=		0xB,
	Warm0=		0xC,		/* 0=or; 1=and */
	Warm1=		0xD,
	Warm2=		0xE,
	Warm3=		0xF,
};

typedef struct GIport GIport;
struct GIport {
	Lock;
	int		init;
	u32int	set;
	u32int	arm;
	u32int	intrmask;
	u32int	enable;

	int	n;			/* 0 to 3 */
	uchar	mode;	/* None, Intr, Barrier */
	uchar	sig;		/* had an interrupt */
	uchar	kernel;	/* kernel owns this one */
	QLock	rl;		/* protects r */
	Rendez	r;		/* wait for interrupt or barrier */
};

enum{
	/* current mode */
	None= 0,
	Intr,
	Barrier,
};

static void
ginit(GIport *p, int n)
{
	ilock(p);
	if(p->init == 0){
		p->set = GibDCR + Wset0 + n;
		p->arm = GibDCR + Warm0 + n;
		p->enable = GibDCR + Wenable0 + n;
		p->n = n;
		p->intrmask = (0x8 >> n) << Rstatus_s;
		p->mode = None;
		p->sig = 0;
		p->kernel = 0;
		p->init = 1;
	}
	iunlock(p);
}

/*
 * follow the procedures on p. 9 of ``BG/L Global Interrupts''
 */

static void
clearandarm(GIport *p, int inv)
{
	ilock(p);
	p->sig = 0;
	dcrput(p->set, 0);
	dcrput(p->arm, inv);	/* inv=1 => AND; =0, OR (and drive 0) */
	imb();
	microdelay(1);		/* TO DO: 50 processor clocks */
	iunlock(p);
}

static void
gbarrier(GIport *p)
{
	ilock(p);
	p->sig = 0;
	dcrput(p->set, 0);
	dcrput(p->set, 1);
	iunlock(p);
}

static void
genable(GIport *p, int on)
{
	dcrput(p->enable, on);	/* TO DO: !on? */
}

enum{
	Ngi=	4,	/* hardware limit */

	Qdir = 0,
	Qbarrier,
	Qintr,
	Nfile=	Qintr-Qdir,	/* files per tree */
	Ndir=	1+(Ngi*Nfile),		/* size of directory */
};
#define	GQID(x, f)	(((x)<<4)|(f))
#define	GPORT(p)	((ulong)(p)>>4)
#define	GFILE(p)	((ulong)(p) & 0xF)

static GIport	ports[Ngi];
static Dirtab	gibdir[Ndir];
static Lock	giblock;		/* unused but would control write access to resets and enables */

static void
gibfile(Dirtab *d, char *name, int i, int path, ulong perm)
{
	snprint(d->name, sizeof(d->name), "gib%d%s", i, name);
	mkqid(&d->qid, GQID(i, path), 0, QTFILE);
	d->perm = perm;
	d->length = 0;
}

/*
 * there are two sets of interrupt lines:
 * the first four are not inverted (OR interrupts, signals);
 * the next four are inverted (AND interrupts, barriers).
 * we switch this function between vectors depending on which file is opened.
 */
static void
interrupt(Ureg*, void *a)
{
	GIport *p;
	int i;

	p = a;
	ilock(p);
	p->sig = 1;	/* record it */
	dcrput(p->set, 0);	/* reset it, and ensure it goes before continuing */
	for(i = 0; dcrget(GibDCR+Wstatus) & p->intrmask && i < 100; i++){
		microdelay(1);
		dcrput(p->set, 0);
	}
	iunlock(p);
	wakeup(&p->r);
}

static int
waitintr(void *a)
{
	GIport *p;

	p = a;
	return p->sig;
}

void
gibinit(void)
{
	Dirtab *d;
	int i;

	d = gibdir;
	strcpy(d->name, ".");
	mkqid(&d->qid, Qdir, 0, QTDIR);
	d->perm = DMDIR|0555;
	d++;
	for(i = 0; i < Ngi; i++){
		gibfile(d, "barrier", i, Qbarrier, 0222);
		d++;
		gibfile(d, "intr", i, Qintr, 0666);
		d++;

		ginit(&ports[i], i);
		genable(&ports[i], 1);		/* disable this port TO DO: active high or active low */
	}
}

static Chan*
gibattach(char* spec)
{
	return devattach('!', spec);
}

static Walkqid*	 
gibwalk(Chan* c, Chan *nc, char** name, int nname)
{
	return devwalk(c, nc, name, nname, gibdir, nelem(gibdir), devgen);
}

static long	 
gibstat(Chan* c, uchar* dp, long n)
{
	return devstat(c, dp, n, gibdir, nelem(gibdir), devgen);
}

static void
setmode(GIport *p, int mode)
{
	ilock(p);
	if(p->mode != None){
		unlock(p);
		error(Einuse);
	}
	p->mode = mode;
	iunlock(p);
}

static Chan*
gibopen(Chan* c, int omode)
{
	GIport *p;

	c = devopen(c, omode, gibdir, nelem(gibdir), devgen);
	if(waserror()){
		c->flag &= ~COPEN;
		nexterror();
	}
	p = &ports[GPORT(c->qid.path)];
	switch(GFILE(c->qid.path)){
	case Qintr:
		setmode(p, Intr);
		clearandarm(p, 0);	/* OR */
		intrenable(VectorGlobalInt0+p->n, interrupt, p, "gib", 0);
		break;

	case Qbarrier:
		setmode(p, Barrier);
		clearandarm(p, 1);	/* AND */
		intrenable(VectorGlobalInt0+4+p->n, interrupt, p, "gib", 0);
		break;
	}
	poperror();
	return c;
}

static void	 
gibclose(Chan *c)
{
	GIport *p;
	int n;

	if((c->flag & COPEN) == 0)
		return;
	n = 0;
	p = &ports[GPORT(c->qid.path)];
	switch(GFILE(c->qid.path)){
	case Qbarrier:
		n = 4;
		/* fall through */
	case Qintr:
		ilock(p);
		p->mode = None;
		iunlock(p);
		intrdisable(VectorGlobalInt0+n+p->n, interrupt, p, "gib");
		break;
	}
}

static long	 
gibread(Chan* c, void* buf, long n, vlong offset)
{
	GIport *p;

	if(c->qid.type & QTDIR)
		return devdirread(c, buf, n, gibdir, nelem(gibdir), devgen);

	p = &ports[GPORT(c->qid.path)];
	switch(GFILE(c->qid.path)){
	default:
		error(Egreg);

	case Qintr:
		qlock(&p->rl);
		if(waserror()){
			qunlock(&p->rl);
			nexterror();
		}
		sleep(&p->r, waitintr, p);
		poperror();
		qunlock(&p->rl);
		if((offset==0)&&(n)) {
			*(char*)buf = '1';
			return 1;
		} else
			return 0;
	}
}

static long	 
gibwrite(Chan* c, void*, long n, vlong)
{
	GIport *p;

	p = &ports[GPORT(c->qid.path)];
	switch(GFILE(c->qid.path)){
	default:
		error(Egreg);

	case Qbarrier:
		qlock(&p->rl);
		if(waserror()){
			qunlock(&p->rl);
			nexterror();
		}
		gbarrier(p);
		sleep(&p->r, waitintr, p);
		poperror();
		qunlock(&p->rl);
		break;

	case Qintr:
		ilock(p);
		p->sig = 0;
		dcrput(p->set, 1);	/* signal */
		iunlock(p);
		break;
	}
	return n;
}

Dev gibdevtab = {
	'!',
	"gib",

	devreset,
	gibinit,
	devshutdown,
	gibattach,
	gibwalk,
	gibstat,
	gibopen,
	devcreate,
	gibclose,
	gibread,
	devbread,
	gibwrite,
	devbwrite,
	devremove,
	devwstat,
};

/*
 * kernel interface to barriers
 */

void
claimbarrier(int n)
{
	GIport *p;

	p = &ports[n];
	if(p->init == 0){
		ginit(p, n);
		genable(p, 1);
	}
	ilock(p);
	if(p->mode == None){
		p->mode = Barrier;
		p->kernel = 1;
	}
	iunlock(p);
	if(!p->kernel)
		panic("barrier %d in use", n);
	clearandarm(p, 1);	/* AND */
}

void
releasebarrier(int n)
{
	GIport *p;

	p = &ports[n];
	if(!p->kernel)
		panic("barrier %d not claimed", n);
	ilock(p);
	p->kernel = 0;
	p->mode = None;
	iunlock(p);
}

int
globalbarrier(int n, ulong msec)
{
	GIport *p;
	int i, r;
	ulong tout;

	p = &ports[n];
	if(!p->kernel)
		panic("barrier %d not claimed", n);
	gbarrier(p);
	r = 0;
	tout = MACHP(0)->ticks + ms2tk(msec);
	while((dcrget(GibDCR+Wstatus) & p->intrmask) == 0){
		/* wait */
		if(tout < MACHP(0)->ticks){
			r = -1;
			break;
		}
	}
	dcrput(p->set, 0);	/* reset it, and ensure it goes before continuing */
	for(i = 0; dcrget(GibDCR+Wstatus) & p->intrmask && i < 100; i++){
		microdelay(1);
		dcrput(p->set, 0);
	}
	return r;
}
