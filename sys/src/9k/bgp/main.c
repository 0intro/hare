/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Based on ppc440/main.c Copyright (C) 2006-2009 Alcatel-Lucent
 *
 * Description: main kernel initialization
 *
 * All rights reserved
 *
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include <tos.h>

#include "init.h"

Mach* machptr[MAXMACH];

Conf conf;

Sys* sys = nil;
usize sizeofSys = sizeof(Sys);

#define	MAXCONF		32

static char confname[MAXCONF][KNAMELEN];
static char *confval[MAXCONF];
static int nconf;

#include "bgcns.h"
BGCNS_ServiceDirectory* cns;

void
squidboy(int cpuno, void*)
{
	uint mask;

	mask = 1<<cpuno;
	while(!(sys->coremask & mask))
		;

	/*
	 * To do: this needs all or some of:
	 *	machinit
	 *	mmuinit
	 *	trapinit
	 *	intrinit
	 *	timersinit
	 *	clockinit
	 *	fpreginit
	 *	syncclock
	 */
	m->machno = cpuno;
	MACHP(cpuno) = m;
	m->cputype = pvrget()>>16;   /* OWN[0:11] and PCF[12:15] fields */
	m->delayloop = 20000;

	m->cpumhz = sys->mhz;
	m->cpuhz = m->cpumhz*1000000;
	m->clockgen = m->cpuhz;

	m->chatty = MACHP(0)->chatty;

	mmuinit();
	clockinit();
	timersinit();
	fpreginit();
	kmapinit();

	lock(&active);
	active.machs |= mask;
	unlock(&active);

	DBG("cpu%d: %ux %ludMHz\n", m->machno, m->cputype, m->cpumhz);

	/*
	 * Restrain your octopus! Don't let it go out on the sea!
	 */
	while(!active.thunderbirdsarego)
		microdelay(100);

	schedinit();
}

static uint
cnstakecpu(void)
{
	int cpu;
	uint mask;

	/* release other CPUs when on CNS */
	mask = sys->coremask;
	for(cpu = 1; cpu < sys->ncores; cpu++){
		if(mask & (1<<cpu))
			continue;
		if(CNS(takeCPU, int, cpu, 0, PADDR(KZERO)) != 0){
			print("ERROR: failed to take CPU %d from CNS\n", cpu);
			continue;
		}
		mask |= (1<<cpu);
	}
	conf.nmach = sys->ncores;

	return mask;
}

static void
fab(int n)
{
	int timeo;

	n = (1<<n)-1;
	for(timeo = 0; timeo < 10000; timeo++){
		if((active.machs & n) == n)
			break;
		delay(1);
	}

	if((active.machs & n) == n) {
		DBG("Professor Matic, Robert and Venus are all aboard\n");
	} else {
		DBG("active.machs %#8.8ux\n", active.machs);
	}
}

/*
 * This is the table of TLB entries loaded on boot by CPU#0.
 * The last one doesn't have TLBVALID set, it gets filled in
 * and loaded in main once the location of CNS is known.
 * This table is also used to load the initial TLB of CPU#[123]
 * (so they get the CNS entry without having to expliciity
 * load it).
 *
 * To do:
 *	remove restriction of 1GB physical memory used;
 *	other entries for devices common to both I/O and CPU node
 *	  kernels should probably be in here.
 */
Tlb tlbstatic[] = {
	{	TLBEPN(VIRTDRAM)|TLB16MB|TLBVALID,	/* RAM */
		TLBRPN(PHYSDRAM)|TLBERPN(PHYSDRAM),
		TLBWL1|TLBU2|TLBM|TLBXWR,
	},
	{	TLBEPN(VIRTDRAM+16*MiB)|TLB16MB|TLBVALID,
		TLBRPN(PHYSDRAM+16*MiB)|TLBERPN(PHYSDRAM+16*MiB),
		TLBWL1|TLBU2|TLBM|TLBXWR,
	},
	{	TLBEPN(VIRTDRAM+32*MiB)|TLB16MB|TLBVALID,
		TLBRPN(PHYSDRAM+32*MiB)|TLBERPN(PHYSDRAM+32*MiB),
		TLBWL1|TLBU2|TLBM|TLBXWR,
	},
	{	TLBEPN(VIRTDRAM+48*MiB)|TLB16MB|TLBVALID,
		TLBRPN(PHYSDRAM+48*MiB)|TLBERPN(PHYSDRAM+48*MiB),
		TLBWL1|TLBU2|TLBM|TLBXWR,
	},
	{	TLBEPN(VIRTTREE0)|TLB1K|TLBVALID,	/* TREE0 */
		TLBRPN(PHYSTREE0)|TLBERPN(PHYSTREE0),
		TLBI|TLBG|TLBWR,
	},
	{	TLBEPN(VIRTTREE1)|TLB1K|TLBVALID,	/* TREE1 */
		TLBRPN(PHYSTREE1)|TLBERPN(PHYSTREE1),
		TLBI|TLBG|TLBWR,
	},
	{	TLBEPN(VIRTBIC)|TLB4K|TLBVALID,		/* BIC */
		TLBRPN(PHYSBIC)|TLBERPN(PHYSBIC),
		TLBI|TLBG|TLBWR,
	},
	{	TLBEPN(PHYSSRAM)|TLB16K|TLBVALID,	/* SRAM */
		TLBRPN(PHYSSRAM)|TLBERPN(PHYSSRAM),
		TLBWL1|TLBU2|TLBXWR,
	},
	{	TLBEPN(PHYSSRAM+16*KiB)|TLB16K|TLBVALID,
		TLBRPN(PHYSSRAM+16*KiB)|TLBERPN(PHYSSRAM+16*KiB),
		TLBWL1|TLBU2|TLBXWR,
	},
	{	TLBEPN(PHYSKLOCKBOX)|TLB16K|TLBVALID,	/* KLOCKBOX */
		TLBRPN(PHYSKLOCKBOX)|TLBERPN(PHYSKLOCKBOX),
		TLBCI|TLBI|TLBG|TLBWR,
	},
	{	TLB256K,				/* CNS */
		0,
		TLBWL1|TLBU2|TLBU3|TLBM|TLBXWR,
	},
	{
		0, 0, 0,				/* must be last */
	},
};

void
main(u32int r3, u32int r4, u32int r5, u32int r6, u32int r7)
{
	uint mask;
	Tlb* entry;
	BGCNS_Descriptor* cnsd;

	cnsd = (BGCNS_Descriptor*)r3;
	USED(r4, r5, r6, r7);

	/*
	 * Fill in the CNS TLB entry.
	 * Need to check size, etc. and take
	 * the space out of the memory map.
	 */
	entry = &tlbstatic[nelem(tlbstatic)-2];
	entry->hi |= TLBEPN(cnsd->baseVirtualAddress)|TLBVALID;
	entry->md = TLBRPN(cnsd->basePhysicalAddress);
	entry->lo |= cnsd->basePhysicalAddressERPN;
	tlbwrx(NTLB-1 - (nelem(tlbstatic)-2), entry->hi, entry->md, entry->lo);

	/*
	 * Incompatible CNS, what to do?  How could this be reported?
	 */
	while(cnsd->version != 0 && !BGCNS_IS_COMPATIBLE(cnsd))
		;

	cns = cnsd->services;

	mach0init();
	archreset();

	fmtinstall('m', mregfmt);
	quotefmtinstall();
	meminit(cnsd->basePhysicalAddress);
	confinit();
	mmuinit();
	mallocinit();
	trapinit();

	/*
	 * Printinit will cause the first malloc
	 * call to happen (printinit->qopen->malloc).
	 * If the system dies here it's probably due
	 * to malloc not being initialised
	 * correctly, or the data segment is misaligned
	 * (it's amazing how far you can get with
	 * things like that completely broken).
	 */
	printinit();

	/*
	 * Currently, archreset only lets I/O nodes and compute
	 * node 1.0.0 ({1}.0) print on the console.
	 * Cuts down on the noise until things are stable.
	 */
	cnsconsole();

	DBG("\nPlan 9 BG/P\n");
	DBG("cpu%d: %ux %ludMHz\n", m->machno, m->cputype, m->cpumhz);
	archprint();
	/* 
	 * EVH: moved this to manual invocation via devarch
	 * since it triggers service node to try and contact ciod
	 *
	sendRAS(0x01, 0x1e, 0x00, 0, 0, 0);
	 */

	mask = cnstakecpu();

	intrinit();

	timersinit();
	clockinit();

	fpuinit();
	psinit(conf.nproc);
	initimage();
	links();
	devtabreset();
	pageinit();
	swapinit();
	kmapinit();
	userinit();

	sys->coremask = mask;
	fab(conf.nmach);
	active.thunderbirdsarego = 1;

	schedinit();
	/* no return */
}

void
mach0init(void)
{
	MACHP(0) = m;
	m->cputype = pvrget()>>16;   /* OWN[0:11] and PCF[12:15] fields */
	m->delayloop = 20000;	/* initial estimate only; set by clockinit */
	/* other values set by archreset */

	conf.nmach = 1;
	active.machs = 1;
	active.exiting = 0;
}

void
confinit(void)
{
	uint kpages;

	/*
	 * This crap has gotten REALLY old.
	 * Meminit has set conf.mem[0] to be the kernel pages
	 * and conf.mem[1] to be the user pages.
	 */
	kpages = conf.mem[0].npage;
	conf.npage = kpages + conf.mem[1].npage;
	conf.upages = conf.mem[1].npage;

	conf.nproc = 1000;
	conf.nimage = 200;
	conf.nswap = 0;
	conf.nswppo = 0;
	conf.copymode = 1;			/* copy on reference */

	conf.ialloc = (kpages/2)*BY2PG;

//	mainmem->maxsize = kpages * BY2PG;
}

void
init0(void)
{
	int i;
	char buf[2*KNAMELEN];

	up->nerrlab = 0;

	spllo();

	/*
	 * These are o.k. because rootinit is null.
	 * Then early kproc's will have a root and dot.
	 */
	up->slash = namec("#/", Atodir, 0, 0);
	pathclose(up->slash->path);
	up->slash->path = newpath("/");
	up->dot = cclone(up->slash);

	devtabinit();
	if(!waserror()){
		snprint(buf, sizeof(buf), "power %s", conffile);
		ksetenv("terminal", buf, 0);
		ksetenv("cputype", "power", 0);
		if(cpuserver)
			ksetenv("service", "cpu", 0);
		else
			ksetenv("service", "terminal", 0);
		poperror();
	}
	for(i = 0; i < nconf; i++){
		if(confval[i] == nil)
			continue;
		if(confname[i][0] != '*'){
			if(!waserror()){
				ksetenv(confname[i], confval[i], 0);
				poperror();
			}
		}
		if(!waserror()){
			ksetenv(confname[i], confval[i], 1);
			poperror();
		}
	}

	kproc("alarm", alarmkproc, 0);

	/*
	 * The initial value of the user stack must be such
	 * that the total used is larger than the maximum size
	 * of the argument list checked in syscall.
	 */
	touser((void*)(USTKTOP-sizeof(Tos)));
}

void
userinit(void)
{
	Proc *p;
	Segment *s;
	KMap *k;
	Page *pg;

	p = newproc();
	p->pgrp = newpgrp();
	p->egrp = smalloc(sizeof(Egrp));
	p->egrp->ref = 1;
	p->fgrp = dupfgrp(nil);
	p->rgrp = newrgrp();
	p->procmode = 0640;

	kstrdup(&eve, "");
	kstrdup(&p->text, "*init*");
	kstrdup(&p->user, eve);

	/*
	 * Kernel Stack
	 *
	 * N.B. make sure there's enough space for syscall to check
	 *	for valid args and
	 *	space for gotolabel's return PC
	 */
	p->sched.pc = PTR2UINT(init0);
	p->sched.sp = PTR2UINT(p->kstack+KSTACK-sizeof(up->arg)-sizeof(uintptr));
	p->sched.sp = STACKALIGN(p->sched.sp);

	/*
	 * User Stack
	 *
	 * Technically, newpage can't be called here because it
	 * should only be called when in a user context as it may
	 * try to sleep if there are no pages available, but that
	 * shouldn't be the case here.
	 */
	s = newseg(SG_STACK, USTKTOP-USTKSIZE, USTKSIZE/BY2PG);
	p->seg[SSEG] = s;
	pg = newpage(1, 0, USTKTOP-BY2PG);
	segpage(s, pg);

	/*
	 * Text
	 */
	s = newseg(SG_TEXT, UTZERO, 1);
	s->flushme++;
	p->seg[TSEG] = s;
	pg = newpage(1, 0, UTZERO);
	memset(pg->cachectl, PG_TXTFLUSH, sizeof(pg->cachectl));
	segpage(s, pg);
	k = kmap(s->map[0]->pages[0]);
	memmove((ulong*)VA(k), initcode, sizeof initcode);
	kunmap(k);

	ready(p);
}

static void
shutdown(int ispanic)
{
	int ms, once;

	lock(&active);
	if(ispanic)
		active.ispanic = ispanic;
	else if(m->machno == 0 && (active.machs & (1<<m->machno)) == 0)
		active.ispanic = 0;
	once = active.machs & (1<<m->machno);
	active.machs &= ~(1<<m->machno);
	active.exiting = 1;
	unlock(&active);

	if(once)
		print("cpu%d: exiting\n", m->machno);
	spllo();
	for(ms = 5*1000; ms > 0; ms -= TK2MS(2)){
		delay(TK2MS(2));
		if(active.machs == 0 && consactive() == 0)
			break;
	}

#ifdef notdef
	if(active.ispanic && m->machno == 0){
		if(cpuserver)
			delay(30000);
		else
			for(;;)
				halt();
	}
	else
#endif /* notdef */
		delay(1000);
}

void
reboot(void*, void*, long)
{
	panic("reboot\n");
}

static void
archreboot(void)
{
//	putevpr(0xFFF00000);
//	firmware(0);
	for(;;)
		;
}

void
exit(int ispanic)
{
	shutdown(ispanic);
	archreboot();
}

static int
findconf(char *name)
{
	int i;

	for(i = 0; i < nconf; i++)
		if(cistrcmp(confname[i], name) == 0)
			return i;
	return -1;
}

void
addconf(char *name, char *val)
{
	int i;

	i = findconf(name);
	if(i < 0){
		if(val == nil || nconf >= MAXCONF)
			return;
		i = nconf++;
		strecpy(confname[i], confname[i]+sizeof(confname[i]), name);
	}
	confval[i] = val;
}

char*
getconf(char *name)
{
	int i;

	i = findconf(name);
	if(i >= 0)
		return confval[i];
	return nil;
}
