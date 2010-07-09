/* first instruction in glibc is movw r1,r9, so use r9 for the jump */
	TEXT	callcnk+0(SB),0,$12
	MOVW	R3,R9
	MOVW	argc+4(FP),R3
	MOVW	argv+8(FP),R4
	MOVW	sp+12(FP), R1
	MOVW	$0, R5
	MOVW	$0, R6
	MOVW	$0, R7
	MOVW	$0,  R8
	MOVW	$0xdeadbeef,  R10
	MOVW	$0xdeadbeef,  R11
	MOVW	$0xdeadbeef,  R12
	MOVW	$0xdeadbeef,  R13
	MOVW	$0xdeadbeef,  R14
	MOVW	$0xdeadbeef,  R15
	MOVW	$0xdeadbeef,  R16
	MOVW	$0xdeadbeef,  R17
	MOVW	$0xdeadbeef,  R18
	MOVW	$0xdeadbeef,  R19
	MOVW	$0xdeadbeef,  R20
	MOVW	$0xdeadbeef,  R21
	MOVW	$0xdeadbeef,  R22
	MOVW	$0xdeadbeef,  R23
	MOVW	$0xdeadbeef,  R24
	MOVW	$0xdeadbeef,  R25
	MOVW	$0xdeadbeef,  R26
	MOVW	$0xdeadbeef,  R27
	MOVW	$0xdeadbeef,  R28
	MOVW	$0xdeadbeef,  R29
	MOVW	$0xdeadbeef,  R30
	MOVW	$0xdeadbeef,  R31
	BL	,0(R9)
	RETURN	,
	END	,
