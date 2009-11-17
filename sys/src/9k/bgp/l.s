/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Based on ppc440/l.s
 *
 * Description: low level assembly routines and bootstrap
 *
 * All rights reserved
 *
 */
#include "mem.h"

/*
 * Special Purpose Registers of interest here (440/450 versions)
 */
#define SPR_DEC		0x016

#define SPR_SRR0	0x01a		/* Save/Restore Register 0 */
#define SPR_SRR1	0x01b		/* Save/Restore Register 1 */

#define SPR_PID		0x030		/* Process ID */
#define SPR_DECAR	0x036
#define SPR_CSRR0	0x03a		/* Critical Save/Restore Register 0 */
#define SPR_CSRR1	0x03b		/* Critical Save/Restore Register 1 */
#define SPR_DEAR	0x03d		/* Data Error Address Register */
#define SPR_ESR		0x03e		/* Exception Syndrome Register */
#define SPR_IVPR	0x03f		/* Instruction Vector Prefix */

#define SPR_SPRG4R	0x104		/* SPR general 4; user/supervisor R */
#define SPR_SPRG5R	0x105		/* SPR general 5; user/supervisor R */
#define SPR_SPRG6R	0x106		/* SPR general 6; user/supervisor R */
#define SPR_SPRG7R	0x107		/* SPR general 7; user/supervisor R */

#define SPR_TBLR	0x10c		/* Time Base Lower R */
#define SPR_TBUR	0x10d		/* Time Base Upper R */

#define SPR_SPRG0	0x110		/* SPR General 0 */
#define SPR_SPRG1	0x111		/* SPR General 1 */
#define SPR_SPRG2	0x112		/* SPR General 2 */
#define SPR_SPRG3	0x113		/* SPR General 3 */
#define SPR_SPRG4W	0x114		/* SPR General 4; supervisor W */
#define SPR_SPRG5W	0x115		/* SPR General 5; supervisor W  */
#define SPR_SPRG6W	0x116		/* SPR General 6; supervisor W  */
#define SPR_SPRG7W	0x117		/* SPR General 7; supervisor W */

#define SPR_TBLW	0x11c		/* Time Base Lower W */
#define SPR_TBUW	0x11d		/* Time Base Upper W */

#define SPR_PVR		0x11f		/* Processor Version Register */

#define SPR_TSR		0x150		/* timer status */
#define SPR_TCR		0x154		/* timer control */

#define SPR_DBCR0	0x134		/* Debug Control Register 0 */

#define SPR_IVOR(n)	(0x190+(n))	/* Interrupt Vector Offset 0-15 */

#define SPR_MCSRR0	0x23a
#define SPR_MCSRR1	0x23b
#define SPR_MCSR	0x23c

#define SPR_CCR1	0x378		/* Core Configuration Register 1 */
#define SPR_MMUCR	0x3b2		/* MMU Control */
#define SPR_CCR0	0x3b3		/* Core Configuration Register 0 */

#define	SAVER0		SPR_SPRG0	/* shorthand use in save/restore */
#define	SAVER1		SPR_SPRG1
#define	SAVELR		SPR_SPRG2
#define	SAVEXX		SPR_SPRG3

#define	SPR_PIR		0x11e		/* Processor Identity Register */

/*
 * special instruction definitions
 */
#define	MBAR		WORD $((31<<26)|(854<<1))
#define	MSYNC		WORD $((31<<26)|(598<<1))
#define	MSRSYNC		MSYNC; ISYNC
#define	ICCCI(a, b)	WORD $((31<<26)|((a)<<16)|((b)<<11)|(966<<1))
#define	DCCCI(a, b)	WORD $((31<<26)|((a)<<16)|((b)<<11)|(454<<1))

#define	TLBRELO(a, t)	WORD $((31<<26)|((t)<<21)|((a)<<16)|(2<<11)|(946<<1))
#define	TLBREMD(a, t)	WORD $((31<<26)|((t)<<21)|((a)<<16)|(1<<11)|(946<<1))
#define	TLBREHI(a, t)	WORD $((31<<26)|((t)<<21)|((a)<<16)|(0<<11)|(946<<1))
#define	TLBWELO(s, a)	WORD $((31<<26)|((s)<<21)|((a)<<16)|(2<<11)|(978<<1))
#define	TLBWEMD(s, a)	WORD $((31<<26)|((s)<<21)|((a)<<16)|(1<<11)|(978<<1))
#define	TLBWEHI(s, a)	WORD $((31<<26)|((s)<<21)|((a)<<16)|(0<<11)|(978<<1))
#define	TLBSXCC(a,b,t)	WORD $((31<<26)|((t)<<21)|((a)<<16)|((b)<<11)|(914<<1)|1)

#define	TBRL		268		/* read time base lower in MFTB */
#define	TBRU		269		/* read time base upper in MFTB */
#define	MFTB(tbr,d)	WORD $((31<<26)|((d)<<21)|((tbr&0x1f)<<16)|(((tbr>>5)&0x1f)<<11)|(371<<1))

#define	BDNZ		BC 16, 0,
#define	RFCI		WORD $((19<<26)|(51<<1)); BR 0(PC)

#define	UREGSPACE	(UREGSIZE+8)

TEXT _start32p<>(SB), 1, $-4
	XORCC	R0, R0				/* jump over CNK header */
	BEQ	_cnk
	WORD	$(('B'<<24)|('G'<<16)|('P'<<8)|'9')
	WORD	$(('C'<<24)|('N'<<16)|('K'<<8)|'!')
_cnk:
	XORCC	R0, R0				/* from now on R0 == 0 ... */

	/*
	 * setup MSR
	 * turn off interrupts
	 * use 0x000 as exception prefix
	 * enable machine check
	 */
	MOVW	MSR, R1
	RLWNM	$0, R1, $~MSR_EE, R1
	RLWNM	$0, R1, $~MSR_CE, R1
	RLWNM	$0, R1, $~(MSR_IS|MSR_DS), R1
	OR	$(MSR_ME), R1
	ISYNC
	MOVW	R1, MSR
	MSRSYNC

	/*
	 * Invalidate the caches.
	 */
	ICCCI(0, 0)
	ISYNC
	DCCCI(0, 0)
	MSYNC

	/*
	 * Save R[3-7] to pass to main or squidboy later.
	 * Squidboy only needs R3 (CPU#) and R4 ('argument'
	 * from the takeCPU call.
	 */
	MOVW	R3, R23
	MOVW	R4, R24				/* init RAM disc start */
	MOVW	R5, R25				/* init RAM disc end */
	MOVW	R6, R26				/* command-line string start */
	MOVW	R7, R27				/* command-line string end */

	MOVW	SPR(SPR_PIR), R28		/* PIN */
	CMP	R28, R0
	BNE	_setSB

	/*
	 * For the bootstrap processor (CPU#0),
	 * save the BGCNS_Descriptor contents in registers.
	 * Whichever TLB entry is currently used to access it
	 * will be blown away by tlbinit below, and the
	 * contents are needed to create the correct mapping
	 * afterwards. This is all a bit pedantic.
	 */
	MOVW	0(R3), R16			/* services */
	MOVW	4(R3), R17			/* baseVirtualAddress */
	MOVW	8(R3), R18			/* size */
	MOVW	12(R3), R19			/* basePhysicalAddress */
	MOVW	16(R3), R20			/* basePhysicalAddressERPN */
	MOVW	20(R3), R21			/* bgcns_private_in_use */
	MOVW	24(R3), R22			/* deprecatedServices */

_setSB:
	MOVW	$setSB-KZERO(SB), R2		/* temporary (SB) */

	BL	l2ci(SB)
	BL	tlbinit<>(SB)

	/*
	 * now running with MMU in kernel address space
	 * invalidate the caches again to flush any addresses
	 * below KZERO
	 */
	ICCCI(0, 0)
	ISYNC
	DCCCI(0, 0)
	MSYNC

	MOVW	$setSB(SB), R2			/* (SB) */
	BL	l2ci(SB)

	/*
	 * Set up vector space (16KiB, 64KiB aligned),
	 * extern registers (m->, up->) and stack.
	 * Mach (and stack) will be cleared along with the
	 * rest of BSS below if this is CPU#0.
	 * Memstart is the first free memory location
	 * after the kernel.
	 */
	MOVW	$end(SB), R6
	MOVW	$(64*KiB-1), R4
	ADD	R4, R6
	NOR	R4, R4
	AND	R4, R6				/* sys-> */

	MOVW	R6, SPR(SPR_IVPR)		/* 64KiB aligned vector base */

	MOVW	$INT_CI, R4
	MOVW	R4, SPR(SPR_IVOR(0))
	MOVW	$INT_MCHECK, R4
	MOVW	R4, SPR(SPR_IVOR(1))
	MOVW	$INT_DSI, R4
	MOVW	R4, SPR(SPR_IVOR(2))
	MOVW	$INT_ISI, R4
	MOVW	R4, SPR(SPR_IVOR(3))
	MOVW	$INT_EI, R4
	MOVW	R4, SPR(SPR_IVOR(4))
	MOVW	$INT_ALIGN, R4
	MOVW	R4, SPR(SPR_IVOR(5))
	MOVW	$INT_PROG, R4
	MOVW	R4, SPR(SPR_IVOR(6))
	MOVW	$INT_FPU, R4
	MOVW	R4, SPR(SPR_IVOR(7))		/* reserved (FPU?) */
	MOVW	$INT_SYSCALL, R4
	MOVW	R4, SPR(SPR_IVOR(8))		/* system call */
	MOVW	$INT_TRACE, R4
	MOVW	R4, SPR(SPR_IVOR(9))		/* reserved (trace?) */
	MOVW	$INT_PIT, R4
	MOVW	R4, SPR(SPR_IVOR(10))		/* decrementer */
	MOVW	$INT_FIT, R4
	MOVW	R4, SPR(SPR_IVOR(11))		/* fixed interval  */
	MOVW	$INT_WDT, R4
	MOVW	R4, SPR(SPR_IVOR(12))		/* watchdog */
	MOVW	$INT_DMISS, R4
	MOVW	R4, SPR(SPR_IVOR(13))		/* data TLB */
	MOVW	$INT_IMISS, R4
	MOVW	R4, SPR(SPR_IVOR(14))		/* instruction TLB */
	MOVW	$INT_DEBUG, R4
	MOVW	R4, SPR(SPR_IVOR(15))		/* debug */

	MOVW	$(16*KiB), R4
	ADD	R4, R6, R5			/* sys->machbase */

	MOVW	$MACHSIZE, R4			/* index into the array */
	MULLW	R28, R4
	ADD	R4, R5, R(MACH)			/* m-> */

	MOVW	R0, R(USER)			/* up-> */
	ADD	$(MACHSIZE-16), R(MACH), R1	/* SP */

	CMP	R28, R0				/* boot processor? */
	BEQ	_CPU0
	CMP	R28, $MAXMACH			/* spin if out of bounds */
	BGE	0(PC)

_squidboy:					/* application processor */
	SUB	$8, R1				/* stack slots for args */
	MOVW	R24, R4				/* takeCPU argument */
	MOVW	R4, 8(R1)
	MOVW	R23, R3				/* CPU# */

	BL	squidboy(SB)
	BR	0(PC)

_CPU0:						/* boot processor */
	MOVW	$edata-4(SB), R3
	MOVW	sizeofSys(SB), R4
	ADD	R6, R4				/* sys->memstart */
_clrbss:					/* clear BSS, Sys */
	MOVWU	R0, 4(R3)
	CMP	R3, R4
	BNE	_clrbss

	MOVW	R6, sys(SB)			/* sys-> */

	/*
	 * Stash the copy of the BGCNS_Descriptor saved in R16-R22
	 * and the saved original R[3-7] in R2[3-7] on the stack then call
	 *	main(r3, r4, r5, r6, r7)
	 * where r3 is a pointer to the saved decriptor and r[4-7] are
	 * the original values passed from CNS.
	 */
	SUB	$60, R1				/* stack slots for args */
	MOVW	R22, 52(R1)			/* deprecatedServices */
	MOVW	R21, 48(R1)			/* bgcns_private_in_use */
	MOVW	R20, 44(R1)			/* basePhysicalAddressERPN */
	MOVW	R19, 40(R1)			/* basePhysicalAddress */
	MOVW	R18, 36(R1)			/* size */
	MOVW	R17, 32(R1)			/* baseVirtualAddress */
	MOVW	R16, 28(R1)			/* services */
	MOVW	R27, 24(R1)			/* r7 */
	MOVW	R26, 20(R1)			/* r6 */
	MOVW	R25, 16(R1)			/* r5 */
	MOVW	R24, 12(R1)			/* r4 */
	MOVW	R23, 8(R1)			/* r3 (BGCNS_Descriptor) */
	ADD	$28, R1, R3			/* &BGCNS_Descriptor on stack */

	BL	main(SB)
_loop:
	BR	_loop

TEXT tlbinit<>(SB), 1, $-4

	/*
	 * SPR_PID: make following TLB entries shared, TID = PID = 0.
	 * SPR_MMUCR: allocate cache on store miss, no dcbf or icbi exception,
	 * tlbsx search 0. Leave U1 transient enable and U2 store without
	 * allocate enable alone.
	 */
	MOVW	R0, SPR(SPR_PID)
	MOVW	SPR(SPR_MMUCR), R3
	MOVW	$(MMUCR_U1TE|MMUCR_U2SWOAE), R4
	AND	R4, R3
	MOVW	R3, SPR(SPR_MMUCR)

	/*
	 * map various things 1:1
	 */
	MOVW	$tlbstatic-KZERO(SB), R4
	SUB	$4, R4				/* beware index off of (SB) */
	MOVW	$63, R3				/* start at the end */
_ldtlb:
	MOVWU	4(R4), R5			/* TLBHI */
	ANDCC	$TLBVALID, R5, R6		/* valid entry? */
	BEQ	_ldtlbe

	TLBWEHI(5, 3)
	MOVWU	4(R4), R5			/* TLBMD */
	TLBWEMD(5, 3)
	MOVWU	4(R4), R5			/* TLBLO */
	TLBWELO(5, 3)
	SUB	$1, R3
	BDNZ	_ldtlb
_ldtlbe:

	/*
	 * clear all remaining entries to
	 * remove most aliasing from boot
	 */
	CMP	R3, $0
	BEQ	_clrtlbe
_clrtlb:
	TLBWEHI(0, 3)
	TLBWEMD(0, 3)
	TLBWELO(0, 3)

	SUB	$1, R3
	CMP	R3, $0
	BGE	_clrtlb
_clrtlbe:

	/*
 	 * we're currently relying on the shadow I/D TLBs.
	 * to switch to the new TLBs,
	 * we need a synchronising instruction. ISYNC won't work here
	 * because the PC is still below KZERO,
	 * but there's no TLB entry to support that,
	 * and once we ISYNC the shadow i-tlb entry vanishes,
	 * taking the PC's location with it.
	 */
	MOVW	LR, R4
	OR	$KZERO, R4
	MOVW	R4, SPR(SPR_SRR0)
	MOVW	MSR, R4
	MOVW	R4, SPR(SPR_SRR1)

	/*
	 * resume in kernel mode in caller.
	 */
	RFI

TEXT l2ci(SB), 1, $-4				/* L2 Cache Invalidate */
	MOVW	R2, R3
	MOVW	$DCACHESIZE, R4

	RLWNM	$0, R3, $~(DCACHELINESZ-1), R5
	SUB	R5, R3
	ADD	R3, R4
	ADD	$(DCACHELINESZ-1), R4
	SRAW	$DCACHELINELOG, R4
	MOVW	R4, CTR
_l2ci:
	DCBI	(R5)
	ADD	$DCACHELINESZ, R5
	BDNZ	dcf0

	SYNC
	RETURN

TEXT CommonNodeServices(SB), 1, $12		/* Common Node Services */
	MOVW	R3, CTR				/* function pointer */
	MOVW	a0+4(FP), R3			/* arguments */
	MOVW	a1+8(FP), R4
	MOVW	a2+12(FP), R5
	MOVW	a3+16(FP), R6
	MOVW	a4+20(FP), R7
	MOVW	a5+24(FP), R8
	MOVW	a6+28(FP), R9

	MOVW	R31, r31-12(SP)
	MOVW	R(USER), up-8(SP)
	MOVW	R(MACH), m-4(SP)

	/*
	 * Leap into the unknown.
	 * Probably need to save/restore more around this.
	 * Should interrupts be off? - probably OK if
	 * only called at startup.
	 */
	BL	(CTR)

	MOVW	m-4(SP), R(MACH)
	MOVW	up-8(SP), R(USER)
	MOVW	r31-12(SP), R31
	MOVW	$setSB(SB), R2
	MOVW	$0, R0

	RETURN

TEXT dcrget(SB), 1, $-4				/* dcrget(dcr) */
	MOVW	DCR(R3), R3
	RETURN

TEXT dcrput(SB), 1, $-4				/* dcrput(dcr, val) */
	MOVW	val+4(FP), R4
	MOVW	R4, DCR(R3)
	RETURN

TEXT dcbf(SB), 1, $-4				/* dcbf(addr) */
	DCBF	(R3)
	RETURN

TEXT dcbi(SB), 1, $-4				/* dcbi(addr) */
	DCBI	(R3)
	RETURN

TEXT dccci(SB), 1, $-4				/* dccci(void) */
	DCCCI(0, 0)
	MSYNC
	RETURN

TEXT icbi(SB), 1, $-4				/* icbi(addr) */
	ICBI	(R3)
	RETURN

TEXT iccci(SB), 1, $-4				/* iccci(void) */
	ICCCI(0, 0)
	ISYNC
	RETURN

/*
 * MBAR ensures that all loads and stores preceding MBAR
 * complete with respect to main storage before any loads
 * and stores following the MBAR access main storage.
 * MBAR replaces the PowerPC EIEIO instruction and uses
 * the same opcode. The M0 field in the instruction is
 * treated as 0 providing a storage ordering function for
 * all storage access instructions executed by the processor.
 * Other processors implemnting the MBAR instruction may
 * support one or more non-zero MO settings, specifying
 * different subsets of storage accesses to be ordered by
 * the MBAR instruction in those implementations.
 */
TEXT mb(SB), 1, $-4
	MBAR
	RETURN

/*
 * MSYNC is execution synchronising and guarantees that
 * all storage accesses initiated by instructions prior to
 * the MSYNC have completed before any instructions after
 * the MSYNC begin executiuon.
 * MSYNC replaces the PowerPC SYNC instruction and uses
 * the same opcode.
 */
TEXT imb(SB), 1, $-4
	MSYNC
	RETURN

TEXT dearget(SB), 1, $-4
	MOVW	SPR(SPR_DEAR), R3
	RETURN

TEXT pidget(SB), 1, $-4
	MOVW	SPR(SPR_PID), R3
	RLWNM	$0, R3, $0xff, R3
	RETURN

TEXT pidput(SB), 1, $-4
	MOVW	R3, SPR(SPR_PID)
	RETURN

TEXT stidget(SB), 1, $-4
	MOVW	SPR(SPR_MMUCR), R3
	RLWNM	$0, R3, $0xff, R3
	RETURN

TEXT stidput(SB), 1, $-4
	MOVW	SPR(SPR_MMUCR), R4
	RLWMI	$0, R3, $0xff, R4
	MOVW	R4, SPR(SPR_MMUCR)
	RETURN

TEXT pitget(SB), 1, $-4
	MOVW	SPR(SPR_DEC), R3
	RETURN

TEXT pitput(SB), 1, $-4
	MOVW	R3, SPR(SPR_DEC)
	MOVW	R3, SPR(SPR_DECAR)
	RETURN

TEXT tbget(SB), 1, $-4
_tbget:
	MOVW	SPR(SPR_TBUR), R4
	MOVW	SPR(SPR_TBLR), R5
	MOVW	SPR(SPR_TBUR), R6
	CMP	R4, R6				/* rollover? */
	BNE	_tbget

	MOVW	R4, 0(R3)
	MOVW	R5, 4(R3)
	RETURN

TEXT tbput(SB), 1, $-4
	MOVW	0(R3), R4
	MOVW	4(R3), R5
	MOVW	$0, R6

	MOVW	R6, SPR(SPR_TBLW)		/* prevent wrap into TBU */
	MOVW	R4, SPR(SPR_TBUW)
	MOVW	R5, SPR(SPR_TBLW)
	RETURN

TEXT ccr0get(SB), 1, $-4
	MOVW	SPR(SPR_CCR0), R3
	RETURN

TEXT ccr1get(SB), 1, $-4
	MOVW	SPR(SPR_CCR1), R3
	RETURN

/*
 * From here on just taken from BG/L.
 * May need some some work for BG/P down the road.
 */
TEXT	tlbinval(SB), 1, $-4
	TLBWEHI(0, 3)
	TLBWEMD(0, 3)
	TLBWELO(0, 3)
	ISYNC
	RETURN

TEXT	splhi(SB), 1, $-4
	MOVW	MSR, R3
	RLWNM	$0, R3, $~MSR_EE, R4
	MOVW	R4, MSR
	MSRSYNC
	MOVW	LR, R31
	MOVW	R31, 4(R(MACH))			/* save PC in m->splpc */
	RETURN

TEXT	splx(SB), 1, $-4
	MOVW	LR, R31
	MOVW	R31, 4(R(MACH))			/* save PC in m->splpc */
	/* fall though */

TEXT	splxpc(SB), 1, $-4
	MOVW	MSR, R4
	RLWMI	$0, R3, $MSR_EE, R4
	MOVW	R4, MSR
	MSRSYNC
	RETURN

TEXT	spllo(SB), 1, $-4
	MOVW	MSR, R3
	OR	$MSR_EE, R3, R4
	MOVW	R4, MSR
	MSRSYNC
	RETURN

TEXT	spldone(SB), 1, $-4
	RETURN

TEXT	islo(SB), 1, $-4
	MOVW	MSR, R3
	MOVW	$MSR_EE, R4
	AND	R4, R3
	RETURN

TEXT	setlabel(SB), 1, $-4
	MOVW	LR, R31
	MOVW	R1, 0(R3)
	MOVW	R31, 4(R3)
	MOVW	$0, R3
	RETURN

TEXT	gotolabel(SB), 1, $-4
	MOVW	4(R3), R31
	MOVW	R31, LR
	MOVW	0(R3), R1
	MOVW	$1, R3
	RETURN

TEXT	touser(SB), 1, $-4
	/* splhi */
	MOVW	MSR, R5
	RLWNM	$0, R5, $~MSR_EE, R5
	MOVW	R5, MSR
	MSRSYNC
	MOVW	R(USER), R4			/* up */
	MOVW	8(R4), R4			/* up->kstack */
	ADD	$(KSTACK-UREGSPACE), R4
	MOVW	R4, SPR(SPR_SPRG7W)		/* save for use in traps/interrupts */
	MOVW	$(UTZERO+32), R5		/* header appears in text */
	MOVW	$UMSR, R4
	MOVW	R4, SPR(SPR_SRR1)
	MOVW	R3, R1
	MOVW	R5, SPR(SPR_SRR0)
	RFI

TEXT	icflush(SB), 1, $-4			/* icflush(virtaddr, count) */
	MOVW	n+4(FP), R4
	RLWNM	$0, R3, $~(ICACHELINESZ-1), R5
	SUB	R5, R3
	ADD	R3, R4
	ADD	$(ICACHELINESZ-1), R4
	SRAW	$ICACHELINELOG, R4
	MOVW	R4, CTR
icf0:	ICBI	(R5)
	ADD	$ICACHELINESZ, R5
	BDNZ	icf0
	ISYNC
	RETURN

TEXT	dcflush(SB), 1, $-4			/* dcflush(virtaddr, count) */
	MOVW	n+4(FP), R4
	RLWNM	$0, R3, $~(DCACHELINESZ-1), R5
	CMP	R4, $0
	BLE	dcf1
	SUB	R5, R3
	ADD	R3, R4
	ADD	$(DCACHELINESZ-1), R4
	SRAW	$DCACHELINELOG, R4
	MOVW	R4, CTR
dcf0:	DCBF	(R5)
	ADD	$DCACHELINESZ, R5
	BDNZ	dcf0
dcf1:
	SYNC
	RETURN

TEXT	tas32(SB), 1, $-4
	SYNC
	MOVW	R3, R4
	MOVW	$0xdead,R5
tas1:
	MSYNC
	LWAR	(R4), R3
	CMP	R3, $0
	BNE	tas0
	STWCCC	R5, (R4)
	BNE	tas1
tas0:
	SYNC
	ISYNC
	RETURN

TEXT ainc(SB), 1, $-4				/* int ainc(int*); */
	MOVW	R3, R4
_ainc:
	MSYNC
	LWAR	(R4), R3
	ADD	$1, R3
	STWCCC	R3, (R4)
	BNE	_ainc

	CMP	R3, $0				/* overflow if -ve or 0 */
	BGT	_return
_trap:
	MOVW	$0, R0
	MOVW	(R0), R0			/* over under sideways down */
_return:
	RETURN

TEXT adec(SB), 1, $-4				/* int adec(int*); */
	MOVW	R3, R4
_adec:
	MSYNC
	LWAR	(R4), R3
	ADD	$-1, R3
	STWCCC	R3, (R4)
	BNE	_adec

	CMP	R3, $0				/* underflow if -ve */
	BLT	_trap

	RETURN

TEXT cas32(SB), 1, $-4			/* int cas32(void*, u32int, u32int) */
	MOVW	R3, R4			/* addr */
	MOVW	old+4(FP), R5
	MOVW	new+8(FP), R6
	MSYNC
	LWAR	(R4), R3
	CMP	R3, R5
	BNE	 fail
	STWCCC	R6, (R4)
	BNE	 fail
	MOVW	 $1, R3
	RETURN
fail:
	MOVW	 $0, R3
	RETURN

TEXT	gettbl(SB), 1, $-4
	MFTB(TBRL, 3)
	RETURN

TEXT	gettbu(SB), 1, $-4
	MFTB(TBRU, 3)
	RETURN

TEXT	gettsr(SB), 1, $-4
	MOVW	SPR(SPR_TSR), R3
	RETURN

TEXT	puttsr(SB), 1, $-4
	MOVW	R3, SPR(SPR_TSR)
	RETURN

TEXT	puttcr(SB), 1, $-4
	MOVW	R3, SPR(SPR_TCR)
	RETURN

TEXT	pvrget(SB), 1, $-4
	MOVW	SPR(SPR_PVR), R3
	RETURN

TEXT	getmsr(SB), 1, $-4
	MOVW	MSR, R3
	RETURN

TEXT	putmsr(SB), 1, $-4
	SYNC
	MOVW	R3, MSR
	MSRSYNC
	RETURN

TEXT	getmcsr(SB), 1, $-4
	MOVW	SPR(SPR_MCSR), R3
	RETURN

TEXT	getesr(SB), 1, $-4
	MOVW	SPR(SPR_ESR), R3
	RETURN

TEXT	putesr(SB), 1, $-4
	MOVW	R3, SPR(SPR_ESR)
	RETURN

TEXT	tlbwrx(SB), 1, $-4
	MOVW	hi+4(FP), R5
	MOVW	mid+8(FP), R6
	MOVW	lo+12(FP), R7
	MSYNC
	TLBWEHI(5, 3)
	TLBWEMD(6, 3)
	TLBWELO(7, 3)
	ISYNC
	RETURN

TEXT	tlbrehi(SB), 1, $-4
	TLBREHI(3, 3)
	RETURN

TEXT	tlbremd(SB), 1, $-4
	TLBREMD(3, 3)
	RETURN

TEXT	tlbrelo(SB), 1, $-4
	TLBRELO(3, 3)
	RETURN

TEXT	tlbsxcc(SB), 1, $-4
	TLBSXCC(0, 3, 3)
	BEQ	tlbsxcc0
	MOVW	$-1, R3	/* not found */
tlbsxcc0:
	RETURN

TEXT	gotopc(SB), 1, $-4
	MOVW	R3, CTR
	MOVW	LR, R31	/* for trace back */
	BR	(CTR)

TEXT	dtlbmiss(SB), 1, $-4
	MOVW	R3, SPR(SPR_SPRG1)
	MOVW	$INT_DMISS, R3
	MOVW	R3, SPR(SAVEXX)	/* value for cause if entry not in soft tlb */
	MOVW	SPR(SPR_DEAR), R3
	BR	tlbmiss

TEXT	itlbmiss(SB), 1, $-4
	MOVW	R3, SPR(SPR_SPRG1)
	MOVW	$INT_IMISS, R3
	MOVW	R3, SPR(SAVEXX)
	MOVW	SPR(SPR_SRR0), R3

tlbmiss:
	/* R3 contains missed address */
	RLWNM	$0, R3, $~(BY2PG-1), R3		/* just the page */
	MOVW	R2, SPR(SPR_SPRG0)
	MOVW	R4, SPR(SPR_SPRG2)
	MOVW	R5, SPR(SPR_SPRG6W)
	MOVW	R6, SPR(SPR_SPRG4W)
	MOVW	CR, R6
	MOVW	R6, SPR(SPR_SPRG5W)

	MOVW	$setSB(SB), R2
	MOVW	$machptr(SB), R2		/* pointer array */
	MOVW	SPR(SPR_PIR), R4		/* PIN */
	SLW	$2, R4				/* offset into pointer array */
	ADD	R4, R2				/* pointer to array element */
	MOVW	(R2), R2			/* m-> */

	MOVW	(6*4)(R2), R4			/* m->tlbfault++ */
	ADD	$1, R4
	MOVW	R4, (6*4)(R2)

	MOVW	SPR(SPR_PID), R4
	SRW	$12, R3, R6
	RLWMI	$2, R4, $(0xFF<<2), R3	/* shift and insert PID for match */
	XOR	R3, R6		/* hash=(va>>12)^(pid<<2); (assumes STLBSIZE is 10 to 12 bits) */
	MOVW	(3*4)(R2), R5			/* m->stlb */
	RLWNM	$0, R6, $(STLBSIZE-1), R6	/* mask */
	SLW	$1, R6, R4
	ADD	R4, R6
	SLW	$2, R6				/* index 12-byte entries */
	MOVWU	(R6+R5), R4	/* fetch Softtlb.hi for comparison; updated address goes to R5 */
	CMP	R4, R3
	BNE	tlbtrap
	MFTB(TBRL, 6)
	MOVW	(4*4)(R2), R4			/* m->utlbhi */
	RLWNM	$0, R6, $(NTLB-1), R6		/* pseudo-random tlb index */
	CMP	R6, R4
	BLE	tlbm1
	SUB	R4, R6
tlbm1:
	RLWNM	$0, R3, $~(BY2PG-1), R3
	OR	$(TLB4K | TLBVALID), R3		/* make valid tlb hi */
	TLBWEHI(3, 6)
	MOVW	4(R5), R4			/* tlb mid */
	TLBWEMD(4, 6)
	MOVW	8(R5), R4			/* tlb lo */
	TLBWELO(4, 6)
	ISYNC
	MOVW	SPR(SPR_SPRG5R), R6
	MOVW	R6, CR
	MOVW	SPR(SPR_SPRG4R), R6
	MOVW	SPR(SPR_SPRG6R), R5
	MOVW	SPR(SPR_SPRG2), R4
	MOVW	SPR(SPR_SPRG1), R3
	MOVW	SPR(SPR_SPRG0), R2
	RFI

tlbtrap:
	MOVW	SPR(SPR_SPRG5R), R6
	MOVW	R6, CR
	MOVW	SPR(SPR_SPRG4R), R6
	MOVW	SPR(SPR_SPRG6R), R5
	MOVW	SPR(SPR_SPRG2), R4
	MOVW	SPR(SPR_SPRG1), R3
	MOVW	SPR(SPR_SPRG0), R2
	MOVW	R0, SPR(SAVER0)
	MOVW	LR, R0
	MOVW	R0, SPR(SAVELR)
	BR	trapcommon

/*
 * following Book E, traps thankfully leave the mmu on.
 * the following code has been executed at the exception
 * vector location already:
 *	MOVW R0, SPR(SAVER0)
 *	(critical interrupts disabled in MSR, using R0)
 *	MOVW LR, R0
 *	MOVW R0, SPR(SAVELR)
 *	bl	trapvec(SB)
 */
TEXT	trapvec(SB), 1, $-4
	MOVW	LR, R0
	MOVW	R0, SPR(SAVEXX)			/* save interrupt vector offset */
trapcommon:					/* entry point for machine checks, and full tlb faults */
	MOVW	R1, SPR(SAVER1)			/* save stack pointer */

	/* did we come from user space? */
	MOVW	SPR(SPR_SRR1), R0
	MOVW	CR, R1
	MOVW	R0, CR
	BC	4,17,ktrap			/* if MSR[PR]=0, we are in kernel space */

	/* was user mode, switch to kernel stack and context */
	MOVW	R1, CR
	MOVW	SPR(SPR_SPRG7R), R1		/* up->kstack+KSTACK-UREGSPACE, set in touser and sysrforkret */
	BL	saveureg(SB)

	MOVW	$machptr(SB), R(MACH)		/* pointer array */
	MOVW	SPR(SPR_PIR), R4		/* PIN */
	SLW	$2, R4				/* offset into pointer array */
	ADD	R4, R(MACH)			/* pointer to array element */
	MOVW	(R(MACH)), R(MACH)		/* m-> */
	MOVW	8(R(MACH)), R(USER)		/* up-> */

	BL	trap(SB)
	BR	restoreureg

ktrap:
	/* was kernel mode, R(MACH) and R(USER) already set */
	MOVW	R1, CR
	MOVW	SPR(SAVER1), R1
	SUB	$UREGSPACE, R1		/* push onto current kernel stack */
	BL	saveureg(SB)
	BL	trap(SB)
	MOVW	R(MACH), (48+(MACH-2)*4)(R1)	/* restore current processor's MACH */

restoreureg:
	MOVMW	48(R1), R2		/* r2:r31 */
	/* defer R1, R0 */
	MOVW	36(R1), R0
	MOVW	R0, CTR
	MOVW	32(R1), R0
	MOVW	R0, XER
	MOVW	28(R1), R0
	MOVW	R0, CR	/* CR */
	MOVW	24(R1), R0
	MOVW	R0, LR
	MOVW	20(R1), R0
	MOVW	R0, SPR(SPR_SPRG7W)	/* kstack for traps from user space */
	MOVW	16(R1), R0
	MOVW	R0, SPR(SPR_SRR0)	/* old PC */
	MOVW	12(R1), R0
	RLWNM	$0, R0, $~MSR_WE, R0	/* remove wait state */
	MOVW	R0, SPR(SPR_SRR1)	/* old MSR */
	/* cause, skip */
	MOVW	40(R1), R0
	MOVW	44(R1), R1		/* old SP */
	RFI

/*
 * machine check.
 * make it look like the others.
 * it's safe to destroy SPR_SRR0/1 because they can only be in
 * use if a critical interrupt has interrupted a non-critical interrupt
 * before it has had a chance to block critical interrupts,
 * but no recoverable machine checks can occur during a critical interrupt,
 * so the lost state doesn't matter.
 */
TEXT	trapmvec(SB), 1, $-4
	MOVW	LR, R0
	MOVW	R0, SPR(SAVEXX)
	MOVW	SPR(SPR_MCSRR0), R0		/* PC or excepting insn */
	MOVW	R0, SPR(SPR_SRR0)
	MOVW	SPR(SPR_MCSRR1), R0		/* old MSR */
	MOVW	R0, SPR(SPR_SRR1)
	BR	trapcommon

/*
 * critical interrupt: shouldn't happen on BG/P, so we'll accept loss of state
 * if interrupt occurred in the state-saving window of another trap.
 */
TEXT	critintrvec(SB), 1, $-4
	MOVW	LR, R0
	MOVW	R0, SPR(SAVEXX)
	MOVW	SPR(SPR_CSRR0), R0		/* PC or excepting insn */
	MOVW	R0, SPR(SPR_SRR0)
	MOVW	SPR(SPR_CSRR1), R0		/* old MSR */
	MOVW	R0, SPR(SPR_SRR1)
	BR	trapcommon

/*
 * external interrupts (non-critical)
 */
TEXT	intrvec(SB), 1, $-4
	MOVW	LR, R0
	MOVW	R0, SPR(SAVEXX)			/* save interrupt vector offset */
	MOVW	R1, SPR(SAVER1)			/* save stack pointer */

	/* did we come from user space? */
	MOVW	SPR(SPR_SRR1), R0
	MOVW	CR, R1
	MOVW	R0, CR
	BC	4,17,intr1			/* if MSR[PR]=0, we are in kernel space */

	/* was user mode, switch to kernel stack and context */
	MOVW	R1, CR
	MOVW	SPR(SPR_SPRG7R), R1		/* up->kstack+KSTACK-UREGSPACE, set in touser and sysrforkret */
	BL	saveureg(SB)

	MOVW	$machptr(SB), R(MACH)		/* pointer array */
	MOVW	SPR(SPR_PIR), R4		/* PIN */
	SLW	$2, R4				/* offset into pointer array */
	ADD	R4, R(MACH)			/* pointer to array element */
	MOVW	(R(MACH)), R(MACH)		/* m-> */
	MOVW	8(R(MACH)), R(USER)		/* up-> */

	BL	intr(SB)
	BR	restoreureg

intr1:
	/* was kernel mode, R(MACH) and R(USER) already set */
	MOVW	R1, CR
	MOVW	SPR(SAVER1), R1
	SUB	$UREGSPACE, R1		/* push onto current kernel stack */
	BL	saveureg(SB)
	BL	intr(SB)
	MOVW	R(MACH), (48+(MACH-2)*4)(R1)	/* restore current processor's MACH */
	BR	restoreureg

/*
 * enter with stack set and mapped.
 * on return, SB (R2) has been set, and R3 has the Ureg*,
 * the MMU has been re-enabled, kernel text and PC are in KSEG,
 * Stack (R1), R(MACH) and R(USER) are set by caller, if required.
 */
TEXT	saveureg(SB), 1, $-4
	MOVMW	R2, 48(R1)			/* save gprs r2 to r31 */
	MOVW	$setSB(SB), R2
	MOVW	SPR(SAVER1), R4
	MOVW	R4, 44(R1)
	MOVW	SPR(SAVER0), R5
	MOVW	R5, 40(R1)
	MOVW	CTR, R6
	MOVW	R6, 36(R1)
	MOVW	XER, R4
	MOVW	R4, 32(R1)
	MOVW	CR, R5
	MOVW	R5, 28(R1)
	MOVW	SPR(SAVELR), R6			/* LR */
	MOVW	R6, 24(R1)
	MOVW	SPR(SPR_SPRG7R), R6		/* up->kstack+KSTACK-UREGSPACE */
	MOVW	R6, 20(R1)
	MOVW	SPR(SPR_SRR0), R0
	MOVW	R0, 16(R1)			/* PC of excepting insn (or next insn) */
	MOVW	SPR(SPR_SRR1), R0
	MOVW	R0, 12(R1)			/* old MSR */
	MOVW	SPR(SAVEXX), R0
	MOVW	R0, 8(R1)			/* cause/vector */
	ADD	$8, R1, R3			/* Ureg* */
	STWCCC	R3, (R1)			/* break any pending reservations */
	MOVW	$0, R0				/* compiler/linker expect R0 to be zero */
	RETURN

/*
 * restore state from Ureg and return from trap/interrupt
 */
TEXT sysrforkret(SB), 1, $-4
	MOVW	R1, 20(R1)	/* up->kstack+KSTACK-UREGSPACE set in ureg */
	BR	restoreureg
