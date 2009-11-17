#define RDTSC 		BYTE $0x0F; BYTE $0x31	/* RDTSC, result in AX/DX (lo/hi) */

TEXT getticks(SB), $0				/* time stamp counter */
	RDTSC
	XCHGL	DX, AX				/* swap lo/hi, zero-extend */
	SHLQ	$32, AX				/* hi<<32 */
	ORQ	DX, AX				/* (hi<<32)|lo */
	RET
