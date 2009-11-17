/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: TCP/IP Offload Memory Access Layer (TOMAL).
 *
 * All rights reserved
 *
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "io.h"

#define ConfigurationCtrl		0x0000
#define RevisionID			0x0060
#define ConsumerMemoryBaseAddr		0x0200
#define PacketDataEngineCtrl		0x0400
#define TxNotificationCtrl		0x0600

#define TxMinTimer(c)			(0x0610+(c)*0x100)
#define TxMaxTimer(c)			(0x0620+(c)*0x100)
#define TxFramePerServiceCtrl(c)	(0x0650+(c)*0x100)
#define TxHWCurrentDescriptorAddrH(c)	(0x0660+(c)*0x100)
#define TxHWCurrentDescriptorAddrL(c)	(0x0670+(c)*0x100)
#define TxPendingFrameCount(c)		(0x0690+(c)*0x100)
#define TxAddPostedFrames(c)		(0x06a0+(c)*0x100)
#define TxNumberOfTransmittedFrames(c)	(0x06b0+(c)*0x100)
#define TxMaxFrameNum(c)		(0x06c0+(c)*0x100)
#define TxMinFrameNum(c)		(0x06d0+(c)*0x100)
#define TxEventStatus(c)		(0x06e0+(c)*0x100)
#define TxEventMask(c)			(0x06f0+(c)*0x100)

#define RxNotificationCtrl		0x0f00
#define RxMinTimer(c)			(0x0f10+(c)*0x100)
#define RxMaxTimer(c)			(0x0f20+(c)*0x100)
#define RxHWCurrentDescriptorAddrH(c)	(0x1020+(c)*0x100)
#define RxHWCurrentDescriptorAddrL(c)	(0x1030+(c)*0x100)
#define RxAddFreeBytes(c)		(0x1040+(c)*0x100)
#define RxTotalBufferSize(c)		(0x1050+(c)*0x100)
#define RxNumberOfReceivedFrames(c)	(0x1060+(c)*0x100)
#define RxDroppedFramesCount(c)		(0x1070+(c)*0x100)
#define RxMaxFrameNum(c)		(0x1080+(c)*0x100)
#define RxMinFrameNum(c)		(0x1090+(c)*0x100)
#define RxEventStatus(c)		(0x10a0+(c)*0x100)
#define RxEventMask(c)			(0x10b0+(c)*0x100)

#define SwNonCriticalErrorsStatus(c)	(0x1800+(c)*0x030)
#define SwNonCriticalErrorsEnable(c)	(0x1810+(c)*0x030)
#define SwNonCriticalErrorsMask(c)	(0x1820+(c)*0x030)

#define RxDataBufferFreeSpace		0x1900
#define TxDataBufferFreeSpace(c)	(0x1910+(c)*0x010)

#define RxMACStatus(c)			(0x1b20+(c)*0x080)
#define RxMACStatusEnable(c)		(0x1b30+(c)*0x080)
#define RxMACStatusMask(c)		(0x1b40+(c)*0x080)
#define TxMACStatus(c)			(0x1b50+(c)*0x080)
#define TxMACStatusEnable(c)		(0x1b60+(c)*0x080)
#define TxMACStatusMask(c)		(0x1b70+(c)*0x080)

#define HwErrorsStatus			0x1e00
#define HwErrorsEnable			0x1e10
#define HwErrorsMask			0x1e20

#define SwCriticalErrorsStatus		0x1f00
#define SwCriticalErrorsEnable		0x1f10
#define SwCriticalErrorsMask		0x1f20
#define RxDescriptorBadCodeFEC		0x1f30
#define TxDescriptorBadCodeFEC		0x1f40
#define InterruptStatus			0x1f80
#define InterruptRoute			0x1f90

#define RxMACBadStatusCounter(c)	(0x2060+(c)*0x100)

#define DebugVectorsCtrl		0x3000
#define DebugVectorsReadData		0x3010

enum {						/* ConfigurationCtrl */
	Sr		= 0x00000001,		/* Soft Reset */
	PLB250		= 0x00000000,		/* Max. value of PLB clock */
	TxMAC1		= 0x00100000,		/* Handle Tx on MAC1 */
	TxMAC0		= 0x00200000,		/* Handle Tx on MAC0 */
	RxMAC1		= 0x00400000,		/* Hadle Rx on MAC1 */
	RxMAC0		= 0x00800000,		/* Handle Rx on MAC0 */
};

enum {						/* PacketDataEngineCtrl */
	TxPrefetch1	= 0x00000000,		/* Descriptor Prefetch Count */
	TxPrefetch8	= 0x00000003,
	RxPrefetch1	= 0x00000000,
	RxPrefetch8	= 0x00000030,
};

enum {						/* TxNotificationCtrl */
	TxCounterStart1	= 0x00000010,
	TxCounterStart0	= 0x00000020,
};

enum {						/* (T|R)Event(S|M) */
	Event		= 0x00000001,
};

enum {						/* RxNotificationCtrl */
	RxCounterStart1	= 0x00000040,
	RxCounterStart0	= 0x00000080,
};

enum {						/* SwNonCriticalErrors(S|E|M) */
	RxTooShortDB	= 0x0000001,		/* Rx Too Short Data Buffer */
	TxPDBadCommand	= 0x0000010,		/* Tx PdCode Bad Command */

	SwNonCeMASK	= TxPDBadCommand|RxTooShortDB,
};

enum {						/* RxMACStatus(E|M) */
	RxInRangeError	= 0x00000001,		/* XEMAC+EMAC4 */
	RxOutRangeError	= 0x00000002,		/* XEMAC+EMAC4 */
	RxFrameTooLong	= 0x00000004,		/* XEMAC+EMAC4 */
	RxBadFCS	= 0x00000008,		/* XEMAC+EMAC4 */
	RxAlignError	= 0x00000010,		/* EMAC4 */
	RxShortEvent	= 0x00000020,		/* EMAC4 */
	RxRuntFrame	= 0x00000040,		/* XEMAC+EMAC4 */
	RxBadFrame	= 0x00000080,		/* XEMAC+EMAC4 */
	RxPauseFrame	= 0x00000100,		/* XEMAC+EMAC4 */
	RxOverrun	= 0x00000200,		/* XEMAC+EMAC4 */
	RxParityError	= 0x00000400,		/* XEMAC+EMAC4 */
	RxCodeError	= 0x00001000,		/* XEMAC */

	RxMACMASK	= RxCodeError|RxParityError|RxOverrun|RxPauseFrame
			 |RxBadFrame|RxRuntFrame|RxBadFCS|RxFrameTooLong
			 |RxOutRangeError|RxInRangeError,
};

enum {						/* TxMACStatus(E|M) */
	TxSqe		= 0x00000001,		/* EMAC4 */
	TxUnderrun	= 0x00000002,		/* XEMAC+EMAC4 */
	TxLateCollision	= 0x00000010,		/* EMAC4 */
	TxExcCollision	= 0x00000020,		/* EMAC4 */
	TxExcDeferral	= 0x00000040,		/* EMAC4 */
	TxLostCarrier	= 0x00000080,		/* EMAC4 */
	TxParityError	= 0x00000100,		/* XEMAC */
	TxBadFCS	= 0x00000200,		/* EMAC4 */
	TxRemoteFault	= 0x00000800,		/* XEMAC */
	TxLocalFault	= 0x00001000,		/* XEMAC */

	TxMACMASK	= TxLocalFault|TxRemoteFault|TxParityError|TxUnderrun,
};

enum {						/* HwErrors(S|E|M) */
	OutDBPE		= 0x0000001,		/* OutBound Data Buffer PE */
	InDBPE		= 0x0000002,		/* InBound Data Buffer PE */
	OutRAPE		= 0x0000004,		/* OutBound Register Array PE */
	InRAPE		= 0x0000008,		/* InBound Register Array PE */

	HwEMASK	= InRAPE|OutRAPE|InDBPE|OutDBPE,
};

enum {						/* SwCriticalErrors(S|E) */
	RxDBadCode	= 0x0000001,		/* Rx Descriptor Bad Code */
	TxDBadCode	= 0x0000002,		/* Tx Descriptor Bad Code */

	SwCeMASK	= TxDBadCode|RxDBadCode,
};

enum {						/* Interrupt(S|Route) */
	Ce		= 0x00000001,		/* Critical Error */
	Sw0NonCe	= 0x00000002,		/* Software NonCe */
	Sw1NonCe	= 0x00000004,		/* Software NonCe */
	PPLBPe		= 0x00000008,		/* PLB Parity Error */
	RxMAC0e		= 0x00000010,		/* MAC0 Rx Error */
	RxMAC1e		= 0x00000020,
	TxMAC0e		= 0x00000040,		/* MAC0 Tx Error */
	TxMAC1e		= 0x00000080,
	Rx0i		= 0x00000100,		/* Channel 0 Rx Interrupt */
	Rx1i		= 0x00000200,
	Tx0i		= 0x00010000,		/* Channel 0 Tx Interrupt */
	Tx1i		= 0x00020000,
};

typedef struct Toe {
	uintptr	reg;

	Mal	mal[2];				/* each path has 2 channels */
} Toe;

static Toe toemal;				/* There is but one */

#define reg32p(t, r)	((u32int*)((t)->reg+(r)))
#define reg32r(t, r)	(*reg32p(t, r))
#define reg32a(t, r)	{ u32int dummy = reg32r((t), (r)); USED(dummy); }
#define reg32w(t, r, v)	(*reg32p(t, r) = (v))

static int
toereset(Toe* toe)
{
	u32int r;
	int timeo;

	if(toe == nil || toe->reg == 0)
		return -1;

	/*
	 * Soft reset.
	 */
	reg32w(toe, ConfigurationCtrl, Sr);
	for(timeo = r = 0; timeo < 100; timeo++){	/* SET(r) */
		r = reg32r(toe, ConfigurationCtrl);
		if(!(r & Sr))
			break;
		microdelay(100);
	}
	if(r & Sr)
		return -1;

	return 0;
}

static int
toesetup(Toe* toe)
{
	if(toe == nil || toe->reg == 0)
		return -1;
	if(toereset(toe) < 0)
		return -1;

	/*
	 * General configuration:
	 *	enable processing of Rx and Tx on MAC[01];
	 *	set maximum value of the PLB_CLOCK;
	 *	set 16MSb of all PLB addresses;
	 *	prefetch 1 descriptor;
	 *	route Channel 1 IRQs to TOE_PLB_INT[1].
	 * To do:
	 *	check soft reset leaves channel 1 state OK
	 *	(only use channel 0).
	 */
	reg32w(toe, ConfigurationCtrl, RxMAC0|RxMAC1|TxMAC0|TxMAC1|PLB250);
	reg32w(toe, ConsumerMemoryBaseAddr, 0);
	reg32w(toe, PacketDataEngineCtrl, RxPrefetch1|TxPrefetch1);
	reg32w(toe, InterruptRoute, Tx1i|Rx1i|TxMAC1e|Sw1NonCe);

	return 0;
}

int
toenpn(void)
{
	Toe *toe;

	if((toe = &toemal)->reg == 0)
		return -1;

	toereset(toe);
	kunmapphys(UINT2PTR(VIRTTOMAL));
	toe->reg = 0;

	return 0;
}

int
toepnp(void)
{
	void* v;
	Toe *toe;

	/*
	 */
	if((toe = &toemal)->reg != 0)
		return -1;
	if((v = kmapphys(VIRTTOMAL, PHYSTOMAL, 16*KiB, TLBI|TLBG|TLBWR)) == nil)
		return -1;
	toe->reg = PTR2UINT(v);

	if(toesetup(toe) < 0){
		kunmapphys(UINT2PTR(VIRTTOMAL));
		toe->reg = 0;

		return -1;
	}

	return 0;
}

static void
malinterrupt(Ureg*, void* arg)
{
	Mal* mal;
	int chno;
	u32int r, s;

	mal = arg;
	chno = mal->chno;

	s = reg32r(mal, InterruptStatus);
	if(s & Ce){
		if((r = reg32r(mal, HwErrorsStatus)) != 0)
			reg32w(mal, HwErrorsStatus, r);
		if((r = reg32r(mal, SwCriticalErrorsStatus)) != 0)
			reg32w(mal, SwCriticalErrorsStatus, r);
		s &= ~Ce;
	}
	if(s & Sw0NonCe){
		if((r = reg32r(mal, SwNonCriticalErrorsStatus(chno))) != 0)
			reg32w(mal, SwNonCriticalErrorsStatus(chno), r);
		s &= ~Sw0NonCe;
	}
	if(s & RxMAC0e){
		if((r = reg32r(mal, RxMACStatus(chno))) != 0)
			reg32w(mal, RxMACStatus(chno), r);
		s &= ~RxMAC0e;
	}
	if(s & Rx0i){
		reg32w(mal, RxEventStatus(chno), 1);
		s &= ~Rx0i;

		{ extern void xemacrxintrxxx(void*, u32int);
		r = reg32r(mal, RxNumberOfReceivedFrames(chno));
		xemacrxintrxxx(mal->ctlr, r);
		}
		reg32w(mal, RxNotificationCtrl, RxCounterStart0);
	}
	if(s & Tx0i){
		r = reg32r(mal, TxNumberOfTransmittedFrames(chno));
		reg32w(mal, TxEventStatus(chno), 1);

		{ extern void xemactxstartxxx(void*, int);
		xemactxstartxxx(mal->ctlr, r);
		}
		s &= ~Tx0i;
	}
	if(s != 0)
		panic("mal[%d]interrupt %#8.8ux\n", chno, s);
}

int
malnpn(Mal* mal)
{
	/*
	 * To be dooby do.
	 */
	mal->ctlr = nil;

	return 0;
}

Mal*
malpnp(int chno, int nrd, int ntd, int fl)
{
	Dd *dd;
	Toe *toe;
	Mal *mal;
	u64int addr;

	if(chno != 0)
		return nil;

	toe = &toemal;
	mal = &toe->mal[chno];
	mal->reg = toe->reg;
	mal->chno = chno;
	mal->fl = fl;

	/*
	 * Device descriptor allocation.
	 */
	if(mal->dd != nil){
		free(mal->dd);
		mal->dd = nil;
	}
	if((dd = mallocalign((nrd+ntd)*sizeof(Dd), 64, 0, 0)) == nil)
		return nil;
	memset(dd, 0, (nrd+ntd)*sizeof(Dd));
	if(mal->rb != nil){
		free(mal->rb);
		mal->rb = nil;
	}
	if((mal->rb = malloc((nrd+ntd)*sizeof(Block*))) == nil){
		free(dd);
		return nil;
	}
	memset(mal->rb, 0, (nrd+ntd)*sizeof(Block*));

	mal->dd = dd;
	mal->rd = mal->dd;
	mal->nrd = nrd;
	mal->td = mal->rd+nrd;
	mal->tb = mal->rb+nrd;
	mal->ntd = ntd;

	/*
	 * Tx.
	 * initialise descriptors into a ring - td[ntd-1] becomes
	 * a Branch descriptor then is never touched, the ring is
	 * effectively ntd-1 in circumference;
	 * initialise the hardware to point to the descriptor ring;
	 * set type of MAC (XEMAC);
	 * configure interrupt generation registers;
	 * enable the interrupt generation mechanism (a write
	 * makes any changes effective).
	 * To do:
	 *	figure out the timer/frame interrupt values.
	 */
	dd = &mal->td[ntd-1];
	dd->code = BdCode;
	dd->address = PADDR(mal->td);

	addr = PADDR(mal->td);
	reg32w(toe, TxHWCurrentDescriptorAddrH(chno), addr>>32);
	reg32w(toe, TxHWCurrentDescriptorAddrL(chno), addr & 0xffffffff);

	reg32w(toe, TxMACStatusMask(chno), TxMACMASK);

	reg32w(toe, TxMinTimer(chno), 8);
	reg32w(toe, TxFramePerServiceCtrl(chno), 4);
	reg32w(toe, TxMaxTimer(chno), 100);
	reg32w(toe, TxMaxFrameNum(chno), mal->ntd);
	reg32w(toe, TxMinFrameNum(chno), 32);

	reg32w(toe, TxEventMask(chno), Event);

	/*
	 * Rx.
	 * Similar to Tx above.
	 */
	dd = &mal->rd[nrd-1];
	dd->code = BdCode;
	dd->address = PADDR(mal->rd);

	addr = PADDR(mal->rd);
	reg32w(toe, RxHWCurrentDescriptorAddrH(chno), addr>>32);
	reg32w(toe, RxHWCurrentDescriptorAddrL(chno), addr & 0xffffffff);

	reg32w(toe, RxMACStatusMask(chno), RxMACMASK);

	reg32w(toe, RxMinTimer(chno), 8);
	reg32w(toe, RxMaxTimer(chno), 100);
	reg32w(toe, RxMinFrameNum(chno), 32);
	reg32w(toe, RxMaxFrameNum(chno), 255);

	reg32w(mal, RxEventMask(chno), Event);

	/*
	 * The remainder of the interrupt setup:
	 * interrupt linkage;
	 * hardware, and software events and errors;
	 * enable Rx/Tx counters.
	 */
	if(chno == 0)
		intrenable(VectorTOMAL0, malinterrupt, mal, "TOMAL0", 0);
	else
		intrenable(VectorTOMAL1, malinterrupt, mal, "TOMAL1", 0);

	reg32w(mal, SwNonCriticalErrorsEnable(chno), SwNonCeMASK);
	reg32w(mal, SwNonCriticalErrorsMask(chno), SwNonCeMASK);

	reg32w(mal, HwErrorsEnable, HwEMASK);
	reg32w(mal, HwErrorsMask, HwEMASK);

	reg32w(mal, SwCriticalErrorsEnable, SwCeMASK);
	reg32w(mal, SwCriticalErrorsMask, SwCeMASK);

	if(chno == 0){
		reg32w(mal, TxNotificationCtrl, TxCounterStart0);
		reg32w(mal, RxNotificationCtrl, RxCounterStart0);
	}
	else{
		reg32w(mal, TxNotificationCtrl, TxCounterStart1);
		reg32w(mal, RxNotificationCtrl, RxCounterStart1);
	}

	reg32w(mal, RxEventMask(chno), Event);
	reg32w(mal, TxEventMask(chno), Event);

	return mal;
}

static Lock malblock;
static Block* malbpool;

static void
malfreeb(Block* b)
{
	b->wp = b->rp = (uchar*)ROUNDUP(PTR2UINT(b->base), 64);

	ilock(&malblock);
	b->next = malbpool;
	malbpool = b;
	iunlock(&malblock);
}

Block*
malallocb(Mal* mal)
{
	Dd *dd;
	Block *b, *r;

	r = mal->rb[mal->ri];
	dd = &mal->rd[mal->ri];

	ilock(&malblock);
	if((b = malbpool) != nil){
		malbpool = b->next;
		b->next = nil;
	}
	iunlock(&malblock);

	if(b != nil){
		if(r != nil)
			r->wp += dd->length;
		mal->rb[mal->ri] = b;
	}
	else{
		b = r;
		r = nil;
	}

	mal->ri++;
	if(mal->ri == mal->nrd-1)
		mal->ri = 0;
	dd->address = PADDR(b->rp);
	dd->length = 0;
	dd->status = 0;
	dd->posted = mal->fl;
	dd->command = 0;
	mb();
	dd->code = PdCode;

	reg32w(mal, RxAddFreeBytes(mal->chno), mal->fl);

	return r;
}

int
malattach(Mal* mal)
{
	Block *b;
	int chno, i;

	/*
	 * The descriptor rings are already allocated
	 * via malpnp, remains to allocate the receive
	 * blocks and initialise the descriptors.
	 * Remember the rings are 1 less due to the Branch
	 * descriptor already initialised at the end.
	 * To do: real buffer management, not just dumb
	 * ialloc.
	 */
	mal->ri = 0;
	for(i = 0; i < mal->nrd*2; i++){
		if((b = iallocb(mal->fl)) == nil)
			break;
		b->free = malfreeb;
		malfreeb(b);
	}
	for(i = 0; i < mal->nrd-1; i++)
		malallocb(mal);

	/*
	 * Enable interrupt generation for the MAC.
	 */
	chno = mal->chno;
	reg32w(mal, RxMACStatusEnable(chno), RxMACMASK);
	reg32w(mal, TxMACStatusEnable(chno), TxMACMASK);
	if(chno == 0){
		reg32w(mal, TxNotificationCtrl, TxCounterStart0);
		reg32w(mal, RxNotificationCtrl, RxCounterStart0);
	}
	else{
		reg32w(mal, TxNotificationCtrl, TxCounterStart1);
		reg32w(mal, RxNotificationCtrl, RxCounterStart1);
	}

	return 0;
}

void
maltxstart(Mal* mal)
{
	if(mal->chno == 0)
		reg32w(mal, TxNotificationCtrl, TxCounterStart0);
	else
		reg32w(mal, TxNotificationCtrl, TxCounterStart1);
	reg32w(mal, TxAddPostedFrames(mal->chno), 1);
}

int
maldetach(Mal* mal)
{
	int chno;

	chno = mal->chno;
	if(chno == 1)
		intrdisable(VectorTOMAL1, malinterrupt, mal, "TOMAL1");
	else
		intrdisable(VectorTOMAL0, malinterrupt, mal, "TOMAL0");

	reg32w(mal, RxEventMask(chno), 0);
	reg32w(mal, TxEventMask(chno), 0);

	reg32w(mal, SwCriticalErrorsEnable, 0);
	reg32w(mal, SwCriticalErrorsMask, 0);
	reg32w(mal, SwNonCriticalErrorsEnable(chno), 0);

	reg32w(mal, HwErrorsEnable, 0);
	reg32w(mal, HwErrorsMask, 0);
	reg32w(mal, SwNonCriticalErrorsMask(chno), 0);

	reg32w(mal, RxMACStatusEnable(chno), 0);
	reg32w(mal, TxMACStatusEnable(chno), 0);
	reg32w(mal, TxMACStatusMask(chno), 0);

	/*
	 * To be dooby do: deallocate buffers, etc.
	 */

	return 0;
}

typedef struct Regs {
	char*	name;
	int	offset;
} Regs;

static Regs toeregs[] = {			/* Tomal + Mal[0] Registers */
	"ConfigurationCtrl",		0x0000,
	"RevisionID",			0x0060,
	"ConsumerMemoryBaseAddr",	0x0200,
	"PacketDataEngineCtrl",		0x0400,
	"TxNotificationCtrl",		0x0600,

	"TxMinTimer",			0x0610,
	"TxMaxTimer",			0x0620,
	"TxFramePerServiceCtrl",	0x0650,
	"TxHWCurrentDescriptorAddrH",	0x0660,
	"TxHWCurrentDescriptorAddrL",	0x0670,
	"TxPendingFrameCount",		0x0690,
	"TxAddPostedFrames",		0x06a0,
	"TxNumberOfTransmittedFrames",	0x06b0,
	"TxMaxFrameNum",		0x06c0,
	"TxMinFrameNum",		0x06d0,
	"TxEventStatus",		0x06e0,
	"TxEventMask",			0x06f0,

	"RxNotificationCtrl",		0x0f00,
	"RxMinTimer",			0x0f10,
	"RxMaxTimer",			0x0f20,
	"RxHWCurrentDescriptorAddrH",	0x1020,
	"RxHWCurrentDescriptorAddrL",	0x1030,
	"RxAddFreeBytes",		0x1040,
	"RxTotalBufferSize",		0x1050,
	"RxDroppedFramesCount",		0x1070,
	"RxMaxFrameNum",		0x1080,
	"RxMinFrameNum",		0x1090,
	"RxEventStatus",		0x10a0,
	"RxEventMask",			0x10b0,

	"SwNonCriticalErrorsStatus",	0x1800,
	"SwNonCriticalErrorsEnable",	0x1810,
	"SwNonCriticalErrorsMask",	0x1820,

	"RxDataBufferFreeSpace",	0x1900,
	"TxDataBufferFreeSpace",	0x1910,

	"RxMACStatus",			0x1b20,
	"RxMACStatusEnable",		0x1b30,
	"RxMACStatusMask",		0x1b40,
	"TxMACStatus",			0x1b50,
	"TxMACStatusEnable",		0x1b60,
	"TxMACStatusMask",		0x1b70,

	"HwErrorsStatus",		0x1e00,
	"HwErrorsEnable",		0x1e10,
	"HwErrorsMask",			0x1e20,

	"SwCriticalErrorsStatus",	0x1f00,
	"SwCriticalErrorsEnable",	0x1f10,
	"SwCriticalErrorsMask",		0x1f20,
	"RxDescriptorBadCodeFEC",	0x1f30,
	"TxDescriptorBadCodeFEC",	0x1f40,
	"InterruptStatus",		0x1f80,
	"InterruptRoute",		0x1f90,
	nil,				0,
};

void
toemal0dump(void)
{
	Regs *r;
	Toe *toe;

	toe = &toemal;
	for(r = toeregs; r->name != nil; r++)
		print("%s: %#8.8ux\n", r->name, reg32r(toe, r->offset));
}
