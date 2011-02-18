/*The Parallel Hello World Program*/
#include "rompi.h"

/* tested may 17 2010 and works */
extern int torusdebug, rompidebug;
u64int	tbget(void);
int node, nproc;

unsigned int getr2 (void)
{
  unsigned int __sp__;
  __asm__ __volatile__ ("mr %0, 2" : "=r" (__sp__));
  return __sp__;
}

int
main(int argc, char **argv)
{
	unsigned int *p;
	printf("Hello, from CNK r2 is 0x%x\n", getr2());
	printf("pid %d\n", getpid());
	p = malloc(2048);
	printf("p %p\n", p);
	free(p);
	p = malloc(412);
	printf("p %p\n", p);
	
	return 0;
}
