/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Based on ppc440/physmem.h Copyright (C) 2006-2009 Alcatel-Lucent
 *
 * Description: physical memory layout of BG/P
 *
 * All rights reserved
 *
 */

#define PHYSDRAM	0ull
#define VIRTDRAM	KZERO
#define PHYSTORUS0	0x601140000ull
#define VIRTTORUS0	0xfffb0000		/* 64KiB */
#define PHYSTORUS1	0x601150000ull
#define VIRTTORUS1	0xfffc0000		/* 64KiB */
#define PHYSDMA		0x600000000ull
#define VIRTDMA		0xfffd0000		/* 16KiB (4KiB each) */
#define PHYSTOMAL	0x720000000ull
#define VIRTTOMAL	0xfffd4000		/* 16KiB */
#define PHYSXEMAC	0x720004000ull
#define VIRTXEMAC	0xfffd8000		/* 4KiB */
#define PHYSDEVBUS	0x720005000ull
#define VIRTDEVBUS	0xfffd9000		/* 4KiB */
#define PHYSUPC		0x710000000ull
#define VIRTUPC		0xfffda000		/* 4KiB */
#define PHYSUPCCTL	0x710001000ull
#define VIRTUPCCTL	0xfffdb000
#define PHYSTREE0	0x610000000ull
#define VIRTTREE0	0xfffdc000		/* 1KiB */
#define PHYSTREE1	0x611000000ull
#define VIRTTREE1	0xfffdd000		/* 1KiB */
#define PHYSBIC		0x730000000ull
#define VIRTBIC		0xfffde000
#define PHYSSRAMERROR	0x7fffdf000ull
#define VIRTSRAMERROR	0xfffdf000		/* 4KiB */
#define PHYSRAMECC	0x7fffe0000ull
#define VIRTSRAMECC	0xfffe0000		/* 64KiB */
#define PHYSKLOCKBOX	0x7ffff0000ull
#define VIRTKLOCKBOX	0xffff0000		/* 16KiB */
#define PHYSULOCKBOX	0x7ffff4000ull
#define VIRTULOCKBOX	0xffff4000		/* 16KiB */
#define PHYSSRAM	0x7ffff8000ull
#define VIRTSRAM	0xffff8000		/* 32KiB */
