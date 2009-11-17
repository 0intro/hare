/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: collective network device interface
 *
 * All rights reserved
 *
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#include "ureg.h"

#include "bgp_personality.h"
#include "bgcns.h"
#include "io.h"

enum{
	Nvc=	2,

	Qdir = 0,
	Qdata,
	Qctl,
	Qstatus,
	Qtag,
	Qp2p,

	PayloadSize=	256,
	Alignment=	16,	/* payload needs to be aligned */
	Hsize=	4,
	Hpad=	Alignment-Hsize,	/* so that Hpad+Hsize = 0 mod Alignment */
	MessageSize=	PayloadSize+Hsize,
};
#define	GQID(x, f)	(((x)<<4)|(f))
#define	GPORT(p)	((ulong)(p)>>4)
#define	GFILE(p)	((ulong)(p) & 0xF)

static Dirtab treedir[] = {
	{".",		{Qdir, 0, QTDIR},	0,	0777},
	{"vc0",	{GQID(0, Qdata), 0},		0,	0666},
	{"vc0ctl",	{GQID(0, Qctl), 0},	0,	0666},
	{"vc0status",	{GQID(0, Qstatus), 0},	0,	0444},
	{"vc0tag",		{GQID(0, Qtag), 0},	0,	0666},
	{"vc0p2p",	{GQID(0, Qp2p), 0},	0,	0666},
	{"vc1",	{GQID(1, Qdata), 0},		0,	0666},
	{"vc1ctl",	{GQID(1, Qctl), 0},	0,	0666},
	{"vc1status",	{GQID(1, Qstatus), 0},	0,	0444},
	{"vc1tag",	{GQID(1, Qtag), 0},	0,	0666},
	{"vc1p2p",	{GQID(1, Qp2p), 0},	0,	0666},
};

typedef struct Vc Vc;
struct Vc {
	int	inuse;
	int	n;				/* virtual channel number */
	u32int	mode;				/* p2p or not */
	u32int	dest;
	u32int	class;
};

static	Vc	treeport[Nvc];

static void
treereset(void)
{
	treenetreset();
}

static void
treeinit(void)
{
	treenetinit();

	treeport[0].n = 0;
	treeport[0].mode = PIH_P2P;
	treeport[0].dest = sys->myp2paddr;
	treeport[0].class = 0;
	treeport[1].n = 1;
}

static Chan*
treeattach(char* spec)
{
	return devattach(':', spec);
}

static Walkqid*
treewalk(Chan* c, Chan *nc, char** name, int nname)
{
	return devwalk(c, nc, name, nname, treedir, nelem(treedir), devgen);
}

static long
treestat(Chan* c, uchar* dp, long n)
{
	return devstat(c, dp, n, treedir, nelem(treedir), devgen);
}

static Chan*
treeopen(Chan* c, int omode)
{
	return devopen(c, omode, treedir, nelem(treedir), devgen);
}

static void
treeclose(Chan*)
{
}

static long
treeread(Chan* c, void* buf, long n, vlong off)
{
	Vc *p;
	char *s;
	int i, l, nr;
	u32int routes[8];

	if(c->qid.type & QTDIR)
		return devdirread(c, buf, n, treedir, nelem(treedir), devgen);

	p = &treeport[GPORT(c->qid.path)];
	switch(GFILE(c->qid.path)){
	default:
		error(Egreg);

	case Qdata:
		return treenetread(p->n, buf, n);

	case Qctl:
		return 0;

	case Qstatus:
		s = smalloc(3*READSTR);
		if(waserror()){
			free(s);
			nexterror();
		}
		l = treenetstats(p->n, s, 2*READSTR);
		nr = treenetroutes(p->n, routes, nelem(routes));
		for(i = 0; i < nr; i++)
			l += snprint(s+l, READSTR-l, "r%d %8.8ux\n", i, routes[i]);
		n = readstr(off, buf, n, s);
		poperror();
		free(s);
		return n;
	}
}

static long
treewrite(Chan *c, void *a, long n, vlong)
{
	Vc *p;
	Block *b;
	Cmdbuf *cb;

	p = &treeport[GPORT(c->qid.path)];
	switch(GFILE(c->qid.path)){
	default:
		error(Egreg);

	case Qdata:
		if(n < 4)
			error("missing tree header");
		if(n > MessageSize)
			n = MessageSize;
		/*
		 * there's a 4-byte header and a 256-byte payload.
		 * we need the payload aligned 0 mod 16, and allocb will
		 * align b->wp to at least that (0 mod BLOCKALIGN),
		 * so adding a further 12 bytes before the 4 byte header
		 * and adjusting wp to skip those 12 leaves the payload aligned.
		 */
		b = allocb(n + Hpad);
		if(waserror()){
			freeb(b);
			nexterror();
		}
		/* b->wp is aligned to at least BLOCKALIGN, so put the 4-byte header so the adjacent data is on 0 mod 16 bytes */
		b->wp += Hpad;
		b->rp = b->wp;
		memmove(b->wp, a, n);
		b->wp += n;
		poperror();
		treenetsend(p->n, b);
		return n;

	case Qtag:{
		/* raw tag packet. Format is 't' in first byte, then cistttto, where
		  * c is class, i is interrupt, s is checksum, tttt is tag, and o is opcode. opcode 8 is send.
		  * example, t010beef8hi would send class 0, interrupt 1, csum 0, tag beef, data hi to net
		  * csum is currently ignored, but we may need it later.
		  * it has proven very convenient to have ascii interface for this so we are sticking with that
		  * for now
		  */
		/* Sorry guys I don't know the Plan 9 coding mantra for this type of thing. Fix as needed. */
		int class, intr, tag, op, i;
		unsigned char *buf = a;
		if (n < 9)
			error("bad message format");
		class = buf[1] - '0';
		intr = buf[2] != '0';
		for(i = tag = 0; i < 4; i++)
			tag = (tag << 4) | (buf[i+4] > '9'?  10 + buf[i+4] - 'a' : buf[i+4]- '0');
		op = buf[8] - '0';
		b = treetag(class, intr, tag, op);
		if(waserror()){
			freeb(b);
			nexterror();
		}
		n -= 9;
		buf += 9;
		if(n > b->lim - b->wp)
			n = b->lim - b->wp;
		memmove(b->wp,buf, n);
		b->wp += n;
		poperror();
		treedumpblock(b, "tagsend");/**/
		return treenetsend(p->n, b);
	}
	case Qp2p:{
		/* raw tag packet. Format is 'p' in first byte, then cispppppp, where
		  * c is class, i is interrupt, s is checksum,pppppp is p2p address
		  * example, p010001122hi would send class 0, interrupt 1, csum 0,
		  * address 001122, data hi
		  * csum is currently ignored, but we may need it later.
		  * it has proven very convenient to have ascii interface for this so we are sticking with that
		  * for now
		  */
		/* Sorry guys I don't know the Plan 9 coding mantra for this type of thing. Fix as needed. */
		int class, p2p,  i;
		unsigned char *buf = a;
		if (n < 10)
			error("bad message format");
		class = buf[1] - '0';
		for(i = p2p = 0; i < 6; i++)
			p2p = (p2p << 4) | (buf[i+4] > '9'?  10 + buf[i+4] - 'a' : buf[i+4]- '0');
		b = treep2p(class, p2p);
		if(waserror()){
			freeb(b);
			nexterror();
		}
		n -= 10;
		buf += 10;
		if(n > b->lim - b->wp)
			n = b->lim - b->wp;
		memmove(b->wp,buf, n);
		b->wp += n;
		poperror();
		treedumpblock(b, "p2psend");/**/
		return treenetsend(p->n, b);
	}
	case Qctl:
		cb = parsecmd(a, n);
		if(waserror()){
			free(cb);
			nexterror();
		}
		if(cb->nf > 1 && strcmp(cb->f[0], "dest") == 0)
			p->dest = strtoul(cb->f[1], 0, 0) & 0xFFFFFF;
		else if(cb->nf > 1 && strcmp(cb->f[0], "class") == 0)
			p->class = strtoul(cb->f[1], 0, 0) & 0xF;
		else if(strcmp(cb->f[0], "kick") == 0){
			extern void treekick(int); treekick(1);
		}else
			cmderror(cb, "invalid request");
		poperror();
		free(cb);
		break;
	}
	return n;
}

static Block*
treebread(Chan *c, long n, vlong offset)
{
	if(GFILE(c->qid.path) != Qdata)
		return devbread(c, n, offset);
	return treenetbread(GPORT(c->qid.path), n);
}

#define ALIGNED(p, a)	(!(((uintptr)(p)) & ((a)-1)))

static long
treebwrite(Chan *c, Block *b, vlong offset)
{
	long n;
	Block *nbp;

	if(GFILE(c->qid.path) != Qdata)
		return devbwrite(c, b, offset);
	n = BLEN(b);
	if(n < Hsize){
		freeb(b);
		error(Etoosmall);
	}
	if(n > MessageSize){
		freeb(b);
		error(Etoobig);
	}
	if(!ALIGNED(b->rp+Hsize, Alignment) || n < MessageSize && b->rp+MessageSize > b->lim){
		/* repack with payload aligned, and grow */
		nbp = allocb(Hpad+MessageSize);
		nbp->rp += Hpad;
		memmove(nbp->rp, b->rp, n);
		/*TO DO: could memset extra bits */
		nbp->wp = nbp->rp+MessageSize;
		freeb(b);
		treenetsend(GPORT(c->qid.path), nbp);
	}else
		treenetsend(GPORT(c->qid.path), adjustblock(b, MessageSize));
	return n;
}

Dev treedevtab = {
	':',
	"tree",

	treereset,
	treeinit,
	devshutdown,
	treeattach,
	treewalk,
	treestat,
	treeopen,
	devcreate,
	treeclose,
	treeread,
	treebread,
	treewrite,
	treebwrite,
	devremove,
	devwstat,
};
