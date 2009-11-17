/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: BG/P 10 gbit ethernet xemac driver
 *
 * Based on ppc440/etheremac44x.c 
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

#include "../port/ethermii.h"
#include "../port/netif.h"

#include "etherif.h"
#include "io.h"

enum {						/* Configuration Registers */
	M0		= 0x00,			/* Mode 0 */
	M1		= 0x04,
	Tm0		= 0x08,			/* Transmit Mode 0 */
	Tm1		= 0x0c,
	Rm		= 0x10,			/* Receive Mode */
	Is		= 0x14,			/* Interrupt Status */
	Ise		= 0x18,			/* Is Enable */
	Iahi		= 0x1c,			/* Individual Address */
	Ialo		= 0x20,
	VTPID		= 0x24,			/* VLAN TPID */
	VTCI		= 0x28,			/* VLAN TCI */
	Pt		= 0x2c,			/* Pause Timer */
	Iaht		= 0x30,			/* Ia Hash Table */
	Gaht		= 0x40,			/* Group Address Hash Table */
	Lsahi		= 0x50,			/* Last Source Address */
	Lsalo		= 0x54,
	Ipgv		= 0x58,			/* Inter-Packet Gap Value */
	STA		= 0x5c,			/* STA Control */
	Trt		= 0x60,			/* Tx Request Threshold  */
	Rwm		= 0x64,			/* Rx High/Low WaterMark */
	SOPcm		= 0x68,			/* SOP Command Mode */
	Siahi		= 0x6c,			/* Secondary Ia */
	Sialo		= 0x70,
	To		= 0x74,			/* Tx Octets */
	Ro		= 0x7c,			/* Rx Octets */
	Rid		= 0x84,			/* Revision ID */
	Hd		= 0x88,			/* Hardware Debug */
};

enum {						/* M0 */
	Wuen		= 0x04000000,
	Rxen		= 0x08000000,
	Txen		= 0x10000000,
	Srst		= 0x20000000,
	Txidl		= 0x40000000,
	Rxidl		= 0x80000000,
};

enum {						/* M1 */
	OPB66MHz	= 0x00000008,
	OPB83MHz	= 0x00000010,
	OPB100MHz	= 0x00000018,
	OPB133MHz	= 0x00000020,
	Jben		= 0x00000800,
	Mws1		= 0x00001000,
	Trq		= 0x00008000,
	Tfs2K		= 0x00020000,
	Tfs4K		= 0x00030000,
	Tfs8K		= 0x00040000,
	Tfs16K		= 0x00050000,
	Tfs32K		= 0x00060000,
	Rfs2K		= 0x00100000,
	Rfs4K		= 0x00180000,
	Rfs8K		= 0x00200000,
	Rfs16K		= 0x00280000,
	Rfs32K		= 0x00300000,
	Rfs64K		= 0x00380000,
	Psen		= 0x08000000,
	Ifen		= 0x10000000,
	Vlen		= 0x20000000,
	Lpen		= 0x40000000,
	Sdr      	= 0x80000000,
};

enum {						/* Tm0 */
	Ftl2		= 0x00000001,		/* FIFO Threshold Limit */
	Ftl4		= 0x00000002,		/* (also Rm) */
	Ftl8		= 0x00000003,
	Ftl16		= 0x00000004,
	Ftl32		= 0x00000005,
	Ftl64		= 0x00000006,
	Ftl128		= 0x00000007,
	Gnp		= 0x80000000,		/* Get New Packet */
};

enum {						/* Rm */
	Siae		= 0x00020000,
	Pume		= 0x00040000,
	Mae		= 0x00080000,
	Bae		= 0x00100000,
	Miae		= 0x00200000,
	Iae		= 0x00400000,
	Pmme		= 0x00800000,
	Pme		= 0x01000000,
	Ppf		= 0x02000000,
	Arie		= 0x04000000,
	Lfd		= 0x08000000,
	Arfe		= 0x10000000,
	Arrf		= 0x20000000,
	Sfcs		= 0x40000000,
	Spad		= 0x80000000,
};

enum {						/* Is/Ise */
	MMAf		= 0x00000001,		/* MMA Operation Failed */
	MMAs		= 0x00000002,		/* MMA Operation Succeeded */
	Te		= 0x00000040,		/* Transmit Error */
	Db		= 0x00000100,		/* Dead Bit */
	Ire		= 0x00010000,		/* In Range Error */
	Ore		= 0x00020000,		/* Out of Range Error */
	Ftl		= 0x00040000,		/* Frame Too Long */
	BFCS		= 0x00080000,		/* Bad FCS */
	Lf		= 0x00200000,		/* Received with code Error */
	Rtf		= 0x00400000,		/* Runt Frame */
	Bdf		= 0x00800000,		/* Bad Frame */
	Psf		= 0x01000000,		/* Pause Frame */
	Ovr		= 0x02000000,		/* Overrun */
	Rffi		= 0x04000000,		/* Rx FIFO Almost Full */
	Tfei		= 0x08000000,		/* Tx FIFO Almost Full */
	Rxpe		= 0x10000000,		/* Rx Parity Error */
	Txpe		= 0x20000000,		/* Tx Parity Error */

	IsMASK		= Txpe|Rxpe|Ovr|Bdf|Lf|BFCS|Ftl|Ore|Ire|Te,
};

enum {						/* STA Control */
	MMAgo		= 0x00008000,		/* start operation */
	PHYerr		= 0x00004000,		/* PHY error */
	Ims		= 0x00002000,		/* Indirect Mode Selection */
	STAdr		= 0x00001000,		/* Direct Read */
	STAdw		= 0x00000800,		/* Direct Write */
	STAia		= Ims|0x00000000,	/* Indirect Address */
	STAiw		= Ims|0x00000800,	/* Indirect Write */
	STAir		= Ims|0x00001800,	/* Indirect Read */
	STAiri		= Ims|0x00001000,	/* Indirect Read Increment */
};

/*
 * There is numerology here concerning various chip alignment
 * constraints and the desire to reduce cache thrashing.
 * To Do: rationalise, describe.
 */
enum {						/* configuration */
	Rbsize		= 1*MiB,
	Flength		= ROUNDUP(9000+14+4+4, 64),

	Nrd		= (Rbsize/Flength) & ~1,
	Ntd		= 64,
};

typedef struct Xgmii Xgmii;
typedef struct Xgmii {
	u32int	reg[9];
} Xgmii;

typedef struct Ctlr Ctlr;
typedef struct Ctlr {
	uintptr	reg;
	Ether*	edev;
	int	port;
	uchar	ia[Eaddrlen];
	void*	xgmii;

	QLock	alock;
	int	attached;

	Mal*	mal;
	Mii*	mii;

	uint	intr;					/* statistics */
	uint	lintr;
	uint	lsleep;
	uint	rintr;
	uint	tintr;
} Ctlr;

#define reg32p(c, r)	((u32int*)((c)->reg+(r)))
#define reg32r(c, r)	(*reg32p(c, r))
#define reg32a(c, r)	{ u32int dummy = reg32r((c), (r)); USED(dummy); }
#define reg32w(c, r, v)	(*reg32p(c, r) = (v))

typedef struct Regs {
	char*	name;
	int	offset;
} Regs;

static Regs xemacregs[] = {			/* Configuration Registers */
	"M0",	M0,
	"M1",	M1,
	"Tm0",	Tm0,
	"Tm1",	Tm1,
	"Rm",	Rm,
	"Is",	Is,
	"Ise",	Ise,
	"Iahi",	Iahi,
	"Ialo",	Ialo,
	"VTPID",VTPID,
	"VTCI",	VTCI,
	"Pt",	Pt,
	"Iaht",	Iaht,
	"Gaht",	Gaht,
	"Lsahi",Lsahi,
	"Lsalo",Lsalo,
	"Ipgv",	Ipgv,
	"STA",	STA,
	"Trt",	Trt,
	"Rwm",	Rwm,
	"SOPcm",SOPcm,
	"Siahi",Siahi,
	"Sialo",Sialo,
	"To",	To,
	"Ro",	Ro,
	"Rid",	Rid,
	"Hd",	Hd,
	nil,	0,
};

void
xemacdump(Ctlr* ctlr)
{
	Regs *r;

	for(r = xemacregs; r->name != nil; r++)
		print("%s: %#8.8ux\n", r->name, reg32r(ctlr, r->offset));
}

static long
xemacifstat(Ether* edev, void* a, long n, ulong offset)
{
	Ctlr *ctlr;
	char *alloc, *e, *p;

	ctlr = edev->ctlr;

	alloc = malloc(2*READSTR);
	e = alloc+2*READSTR;
	p = seprint(alloc, e, "intr: %ud\n", ctlr->intr);
	p = seprint(p, e, "lintr: %ud\n", ctlr->lintr);
	p = seprint(p, e, "lsleep: %ud\n", ctlr->lsleep);
	p = seprint(p, e, "rintr: %ud\n", ctlr->rintr);
	p = seprint(p, e, "tintr: %ud\n", ctlr->tintr);

	if(ctlr->mii != nil && ctlr->mii->curphy != nil)
		miidumpphy(ctlr->mii, p, e);

	n = readstr(offset, a, n, alloc);
	free(alloc);

	return n;
}

static void
xemacpromiscuous(void* arg, int on)
{
	USED(arg, on);
}

static void
xemacmulticast(void* arg, uchar* addr, int on)
{
	USED(arg, addr, on);
}

#include "ureg.h"

void
xemacinterrupt(Ureg*, void* arg)
{
	u32int is;
	Ctlr *ctlr;
	Ether *edev;

	edev = arg;
	ctlr = edev->ctlr;

	ctlr->intr++;
	is = reg32r(ctlr, Is);
	iprint("Is %#8.8ux %#8.8ux\n", is, (is & IsMASK));
}

static int
xemacstaready(Ctlr* ctlr)
{
	u32int r;
	int timeo;

	r = 0;
	for(timeo = 0; timeo < 100; timeo++){
		r = reg32r(ctlr, STA);
		if(!(r & MMAgo))
			break;
		microdelay(1);
	}

	return r;
}

static int
xemacmmdio(Mii* mii, int write, int pa, int ra, int data)
{
	u32int r;
	Ctlr *ctlr;

	/*
	 * MMD - MDIO Manageable Device.
	 */
	ctlr = mii->ctlr;
	if((r = xemacstaready(ctlr)) & (MMAgo|PHYerr)){
		print("xemacmmdio: not ready %#ux\n", r);
		return -1;
	}

	reg32w(ctlr, STA, (ra<<16)|MMAgo|STAia|pa);
	if((r = xemacstaready(ctlr)) & (MMAgo|PHYerr)){
		print("xemacmmdio: STAia %#ux\n", r);
		return -1;
	}

	if(write == 0)
		reg32w(ctlr, STA, (data<<16)|MMAgo|STAir|pa);
	else
		reg32w(ctlr, STA, (data<<16)|MMAgo|STAiw|pa);
	if((r = xemacstaready(ctlr)) & (MMAgo|PHYerr)){
		print("xemacmmdio: write %d pa %d ra %d data %#ux %#8.8ux\n",
			write, pa, ra, data, r);
		return -1;
	}

	return r>>16;
}

static void
xemaclink(Ctlr *ctlr)
{
	int l1, l3, l4;

	/*
	 * The BmsrLs bit of PHYID#4 (XGXS) gives the transmit
	 * link status, and that of PHYID#3 (PCS) the receive link status.
	 */
	l1 = xemacmmdio(ctlr->mii, 0, 1, Bmsr, 0);
	l3 = xemacmmdio(ctlr->mii, 0, 3, Bmsr, 0);
	l4 = xemacmmdio(ctlr->mii, 0, 4, Bmsr, 0);
	iprint("l1 %#4.4ux l3 %#4.4ux l4 %#4.4ux\n", l1, l3, l4);
}

static void
xemacralloc(Ctlr* ctlr)
{
	USED(ctlr);
}

static void
xemacdetach(Ether* edev)
{
	Ctlr *ctlr;

	ctlr = edev->ctlr;

	/*
	 * To Do: undo what was done below.
	xemacoffline(ctlr);
	xemacrfree(ctlr);
	etc.
	 */
	USED(edev, ctlr);
}

static void
xemacattach(Ether* edev)
{
	u32int r;
	Ctlr *ctlr;

	ctlr = edev->ctlr;
	qlock(&ctlr->alock);
	if(ctlr->attached){
		xemacdetach(edev);
		qunlock(&ctlr->alock);
		return;
	}

	if(waserror()){
		qunlock(&ctlr->alock);
		nexterror();
	}
	ctlr->attached++;

	//tomal rx/tx channels already enabled (configuration control)
	//Ise already set in xemacsetup
	//intrenable already set by etherreset
	//enable emac rx/tx.

	malattach(ctlr->mal);

	reg32w(ctlr, Is, ~0);
	r = reg32r(ctlr, Tm0);
	reg32w(ctlr, Tm0, Gnp|r);
	reg32w(ctlr, M0, Txen|Rxen);
	mb();

	qunlock(&ctlr->alock);
	poperror();
}

void
xemactransmit(Ether* edev)
{
	int x;
	Dd *dd;
	Mal *mal;
	Block *b;
	Ctlr *ctlr;

	ctlr = edev->ctlr;
	mal = ctlr->mal;

	ilock(&mal->tlock);
	x = mal->tpi;
	while(mal->tpf > 0){
		dd = &mal->td[x];
		if(dd->status == 0)
			break;

		freeb(mal->tb[x]);
		mal->tb[x] = nil;

		x++;
		if(x == mal->ntd-1)
			x = 0;
		mal->tpf--;
	}
	mal->tpi = x;

	while(mal->tpf < mal->ntd-2){
		if((b = qget(edev->oq)) == nil)
			break;

		dd = &mal->td[mal->tci];
		dd->address = PADDR(b->rp);
		dd->length = dd->status = 0;
		dd->posted = BLEN(b);
		dd->command = GenFCS|GenPad;
		mal->tb[mal->tci] = b;
		mb();
		dd->code = PdCode|Signal|NotifyReq|LastTx;

		mal->tci++;
		if(mal->tci == mal->ntd-1)
			mal->tci = 0;
		mal->tpf++;

		maltxstart(mal);
	}
	iunlock(&mal->tlock);
}

void
xemactxstartxxx(void* v, int)
{
	xemactransmit(((Ctlr*)(v))->edev);
}

void
xemacrxintrxxx(void* v, u32int nrf)
{
	Dd *dd;
	Mal *mal;
	Block *b;
	Ctlr *ctlr;

	ctlr = v;
	mal = ctlr->mal;

	while(mal->rf != nrf){
		dd = &mal->rd[mal->ri];
		if(!(dd->status & LastRx))
			break;

		if((b = malallocb(mal)) != nil)
			etheriq(ctlr->edev, b, 1);
		else
			ctlr->edev->soverflows++;

		mal->rf++;
	}
}

static Mii*
xemacmii(Ctlr* ctlr)
{
	Mii *mii;
	uint mask;
	MiiPhy *phy;

	/*
	 * The part is a Broadcom BCM8704 Serial 10GbE PHY.
	 * But wait, there's more! This comprises an XGXS (PHYID#4),
	 * PMA/PMD functions (PHYID#1) and PCS (PHYID#3).
	 * None of this is very approachable via IEEE 802.3ae-2002.
	 */
	mask = archmiiport(ctlr->port);
	if((mii = miiattach(ctlr, mask, xemacmmdio)) == nil)
		return nil;

	if(mii->mask != mask){
buggery:
		miidetach(mii);
		return nil;
	}

	/*
	 * Check the parts are what is expected.
	 * Multiple PHYs are not pleasant to deal with.
	 */
	if((mii->phy[1]->phyid & 0xfffffff0) != 0x00206030)
		goto buggery;
	mii->curphy = mii->phy[1];
	if(miireset(mii) == -1)
		goto buggery;
	if((mii->phy[3]->phyid & 0xfffffff0) != 0x00206030)
		goto buggery;
	mii->curphy = mii->phy[3];
	if(miireset(mii) == -1)
		goto buggery;

	/*
	 * Broadcom. Sigh.
	 */
	if(miimiw(mii, 0xc803, 0x0164) == -1)
		goto buggery;
	if(miimiw(mii, 0xc800, 0x7f80) == -1)
		goto buggery;

	if((mii->phy[4]->phyid & 0xfffffff0) != 0x002063a0)
		goto buggery;
	phy = mii->curphy = mii->phy[4];
	phy->anar = Ana10G;
	phy->mscr = 0;
	phy->link = 0;
	phy->fc = 0;
	phy->speed = 10000;
	phy->fd = 1;
	phy->tfc = phy->rfc = 0;

	/*
	 * The BmsrLs bit of PHYID#4 (XGXS) gives the transmit
	 * link status, and that of PHYID#3 (PCS) the receive link status.
	 */
	mii->curphy = mii->phy[3];
	phy->link = ((miimir(mii, Bmsr) & BmsrLs) != 0);
	mii->curphy = mii->phy[4];
	phy->link &= ((miimir(mii, Bmsr) & BmsrLs) != 0);

	return mii;
}

static int
xemacreset(Ctlr* ctlr)
{
	u32int r;
	int timeo;

	/*
	 * Soft reset.
	 */
	reg32w(ctlr, M0, Srst);
	SET(r);
	for(timeo = 0; timeo < 100; timeo++){
		r = reg32r(ctlr, M0);
		if(!(r & Srst))
			break;
		microdelay(100);
	}
	if(r & Srst){
		DBG("XEMAC: soft-reset failed\n");
		return -1;
	}

	return 0;
}

static int
xemacsetup(Ctlr* ctlr)
{
	int i;
	u32int r;

	/*
	 * Configure for operation:
	 *  Mode Register 1:
	 *	believe the Rx/Tx FIFOs are 8K each - check;
	 *	other values depend on whether jumbo or not;
	 *  set/clear MAC addresses;
	 *  VLAN Tag Protocol ID;
	 *  Receive Mode Register:
	 *	strip padding, FCS; individual and broadcast compare;
	 *	FIFO Threshold Limit of 128 entries;
	 *  Transmit Mode Register 1:
	 *	that's just the way it is...;
	 *  Transmit Request Threshold:
	 *	ditto;
	 *  Receive Low/High WaterMark:
	 *	these are in FIFO entries (16 bytes). Low is an eighth,
	 *	high is a quarter, use the default values;
	 *  Pause Timer:
	 *	set to maximum;
	 *  and that should be enough.
	 *
	 * To do: fill in constants, explain (Ha Ha).
	 */
	reg32w(ctlr, M1, Sdr|Rfs8K|Tfs8K|Trq|Mws1|Jben|OPB133MHz);

	r = (ctlr->ia[0]<<8)|ctlr->ia[1];
	reg32w(ctlr, Iahi, r);
	r = (ctlr->ia[2]<<24)|(ctlr->ia[3]<<16)|(ctlr->ia[4]<<8)|ctlr->ia[5];
	reg32w(ctlr, Ialo, r);
	for(i = 0; i < 4*4; i += 4){
		reg32w(ctlr, Iaht+i, 0);
		reg32w(ctlr, Gaht+i, 0);
	}
	reg32w(ctlr, Siahi, r);
	reg32w(ctlr, Sialo, r);

	reg32w(ctlr, VTPID, 0x8100);
	reg32w(ctlr, Rm, Spad|Sfcs|Iae|Bae|Ftl128);
	reg32w(ctlr, Tm1, 0x02200240);
	reg32w(ctlr, Trt, 0x17000000);
	reg32w(ctlr, Rwm, 0x06000700);
	reg32w(ctlr, Pt, 0xffff);

	/*
	 */
	reg32w(ctlr, Ise, IsMASK);

	return 0;
}

static int
xemacnpn(Ether* edev)
{
	Ctlr *ctlr;

	if((ctlr = edev->ctlr) != nil){
		if(ctlr->mii != nil)
			miidetach(ctlr->mii);
		if(ctlr->mal != nil)
			malnpn(ctlr->mal);
		if(ctlr->xgmii != nil)
			xgmiinpn(ctlr->xgmii);
		xemacreset(ctlr);
		kunmapphys(UINT2PTR(ctlr->reg));
		edev->ctlr = nil;
		free(ctlr);
	}
	toenpn();

	return -1;
}

static int
xemacpnp(Ether* edev)
{
	void *v;
	Ctlr *ctlr;
	uchar ea[Eaddrlen];

	/*
	 * Check basic configuration information.
	 * Insist that the platform-specific code provide the
	 * physical address, IRQ, and Ethernet address.
	 */
	if(edev->pa == 0 || edev->irq < 0){
		print("xemac%d: missing phys address or irq\n", edev->port);
		return -1;
	}

	memset(ea, 0, Eaddrlen);
	if(memcmp(ea, edev->ea, Eaddrlen) == 0){
		print("%s%d: no MAC address\n", edev->type, edev->port);
		return -1;
	}

	/*
	 * Attempt to allocate all the resources in roughly
	 * the correct order and prepare them to be enabled by
	 * attach.
	 */
	if(toepnp() < 0)
		return -1;

	if((v = kmapphys(VIRTXEMAC, edev->pa, 4*KiB, TLBI|TLBG|TLBWR)) == 0){
		toenpn();
		return -1;
	}
	ctlr = malloc(sizeof(Ctlr));
	ctlr->reg = PTR2UINT(v);
	ctlr->edev = edev;
	edev->ctlr = ctlr;
	ctlr->port = edev->port;
	memmove(ctlr->ia, edev->ea, Eaddrlen);

	if((ctlr->xgmii = xgmiipnp()) == nil)
		return xemacnpn(edev);

	if((ctlr->mal = malpnp(0, Nrd, Ntd, Flength)) == nil)
		return xemacnpn(edev);
	ctlr->mal->ctlr = ctlr;

	if(xgmiireset(ctlr->xgmii) < 0)
		return xemacnpn(edev);
	if(xemacreset(ctlr) < 0)
		return xemacnpn(edev);
	if(xemacsetup(ctlr) < 0)
		return xemacnpn(edev);

	if((ctlr->mii = xemacmii(ctlr)) == nil)
		return xemacnpn(edev);

	edev->ctlr = ctlr;
	edev->port = ctlr->port;
	edev->reg = ctlr->reg;
	edev->mbps = 10000;

	/*
	 * Linkage to the generic ethernet driver.
	 */
	edev->attach = xemacattach;
	edev->transmit = xemactransmit;
	edev->interrupt = xemacinterrupt;
	edev->ifstat = xemacifstat;
	edev->ctl = nil;

	edev->arg = edev;
	edev->promiscuous = xemacpromiscuous;
	edev->multicast = xemacmulticast;

	return 0;
}

void
etherxemaclink(void)
{
	addethercard("XEMAC", xemacpnp);
}

void
etherxlink(void)
{
	addethercard("XEMAC", xemacpnp);
}
