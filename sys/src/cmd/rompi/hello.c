/*The Parallel Hello World Program*/
#include "rompi.h"

/* tested may 17 2010 and works */
extern int torusdebug, rompidebug;
u64int	tbget(void);
int node, nproc;

int
main(int argc, char **argv)
{
	printf("Hello, from CNK\n");
	return 0;
}
