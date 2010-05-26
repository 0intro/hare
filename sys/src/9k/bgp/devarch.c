/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Based on ppc440/devarch.c Copyright (C) 2006-2009 Alcatel-Lucent 
 *
 * Description: platform specific device interface for BG/P
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

#include "../ip/ip.h"

#include "bgp_personality.h"

enum {
	Qdir = 0,
	Qbase,

	Qmax = 16,
};

typedef long Rdwrfn(Chan*, void*, long, vlong);

static Rdwrfn *readfn[Qmax];
static Rdwrfn *writefn[Qmax];

static Dirtab archdir[Qmax] = {
	".",		{ Qdir, 0, QTDIR },	0,	0555,
};

static Lock archwlock;		/* the lock is only for changing archdir */
static int narchdir = Qbase;

/*
 * Add a file to the #P listing.  Once added, you can't delete it.
 * You can't add a file with the same name as one already there,
 * and you get a pointer to the Dirtab entry so you can do things
 * like change the Qid version.  Changing the Qid path is disallowed.
 */
Dirtab*
addarchfile(char* name, int perm, Rdwrfn* rdfn, Rdwrfn* wrfn)
{
	int i;
	Dirtab d;
	Dirtab *dp;

	memset(&d, 0, sizeof d);
	strcpy(d.name, name);
	d.perm = perm;

	lock(&archwlock);
	if(narchdir >= Qmax){
		unlock(&archwlock);
		return nil;
	}

	for(i=0; i<narchdir; i++)
		if(strcmp(archdir[i].name, name) == 0){
			unlock(&archwlock);
			return nil;
		}

	d.qid.path = narchdir;
	archdir[narchdir] = d;
	readfn[narchdir] = rdfn;
	writefn[narchdir] = wrfn;
	dp = &archdir[narchdir++];
	unlock(&archwlock);

	return dp;
}

static Chan*
archattach(char* spec)
{
	return devattach('P', spec);
}

static Walkqid*
archwalk(Chan* c, Chan* nc, char** name, int nname)
{
	return devwalk(c, nc, name, nname, archdir, narchdir, devgen);
}

static long
archstat(Chan* c, uchar* dp, long n)
{
	return devstat(c, dp, n, archdir, narchdir, devgen);
}

static Chan*
archopen(Chan* c, int omode)
{
	return devopen(c, omode, archdir, narchdir, devgen);
}

static void
archclose(Chan*)
{
}

static long
archread(Chan* c, void* a, long n, vlong offset)
{
	Rdwrfn *fn;

	switch((uint)c->qid.path){
	case Qdir:
		return devdirread(c, a, n, archdir, narchdir, devgen);

	default:
		if(c->qid.path < narchdir && (fn = readfn[c->qid.path]))
			return fn(c, a, n, offset);
		error(Eperm);
		break;
	}

	return 0;
}

static long
archwrite(Chan* c, void* a, long n, vlong offset)
{
	Rdwrfn *fn;

	if(c->qid.path < narchdir && (fn = writefn[c->qid.path]))
		return fn(c, a, n, offset);
	error(Eperm);

	return 0;
}

static long
cputyperead(Chan*, void* a, long n, vlong offset)
{
	char str[32];

	snprint(str, sizeof(str), "cpu%d: %#ux %ludMHz\n",
		m->machno, m->cputype, m->cpumhz);

	return readstr(offset, a, n, str);
}

static long
blockidread(Chan*, void* a, long n, vlong offset)
{
	char buf[32];

	seprint(buf, &buf[sizeof(buf)], "%#8.8ux\n", sys->block);

	return readstr(offset, a, n, buf);
}

static long
xyzipread(Chan*, void* a, long n, vlong offset)
{
	u8int z;
	char buf[128];

	/*
	 * Return the information needed to create IP addresses
	 * for setting this node up and routing to the pset I/O node.
	 * Class A network addresses are used within the machine,
	 * different ones for the different interconnects, but the least
	 * significant 3 octets are the same for each network.
	 * The returned string is 4 fields:
	 *	x.y.z	least significant 3 octets of this nodes's address
	 *	mask	for this node's IP address (255.0.0.0 for Class A)
	 *	network	x.y.z & mask
	 *	x.y.z	for the I/O node of this pset
	 * The first of these will be "255.255.255" on an I/O node,
	 * the last of these is synthesised from the P2P address of the
	 * pset I/O node and, on an I/O node, will be the least signifant
	 * 3 octets of the node's address (since the first field
	 * will be "255.255.255".
	 * I/O node P2P addresses are indexed by pset number
	 * starting after the namespace of compute nodes.
	 */
	if((z = sys->z) != 255)
		z += 1;
	seprint(buf, &buf[sizeof(buf)], "%d.%d.%d 255.0.0.0 0.0.0 %d.%d.%d\n",
		sys->x, sys->y, z, sys->iox, sys->ioy, sys->ioz);

	return readstr(offset, a, n, buf);
}

static long
ioipread(Chan*, void* a, long n, vlong offset)
{
	char buf[128];

	/*
	 * This will print in v6 format - perhaps v4 is wanted here?
	 */
	seprint(buf, &buf[sizeof(buf)], "%I %M %I %I\n",
		 sys->ipaddr, sys->ipmask, sys->ipbcast, sys->ipgw);

	return readstr(offset, a, n, buf);
}

static long
personalityread(Chan*, void* a, long n, vlong offset)
{
	char *alloc;

	alloc = malloc(2*READSTR);
	archpersonalityfmt(alloc, alloc+2*READSTR);
	n = readstr(offset, a, n, alloc);
	free(alloc);

	return n;
}

static long
psetread(Chan*, void* a, long n, vlong offset)
{
	char buf[128];

	/* pset# psetsize# rankinpset# rank# ionoderank# */
	seprint(buf, &buf[sizeof(buf)], "%d %d %d %d %d\n",
			sys->pset, sys->psetsz, sys->prank,
			sys->rank, sys->iorank);

	return readstr(offset, a, n, buf);
}

static long
cnkread(Chan*, void* a, long n, vlong offset)
{
	char str[32];

	snprint(str, sizeof(str), "cnkexec %d cnk %d", up->cnkexec, up->cnk);

	return readstr(offset, a, n, str);
}

static long
cnkwrite(Chan*, void* a, long, vlong offset)
{
	char *cp;
	unsigned int val;

	/*
	 * FIX: these error messages don't make much sense?
	 */
	if (offset)
		error("cnkwrite: offset 0, value 0 or bit 0 set or bit 1 set, use high 6 bits for debug");

	cp = a;
	val = strtoul(cp, 0, 16);
	if (val == 0)
		up->cnkexec = up->cnk = 0;
	else if (val & 1)
		up->cnkexec = val;
	else if (val & 2)
		up->cnk = val;
	else error("cnkwrite: value 0 or bit 0 set or bit 1 set, use high 6 bits for debug");

	return 1;
}

extern void intrstats(char *buf, int size);

static long
bicread(Chan*, void* a, long n, vlong offset)
{
	char buf[1024];

	intrstats(buf, 1024);
	
	return readstr(offset, a, n, buf);
}

static long
raswrite(Chan*, void* v, long n, vlong)
{
	Cmdbuf *cb;
	int a[6], count;

	cb = parsecmd(v, n);
	if(waserror()){
		free(cb);
		nexterror();
	}
	if(cb->nf != 6)
		error(Ebadarg);

	for(count = 0; count < cb->nf; count++) 
		a[count] = atoi(cb->f[count]);

	sendRAS(a[0], a[1], a[2], a[3], a[4], a[5]);

	poperror();
	free(cb);

	return n;
}

/* Maybe eventually this moves to console? */

static long
verread(Chan *, void *a, long n, vlong offset)
{
	char buf[256];
	extern ulong kerndate;
	extern char *kernbuilduser;
	extern char *kernbuildhost;
	extern char *kernbuildver;
	
	snprint(buf, sizeof(buf), "Kernel Build Date: %lud\n"
	  "Kernel Build User: %s\nKernel Build Host: %s\n"
	  "Kernel Build Version: %s\n", kerndate, kernbuilduser,
	  kernbuildhost, kernbuildver);
	
	return readstr(offset, a, n, buf);
}

static void
archinit(void)
{
	addarchfile("cputype", 0444, cputyperead, nil);
	addarchfile("block", 0444, blockidread, nil);
	addarchfile("xyzip", 0444, xyzipread, nil);
	addarchfile("ioip", 0444, ioipread, nil);
	addarchfile("personality", 0444, personalityread, nil);
	addarchfile("pset", 0444, psetread, nil);
	addarchfile("cnk", 0644, cnkread, cnkwrite);
	addarchfile("version",0444, verread, nil);
	addarchfile("bic", 0444, bicread, nil);
	addarchfile("ras",0222, nil, raswrite);
}

Dev archdevtab = {
	'P',
	"arch",

	devreset,
	archinit,
	devshutdown,
	archattach,
	archwalk,
	archstat,
	archopen,
	devcreate,
	archclose,
	archread,
	devbread,
	archwrite,
	devbwrite,
	devremove,
	devwstat,
};
