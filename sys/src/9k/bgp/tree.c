/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: collective network implementation
 *
 * All rights reserved
 *
 */


/*
 * notes:
 *	p. 8	PSRx give header and payload counts
 *		VCFGx has payload watermark level
 *		NADDR has 24-bit node address
 *	p. 10	empty receive FIFO and Dprxf
 *	p. 11	Dprxf clear-on-read and write 1 to set, Dprxen to enable
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



#define ALIGNED(p, a)	(!(((uintptr)(p)) & ((a)-1)))

#define TXBITUSE	0

enum{				/* DCRs */
	/* verified from u-boot AND KH*/
	Drdr0=	0xc00,	/* source and targets for class 0 and class 1 packets */
	Drdr1,			/* classes 2 and 3 */
	Drdr2,			/* classes 4 and 5 */
	Drdr3,			/* classes 6 and 7 */
	Drdr4,			/* classes 8 and 9 */
	Drdr5,			/* classes 10 and 11 */
	Drdr6,			/* classes 12 and 13 */
	Drdr7,			/* classes 14 and 15 */
	Disra,			/* Idle Sequence Register bits 0..31 */
	Disrb,			/* Idle Sequence Register bits 32..63 */

	Drcfg=	0xc10,	/* general CNRI router configuration */
	Drto,				/* router timeout */
	Drtime,			/* router time */
	Drstat,			/* router status */
	Dhd00,			/* next packet header in channel 0, virtual channel 0 */
	Dhd01,			/* ... channel 0, virtual channel 1 */
	Dhd10,
	Dhd11,
	Dhd20,
	Dhd21,
	Dhdi0,			/* header injection vc0 */
	Dhdi1,			/* header injection vc1 */
	Dforcec,			/* force route control */
	Dforcer,			/* force route */
	Dforceh,			/* force route header */
	Dxstat,			/* router extended status */
	Drstat0 = 0xc20,
	Drctrl0,
	Dsstat0,
	Dsctrl0,
	Dtnack0,
	Dcnack0,
	Dtidle0,
	Dcidle0,

	/* from KH */
	Dch0ctl=	0xc20,	/* channel 0 control registers */
	Dch1ctl=	0xc28,	/* channel 1 */
	Dch2ctl=	0xc30,	/* channel 2 */

	Dfptr= 	0xc40,		/* FIFO pointer */

	/* verified from u- boot */
	/* u-boot has two names for the same things, e.g.:
		BGP_DCR_TR_GLOB_NADDR 0xc41
		BGP_DCR_TR_GLOB_VCFG0 0xc42
		BGP_DCR_TR_GLOB_VCFG1 0xc43
		BGP_DCR_TREE_REC 0xc44
		BGP_DCR_TR_REC_PRXF 0xc44
		BGP_DCR_TREE_REC_PRXEN 0xc45
		BGP_DCR_TR_REC_PRXEN 0xc45
		BGP_DCR_TREE_INJ 0xc48
		BGP_DCR_TR_INJ_PIXF 0xc48
		BGP_DCR_TREE_INJ_PIXEN 0xc49
		BGP_DCR_TR_INJ_PIXEN 0xc49
		BGP_DCR_TREE_INJ_CSPY0 0xc4c
		BGP_DCR_TREE_INJ_CSHD0 0xc4d
		BGP_DCR_TREE_INJ_CSPY1 0xc4e
		BGP_DCR_TREE_INJ_CSHD1 0xc4f
	 */

	Dnaddr = 0xc41,		/* node address (for point to point traffic) */
	Dvcfg0,			/* VC0 configuration (fifo watermarks and checksum config) */
	Dvcfg1,			/* VC1 configuration */
	Dprxf,			/* reception interrupt flags */
	Dprxen,			/* reception interrupt enables */
	Dpixf = 0xc48,			/* injection interrupt flags */
	Dpixen,			/* injection interrupt enables */
	Dcspy0 = 0xc4c,			/* VC0 payload checksum */
	Dcshd0,			/* VC0 header checksum */
	Dcspy1,			/* VC1 payload checksum */
	Dcshd1,			/* VC1 header checksum */
};

/* Confirmed with KH */
enum {	/* Dprxf and Dprxen */
	RxP0parity=		IB(8),	/* read on P0 port had bad address parity */
	RxP1parity=		IB(9),
	RxP0align=		IB(10),	/* read on P0 was not quadword aligned */
	RxP1align=		IB(11),
	RxP0addr=		IB(12),	/* read on P0 with bad address */
	RxP1addr=		IB(13),
	RxFIFOcollision=	IB(14),	/* simultaneous reads to same FIFO on P0 and P1 */
	RxBadECC=		IB(15),	/* uncorrectable SRAM ECC error */
	RxVc0nodata=	IB(26),	/* VC0 payload FIFO underrun (critical) */
	RxVc1nodata=	IB(27),
	RxVc0nohdr=	IB(28),	/* VC0 header FIFO underrun (critical) */
	RxVc1nohdr=	IB(29),
	RxVc0mark=	IB(30),	/* VC0 payload FIFO exceeds watermark (critical) */
	RxVc1mark=	IB(31),

	/* hardware errors can happen rarely */
	RxHwErrors=	RxP0parity | RxP1parity | RxBadECC,

	/* software errors `cannot happen' */
	RxSwErrors=	RxP0align | RxP1align | RxP0addr | RxP1addr | RxFIFOcollision |
				RxVc0nodata | RxVc1nodata | RxVc0nohdr | RxVc1nohdr,
};
/* confirmed with KH note that bits 6..9 used to be 8..11 but
 * moved as the addr error is new*/
enum {	/* Dpixf and Dpixen */
	TxP0parity=	IB(6),	/* write to P0 port had bad address parity */
	TxP1parity=	IB(7),
	TxP0align=	IB(8),	/* write to P0 port not quadword aligned */
	TxP1align=	IB(9),
	TxP0addr=	IB(10),	/* write to P0 port address error*/
	TxP1addr=	IB(11),
	TxP0data=	IB(12),	/* write to P0 had bad data parity */
	TxP1data=	IB(13),
	TxFIFOcollision= IB(14),	/* simultaneous writes to same FIFO on P0 and P1 */
	TxBadECC=	IB(15),	/* uncorrectable SRAM ECC error */

	TxVc0overdata=	IB(25),	/* VC0 payload FIFO overflow (critical) */
	TxVc1overdata=	IB(26),
	TxVc0overhdr=	IB(27),	/* VC0 header FIFO overflow (critical) */
	TxVc1overhdr=	IB(28),
	TxVc0mark=	IB(29),	/* VC0 payload FIFO below watermark (critical) */
	TxVc1mark=	IB(30),
	InjectionEnable=	IB(31),	/* =0, injection disabled when Dpixen bit is 0; =1, always enabled */

	/* hardware errors can happen rarely */
	TxHwErrors=	TxP0parity | TxP1parity | TxP0data | TxP1data | TxBadECC,

	/* software errors `cannot happen' */
	TxSwErrors=	TxP0align | TxP1align | TxFIFOcollision |
				TxVc0overdata | TxVc1overdata | TxVc0overhdr | TxVc1overhdr,
};

/* This set of enums confirmed from KH */
enum {	/* Drcfg */
	Src00dis=		IB(0),	/* disable source channel 0, virtual channel 0 */
	Src01dis=		IB(1),	/* disable source channel 0, virtual channel 1 */
	Tar00dis=		IB(2),	/* disable target channel 0, virtual channel 0 */
	Tar01dis=		IB(3),	/* disable target channel 0, virtual channel 1 */
	Src10dis=		IB(4),	/* disable source 1, vc0 */
	Src11dis=		IB(5),	/* disable source 1, vc1 */
	Tar10dis=		IB(6),	/* disable target 1, vc0 */
	Tar11dis=		IB(7),	/* disable target 1, vc1 */
	Src20dis=		IB(8),	/* disable source 2, vc0 */
	Src21dis=		IB(9),	/* disable source 2, vc1 */
	Tar20dis=		IB(10),	/* disable target 2, vc0 */
	Tar21dis=		IB(11),	/* disable target 2, vc1 */
	C2loop=		IB(25),	/* enable channel 2 loop back */
	C1loop=		IB(26),	/* enable channel 1 loop back */
	C0loop=		IB(27),	/* enable channel 0 loop back */
	Timeout0=	IF(0, 28, 2),	/* no timeout interrupt */
	Timeout1=	IF(1, 28, 2),	/* normal timeout */
	Timeout2=	IF(2, 28, 2),	/* watchdog timeout */
	ManualMode=	IB(30),	/* =0, router is enabled */
	RouterReset=	IB(31),	/* full router reset */
};

enum {	/* Dvcfg0, Dvcfg1 */
	ReceiveAll=	IB(0),	/* =1, receiver accepts all p2p addresses; =0, receiver accepts only Dnaddr */
	Ice_m=		IF(~0, 8, 8),	/* injection checksum exclusion (1+number of halfwords at start or end to exclude) */
	Rmark_m=	IF(~0, 21, 3),	/* reception fifo watermark: exception when this is exceeded */
	Imark_m=	IF(~0, 29, 3),	/* injection fifo watermark: exception when <= this value */
};

enum {
	Nvc=	2,

	Fifo1Offset		=	0x01000000,
	PayloadSize		=	256,
	Nquads			=	256 / 16,		/* PayloadSize in quads */
	HeaderSize		=	4,		/* four byte destination header */

	/* non-critical interrupts (index from VectorTree0) */
	IrqTimeout=	0,		/* router indicates timeout configured in Drcfg and Drto */
	IrqNoTarget=	1,		/* receiver had packet but could not find target */
	IrqOverflow=	2,		/* ALU overflow */
	IrqTxE=		3,		/* injection exception (non-critical) */
	IrqRxE=		4,		/* reception exception (non-critical) */
	IrqC0Full=	5,		/* arbiter attempted to route to a full channel 0 */
	IrqC0TxECC=	6,		/* non-correctable ECC error when reading vc0 memory */
	IrqC0TxCRC=	7,		/* parity error sending on link for channel 0 */
	IrqC1Full=	8,		/* arbiter attempted to route to a full channel 1 */
	IrqC1TxECC=	9,
	IrqC1TxCRC=	10,
	IrqC2Full=	11,
	IrqC2TxECC=	12,
	IrqC2TxCRC=	13,
	IrqC0RxECC=	14,		/* receiver of channel 0 found hard ECC error */
	IrqC0RxCRC=	15,		/* receiver of channel 0 found parity error */
	IrqC1RxECC=	16,
	IrqC1RxCRC=	17,
	IrqC2RxECC=	18,
	IrqC2RxCRC=	19,
	IrqTx=		20,	/* injection exception  */
	IrqRx=		21,	/* reception exception  */
	IrqVc0Intr=	22,	/* packet in vc0 has interrupt bit set */
	IrqVc1Intr=	23,	/* packet in vc1 has interrupt bit set */
	Nirq,
};

/* Confirmed from KH */
enum {	/* byte offsets to memory mapped registers */
	Mpid=	0x00,		/* data injection FIFO */
	Mpih=	0x10,		/* header injection FIFO */
	Mprd=	0x20,		/* data receive FIFO */
	Mprh=	0x30,		/* header receive FIFO */
	Mpsr=	0x40,		/* status register */
	Mpsra=	0x50,		/* alias for other channel's status register */
};

typedef struct Tchan Tchan;
struct Tchan {
	int	n;		/* virtual channel number */
	int	ionode;	/* !=0, this is an IO node not a cpu node */
	u32int	p2paddr;	/* our 20-bit address for p2p traffic */
	u32int*	regs;		/* memory mapped registers for this virtual channel */
	Lock;
	uchar*	quads;	/* quad aligned storage */
	Queue*	iq;		/* data from tree */
	Queue*	oq;		/* data to tree */
	u32int	txbit;	/* TxVc[01]mark */
	u32int	rxbit;	/* RxVc[01]mark */
	ulong	ntxintr;
	ulong	ntxkick;
	ulong	nrxintr;
	ulong	nrxkick;
	ulong	ntx;
	ulong	nrx;
	/* measurement for time in txkick */
	u32int	waitempty;
	u32int 	timeempty;
	u32int	waitdone;
	u32int	timedone;
	u32int	dropfull;
	u32int	badalign;
	/* debug info */
	u32int	lastheader;
};
#define	REG(ctlr, r)	((ctlr)->regs[(r)/sizeof(u32int)])

static int rxfifoempty(Tchan*);
static int txfifofull(Tchan*);

static Tchan	treechan[Nvc];		/* one for each virtual channel */

char *irqnames[] = {
	[IrqTimeout] = "IrqTimeout",
	[IrqNoTarget] = "IrqNoTarget",
	[IrqOverflow] = "IrqOverflow",
	[IrqTxE] = "IrqTxE",
	[IrqRxE] = "IrqRxE",
	[IrqC0Full] = "IrqC0Full",
	[IrqC0TxECC] = "IrqC0TxECC",
	[IrqC0TxCRC] = "IrqC0TxCRC",
	[IrqC1Full] = "IrqC1Full",
	[IrqC1TxECC] = "IrqC1TxECC",
	[IrqC1TxCRC] = "IrqC1TxCRC",
	[IrqC2Full] = "IrqC2Full",
	[IrqC2TxECC] = "IrqC2TxECC",
	[IrqC2TxCRC] = "IrqC2TxCRC",
	[IrqC0RxECC] = "IrqC0RxECC",
	[IrqC0RxCRC] = "IrqC0RxCRC",
	[IrqC1RxECC] = "IrqC1RxECC",
	[IrqC1RxCRC] = "IrqC1RxCRC",
	[IrqC2RxECC] = "IrqC2RxECC",
	[IrqC2RxCRC] = "IrqC2RxCRC",
	[IrqTx] = "injection exception  ",
	[IrqRx] = "reception exception  ",
	[IrqVc0Intr] = "packet in vc0 has interrupt bit set ",
	[IrqVc1Intr] = "packet in vc1 has interrupt bit set ",
};

static long
treestats(int vc, char *buf, long n)
{
	int i;
	Tchan *t;
	char *p, *e;
	u32int r;

	t = &treechan[vc];
	p = buf;
	e = buf+n;

	p = seprint(p, e, "vc%d: p2paddr %#8.8ux\n", vc, dcrget(Dnaddr));
	p = seprint(p, e, "vcfg %#8.8ux rcfg %#8.8ux rstat %#8.8ux\n",
		 dcrget(Dvcfg0+vc), dcrget(Drcfg), dcrget(Drstat));

	r = REG(t, Mpsr);

	p = seprint(p, e, "pixen %#8.8ux ntxintr %lud ntx %lud ntxkick %lud\n",
		dcrget(Dpixen), t->ntxintr, t->ntx, t->ntxkick);
	p = seprint(p, e, "prxen %#8.8ux nrxintr %lud nrx %lud nrxkick %lud\n",
		dcrget(Dprxen), t->nrxintr, t->nrx, t->nrxkick);
	p = seprint(p, e, "psr: ipc %d iqw %d ihc %d rpc %d rqw %d rhc %d rpi %d\n",
		r>>28, (r>>24)&0x0f, (r>>16)&0x0f,
		(r>>12)&0xf, (r>>8)&0x0f,
		r&0xf, r&0x10);
	p = seprint(p, e, "rto %ud xstat %#8.8ux rxempty %d iqfull %d txfull %d\n",
		dcrget(Drto), dcrget(Dxstat), rxfifoempty(t), qfull(t->iq), txfifofull(t));

	i = vc*8;
	p = seprint(p, e, "rstat %#8.8ux rctrl %#8.8ux\n",
		dcrget(Drstat0+i), dcrget(Drctrl0+i));
	p = seprint(p, e, "sstat %#8.8ux sctrl %#8.8ux\n",
		dcrget(Dsstat0+i), dcrget(Dsctrl0+i));
	p = seprint(p, e, "tnack %#8.8ux cnack %#8.8ux\n",
		dcrget(Dtnack0+i), dcrget(Dcnack0+i));
	p = seprint(p, e, "tidle %#8.8ux cidle %#8.8ux\n",
		dcrget(Dtidle0+i), dcrget(Dcidle0+i));
	p = seprint(p, e, "waitempty %ud timeempty %ud waitdone %ud timedone %ud dropfull %ud badalign %ud\n",
		t->waitempty, t->timeempty, t->waitdone, t->timedone, t->dropfull, t->badalign);
	p = seprint(p, e, "lastheader %#8.8ux\n", t->lastheader);
	return p-buf;
}

/*
 * Mpsr:
 *	0: 7		injection payload count (in 16-byte units); bits 0:3 count complete packets
 *	12: 15	injection header count (in headers)
 *	16: 23	reception payload count (in 16-byte units); bits 16: 19 count complete packets
 *	interrupt request:	IB(27)	at least one packet with Interrupt Request bit set
 *	28: 31	reception header count (in headers)
 */
static int
rxfifoempty(Tchan *ctlr)
{
	/* use the payload count, which is updated last (see p. 8),
	   and count only complete packets (p. 17) */
	return (REG(ctlr, Mpsr) & IF(~0, 16, 4)) == 0;
}

static int
txfifofull(Tchan *ctlr)
{
	return (dcrget(Dpixf) & ctlr->txbit) == 0;	/* not <= 7, so it's full */
}

static void
txstart(Tchan *t)
{
	Block *b;
	int size, psize;
	uchar *data;
	u32int hdr, r;
	ulong start;

	while(qlen(t->oq) > 0){
		if(txfifofull(t)){
			/* give it a chance to empty, in case we can save an interrupt */
			start = perfticks();
			for(r = 0; txfifofull(t) && r < 10000; r++)
				{}
			t->waitempty += r;
			t->timeempty += perfticks() - start;
			if(txfifofull(t)){
				/* give up until there's space */
				dcrput(Dpixen, dcrget(Dpixen) | t->txbit);
				return;
			}
		}
		b = qget(t->oq);
		if(b == nil)
			break;
		size = BLEN(b)-4;
		if(size <= 0){
			freeb(b);
			continue;
		}
		hdr = (b->rp[0]<<24)|(b->rp[1]<<16)|(b->rp[2]<<8)|b->rp[3];
		b->rp += 4;
		if(size < PayloadSize || !ALIGNED(b->rp, 16)){
			/* need the bounce buffer */
			data = t->quads;
			if(size < PayloadSize){
				psize = PayloadSize-size;
				memset(data+size, 0, psize);
				data[PayloadSize-1] = psize;
				/* TO DO: set a bit in the header to mark a short packet */
			}else if(size > PayloadSize)
				size = PayloadSize;
			memmove(data, b->rp, size);
			t->badalign++;
		}else
			data = b->rp;
		/*
		 *  the book says order of writing data, hdr is immaterial. That said, I'm having
		 * better luck writing data first, then hdr
		 */
		quad2fifo(&REG(t, Mpid), data, Nquads);
		REG(t, Mpih) = hdr;
		freeb(b);
		t->lastheader = hdr;
		t->ntx++;
	}
	/* clear request for interrupt on low watermark */
	r = dcrget(Dpixen);
	if(r & t->txbit)
		dcrput(Dpixen, r & ~t->txbit);
}

static void chanreceive(Tchan*);

static void
rxkick(void *a)
{
	Tchan *t;

	t = a;
	ilock(t);
	dcrput(Dprxen, dcrget(Dprxen) | t->rxbit);	/* TO DO: this should probably go elsewhere */
	t->nrxkick++;
	iunlock(t);
	chanreceive(t);
}

static void
chanreceive(Tchan *t)
{
	u8int len;
	u32int hdr;
	Block *b;

	ilock(t);
	t->nrxintr++;

	while(!rxfifoempty(t) && !qfull(t->iq)){
		b = iallocb(PayloadSize + 16);	/* b->wp will be quad-aligned */
		hdr = REG(t, Mprh);
		b->wp += 12;		/* skip over these to keep payload data aligned after 4 byte header */
		b->rp = b->wp;
		b->wp[0] = hdr>>24;
		b->wp[1] = hdr>>16;
		b->wp[2] = hdr>>8;
		b->wp[3] = hdr;
		fifo2quad(b->wp+4, &REG(t, Mprd), Nquads);	/* it's quad aligned */
		b->wp += PayloadSize+4;
		if(t->n == 1){ 				/* IP network */
			qpassnolim(t->iq, b);
		}
		else if(sys->ionode){
			/* null terminated for now. Later on, we *could* make b->rp[0] the len */
			/* we don't need the header that tells us it is for us */
			b->rp += 4;
			len = strlen((char*)b->rp);
			b->wp = b->rp + len;
			qpassnolim(t->iq, b);
		}
		else{ /* we are a CPU node, it's from an IO node */
			/* we don't need the header that tells us it is for us */
			b->rp += 4;
			len = strlen((char*)b->rp);
			b->wp = b->rp + len;
			//treedumpblock(b, "qnetC");
			qpassnolim(t->iq, b);
		}
		t->nrx++;
	}
	if(qfull(t->iq)){
		/* disable receive interrupt until kicked */
		dcrput(Dprxen, dcrget(Dprxen) & ~t->rxbit);
	}
	iunlock(t);
}

static void
rxinterrupt(Ureg*, void*)
{
	u32int r;

	r = dcrget(Dprxf);	/* clear on read */
//	iprint("rxinterrupt r %#8.8ux mark %ux %ux\n", r, RxVc0mark, RxVc1mark);
	if(r & RxVc0mark)
		chanreceive(&treechan[0]);
	if(r & RxVc1mark)
		chanreceive(&treechan[1]);
	if(r & RxHwErrors)
		iprint("tree rx hw err: %#8.8ux\n", r & RxHwErrors);
	if(r & RxSwErrors)
		iprint("tree rx sw err: %#8.8ux\n", r & RxSwErrors);
}

static void
txinterrupt(Ureg*, void*)
{
	Tchan *t;
	u32int r;
	int i;

	r = dcrget(Dpixf);	/* clear on read */
//	dcrput(Dpixf, InjectionEnable);
	for(i=0; i<Nvc; i++){
		t = &treechan[i];
		if(r & t->txbit){
			ilock(t);
			t->ntxintr++;
			txstart(t);
			iunlock(t);
		}
	}
	if(r & TxHwErrors)
		iprint("tree tx hw err: %#8.8ux\n", r & TxHwErrors);
	if(r & TxSwErrors)
		iprint("tree tx sw err: %#8.8ux\n", r & TxSwErrors);
	if((r & InjectionEnable) == 0)
		panic("tree: injection disabled: pixf=%#ux pixen=%#ux prxen=%#ux txbit=%#ux rxbit=%#ux",
			r, dcrget(Dpixen), dcrget(Dprxen), treechan[1].txbit, treechan[1].rxbit);
}

static void
txkick(void *v)
{
	Tchan *t = v;

	ilock(t);
	t->ntxkick++;
	txstart(t);
	iunlock(t);
}

static void
errorinterrupt(Ureg *ur, void*)
{
	iprint("tree err irq: %ld(%s)\n", ur->cause-VectorTree0,
					irqnames[ur->cause-VectorTree0]);
}

static void
treechaninit(int i, u32int addr, int ionode)
{
	Tchan *t;
	u32int vcfg;

	t = &treechan[i];

	/* set reception watermark to 0 (interrupt on payload);
	   injection watermark to <=6 payloads (ie, at least one more will fit);
	   injection checksum exclusion to 0 */
	vcfg = IF(0, 8, 8) | IF(0, 21, 3) | IF(6, 29, 3);

	switch(i){
	default:
		panic("treechaninit %d\n", i);
		return;
	case 0:
		/*
		 * Mapped at startup via tlbstatic[].
		t->regs = kmapphys(VIRTTREE0, PHYSTREE0, 1*KiB, TLBI|TLBG|TLBWR);
		 */
		t->regs = UINT2PTR(VIRTTREE0);
		t->txbit = TxVc0mark;
		t->rxbit = RxVc0mark;
		if(ionode)
			vcfg |= ReceiveAll;
		t->n = 0;
		break;
	case 1:
		/*
		 * Mapped at startup via tlbstatic[].
		t->regs = kmapphys(VIRTTREE1, PHYSTREE1, 1*KiB, TLBI|TLBG|TLBWR);
		 */
		t->regs = UINT2PTR(VIRTTREE1);
;
		t->txbit = TxVc1mark;
		t->rxbit = RxVc1mark;
		t->n = 1;
		break;
	}
	dcrput(Dvcfg0+i, vcfg);

	t->quads = mallocalign(PayloadSize, 16, 0, 0);
	t->iq = qopen(1024*1024, Qmsg, rxkick, t);
	t->oq = qopen(256*1024, Qmsg|Qkick, txkick, t);
	t->p2paddr = addr;
}

static	Lock	treeinitlock;
static	int	treeinited;

void
treenetreset(void)
{
	int i;
	u16int *routes;
	u32int config, p2paddr, rcfg;

	lock(&treeinitlock);
	if(treeinited){
		unlock(&treeinitlock);
		return;
	}
	treeinited = 1;
	unlock(&treeinitlock);

	/*
	 * on bg/p we assume the jolly cns has set up and trained
	 * the collective network links for us
	 */

	p2paddr = sys->myp2paddr;

	dcrput(Dnaddr, p2paddr);

	/* clear injection checksums */
	dcrput(Dcspy0, 0);
	dcrput(Dcshd0, 0);
	dcrput(Dcshd1, 0);
	dcrput(Dcspy1, 0);

	/* set link idle pattern */
	dcrput(Disra, 0xabcdabcd);
	dcrput(Disrb, 0xabcdabcd);

	rcfg = Timeout0;

	config = personality->Kernel_Config.NodeConfig;
	if(config & BGP_PERS_ENABLE_LoopBack){
		/* enable loop back */
		rcfg |= C0loop | C1loop | C2loop;
	}else{
		if(!(config & BGP_PERS_ENABLE_TreeA))
			rcfg |= Src00dis|Tar00dis|Src01dis|Tar01dis;
		if(!(config & BGP_PERS_ENABLE_TreeB))
			rcfg |= Src10dis|Tar10dis|Src11dis|Tar11dis;
		if(!(config & BGP_PERS_ENABLE_TreeC))
			rcfg |= Src20dis|Tar20dis|Src21dis|Tar21dis;
	}
	dcrput(Drcfg, rcfg);

	/* load class routes */
	routes = personality->Network_Config.TreeRoutes;
	for(i = 0; i < nelem(personality->Network_Config.TreeRoutes); i += 2)
		dcrput(Drdr0+i, (routes[i]<<16)|routes[i+1]);

	treechaninit(0, p2paddr, sys->ionode);
	treechaninit(1, p2paddr, sys->ionode);

	/* print("Tree: p2paddr %#8.8ux\n", p2paddr); */
}


void
treenetinit(void)
{
	int i;
	void (*handler)(Ureg*, void*);
	char name[KNAMELEN];

	for(i = 0; i < Nirq; i++){
		snprint(name, sizeof(name), "tree.%d", i);
		switch(i){
		case IrqTxE:
		case IrqTx:
			handler = txinterrupt;
			break;
		case IrqRxE:
		case IrqRx:
			handler = rxinterrupt;
			break;
		default:
			handler = errorinterrupt;
			break;
		}
		intrenable(VectorTree0+i, handler, nil, name, 0);
	}

	/* ignore the interrupt-packet interrupts (in the critical interrupt section): we'll find them soon enough */

	/* injection interrupts: enable all */
	dcrget(Dpixf);	/* read and clear flags */
//	dcrput(Dpixf, InjectionEnable);
	dcrput(Dpixen, TxHwErrors | TxSwErrors /*| InjectionEnable*/);

	/* reception interrupt state: interrupt when packet arrives in FIFO */
	dcrget(Dprxf);
	dcrput(Dprxen, RxHwErrors | RxSwErrors | RxVc0mark | RxVc1mark);
}

long
treenetsend(int vc, Block *b)
{
	return qbwrite(treechan[vc].oq, b);
}

Block*
treenetbread(int vc, long n)
{
	return qbread(treechan[vc].iq, n);
}

long
treenetread(int vc, void *a, long n)
{
	return qread(treechan[vc].iq, a, n);
}

long
treenetstats(int vc, char *buf, long n)
{
	return treestats(vc, buf, n);
}

Block*
treep2p(int class, int p2p)
{
	u32int h;
	Block *b;

	h = MKP2P(class, p2p) | PIH_CSUM_NONE;
	b = allocb(PayloadSize + 16);
	if(b == nil)
		return nil;
	b->wp += 12;
	b->rp = b->wp;
	b->wp[0] = h>>24;
	b->wp[1] = h>>16;
	b->wp[2] = h>>8;
	b->wp[3] = h;
	b->wp += 4;
	return b;
}

Block*
treetag(int class, int intr, int tag, int op)
{
	u32int h;
	Block *b;

	h = MKTAG(class, intr, tag, op) | PIH_CSUM_NONE;
	b = allocb(PayloadSize + 16);
	if(b == nil)
		return nil;
	b->wp += 12;
	b->rp = b->wp;
	b->wp[0] = h>>24;
	b->wp[1] = h>>16;
	b->wp[2] = h>>8;
	b->wp[3] = h;
	b->wp += 4;
	return b;
}

void
treedumpblock(Block *b, char *s)
{
	int i, n;

	n = BLEN(b);
	print("%s: %#p [%d]", s, b, n);
	if(n > 16)
		n = 16;
	for(i=0; i<n; i++)
		print(" %.2ux", b->rp[i]);
	print("\n");
}

int
treenetroutes(int, u32int *a, int n)
{
	int i;

	if(n > 8)
		n = 8;
	for(i = 0; i < n; i++)
		a[i] = dcrget(Drdr0+i);
	return n;
}

void
treekick(int vc)
{
	chanreceive(&treechan[vc]);
	txkick(&treechan[vc]);
}
