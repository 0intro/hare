/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: hummer support assembly code
 *
 * All rights reserved
 *
 */

#include "mem.h"

#define MSYNC	WORD $((31<<26)|(598<<1))
#define MSRSYNC	MSYNC; ISYNC
#define BDNZ	BC	16,0,

/*
 * void fifo2quad(void*, void*, int);
 *	move multiple quads to a FIFO
 */
TEXT fifo2quad(SB), 1, $(2*16)			/* F0 + slop */
	MOVW	fifo+4(FP), R4
	MOVW	n+8(FP), R5
	CMP	R5, $0
	BLE	_return

	MOVW	$16, R16

	MOVW	MSR, R27			/* R27 = MSR */
	MOVW	$~(MSR_CE|MSR_EE), R24
	AND	R24, R27, R25
	OR	$MSR_FP, R25

	SYNC
	MOVW	R25, MSR
	MSRSYNC

	ANDCC	$MSR_FP, R27, R26		/* R26 = MSR & MSR_FP */
	BEQ	_fifo2quad

	/* save fp here */
	ADD	$15, R1, R25
	RLWNM	$0, R25, $~15, R25		/* R25 = fp save area */

	FPMOVD	F0, (R25)

_fifo2quad:
	/* do the work here */
	SUB	R16, R3
	MOVW	R5, CTR

_fifo2quadloop:
	FPMOVD	(R4), F0
	FPMOVDU	F0, (R16+R3)

	BDNZ	_fifo2quadloop

_done:
	ANDCC	$MSR_FP, R27, R26		/* R26 = MSR & MSR_FP */
	BEQ	_msr

	FPMOVD	(R25), F0

_msr:
	SYNC
	MOVW	R27, MSR
	MSRSYNC

_return:
	RETURN

/*
 * void quad2fifo(void*, void*, int);
 *	move multiple chunks to a FIFO
 */
TEXT quad2fifo(SB), 1, $(2*16)			/* F0 + slop */
	MOVW	mem+4(FP), R4
	MOVW	n+8(FP), R5
	CMP	R5, $0
	BLE	_return

	MOVW	$16, R16

	MOVW	MSR, R27			/* R27 = MSR */
	MOVW	$~(MSR_CE|MSR_EE), R24
	AND	R24, R27, R25
	OR	$MSR_FP, R25

	SYNC
	MOVW	R25, MSR
	MSRSYNC

	ANDCC	$MSR_FP, R27, R26		/* R26 = MSR & MSR_FP */
	BEQ	_quad2fifo

	/* save fp here */
	ADD	$15, R1, R25
	RLWNM	$0, R25, $~15, R25		/* R25 = fp save area */

	FPMOVD	F0, (R25)

_quad2fifo:
	/* do the work here */
	SUB	R16, R4
	MOVW	R5, CTR

_quad2fifoloop:
	FPMOVDU	(R16+R4), F0
	FPMOVD	F0, (R3)

	BDNZ	_quad2fifoloop
	BR	_done

/*
 * void chunk2fifo(void*, void*, int);
 *	move multiple chunks to a FIFO
 */
TEXT chunk2fifo(SB), 1, $(2*16)			/* F0 + slop */
	MOVW	mem+4(FP), R4
	MOVW	n+8(FP), R5
	MOVW	$16, R16

	MOVW	MSR, R27			/* R27 = MSR */
	MOVW	$~(MSR_CE|MSR_EE), R24
	AND	R24, R27, R25
	OR	$MSR_FP, R25

	SYNC
	MOVW	R25, MSR
	MSRSYNC

	ANDCC	$MSR_FP, R27, R26		/* R26 = MSR & MSR_FP */
	BEQ	_chunk2fifo

	/* save fp here */
	ADD	$15, R1, R25
	RLWNM	$0, R25, $~15, R25		/* R25 = fp save area */

	FPMOVD	F0, (R25)

_chunk2fifo:
	/* do the work here */
	SUB	R16, R4
	MOVW	R5, CTR

_chunk2fifoloop:
	FPMOVDU	(R16+R4), F0
	FPMOVD	F0, (R3)
	FPMOVDU	(R16+R4), F0
	FPMOVD	F0, (R3)

	BDNZ	_chunk2fifoloop

_chunk2fifodone:
	ANDCC	$MSR_FP, R27, R26		/* R26 = MSR & MSR_FP */
	BEQ	_chunk2fifomsr

	FPMOVD	(R25), F0

_chunk2fifomsr:
	SYNC
	MOVW	R27, MSR
	MSRSYNC

	RETURN

/*
 * void fifo2chunk(void*, void*, int);
 *	move multiple chunks to a FIFO
 */
TEXT fifo2chunk(SB), 1, $(2*16)			/* F0 + slop */
	MOVW	fifo+4(FP), R4
	MOVW	n+8(FP), R5
	MOVW	$16, R16

	MOVW	MSR, R27			/* R27 = MSR */
	MOVW	$~(MSR_CE|MSR_EE), R24
	AND	R24, R27, R25
	OR	$MSR_FP, R25

	SYNC
	MOVW	R25, MSR
	MSRSYNC

	ANDCC	$MSR_FP, R27, R26		/* R26 = MSR & MSR_FP */
	BEQ	_fifo2chunk

	/* save fp here */
	ADD	$15, R1, R25
	RLWNM	$0, R25, $~15, R25		/* R25 = fp save area */

	FPMOVD	F0, (R25)

_fifo2chunk:
	/* do the work here */
	SUB	R16, R3
	MOVW	R5, CTR

_fifo2chunkloop:
	FPMOVD	(R4), F0
	FPMOVDU	F0, (R16+R3)
	FPMOVD	(R4), F0
	FPMOVDU	F0, (R16+R3)

	BDNZ	_fifo2chunkloop

_fifo2chunkdone:
	ANDCC	$MSR_FP, R27, R26		/* R26 = MSR & MSR_FP */
	BEQ	_fifo2chunkmsr

	FPMOVD	(R25), F0

_fifo2chunkmsr:
	SYNC
	MOVW	R27, MSR
	MSRSYNC

	RETURN
