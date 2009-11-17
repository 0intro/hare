TEXT _profin(SB), 1, $0
	MOVW	traceactive+0(SB),R5
	CMP	R5,$0
	BEQ	inotready
	SUB		$24,R1
	MOVW	R3,8(R1)
	MOVW	32(R1),R6 /* arg1 */
	MOVW	R6,12(R1)
	MOVW	36(R1),R4 /* arg2+ */
	MOVW	R4,16(R1)
	MOVW	40(R1), R4 /* arg3+ */
	MOVW	R4, 20(R1)
	MOVW	LR,R3
	MOVW	R3, 4(R1)
	BL		tracein(SB)
	MOVW	4(R1), R3
	MOVW	R3, LR
	MOVW	8(R1), R3
	ADD		$24,R1
inotready:
	RETURN

TEXT _profout(SB), 1, $0
	MOVW	traceactive+0(SB),R5
	CMP	R5,$0
	BEQ	notready
	SUB		$24,R1
	MOVW	R3,8(R1)
	MOVW	32(R1),R6 /* arg1 */
	MOVW	R6,12(R1)
	MOVW	36(R1),R4 /* arg2+ */
	MOVW	R4,16(R1)
	MOVW	40(R1), R4 /* arg3+ */
	MOVW	R4, 20(R1)
	MOVW	LR,R3
	MOVW	R3, 4(R1)
	BL		traceout(SB)
	MOVW	4(R1), R3
	MOVW	R3, LR
	MOVW	8(R1), R3
	ADD		$24,R1
notready:
	RETURN

