/*
 * This is the same as the C programme:
 *
 *	void
 *	main(char* argv0)
 *	{
 *		startboot(argv0, &argv0);
 *	}
 *
 * It is in assembler because SB needs to be
 * set and doing this in C drags in too many
 * other routines.
 */
TEXT _main(SB), 1, $8
	MOVW	$setSB(SB), R2
	MOVW	$0, R0
	SUB	$16, R1				/* make a frame */

	/*
	 * Argv0 is already passed in R3
	 * so it is already the first arg.
	 * Copy argv0 into the stack and push
	 * its address as the second arg.
	 */
	MOVW	R3, 20(R1)
	ADD	$20, R1, R6
	MOVW	R6, 8(R1)

	BL	startboot(SB)

_loop:
	BR	_loop				/* should never get here */
