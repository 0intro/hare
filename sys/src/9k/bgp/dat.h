typedef struct Conf	Conf;
typedef struct Confmem	Confmem;
typedef struct KMap 	KMap;
typedef struct Label	Label;
typedef struct Lock	Lock;
typedef struct MKMAP	MKMAP;
typedef struct Mach	Mach;
typedef u32int Mreg;				/* Msr - bloody UART */
typedef struct Page	Page;
typedef struct PFPU	PFPU;
typedef struct PMMU	PMMU;
typedef struct PNOTIFY	PNOTIFY;
typedef struct Proc	Proc;
typedef struct Softtlb	Softtlb;
typedef struct Sys	Sys;
typedef struct Tlb	Tlb;
typedef struct Ureg	Ureg;

#pragma incomplete Ureg

#define MAXSYSARG	5	/* for mount(fd, mpt, flag, arg, srv) */

/*
 *  parameters for sysproc.c
 */
#define AOUT_MAGIC	Q_MAGIC

/*
 *  machine dependent definitions used by ../port/portdat.h
 */
struct Lock
{
	u32int	key;
	Mreg	sr;
	uintptr	pc;
	Proc*	p;
	Mach*	m;
	int	isilock;
};

struct Label
{
	Mreg	sp;
	Mreg	pc;
};

/*
 * emulated floating point
 */
struct PFPU
{
	union{
		uchar	fpusave[33*sizeof(double)];
		struct{
			double	fpreg[32];
			u32int	pad;
			u32int	fpscr;
		};
	};
	int	fpustate;
};

/*
 * PFPU.status
 */
enum
{
	FPinit,
	FPactive,
	FPinactive,
};

struct Confmem
{
	ulong	base;
	ulong	npage;
	ulong	kbase;
	ulong	klimit;
};

struct Conf
{
	ulong	nmach;		/* processors */
	ulong	nproc;		/* processes */
	Confmem	mem[4];
	ulong	npage;		/* total physical pages of memory */
	ulong	upages;		/* user page pool */
	ulong	nimage;		/* number of page cache image headers */
	ulong	nswap;		/* number of swap pages */
	int	nswppo;		/* max # of pageouts per segment pass */
	ulong	copymode;	/* 0 is copy on write, 1 is copy on reference */
	ulong	ialloc;		/* bytes available for interrupt time allocation */
	ulong	pipeqsize;	/* size in bytes of pipe queues */
};

/*
 *  mmu goo in the Proc structure
 */
#define NCOLOR 1

struct PMMU
{
	int	mmupid[MAXMACH];		/* pidonmach */

	int	cnk;				/* CNK emulation */
	int	cnkexec;
	void*	cnkenv;
	/* Segments don't exist here */
	void *heapseg;
};

/*
 *  things saved in the Proc structure during a notify
 */
struct PNOTIFY
{
	void	emptiness;
};

#include "../port/portdat.h"

/*
 *  machine dependent definitions not used by ../port/portdat.h
 */
struct KMap {
	Ref;
	uintptr	va;
	uintptr	pa;
	usize	sz;				/* not yet */
	KMap*	next;
	KMap*	kmonmach[MAXMACH];
	Page*	pg;
	uintptr	pc;
};

struct MKMAP {
	int	kmtlblo;
	int	kmtlbhi;

	KMap*	kmactive;			/* active on this machine */
	uchar	kmtlbx[NTLB];			/* tlb index used for kmap */
	uchar	kmtlbnext;
};

#define VA(k)		((k)->va)

struct Mach
{
	/* OFFSETS OF THE FOLLOWING KNOWN BY l.s */
	int	machno;			/* physical id of processor [0*4] */
	uintptr	splpc;			/* pc that called splhi() [1*4] */
	Proc	*proc;			/* current process on this processor [2*4] */
	Softtlb*stlb;			/* software tlb cache [3*4] */
	int	utlbhi;			/* lowest tlb index in use by kernel [4*4] */
	int	utlbnext;		/* next tlb entry to use for user (round robin) [5*4] */
	int	tlbfault;		/* number of tlb i/d misses [6*4] */

	/* ordering from here on irrelevant */

	ulong	ticks;			/* of the clock since boot time */
	Label	sched;			/* scheduler wakeup */
	Lock	alarmlock;		/* access to alarm list */
	void	*alarm;			/* alarms bound to this clock */
	int	inclockintr;

	Proc*	readied;		/* for runproc */
	ulong	schedticks;		/* next forced context switch */

	vlong	cpuhz;			/* general system clock (cycles) */
	long	cpumhz;
	vlong	clockgen;		/* clock generator frequency (cycles) */
	uint	delayloop;
	uvlong	cyclefreq;		/* frequency of user readable cycle clock */

	uint	cputype;
	int	chatty;			/* can print on console */

	uvlong	fastclock;
	Perf	perf;			/* performance counters */

	int	tlbpurge;		/* ... */
	int	pfault;
	int	cs;
	int	syscall;
	int	load;
	int	intr;
	int	mmuflush;		/* make current proc flush its mmu state */
	int	ilockdepth;

	int	lastpid;		/* last TLB pid allocated on this machine */
	QLock	stlblock;		/* prevent context switch during tlb update */
	Proc*	pidproc[NTLBPID];	/* which proc owns a given pid */

	MKMAP;

	/* MUST BE LAST */
	uchar	stack[];
};

struct Tlb {
	u32int	hi;
	u32int	md;
	u32int	lo;
};

struct Softtlb {
	u32int	hi;	/* tlb hi, except that low order 10 bits have (pid[8]<<2) */
	u32int	mid;
	u32int	lo;
};

Lock phystlblock;
int phystlbhi;
Tlb phystlb[9];

struct Sys {					/*  */
	u32int	vectorbase[16*KiB/sizeof(u32int)];
	union {
		Mach	mach;
		uchar	machpage[MACHSIZE];
	} machbase[MAXMACH];

	int	ncores;
	uint	coremask;

	u32int	mhz;				/* FreqMHz */
	u16int	mb;				/* DDRSizeMB */
	u32int	tom;				/* top of memory in Mbytes */
	u32int	block;				/* BlockID */
	u32int	config;				/* NodeConfig */

	int	ionode;				/* Is I/O Node */

	u8int	x;				/* [XYZ]Coord */
	u8int	y;
	u8int	z;
	u8int	nx;				/* [XYZ]Nodes */
	u8int	ny;
	u8int	nz;

	int	yspan;
	int	zspan;
	int	nxyz;				/* Number of Compute Nodes */

	u32int	iop2paddr;			/* PSet I/O Node P2P address */
	u8int	iox;				/* PSet I/O Node Coordinate */
	u8int	ioy;
	u8int	ioz;
	u32int	myp2paddr;			/* Compute node P2P address */
	u16int	routes[16];			/* TreeRoutes[] */

	u8int	ea[6];				/* EmacID (Eaddrlen) */
	u8int	ipaddr[16];			/* IPAddress (IPaddrlen) */
	u8int	ipmask[16];			/* IPNetmask */
	u8int	ipbcast[16];			/* IPBroadcast */
	u8int	ipgw[16];			/* IPGateway */
	
	u32int	rank;		
	u32int	pset;		
	u32int	prank;
	u32int	psetsz;
	u32int	iorank;

	uchar	memstart[];
};
extern usize sizeofSys;

extern Sys* sys;

struct {
	Lock;
	uint	machs;			/* bitmap of active CPUs */
	int	exiting;		/* shutdown */
	int	ispanic;		/* shutdown in response to a panic */
	int	thunderbirdsarego;	/* F.A.B. */
} active;

extern Mach* machptr[MAXMACH];

#define MACHP(n)	(machptr[n])

extern register Mach*	m;			/* R30 */
extern register Proc*	up;			/* R29 */

/*
 * Horrid.
 */
#ifdef _DBGC_
#define DBGFLG		(dbgflg[_DBGC_])
#else
#define DBGFLG		(0)
#endif /* _DBGC_ */

#define DBG(...)	if(!DBGFLG){}else dbgprint(__VA_ARGS__)

int vflag;
extern char dbgflg[256];

#define dbgprint	print		/* for now */
