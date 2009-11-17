#define RDTSC 		BYTE $0x0F; BYTE $0x31	/* RDTSC, result in AX/DX (lo/hi) */

TEXT _getticks(SB), $0				/* time stamp counter */
	RDTSC
	MOVL	vlong+0(FP), CX			/* &vlong */
	MOVL	AX, 0(CX)			/* lo */
	MOVL	DX, 4(CX)			/* hi */
	RET
