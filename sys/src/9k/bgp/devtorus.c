/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: torus device interface
 *
 * All rights reserved
 *
 */

/*
 * TO DO
 *	+ ilocks on tx/rx state (eg, for SMP)
 *	- `reception descriptor' is misleading: it's a block of chunks filled as a fifo
 *          will need to do something for variable-length packets
 *	- fifo map (eg, set_inj_fifo_map)
 *	- set or inspect hint bits to determine routing
 *	- self-addressed messages
 *	+ check each software ring against interrupt state instead of scanning bit sets
 *	+ eliminate in32/out32 for memory-mapped access
 *	- inspect and eliminate values from torus.h
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

#include "frag.h"

/* Torus definitions from Linux which might need to be cleaned up */
#include "torus.h"

#define NEXT(x, l)	(((x)+1)%(l))
#define PREV(x, l)	(((x) == 0) ? (l)-1: (x)-1)

enum {						/* DCRs (page 23) */
	Dreset		= 0xd00,	/* DMA Reset */
	Dbasectl	= 0xd01,	/* Base Controls */
	Didmamin	= 0xd02,	/* Injection Min Valid Addr */
	Didmamax	= 0xd03,	/* Injection Max Valid Addr */
	Didmatlb	= 0xd12,	/* Injection Range TLB Attributes */
	Drdmatlb	= 0xd13,	/* Reception Range TLB Attributes */
	Drdmamin	= 0xd14,	/* Reception Min Valid Addr */
	Drdmamax	= 0xd15,	/* Reception Max Valid Addr */
	Dififowm0	= 0xd24,	/* Torus fifo injection threshold */
	Dififowm1	= 0xd25,	/* Torus fifo injection threshold */
	Dilocalwm0	= 0xd26,	/* Local inj fifo threshold */
	Drfifowm0	= 0xd27,	/* Torus fifo reception threshold */
	Drlocalwm0	= 0xd2b,	/* Local rec fifo threshold */
	Dififoctl	= 0xd2c,	/* Injection fifo enable */
	Drfifoctl	= 0xd30,	/* Reception fifo enable */
	Dififopri	= 0xd32,	/* Injection fifo priority */
	Diquanta	= 0xd37,	/* Injection Service Quanta */
	Drthresh	= 0xd3a,	/* Threshold for recv fifo type 0 (type 1 next) */
	Dififomap	= 0xd3c,	/* imFifo to torus inj fifo map */
	Dififolocal	= 0xd5c,	/* imFifo is used for local copies */
	Drfifomap0	= 0xd60,	/* reception fifo to rmfifo map */
	Drfifomap1	= 0xd61,	/* reception fifo to rmfifo map */
	Drclrmask	= 0xd68,	/* Recption DMA Fifo Clear enable */
	Drhdrclrmask= 0xd6c,	/* Reception DMA header fifo clear */
	Drintron	= 0xd71,	/* Reception fifo intr enable */
	Dichzintron	= 0xd76,	/* Injection Ctr Hits Zero Intr Enable */
	Dfatalon	= 0xd7e,	/* Fatal Error Enable 0 */
	Derrthresh	= 0xd89,	/* Thresholds for SRAM ECC errors */
	Dififoce	= 0xd8a,	/* Injection fifo ce (start/end) */
	Dclear0		= 0xdb1,	/* Clear_0 */
	Dclear1		= 0xdb2,	/* Clear_1 */
};

enum {						/* Dbasectl */
	Rusedma		= IB(0),	/* =1, use DMA; =0, core does the work */
	Rstorehdr	= IB(1),	/* =1, store DMA headers in FIFO (debug) */
	Rnoprefetch	= IB(2),	/* =1, disable torus prefetch (0 is default) */
	RL3burst	= IB(3),	/* enable burst reads to L3 */
	Rinjenable	= IB(4),	/* enable injection unit */
	Rrcvenable	= IB(5),	/* enable reception unit */
	Rimfuenable	= IB(6),	/* enable imfu_arb fifo/counter */
	Rrmfuenable	= IB(7),	/* enable rmfu_arb fifo/counter */
	RL3noprefetch= IB(8),	/* disable L3 read prefetch for DMA */
	Rrdmastop	= IB(28),	/* =1, rdma stop when fifo full; =0, rdma retry when full */
	Rrcvsticky	= IB(29),	/* =1, fifo cross threshold not sticky; =0, is sticky (default) */
	Rinjsticky	= IB(30),	/* =1, fifo cross threshold not sticky, ... */
};

enum {
	X		= 0,			/* dimension */
	Y		= 1,
	Z		= 2,
	N		= 3,

	Xp		= 0,			/* direction */
	Xm		= 1,
	Yp		= 2,
	Ym		= 3,
	Zp		= 4,
	Zm		= 5,
	Nd		= 6,

	Hp		= 7,
};

enum {
	Torushdrlen=	8,	/* hardware header only */
	Tpkthdrlen=		16,	/* both hardware and software headers */

	PayloadSize	= 256-Tpkthdrlen,
	Chunk		= 32,			/* Chunk size (bytes) for size field */
	Quad		= 16,			/* 128 bits */

	Ngroup		= 4,			/* groups */
	Ninjfifo	= 32,			/* injection fifos (per group) */
	Nrcvfifo	= 8,			/* receive fifos (per group) */
	Ncounter	= 64,			/* counters per group, for each of injection and reception */
	Npid		= 4,			/* [Pid0, Pid1] */
	Nregion		= 8,			/* DMA regions (mainly for put/get modes) */

	/* software choices */
	Ntx			= 256,		/* injection fifo entries */
	Nrx			= 512,		/* reception fifo entries */

	KernelGroup		= Ngroup-1,		/* arbitrary, until ron gets started */
	KernelPid		= Npid-1,
	KernelTxFifo	= Ninjfifo-1,
	KernelRxFifo	= Nrcvfifo-1,
	KernelRxRegion	= Nregion-1,	/* not really used, see init_rx_queue below */
};

/*
 * Normal (memory FIFO mode) packet header.
 * The hardware requires an 8-byte header
 * of which the last two bytes are reserved (they contain a sequence
 * number and a header checksum inserted by the hardware).
 * The next 8 bytes are a software header in memory FIFO
 * mode, which we use for the fragmentation header,
 * but other formats are possible in the direct-put/remote-get modes.
 */
typedef struct Tpkt Tpkt;
struct Tpkt {
	/* hardware header */
	u8int	sk;					/* Skip Checksum Control */
	u8int	hint;				/* Hint|Dp|Pid0 */
	u8int	size;				/* Size|Pid1|Dm|Dy|VC */
	u8int	dst[N];				/* Destination Coordinates */
	u8int	_6_[2];				/* reserved */

	/* hardware and software headers */
	union{
		struct{	/* direct-put DMA mode */
			u8int	paoff[4];		/* put address offset (updated by hw for large packets) */
			u8int	rdmaid;		/* RDMA counter ID */
			u8int	valid;		/* valid bytes in payload (updated by hw) */
			u8int	rgflags;		/* remote get flags */
			u8int	idmaid;		/* IDMA FIFO ID */
		};
		struct{	/* memory FIFO mode */
			u8int	putoff[4];		/* put offset (updated by hw) */
			u8int	other[4];		/* software */
		};
		Hdr;		/* our software header (frag.h), variant of memory FIFO */
	};
	u8int	payload[];
};

/*
 * SKIP is a field in .sk giving the number of 2-bytes
 * to skip from the top of the packet before including
 * the packet bytes into the running checksum.
 * SIZE is a field in .size giving the size of the
 * packet in 32-byte 'chunks'.
 *
 * I realize using this format for the bitfields is
 * overkill, but bear with me for consistency.
 */
#define SKIP(n)		F(n, 1, 7)
#define SIZE(n)		F(n, 5, 3)
#define PID0(n)	((n)&2? Pid0: 0)
#define PID1(n)	((n)&1? Pid1: 0)

enum {
	Sk		= 0x01,			/* Skip Checksum */

	Pid0	= 0x01,			/* Destination Group FIFO MSb */
	Dp		= 0x02,			/* Multicast Deposit */
	Hzm		= 0x04,			/* Z- Hint */
	Hzp		= 0x08,			/* Z+ Hint */
	Hym		= 0x10,			/* Y- Hint */
	Hyp		= 0x20,			/* Y+ Hint */
	Hxm		= 0x40,			/* X- Hint */
	Hxp		= 0x80,			/* X+ Hint */

	Vcd0	= 0x00,			/* Dynamic 0 VC */
	Vcd1	= 0x01,			/* Dynamic 1 VC */
	Vcbn	= 0x02,			/* Deterministic Bubble VC */
	Vcbp	= 0x03,			/* Deterministic Priority VC */
	Dy		= 0x04,			/* Dynamic Routing */
	Dm		= 0x08,			/* DMA Mode */
	Pid1	= 0x10,			/* Destination Group FIFO LSb */
};

/*
 * Reception FIFO is a ring of full packets with hw/sw headers
 */
typedef union Rcvbuf Rcvbuf;
union Rcvbuf {
	Tpkt;
	u8int packet[256];
};

/*
 * Injection descriptors (p. 14)
 *
 * Injection FIFO is a ring of DMA control data
 * (with reference to payload) and packet headers,
 * payload elsewhere
 */
typedef struct Injdesc Injdesc;
struct Injdesc {
	u32int	flags;		/* low-order byte: B(0)-B(5) unused; */
						/* B(6) pre-fetch on local memmove; */
						/* B(7) 1=local, 0=torus */
	u32int	counter;	/* low-order byte: injection counter ID */
	u32int	base;		/* base physical address (>>4) of payload */
	u32int	length;		/* count in bytes */
	Tpkt	hdrs;		/* MAC-level header, then software header */
};

/*
 * Injection and Reception FIFO (ring)
 */
typedef struct Fifo Fifo;
struct Fifo {
	u32int	start;		/* 32-byte aligned */
	u32int	end;
	u32int	head;		/* 16-byte aligned */
	u32int	tail;		/* address to write new descriptor (host owned) */
};

/*
 * DMA Fifos are controlled through a memory-mapped region
 */
typedef struct Fifoctl Fifoctl;
struct Fifoctl {
	struct {
		Fifo	fifo[Ninjfifo];				// 0 - 1ff
		u32int	empty;						// 200
		u32int	__unused0;					// 204
		u32int	avail;						// 208
		u32int	__unused1;					// 20c
		u32int	threshold;					// 210
		u32int	__unused2;					// 214
		u32int	clear_threshold;			// 218
		u32int	__unused3;					// 21c
		u32int	dma_active;					// 220
		u32int	dma_activate;				// 224
		u32int	dma_deactivate;				// 228
		u8int	__unused4[0x100-0x2c];		// 22c - 2ff

		u32int	counter_enabled[2];			// 300
		u32int	counter_enable[2];			// 308
		u32int	counter_disable[2];			// 310
		u32int	__unused5[2];				// 318
		u32int	counter_hit_zero[2];		// 320
		u32int	counter_clear_hit_zero[2];	// 328
		u32int	counter_group_status;		// 330
		u8int	__unused6[0x400-0x334];		// 334 - 3ff

		struct {
			u32int	counter;
			u32int	increment;
			u32int	base;
			u32int	__unused;
		} counter[Ncounter];				// 400 - 7ff
	} inj;

	struct {
		Fifo	fifo[Nrcvfifo+1];			// 800 - 87f (rcv fifos), 
											// 880-88f (special header fifo)
		u8int	__unused0[0x900-0x890];		// 890 - 900

		u32int	glob_ints[16];				// 900 - 93f
		u8int	__unused1[0xa00-0x940];		// 940 - 9ff

		u32int	notempty[2];				// a00
		u32int	available[2];				// a08
		u32int	threshold[2];				// a10
		u32int	clear_threshold[2];			// a18
		u8int	__unused2[0xb00 - 0xa20];	// a20 - aff

		u32int	counter_enabled[2];			// b00
		u32int	counter_enable[2];			// b08
		u32int	counter_disable[2];			// b10
		u32int	__unused3[2];				// b18
		u32int	counter_hit_zero[2];		// b20
		u32int	counter_clear_hit_zero[2];	// b28
		u32int	counter_group_status;		// b30
		u8int	__unused4[0xc00 - 0xb34];	// b34 - bff
		
		struct {
			u32int	counter;
			u32int	increment;
			u32int	base;
			u32int	limit;
		} counter[Ncounter];				// c00 - fff
	} rcv;
};

/* Software structure to help track tx/rx rings */
typedef struct TxRing TxRing;
struct TxRing {
//	Lock	lock;

	uint	active;
	Injdesc*	desc;		/* descriptor ring (in virtual space) */
	Block	**blocks;		/* blocks currently on ring */
	Fifo*	fifo;			/* hardware memory fifo assigned to ring */
	u32int	start;			/* main memory copy of fifo->start */
	uint	tail_idx;
	uint	pending_idx;

	u32int*	incr;		/* address of increment register for counter */
	u32int	ctrid;		/* counter ID for descriptor (group<<6 | fifo-index) */

	int	ntd;			/* number of transmit descriptors */
	int	np;				/* number of packets in fifo */
	
	int full;			/* number of times the txring was full */
	
	QLock	rl;
	Rendez	r;		/* wait for free space */

	int	group;
	int	index;
	TxRing*	next;
};

typedef struct RxRing RxRing;
struct RxRing {
//	Lock	lock;

	uint	active;
	Rcvbuf*	desc;		/* packet buffer ring (in virtual space) */
	Fifo*	fifo;		/* hardware memory fifo assigned to ring */
	u32int	start;		/* main memory copy of fifo->start */
	uint	head_idx;

	/* statistics */
	u32int	dropped;
	u32int	delivered;

	int	nrd;			/* number of reception descriptors */

	int	group;
	int	index;
	RxRing*	next;
};

typedef struct Torus Torus;
struct Torus {
	u8int	addr[N];		/* of this node */
	u8int	size[N];		/* of the torus */
	u8int	torus[N];		/* in that dimension (!mesh) */
	u8int	cp[N];			/* cutoff in [XYZ]p */
	u8int	cm[N];			/* cutoff in [XYZ]m */

	u32int	zmzpymypxmxp;	/* reset FIFOs */
	u32int	xpxmypymzpzm;	/* ditto, in IBM field order */

	QLock	alock;
	int	ref;

	Queue*	rcvq;

	/*
	 * these counters will need to become per DMA group
	 * (eg, in TxRing or RxRing) when more than one fifo is used,
	 * to allow concurrent access
	 */
	uint	rcvintr;
	uint	rcvpackets;
	uint	rcvtoss;
	uint	rcvqfull;
	uint	rcvpause;
	uint	rcvresume;
	uint	injintr;
	uint	injgetfifo;
	uint	injgetstatus;
	uint	memfifofull;
	uint	unreachable;
	uint	self;
	uint	injpackets;
	uint	unaligned;

	Fifoctl*	dma;				/* pointer to memory-mapped hw control structure */
	TxRing	txr[Ngroup][Ninjfifo];	/* software data for injection */
	RxRing	rxr[Ngroup][Nrcvfifo];	/* software data for reception */

	Lock	txlock;					/* protect all groups for now */
	Lock	rxlock;					/* protect all groups for now */

	/* reception counters aren't needed for memory FIFO mode */

	Frags*	frags;
};

enum {
	DbgDMA=	1<<2,
	DbgSetup=	1<<1,
	DbgIO=		1<<0,
};

static int debug_torus;

static Torus torus;
int torus_dump_cons = 1;

static void torusdisable(int cause);
static void torusinject(Torus*, TxRing*, Block*);

/* sorry for the ugliness, I'll search and replace it eventually */
#define DUMP_DCR(desc, x) p = seprint(p, e, " " desc "(%ux): %ux\n", (unsigned int)x, dcrget(x))

typedef struct Dcr Dcr;
struct Dcr {
	u32int	dcr;
	char*	name;
};

static char*
dump_dcrs(char *p, char *e)
{
	int i;

	DUMP_DCR("torus reset", 0xc80);
	DUMP_DCR("number nodes", 0xc81);
	DUMP_DCR("node coordinates", 0xc82);
	DUMP_DCR("neighbour plus", 0xc83);
	DUMP_DCR("neighbour minus", 0xc84);
	DUMP_DCR("cutoff plus", 0xc85);
	DUMP_DCR("cutoff minus", 0xc86);
	DUMP_DCR("VC threshold", 0xc87);
	DUMP_DCR("torus loopback", 0xc92);
	DUMP_DCR("torus non-rec err", 0xcdc);
	DUMP_DCR("dma reset", 0xd00);
	DUMP_DCR("base ctrl", 0xd01);
	for(i = 0; i < 8; i++){
		DUMP_DCR("inj min valid", 0xd02 + i * 2);
		DUMP_DCR("inj max valid", 0xd03 + i * 2);
		DUMP_DCR("rec min valid", 0xd14 + i * 2);
		DUMP_DCR("rec max valid", 0xd15 + i * 2);
	}
	DUMP_DCR("inj fifo enable", 0xd2c);
	DUMP_DCR("inj fifo enable", 0xd2d);
	DUMP_DCR("inj fifo enable", 0xd2e);
	DUMP_DCR("inj fifo enable", 0xd2f);
	DUMP_DCR("rec fifo enable", 0xd30);
	DUMP_DCR("rec hdr fifo enable", 0xd31);
	DUMP_DCR("inj fifo prio", 0xd32);
	DUMP_DCR("inj fifo prio", 0xd33);
	DUMP_DCR("inj fifo prio", 0xd34);
	DUMP_DCR("inj fifo prio", 0xd35);
	DUMP_DCR("remote get inj fifo threshold", 0xd36);
	DUMP_DCR("remote get inj service quanta", 0xd37);
	DUMP_DCR("rec fifo type", 0xd38);
	DUMP_DCR("rec header fifo type", 0xd39);
	DUMP_DCR("threshold rec type 0", 0xd3a);
	DUMP_DCR("threshold rec type 1", 0xd3b);
	for(i = 0; i < 32; i++)
		DUMP_DCR("inj fifo map", 0xd3c + i);
	DUMP_DCR("rec fifo map 00", 0xd60);
	DUMP_DCR("rec fifo map 00", 0xd61);
	DUMP_DCR("rec fifo map 01", 0xd62);
	DUMP_DCR("rec fifo map 01", 0xd63);
	DUMP_DCR("rec fifo map 10", 0xd64);
	DUMP_DCR("rec fifo map 10", 0xd65);
	DUMP_DCR("rec fifo map 11", 0xd66);
	DUMP_DCR("rec fifo map 11", 0xd67);
	DUMP_DCR("inj fifo irq enable grp0", 0xd6d);
	DUMP_DCR("inj fifo irq enable grp1", 0xd6e);
	DUMP_DCR("inj fifo irq enable grp2", 0xd6f);
	DUMP_DCR("inj fifo irq enable grp3", 0xd70);
	DUMP_DCR("rec fifo irq enable type0", 0xd71);
	DUMP_DCR("rec fifo irq enable type1", 0xd72);
	DUMP_DCR("rec fifo irq enable type2", 0xd73);
	DUMP_DCR("rec fifo irq enable type3", 0xd74);
	DUMP_DCR("rec hdr irq enable", 0xd75);
	DUMP_DCR("inj cntr irq enable", 0xd76);
	DUMP_DCR("inj cntr irq enable", 0xd77);
	DUMP_DCR("inj cntr irq enable", 0xd78);
	DUMP_DCR("inj cntr irq enable", 0xd79);
	DUMP_DCR("rec cntr irq enable", 0xd7a);
	DUMP_DCR("rec cntr irq enable", 0xd7b);
	DUMP_DCR("rec cntr irq enable", 0xd7c);
	DUMP_DCR("rec cntr irq enable", 0xd7d);
	DUMP_DCR("ce count inj fifo0", 0xd8a);
	DUMP_DCR("ce count inj fifo1", 0xd8b);
	DUMP_DCR("ce count inj counter", 0xd8c);
	DUMP_DCR("ce count inj desc", 0xd8d);
	DUMP_DCR("ce count rec fifo0", 0xd8e);
	DUMP_DCR("ce count rec fifo1", 0xd8f);
	DUMP_DCR("ce count rec counter", 0xd90);
	DUMP_DCR("ce count local fifo0", 0xd91);
	DUMP_DCR("ce count local fifo1", 0xd92);
	DUMP_DCR("fatal error 0", 0xd93);
	DUMP_DCR("fatal error 1", 0xd94);
	DUMP_DCR("fatal error 2", 0xd95);
	DUMP_DCR("fatal error 3", 0xd96);
	DUMP_DCR("wr0 bad address", 0xd97);
	DUMP_DCR("rd0 bad address", 0xd98);
	DUMP_DCR("wr1 bad address", 0xd99);
	DUMP_DCR("rd1 bad address", 0xd9a);
	DUMP_DCR("imfu err 0", 0xd9b);
	DUMP_DCR("imfu err 1", 0xd9c);
	DUMP_DCR("rmfu err", 0xd9d);
	DUMP_DCR("rd out-of-range", 0xd9e);
	DUMP_DCR("wr out-of-range", 0xd9f);
	DUMP_DCR("dbg dma warn", 0xdaf);

	DUMP_DCR("DMA Inj Fifo: da7", 0xda7);
	DUMP_DCR("DMA Inj Fifo: da8", 0xda8);
	DUMP_DCR("DMA Inj Fifo: da9", 0xda9);
	DUMP_DCR("DMA Inf Fifo: daa", 0xdaa);
	DUMP_DCR("DMA Internal State: dab", 0xdab);
	DUMP_DCR("DMA Internal State: dac", 0xdac);
	DUMP_DCR("DMA Internal State: dad", 0xdad);
	DUMP_DCR("DMA Internal State: dae", 0xdae);
	DUMP_DCR("DMA Warnings: daf", 0xdaf);

	return p;
}

static void
dump_dcrs_cons(void)
{
	char *p, *e, *f;

	p = malloc(8*1024); /* I hope that's enough */
	e = p + 8*1024;
	f = dump_dcrs(p, e);
	*f = '\0';
	print("TORUS DCRS:\n");
	e = p;
	while(e < f)
		e += print("%s", e);
	free(p);
	print("\n");
}

static char*
dump_inj_channels(char *p, char *e, int group)
{
	int i, ena, hit;
	Fifoctl *ctl;

	ctl = &torus.dma[group];
	mb();

	for(i = 0; i < Ninjfifo; i++){
		Fifo *fifo = &ctl->inj.fifo[i];
		p = seprint(p, e, "inj fifo %d: start=%8.8x, end=%8.8x, head=%8.8x, tail=%8.8x\n",
			i, fifo->start, fifo->end, fifo->head, fifo->tail);
	}

	p = seprint(p, e, "counter enabled: %#8.8ux, %#8.8ux\n",
		ctl->inj.counter_enabled[0],
		ctl->inj.counter_enabled[1]);

	p = seprint(p, e, "counter hit zero: %#8.8ux, %#8.8ux\n",
		ctl->inj.counter_hit_zero[0],
		ctl->inj.counter_hit_zero[1]);

	for(i = 0; i < Ncounter; i++){
		ena = (ctl->inj.counter_enabled[i/32] >> (31 - i % 32)) & 1;
		hit = (ctl->inj.counter_hit_zero[i/32] >> (31 - i % 32)) & 1;
		if(ctl->inj.counter[i].base != 0 || ctl->inj.counter[i].counter != 0 || ena || hit){
			p = seprint(p, e, "  ctr %.2d: count=%8.8ux, base=%8.8ux, enabled=%d, hitzero=%d\n",
				i, ctl->inj.counter[i].counter, ctl->inj.counter[i].base, ena, hit);
		}
	}
	return p;
}

static void
dump_inj_channels_cons(int group)
{
	char *p, *e, *f;

	p = malloc(8*1024); /* I hope that's enough */
	e = p + 8*1024;
	f = dump_inj_channels(p, e, group);
	*f = '\0';
	print("TORUS INJ CHAN:\n");
	e = p;
	while(e < f)
		e += print("%s", e);
	print("\n");
	free(p);
}

static char*
dump_rcv_channels(char *p, char *e, int group)
{
	int i, ena, hit;
	Fifoctl *ctl;

	ctl = &torus.dma[group];

	mb();
	for(i = 0; i < Nrcvfifo; i++){
		Fifo *fifo = &ctl->rcv.fifo[i];
		p = seprint(p, e, "rec fifo %d: start=%8.8ux, end=%8.8ux, head=%8.8ux, tail=%8.8ux\n",
			i, fifo->start, fifo->end, fifo->head, fifo->tail);
	}

	p = seprint(p, e, "counter enabled: %8.8ux, %8.8ux\n",
		ctl->rcv.counter_enabled[0],
		ctl->rcv.counter_enabled[1]);

	p = seprint(p, e, "counter hit zero: %8.8ux, %8.8ux\n",
		ctl->rcv.counter_hit_zero[0],
		ctl->rcv.counter_hit_zero[1]);

	p = seprint(p, e, "counter group status: %8.8ux\n",
		ctl->rcv.counter_group_status);

	for(i = 0; i < Ncounter; i++){
		ena = (ctl->rcv.counter_enabled[i/32] >> (31 - i % 32)) & 1;
		hit = (ctl->rcv.counter_hit_zero[i/32] >> (31 - i % 32)) & 1;
		if(ctl->rcv.counter[i].base != 0 || ctl->rcv.counter[i].counter != 0 || ena || hit){
			p = seprint(p, e, "  ctr %.2d: count=%8.8ux, base=%8.8ux, enabled=%d, hitzero=%d\n",
				i, ctl->rcv.counter[i].counter, ctl->rcv.counter[i].base, ena, hit);
		}
	}
	return p;
}

static void
dump_rcv_channels_cons(int group)
{
	char *p, *e, *f;

	p = malloc(8*1024); /* I hope that's enough */
	e = p + 8*1024;
	f = dump_rcv_channels(p, e, group);
	*f = '\0';
	print("TORUS RCV CHAN:\n");
	e = p;
	while(e < f)
		e += print("%s", e);
	print("\n");
	free(p);
}

void
dump_debug(char *alloc, int size)
{
	char *e = alloc+size;
	char *p = alloc;

	p = dump_dcrs(p, e);
	p = dump_inj_channels(p, e, KernelGroup);
	dump_rcv_channels(p, e, KernelGroup);
}

enum{
	Qdir 		= 0,
	Qdata,
	Qctl,
	Qstatus,
	Qdebug,
};

#define	QFILE(p)	((uint)(p) & 0xF)

static Dirtab torusdir[] = {
	{ ".",		{ Qdir, 0, QTDIR }, 0,	0777 },
	{ "torus",	{ Qdata, 0, 0},	0,	0666 },
	{ "torusctl", { Qctl, 0, 0},	0,	0666 },
	{ "torusstatus",	{ Qstatus, 0, 0},	0,	0444 },
	{ "torusdebug", { Qdebug, 0, 0}, 0,	0444 },
};

/*
 * TO DO: handle self-addressed messages
 */
static int
misguided(Torus *t, Tpkt *tpkt)
{
	int m, count;

	m = 0;
	for(count = 0; count < N; count++){
		if(tpkt->dst[count] == t->addr[count])
			m++;
		if(tpkt->dst[count] >= t->size[count])
			break;
	}
	if(count < N || m == N){
		if(count < N)
			t->unreachable++;
		else
			t->self++;
		return 1;
	}
	return 0;
}

static int
txspaceavail(void *a)
{
	TxRing *tx = (TxRing *) a;

	if (tx->np >= tx->ntd-1)
		tx->full++;
		
	return tx->np < tx->ntd-1;
}

static void
torusfragsend(Block *b, void *arg)
{
	TxRing *tx = (TxRing *) arg;
	Torus *t = &torus;
	
	qlock(&tx->rl);
	if(waserror()){
		qunlock(&tx->rl);
		freeb(b);
		nexterror();
	}
	sleep(&tx->r, txspaceavail, tx);
	poperror();
	ilock(&t->txlock);
	torusinject(t, tx, b);
	tx->np++;
	iunlock(&t->txlock);
	qunlock(&tx->rl);
}

static long
toruswrite(Chan* c, void*a, long n, vlong)
{
	Block *b;
	Cmdbuf *cb;
	Torus *t = &torus;

	switch(QFILE(c->qid.path)){
	default:
		error(Egreg);

	case Qdata:
		if(n <= Tpkthdrlen)
			error(Etoosmall);
		if(n > Maxmsglen+Tpkthdrlen)
			error(Etoobig);
		if(misguided(t, a))
			error("bad destination");

		b = allocb(n);
		if(waserror()){
			freeb(b);
			nexterror();
		}
		memmove(b->rp, a, n);
		b->wp += n;
		poperror();
		ttphwfragment(torus.frags, b, torusfragsend, &torus.txr[KernelGroup][KernelTxFifo]);
		break;

	case Qctl:
		cb = parsecmd(a, n);
		if(waserror()){
			free(cb);
			nexterror();
		}
		if(cb->nf >= 1){
			if(strcmp(cb->f[0], "debug") == 0){
				if(cb->nf <= 1){
					if(debug_torus == 0)
						debug_torus = DbgIO;
					else
						debug_torus = 0;
				}else
					debug_torus = strtol(cb->f[1], nil, 0);
			}else
				cmderror(cb, "invalid request");
		}
		poperror();
		free(cb);
		break;
	}
	return n;
}

static long
torusbwrite(Chan *c, Block *b, vlong)
{
	long n;

	if(b->next)
		b = concatblock(b);	/* TO DO (but probably never happens) */
	n = BLEN(b);
	if(QFILE(c->qid.path) != Qdata){
		if(waserror()){
			freeb(b);
			nexterror();
		}
		n = toruswrite(c, b->rp, n, 0);
		poperror();
		freeb(b);
		return n;
	}

	b = pullupblock(b, Tpkthdrlen);
	if(b == nil)
		error(Etoosmall);
	if(misguided(&torus, (Tpkt*)b->rp)){
		freeb(b);
		error("bad destination");
	}
	ttphwfragment(torus.frags, b, torusfragsend, &torus.txr[KernelGroup][KernelTxFifo]);
	return n;
}

static Block*
torusbread(Chan* c, long n, vlong offset)
{
	Torus *t;

	t = &torus;
	switch(QFILE(c->qid.path)){
	default:
		return devbread(c, n, offset);

	case Qdata:
		return qbread(t->rcvq, n);
	}
}

static long
torusread(Chan* c, void *a, long n, vlong off)
{
	Torus *t;
	char *alloc, *e, *p;

	USED(off);

	t = &torus;

	if(c->qid.type & QTDIR)
		return devdirread(c, a, n, torusdir, nelem(torusdir), devgen);

	switch(QFILE(c->qid.path)){
	case Qdebug:
		alloc = malloc(16*1024); /* I hope that's enough */
		dump_debug(alloc, 16*1024);
		n = readstr(off, a, n, alloc);
		free(alloc);
		return n;

	case Qctl:
		return 0;

	case Qstatus:
		alloc = malloc(2*READSTR);
		if(waserror()){
			free(alloc);
			nexterror();
		}
		e = alloc+2*READSTR;
		p = seprint(alloc, e, "addr: x %d y %d z %d\n",
			t->addr[X], t->addr[Y], t->addr[Z]);
		p = seprint(p, e, "size: x %d y %d z %d\n",
			t->size[X], t->size[Y], t->size[Z]);
		p = seprint(p, e, "cutoff: xp %d xm %d yp %d ym %d zp %d zm %d\n",
			t->cp[X], t->cm[X], t->cp[Y],
			t->cm[Y], t->cp[Z], t->cm[Z]);
		p = seprint(p, e, "zmzpymypxmxp: %#ux\n", t->zmzpymypxmxp);

		p = seprint(p, e, "rcv: %d\n", t->rcvpackets);
		p = seprint(p, e, "inj: %d\n", t->injpackets);
		p = seprint(p, e, "rcvtoss: %d\n", t->rcvtoss);
		p = seprint(p, e, "rcvqfull: %d\n", t->rcvqfull);
		//p = seprint(p, e, "rcvpause: %d\n", t->rcvpause);
		//p = seprint(p, e, "rcvresume: %d\n", t->rcvresume);

		p = seprint(p, e, "memfifofull: %d\n", t->memfifofull);
		p = seprint(p, e, "rcvintr: %d\n", t->rcvintr);
		p = seprint(p, e, "injintr: %d\n", t->injintr);
		p = seprint(p, e, "\nreintr:");

		p = seprint(p, e, "unreachable: %ud\n", t->unreachable);
		p = seprint(p, e, "self: %ud\n\n", t->self);
		
		p = seprint(p, e, "\ninj:\n  empty: %x\n", t->dma[KernelGroup].inj.empty);
		p = seprint(p, e, "  avail: %x\n", t->dma[KernelGroup].inj.avail);
		p = seprint(p, e, "  thresh: %x\n", t->dma[KernelGroup].inj.threshold);
		p = seprint(p, e, "  status: %x\n", t->dma[KernelGroup].inj.counter_group_status);
		p = seprint(p, e, "  full: %x\n", t->txr[KernelGroup][KernelTxFifo].full);
		
		p = seprint(p, e, "\nrcv:\n  not empty: %x %x\n", t->dma[KernelGroup].rcv.notempty[0], t->dma[KernelGroup].rcv.notempty[1]);
		p = seprint(p, e, "  avail: %x %x\n", t->dma[KernelGroup].rcv.available[0], t->dma[KernelGroup].rcv.available[1]);
		p = seprint(p, e, "  thresh: %x %x\n", t->dma[KernelGroup].rcv.threshold[0], t->dma[KernelGroup].rcv.threshold[1]);
		p = seprint(p, e, "  status: %x\n", t->dma[KernelGroup].rcv.counter_group_status);
		
		USED(p, e);

		n = readstr(off, a, n, alloc);
		poperror();
		free(alloc);

		return n;

	case Qdata:
		return qread(t->rcvq, a, n);

	default:
		error(Egreg);
		return 0;	/* not reached */
	}

}

static void
torusclose(Chan*)
{
	/* TBD */
}

static Chan*
torusopen(Chan* c, int omode)
{
	return devopen(c, omode, torusdir, nelem(torusdir), devgen);
}

static long
torusstat(Chan* c, uchar* dp, long n)
{
	return devstat(c, dp, n, torusdir, nelem(torusdir), devgen);
}

static Walkqid*
toruswalk(Chan* c, Chan* nc, char** n, int nn)
{
	return devwalk(c, nc, n, nn, torusdir, nelem(torusdir), devgen);
}

int torus_unhandled_interrupts = 0;

static void
unhandled_interrupt(Ureg* ureg, void* arg)
{
	iprint("torus: bad interrupt: cause %#8.8lux %s\n", ureg->cause, (char*)arg);
	/* TODO: Report fatal interrupt registers */

	torusdisable(ureg->cause);

	if( torus_unhandled_interrupts++ > 10 ){
		/* TODO: Reset the whole bloody mess */
		for(;;){
			/* void */
		}
	}

	if(torus_dump_cons){
		dump_dcrs_cons();
		dump_inj_channels_cons(KernelGroup);
		dump_rcv_channels_cons(KernelGroup);
	}
}

/* Something has gone wrong, everybody PANIC */
/* umm..shouldn't these all just basically be the same implementation */
/* Leaving as separate to help us specialize instrumentation as necessary */

static void
fatal_interrupt(Ureg* ureg, void* arg)
{
	u32int i, zero, one, two, three;

	torusdisable(ureg->cause);

	/* disable and reset to 0 fatal interrupts for now - DEBUG ONLY */
	for(i=0; i < Ngroup; i++)
		dcrput(Dfatalon+i, ~0U);

	zero = dcrget(0xd93);
	one = dcrget(0xd94);
	two = dcrget(0xd95);
	three = dcrget(0xd96);

	panic("torus: fatal interrupt: cause %#8.8lux %s %#8.8ux %#8.8ux %#8.8ux %#8.8ux\n",
		ureg->cause, (char*)arg, zero, one, two, three);

}

static void
fatal_count_threshold(Ureg* ureg, void*)
{
	panic("torus: fatal error: count threshold (IRQ %#8.8lux)", ureg->cause);
}

static void
dma_fatal_count_threshold(Ureg* ureg, void*)
{
	panic("torus: DMA fatal error: count threshold (IRQ %#8.8lux)", ureg->cause);
}

static void
dma_fatal_error(Ureg* ureg, void*)
{
	panic("torus: DMA fatal error: (IRQ %#8.8lux)", ureg->cause);
}

static void
dma_inj_counter_error(Ureg* ureg, void* arg)
{
	iprint("torus: DMA Injection Counter Error: cause %#8.8lux %s\n", ureg->cause, (char*) arg);
	if(torus_dump_cons){
		dump_dcrs_cons();
		dump_inj_channels_cons(KernelGroup);
	}
}

static void
dma_rcv_counter_error(Ureg* ureg, void *arg)
{
	iprint("torus: DMA Injection Counter Error: cause %#8.8lux %s\n", ureg->cause, (char*)arg);
	if(torus_dump_cons){
		dump_dcrs_cons();
		dump_rcv_channels_cons(KernelGroup);
	}
}

static void
torus_inj_fifo_watermark(Ureg* ureg, void*)
{
	panic("torus: inject fifo watermark (IRQ %#8.8lux)\n", ureg->cause);
}


static void
dma_inj_fifo_threshold(Ureg* ureg, void*)
{
	panic("torus: inject fifo watermark (IRQ %#8.8lux)\n", ureg->cause);
}

static void
torus_process_rx(Torus *t, int group)
{
	int bitno;
	u32int pending, bit;
	Tpkt *tpkt;
	Fifo *fifo;
	RxRing *rx;
	u32int tail;
	uint tail_idx;
	Rcvbuf *desc;
	Block *b;
	Fifoctl *dma;
	int index;

	ilock(&t->rxlock);	/* TODO: Switch to a more granular lock */

	/*
	 * Clear all pending thresholds BEFORE looking at the fifos, so that
	 * any new fragments that arrive after we've read a fifo's tail
	 * pointer will raise new threshold IRQs.
	 */
	t->dma[group].rcv.clear_threshold[0] = ~0;
	t->dma[group].rcv.clear_threshold[1] = ~0;
	imb();
	
	for(index = 0; index < Nrcvfifo; index++) {
		rx = &t->rxr[group][index];
		if(!rx->active)
			continue;
				
		pending = t->dma[rx->group].rcv.notempty[0];	/* it's only ever in the first Ngroup*Nrcvfifo = 32 */
		bitno = rx->group*Nrcvfifo + rx->index;
		bit = IB(bitno);
		if(pending & bit){
			fifo = rx->fifo;
			tail = fifo->tail;
			if(debug_torus & DbgIO){
				print("fifo %d:%d ibit=%d pend=%#8.8ux: "
					"start=%#8.8ux end=%#8.8ux head=%#8.8ux tail=%#8.8ux\n",
					rx->group, rx->index, bitno, pending,
					fifo->start, fifo->end, fifo->head, tail);
			}

			tail_idx = (tail - rx->start) / (sizeof(Rcvbuf) >> 4);

			//ideally this lock would be used, but pending (above) is per-group
			//ilock(&rx->lock);

			while(rx->head_idx != tail_idx){
				desc = &rx->desc[rx->head_idx];

				if(debug_torus & DbgIO){
					print("desc: hw: size %#ux (%d, %d, %d)\n", desc->size, desc->dst[0], desc->dst[1], desc->dst[2]);
					print("desc: sw: src (%d %d %d) off: %#.4ux len: %#.4ux seq: %#.2ux%.2ux\n",
						desc->src[0], desc->src[1], desc->src[2], nhgets(desc->off)&~0xF, nhgets(desc->len),
						nhgets(desc->off)&0xF, desc->seq);
				}
				t->rcvpackets++;
				tpkt = desc;
				if(tpkt->seq != 0 || nhgets(tpkt->off) != 0){
					/* protocol with fragments */
					b = ttpreassemble(t->frags, (uchar*)desc, 256);	/* TO DO: 256? */
				}else{
					b = iallocb(256);
					if(b != nil){
						memmove(b->rp, desc, 256);
						b->wp += 256;
					}else
						t->rcvtoss++;
				}
				if(b != nil){
					if(qpass(t->rcvq, b) < 0){
						print("torus overrun\n");
						/* torusrcvpause();  EVH: this would be a very bad thing */
						t->rcvqfull++;
					}
				}
				rx->head_idx = NEXT(rx->head_idx, Nrx);
				if(debug_torus & DbgIO)
					print("new head idx: %x, tail idx: %x\n",
						rx->head_idx, tail_idx);
			}
			imb();
			fifo->head = rx->start + rx->head_idx * sizeof(Rcvbuf)/16;
			imb();
			//iunlock(&rx->lock);
		}
	}
	iunlock(&t->rxlock);

	if(debug_torus & DbgIO){
		dma = &t->dma[KernelGroup];
		iprint("torus rxintr: not empty: [%#8.8ux %#8.8ux], avail: [%#8.8ux %#8.8ux], "
			"thresh: [%#8.8ux %#8.8ux]\n",
			dma->rcv.notempty[0], dma->rcv.notempty[1],
			dma->rcv.available[0], dma->rcv.available[1],
			dma->rcv.threshold[0], dma->rcv.threshold[1]);
	}
}

static void
torus_rcv_fifo_watermark(Ureg* ureg, void*)
{
	panic("torus: rcv fifo watermark (IRQ %#8.8lux)\n", ureg->cause);
}


static void
dma_rcv_fifo_threshold(Ureg* ureg, void *)
{
	Torus *t;
	Fifoctl *dma;

	t = &torus;
	if(debug_torus & DbgIO){
		dma = &t->dma[KernelGroup];
		iprint("torus receive fifo threshold (irq %8.8lux)\n", ureg->cause);
		iprint("fifo: not empty: [%#8.8ux %#8.8ux], avail: [%#8.8ux %#8.8ux], "
		   "thresh: [%#8.8ux %#8.8ux]\n",
		   dma->rcv.notempty[0], dma->rcv.notempty[1],
		   dma->rcv.available[0], dma->rcv.available[1],
		   dma->rcv.threshold[0], dma->rcv.threshold[1]);
	}
	t->rcvintr++;
	torus_process_rx(t, KernelGroup);
}

static void
dma_rcv_counter_hitzero(Ureg* ureg, void*)
{
	/* KH usermode code wakes up usermode here */
	panic("torus: DMA rcv counter hitzero (IRQ %#8.8lux)\n", ureg->cause);
}

static void
dma_mem_fifo_full(Ureg*, void*)
{
	Torus *t = &torus;
	
	t->memfifofull++;
	
	dcrput(Dclear0, _DMA_CLEAR0_RMFU_ARB_WERR);
}

static void
dma_error(Ureg* ureg, void*)
{
	dump_dcrs_cons();
	panic("torus: DMA error (IRQ %#8.8lux)\n", ureg->cause);
}

static void
torusinject(Torus *t, TxRing *tx, Block *b)
{
	Injdesc *desc;
	u32int *w;
	int len;

	desc = &tx->desc[tx->tail_idx];
	memmove(&desc->hdrs, b->rp, Tpkthdrlen);	/* user provides both headers, for now */
	b->rp += Tpkthdrlen;

	len = BLEN(b);
	if(len <= 0)
		panic("torus inject: invalid length");

	desc->flags = 0;
	desc->counter = tx->ctrid;
	desc->length = len;	/* payload size */
	desc->base = PADDR(b->rp);

	/* force certain header bits, at least for now */
	memmove(desc->hdrs.src, t->addr, N);
	desc->hdrs.sk = Sk;	/* was SKIP(4) on BG/L */
	desc->hdrs.hint = PID0(KernelPid);
	desc->hdrs.size = SIZE(7) | PID1(KernelPid)|Dy|Vcd0;
	/* TO DO: SIZE(7) was SIZE(chunks-1) in BG/L */
	/* can't change that until the handling of Rx fifo memory is sorted out */

	tx->blocks[tx->tail_idx] = b;

	if(debug_torus & DbgIO){
		iprint("tailidx=%d, desc=%p, vdata=%p, pdata=%lux\n",
			tx->tail_idx, desc, b->rp, PADDR(b->rp));

		w = (u32int*)desc;
		iprint("inj %p D: [%8.8ux %8.8ux %8.8ux %8.8ux] H: [%8.8ux %8.8ux] S: [%8.8ux %8.8ux]\n",
			desc, w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
	}

	tx->tail_idx = NEXT(tx->tail_idx, tx->ntd);
	imb();   /* was wmb() in kh and mb() previously */

	t->injpackets++;

	/* arm inject fifo */
	*tx->incr = len;
	imb();
	tx->fifo->tail = tx->start + tx->tail_idx*(sizeof(Injdesc) >> 4);
	imb();
	tx->np++;
}

/*
 * injection (tx) interrupts
 */
static void
torus_process_tx(TxRing *tx)
{
	uint head_idx;
	Block *b;

	head_idx = (tx->fifo->head - tx->start) / (sizeof(Injdesc) >> 4);

	if(debug_torus & DbgIO)
		iprint("torus: processing tx queue %d:%d, head_idx=%d, pending_idx=%d\n",
			tx->group, tx->index, head_idx, tx->pending_idx);

	// ideally, we'd have this lock on just this fifo, but the pending
	// bits are global so we use a lock in the caller
	//ilock(&tx->lock);
	while(tx->pending_idx != head_idx){
		b = tx->blocks[tx->pending_idx];
		if(b == nil)
			panic("torus_process_tx: expected block");
		tx->blocks[tx->pending_idx] = nil;
		freeb(b);
		tx->np--;
		tx->pending_idx = NEXT(tx->pending_idx, tx->ntd);
	}
	wakeup(&tx->r);
	//iunlock(&tx->lock);
}

static void
dma_inj_counter_hitzero(Ureg* ureg, void*)
{
	Torus *t = &torus;
	u32int pending, bit;
	TxRing *tx = &t->txr[KernelGroup][KernelTxFifo]; /* TODO: enable multififos */
	Fifoctl *dma;
	int slot;

	t->injintr++;
	
	ilock(&t->txlock);
	
	dma = &t->dma[tx->group];
	slot = (tx->ctrid>>5) & 1;
	pending = dma->inj.counter_hit_zero[slot];
	bit = IB(tx->ctrid & 0x1F);
	if(debug_torus & DbgIO){
		iprint("torus inject counter hit zero (%#lux) [tx %d:%d pending %8.8ux bit %8.8ux]\n",
				ureg->cause, tx->group, tx->index, pending, bit);
	}
	if(pending & bit){
		dma->inj.counter_clear_hit_zero[slot] = bit;
		imb();
		torus_process_tx(tx);
	}

	iunlock(&t->txlock);
}

#define DEF_IRQ(_irq, _count, _used, _percore, _name, _handler) \
{ .irq = _irq, .count = _count, .used = _used, .percore = _percore, .name = _name, .handler = _handler }

struct bgtorus_irq {
	uint	irq;			/* initial irq */
	uint	count;			/* number of interrupts */
	uint	used;			/* whether or not its used */
	uint	percore;		/* per core IRQs */ 
	char*	name;
	void	(*handler)(Ureg*, void *);
};

/*
 EVH NOTE:
 Watermark interrupt is removed for now until we understand why it is going off.
*/

static struct bgtorus_irq bgtorus_irqs[] =
{
    DEF_IRQ( 0x20,  1, 1, 0, "Torus fatal error", fatal_interrupt ),
    DEF_IRQ( 0x21, 20, 0, 0, "Torus count threshold", fatal_count_threshold ),
    DEF_IRQ( 0x36,  8, 1, 0, "Torus DMA fatal count threshold",
					    dma_fatal_count_threshold ),
    DEF_IRQ( 0x3f,  1, 1, 0, "Torus DMA fatal error", dma_fatal_error ),
    DEF_IRQ( 0x40,  8, 0, 0, "Torus inj fifo watermark", torus_inj_fifo_watermark ),
	DEF_IRQ( 0x48, 14, 0, 0, "Torus rcv fifo watermark", torus_rcv_fifo_watermark ),
    DEF_IRQ( 0x56,  2, 1, 0, "Torus read invalid address", unhandled_interrupt ),
    DEF_IRQ( 0x58,  4, 1, 1, "Torus inj fifo threshold", dma_inj_fifo_threshold ),
    DEF_IRQ( 0x5c,  4, 1, 1, "Torus rcv fifo threshold", dma_rcv_fifo_threshold ),
    DEF_IRQ( 0x60,  4, 1, 1, "Torus inj counter 0", dma_inj_counter_hitzero ),
    DEF_IRQ( 0x64,  4, 1, 1, "Torus rec counter 0", dma_rcv_counter_hitzero ),
    DEF_IRQ( 0x68,  4, 1, 1, "Torus inj counter error", dma_inj_counter_error ),
    DEF_IRQ( 0x6c,  2, 0, 0, "Torus rcv counter error", dma_rcv_counter_error ),
    DEF_IRQ( 0x6e,  1, 1, 0, "Torus memory fifo full", dma_mem_fifo_full ),
    DEF_IRQ( 0x6f,  6, 1, 0, "Torus DMA error", dma_error ),
    DEF_IRQ( 0x75,  1, 1, 0, "Torus link check", unhandled_interrupt ),
    DEF_IRQ( 0x76,  4, 0, 0, "Torus DD2 error", unhandled_interrupt ),
    { .count = 0 },
};

/* disable a torus interrupt for debug */
static void
torusdisable(int cause)
{
	struct bgtorus_irq *desc = bgtorus_irqs;
	int count = 0, i, virq;

	while(desc->count){
		virq = desc->irq;
		for(i = 0; i < desc->count; i++){
			if(virq == cause)
				intrdisable(virq, desc->handler, desc->name, desc->name);
			virq++;
		}
		count += desc->count;
		desc++;
	}
}

static void
set_dma_receive_ranges(int range, ulong start, ulong end)
{
	dcrput(Drdmamin+(range*2), start);
	dcrput(Drdmamax+(range*2), end);
}

static void
set_dma_inject_ranges(int range, ulong start, ulong end)
{
	dcrput(Didmamin+(range*2), start);
	dcrput(Didmamax+(range*2), end);
}

static void
set_inj_fifo_map(int group, uint fifo, u8int fifomap)
{
	u32int val = dcrget(Dififomap + group*8 + (fifo / 4));
	val &= ~(0xff000000 >> (8 * (fifo % 4)));
	val |= (u32int)fifomap << (8 * (3 - (fifo % 4)));
	dcrput(Dififomap + group * 8 + (fifo / 4), val);
}

static void
set_inj_fifo_prio(int group, int fifo, int prio)
{
	u32int val = dcrget(Dififopri + group);
	val &= ~(0x80000000 >> (fifo % 32));
	val |= ((prio << 31) >> (fifo % 32));
	dcrput(Dififopri + group, val);
}

static void
set_inj_fifo_local_dma(int group, int fifo, int local_dma)
{
	u32int val = dcrget(Dififolocal + group);
	val &= ~(0x80000000 >> (fifo % 32));
	val |= ((local_dma << 31) >> (fifo % 32));
	dcrput(Dififolocal + group, val);
}

static void
config_inj_fifo(int group, int fifo, u8int fifomap, int prio, int local_dma)
{
	set_inj_fifo_map(group, fifo, fifomap);
	set_inj_fifo_prio(group, fifo, prio);
	set_inj_fifo_local_dma(group, fifo, local_dma);
}

static void
reset_inj_fifo(int group, int fifo)
{
	config_inj_fifo(group, fifo, BGP_DEFAULT_FIFOMAP, 0, 0);
}

/* mapping from pid -> (group/rcv fifo) */
static void
set_rcv_fifo_map(int pid, int group, int fifoid)
{
	u8int fifomap = (group << 3) + (fifoid & 0x7);

	dcrput(Drfifomap0 + (pid * 2),
		_rDMA_TS_REC_FIFO_MAP_XP(fifomap) |
		_rDMA_TS_REC_FIFO_MAP_XM(fifomap) |
		_rDMA_TS_REC_FIFO_MAP_YP(fifomap) |
		_rDMA_TS_REC_FIFO_MAP_YM(fifomap));

	dcrput(Drfifomap1 + (pid * 2),
		_rDMA_TS_REC_FIFO_MAP_ZP(fifomap) |
		_rDMA_TS_REC_FIFO_MAP_ZM(fifomap) |
		_rDMA_TS_REC_FIFO_MAP_HIGH(fifomap) |
		_rDMA_TS_REC_FIFO_MAP_LOCAL(fifomap));
}

static void
enable_inj_fifo(int group, int fifo)
{
	u32int val = dcrget(Dififoctl + group) | (0x80000000 >> fifo);

	if(debug_torus & DbgSetup){
		print("enable inj fifo %d:%d (%x -> %x)\n",
			group, fifo, dcrget(Dififoctl+group), val);
	}

	dcrput( Dififoctl + group, val );
}

static void
activate_inj_fifo(Torus *t, int group, int fifo)
{
	t->dma[group].inj.dma_activate = 0x80000000U >> fifo;
}

static void
disable_inj_fifo(int group, int fifo)
{
	u32int r;

	r = dcrget(Dififoctl + group) & ~(0x80000000U >> fifo);
	if(debug_torus & DbgSetup){
		print("disable inj fifo g=%d, f=%d (%x -> %x)\n",
			group, fifo, dcrget(Dififoctl+group), r);
	}
	dcrput( Dififoctl + group, r);
}

static void
deactivate_inj_fifo(Torus *t, int group, int fifo)
{
	t->dma[group].inj.dma_deactivate = 0x80000000U >> fifo;
}

static void
enable_rcv_fifo(int group, int fifo)
{
	u32int r;

	r = dcrget(Drfifoctl) | (0x80000000U >> (group * Nrcvfifo + fifo));
	dcrput(Drfifoctl, r);
}

static void
disable_rcv_fifo(int group, int fifo)
{
	u32int r;

	r = dcrget(Drfifoctl) & ~(0x80000000U >> (group * Nrcvfifo + fifo));
	dcrput(Drfifoctl, r);
}


static void
set_rcv_fifo_threshold(int type, u32int val)
{
	dcrput(Drthresh + type, val);
}

static void
enable_rcv_fifo_threshold_interrupt(int type, int group, int fifo)
{
	dcrput(Drintron+type, dcrget(Drintron+type) |
		    (0x80000000U >> (group * Nrcvfifo + fifo)));	/* TODO: fix */
}

static void
set_inj_counter_zero_interrupt(int group, int counter, int irqgroup)
{
	dcrput(Dichzintron+irqgroup, dcrget(Dichzintron+irqgroup) |
		(0x80000000U >> ((group * Ncounter + counter) / 8)));
}

static void
init_inj_counter(Torus *t, int group, uint counter)
{
	u32int mask;

	mask = 0x80000000U >> (counter % 32);
	t->dma[group].inj.counter_enable[counter / 32] = mask;
	t->dma[group].inj.counter[counter].counter = 0;
	t->dma[group].inj.counter_clear_hit_zero[counter / 32] = mask;
	imb();
}

/*
 * allocate size bytes for fifo memory
 * (injection descriptors for tx side, packet space for rx side)
 */
static void*
init_fifo(Fifo *fifo, ulong size)
{
	void *buf;
	uintptr pa;

	if(debug_torus & DbgSetup)
		print("torus: allocating %lud for fifo memory\n", size);

	buf = mallocalign(size, 32, 0, 0);	/* 32-byte aligned: see p. 20 */
	if(buf == nil)
		panic("torus: init_fifo: refused %lud bytes", size);

	memset(buf, 0, size);
	mb();

	pa = PADDR(buf);
	if(debug_torus & DbgSetup)
		print("torus: fifo init va=%p, pa=%#8.8lux\n", buf, pa);

	fifo->start = pa >> 4;
	fifo->end = (pa + size) >> 4;
	fifo->head = pa >> 4;
	fifo->tail = pa >> 4;
	imb();
	return buf;
}

static void
init_tx_queue(Torus *t, int group, int index)
{
	TxRing *tx;

	tx = &t->txr[group][index];
	tx->group = group;
	tx->index = index;

	tx->fifo = &t->dma[group].inj.fifo[index];
	tx->desc = init_fifo(tx->fifo, sizeof(Injdesc)*Ntx);
	tx->start = tx->fifo->start;

	tx->blocks = mallocz(Ntx*sizeof(Block*), 1);
	if(tx->blocks == nil)
		error(Enomem);
	tx->ntd = Ntx;

	tx->fifo = &t->dma[group].inj.fifo[index];
	tx->ctrid = (group * Ncounter) + index;		/* indexed by fifo number within group */
	init_inj_counter(t, group, index);
	set_inj_counter_zero_interrupt(group, index, group);
	tx->incr = &t->dma[group].inj.counter[index].increment;

	if(debug_torus & DbgSetup)
		print("torus: allocated tx queue %d:%d: %p (start=%#8.8ux, end=%#8.8ux)\n",
		    group, index, tx->desc, tx->fifo->start, tx->fifo->end);

	tx->active++;
}

static void
init_rx_queue(Torus *t, int group, int index, int region)
{
	RxRing *rx;

	rx = &t->rxr[group][index];
	rx->group = group;
	rx->index = index;

	rx->fifo = &t->dma[group].rcv.fifo[index];
	rx->desc = init_fifo(rx->fifo, sizeof(Rcvbuf)*Nrx);
	rx->start = rx->fifo->start;
	rx->nrd = Nrx;

	/* EVH: This next comment was in the original file */
	/* XXX: set dma region for rx fifo */
	if(debug_torus & DbgSetup)
		print("Torus: allocated rx queue %d:%d: %p (start=%#8.8ux, end=%#8.8ux) region %d\n",
		     group, index, rx->desc, rx->fifo->start, rx->fifo->end, region);

	rx->active++;
}

Chan*
torusattach(char* spec)
{
	char *p;
	Chan *c;
	int i, pid;
	Torus *t;
	int group, index;

	/*
	 * There is but one, and it shall be named '0'.
	 */
	if(*spec != 0){
		i = strtoul(spec, &p, 0);
		if(i != 0 || (i == 0 && p == spec) || *p != 0)
			error(Ebadspec);
	}

	t = &torus;
	if(t->size[X] == 0 || t->size[Y] == 0 || t->size[Z] == 0)
		error(Enodev);

	c = devattach(L'∞', spec);

	qlock(&t->alock);
	if(t->ref++ > 1){
		qunlock(&t->alock);
		return c;
	}

	if(waserror()){
		qclose(t->rcvq);
		freefrags(t->frags);
		t->ref--;
		cclose(c);
		qunlock(&t->alock);
		nexterror();
	}

	if(t->rcvq == nil && (t->rcvq = qopen(1024*1024, Qmsg, nil, 0)) == nil)
		error(Enomem);
	qreopen(t->rcvq);

	if((t->frags = newfrags(Torushdrlen, 16, sizeof(Rcvbuf))) == nil)
		error(Enomem);

	config_inj_fifo(KernelGroup, KernelTxFifo, BGP_KERNEL_FIFOMAP, 1, 0);

	/* Set all pids to point at the kernel's reception fifo. */
	/* pid == group for our case */
	for(pid=0; pid < Npid; pid++)
		set_rcv_fifo_map(pid, pid, KernelRxFifo);

	init_tx_queue(t, KernelGroup, KernelTxFifo);
	init_rx_queue(t, KernelGroup, KernelRxFifo, KernelRxRegion);

	poperror();
	qunlock(&t->alock);

	for(group = 0; group < Ngroup; group ++) {
		for(index = 0; index < Ninjfifo; index++) {
			if(t->txr[group][index].active) {
				enable_inj_fifo(group, index);
				activate_inj_fifo(t, group, index);
			}
		}
	}

	set_rcv_fifo_threshold(0, ((Nrx - 1) * 256) / 16);		/* TO DO: why this value? */

	for(group = 0; group < Ngroup; group ++) {
		for(index = 0; index < Nrcvfifo; index++) {
			if(t->rxr[group][index].active) {
				enable_rcv_fifo_threshold_interrupt(group, group, index);
				enable_rcv_fifo(group, index);
			}
		}
 	}

	return c;
}

static void
torusshutdown(void)
{
	/*
	 * To do here: need finer control over startup
	 * to enable controller reset on errors and shutdown.
	 */
}

static void
torusinit(void)
{
	if(sys->ionode)
		return;
}

static void
torusreset(void)
{
	Torus *t;
	u32int config;
	int i, group;
	struct bgtorus_irq *desc;
	int virq;

	if(sys->ionode)
		return;
	if(!(personality->Kernel_Config.NodeConfig & BGP_PERS_ENABLE_Torus))
		return;

	if(debug_torus & DbgSetup)
		print("Torus initialization starting\n");

	/*
	 * Extract the configuration from the personality.
	 */
	t = &torus;
	t->addr[X] = personality->Network_Config.Xcoord;
	t->addr[Y] = personality->Network_Config.Ycoord;
	t->addr[Z] = personality->Network_Config.Zcoord;
	t->size[X] = personality->Network_Config.Xnodes;
	t->size[Y] = personality->Network_Config.Ynodes;
	t->size[Z] = personality->Network_Config.Znodes;

	config = personality->Kernel_Config.NodeConfig;
	t->torus[X] = !(config & BGP_PERS_ENABLE_TorusMeshX);
	t->torus[Y] = !(config & BGP_PERS_ENABLE_TorusMeshY);
	t->torus[Z] = !(config & BGP_PERS_ENABLE_TorusMeshZ);

	/* reset hardware */

	/* map torus dma structure */
	t->dma = kmapphys(VIRTDMA, PHYSDMA, 16*KiB, TLBI|TLBG|TLBWR);
	if(debug_torus & DbgSetup)
		print("TORUS MAPPING: 16k, %d %p\n", Ngroup*sizeof(Fifoctl), t->dma);

	for(desc = bgtorus_irqs; desc->count; desc++){
		if(!desc->used)
			continue;
		virq = desc->irq;

		for(i = 0; i < desc->count; i++){
			if(desc->percore)
				intrenable(virq, desc->handler, (void *)i, desc->name, 0);
			else
				intrenable(virq, desc->handler, nil, desc->name, i);
			virq++;
		}
	}

/* kh: this code based on bgtorus_reset in kittyhawk code */
	/* pull and release reset */
	dcrput(Dreset, 0xffffffff);
	microdelay(50000);
	dcrput(Dreset, 0);
	microdelay(50000);

	/* enable DMA use - REVISIT: does this clear too much? */
	dcrput(Dbasectl, Rusedma);

	/* set receive DMA ranges to all */
	set_dma_receive_ranges(0, 0, 0xffffffff);
	set_dma_inject_ranges(0, 0, 0xffffffff);

	/* clear rest */
	for(i=1; i < Nregion; i++){
		set_dma_receive_ranges(i, 1, 0);
		set_dma_inject_ranges(i, 1, 0);
	}

	/* set injection fifo watermarks: EVH SUSPECT */
	dcrput(Dififowm0, _iDMA_TS_FIFO_WM0_INIT); /* set all to 20 */	/* IF(20,2,6)|IF(20,10,6)|IF(20,18,6)|IF(20,26,6) */
	dcrput(Dififowm1, _iDMA_TS_FIFO_WM1_INIT);

	/* set reception fifo watermarks */
	for(i=0; i < Ngroup; i++)
		dcrput(Drfifowm0+i, 0);	/* must be zero */

	/* set local fifo watermark */
	dcrput(Dilocalwm0, _iDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY_INIT);
	dcrput(Drlocalwm0, 0);	/* suggested values all zero, p. 26 */

	dcrput(Diquanta, (0<<16)|0);	/* suggested values (1 packet), see p. 27 */

	dcrput(Drclrmask+0, 0xFF000000);	/* suggested values, see p. 28 */
	dcrput(Drclrmask+1, 0x00FF0000);
	dcrput(Drclrmask+2, 0x0000FF00);
	dcrput(Drclrmask+3, 0x000000FF);
	dcrput(Drhdrclrmask, 0x08040201);	/* suggested value, see p. 29 */

	/* disable all counters, init all fifos */
	for(group=0; group < Ngroup; group++){
		for(i=0; i < Ncounter; i++){
	    		t->dma[group].inj.counter[i].counter = ~0;
	    		t->dma[group].inj.counter[i].base = 0;
	    		t->dma[group].rcv.counter[i].counter = ~0;
	    		t->dma[group].rcv.counter[i].base = 0;
	    		t->dma[group].rcv.counter[i].limit = ~0;
		}

		t->dma[group].inj.counter_disable[0] = ~0;
		t->dma[group].inj.counter_disable[1] = ~0;
		t->dma[group].rcv.counter_disable[0] = ~0;
		t->dma[group].rcv.counter_disable[1] = ~0;

		t->dma[group].inj.counter_disable[0] = ~0;
		t->dma[group].inj.counter_disable[1] = ~0;
		t->dma[group].rcv.counter_disable[0] = ~0;
		t->dma[group].rcv.counter_disable[1] = ~0;

		t->dma[group].inj.dma_deactivate = ~0;

		for(i=0; i < Ninjfifo; i++){
			Fifo *fifo = &t->dma[group].inj.fifo[i];
			fifo->start = 0;
			fifo->end = 0;
			fifo->head = 0;
			fifo->tail = 0;
			
			t->txr[group][i].active = 0;

			reset_inj_fifo(group, i);
		}

		/* reset all receive fifos, including the one for headers (debugging) */
		for(i=0; i < Nrcvfifo + 1; i++){
			Fifo *fifo = &t->dma[group].rcv.fifo[i];
			fifo->start = 0;
			fifo->end = 0;
			fifo->head = 0;
			fifo->tail = 0;
			
			t->rxr[group][i].active = 0;
		}

		/* clear thresholds */
		t->dma[group].inj.clear_threshold = ~0;
		t->dma[group].rcv.clear_threshold[0] = ~0;
		t->dma[group].rcv.clear_threshold[1] = ~0;
	}
	imb();

	/* clear error count DCRs */
	for(i=0; i< 9; i++)
		dcrput(Dififoce+i, 0);

	/* set all the error count threshold and enable all interrupts */
	dcrput(Derrthresh, 1);

	/* clear the errors associated with the UE's on initial sram writes */
	dcrput(Dclear0, ~0U);
	dcrput(Dclear1, ~0U);

	for(i=0; i < Ngroup; i++)
		dcrput(Dfatalon+i, ~0U);

	/* enable the state machines */
	dcrput(Dbasectl, Rusedma|RL3burst|Rinjenable|Rrcvenable|Rimfuenable|Rrmfuenable);

	imb();
	if(debug_torus & DbgSetup)
		print("Torus reset complete\n");
}

Dev torusdevtab = {
	L'∞',						/* Alt-X-2-2-1-e */
	"torus",

	torusreset,
	torusinit,
	torusshutdown,
	torusattach,
	toruswalk,
	torusstat,
	torusopen,
	devcreate,
	torusclose,
	torusread,
	torusbread,
	toruswrite,
	torusbwrite,
	devremove,
	devwstat,
};
