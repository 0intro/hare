/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: XGMII implementation
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

static char* xgmiiregs[] = {			/* XGMII Registers */
	"Function",
	"L3 Coherency",
	"Tomal and EMAC Control",
	"Xgxs Control 0",
	"Xgxs Control 1",
	"Xgxs Control 2",
	"Xgxs Status",
	"DMA Arbiter Control/Status",
	nil,
};

void
xgmiidump(void* v)
{
	int i;
	u32int* xgmii;

	if((xgmii = v) == nil)
		return;
	for(i = 0; xgmiiregs[i] != nil; i++)
		print("%s %#8.8ux\n", xgmiiregs[i], xgmii[i]);

	print("DCR L30 %#8.8ux DCR L31 %#8.8ux\n",
		dcrget(0x50a), dcrget(0x54a));
}

int
xgmiireset(void* v)
{
	u32int* xgmii;

	/*
	 * Reset XENPACK (active low).
	 * Signal PMA clocks ready.
	 */
	if((xgmii = v) == nil)
		return -1;
	xgmii[2] &= ~IB(17);
	delay(10);
	xgmii[2] |= IB(17);
	delay(10);

	xgmii[3] |= IB(21);
	mb();

	return 0;
}

int
xgmiinpn(void* xgmii)
{
	kunmapphys(xgmii);

	return 0;
}

void*
xgmiipnp(void)
{
	void *v;
	u32int dcr, *xgmii;

	v = kmapphys(VIRTDEVBUS, PHYSDEVBUS, 4*KiB, TLBI|TLBG|TLBWR);
	if((xgmii = v) == nil)
		return nil;

	/*
	 * Scraped from Das Boot.
	 * Not sure what all the bits mean. It's worrying that some
	 * of this fiddles with the L3 cache and coherency.
	 */
#ifdef notdef
	/*
	 * Actually, this seems wedge the system.
	 * Perhaps it only should be done once and Das Boot
	 * and CNS have already done it? Who knows.
	 */
	xgmii[0] = 0;
	xgmii[1] = 0x00028000;	//hmmm	|=?	/* L3 = IB(14)|IB(16) */
	xgmii[2] = 0x30400000;
	xgmii[2] |= 0x000c2f00;
	xgmii[3] = 0x0000fa00;

	dcr = dcrget(0x500+0x0a);		/* L3 cache 0 */
	dcr |= 0x00003000;
	dcrput(0x500+0x0a, dcr);

	dcr = dcrget(0x540+0x0a);		/* L3 cache 1 */
	dcr |= 0x00003000;
	dcrput(0x540+0x0a, dcr);
#else
	SET(dcr); USED(dcr);
	USED(xgmii);
#endif /* notdef */

	return v;
}
