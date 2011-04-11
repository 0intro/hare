
/*                  pong.c Generic Benchmark code
 *               Dave Turner - Ames Lab - July of 1994+++
 *
 *  Most Unix timers can't be trusted for very short times, so take this
 *  into account when looking at the results.  This code also only times
 *  a single message passing event for each size, so the results may vary
 *  between runs.  For more accurate measurements, grab NetPIPE from
 *  http://www.scl.ameslab.gov/ .
 */
#include "rompi.h"

typedef uvlong ticks;
struct AmRing * amringsetup(int logN, int core);
void amrstartup(struct AmRing *amr);
int amrsend(struct AmRing *amr, void *data, int size, int rank);
unsigned char *amrrecvbuf(struct AmRing *amr, unsigned char *p, int *rank);
double MPI_Wtime(void);
int MPI_Init(int *argc,char ***argv);
extern int myproc, nproc;

static __inline__ ticks getticks(void)
{
     unsigned int tbl, tbu0, tbu1;

     do {
	  __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
	  __asm__ __volatile__ ("mftb %0" : "=r"(tbl));
	  __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
     } while (tbu0 != tbu1);

     return (((unsigned long long)tbu0) << 32) | tbl;
}

uvlong
mynsec(void)
{
	uvlong t;
	static int fd = -1;
	extern int pread(int, void *, int, unsigned long long);

	if (fd < 0) {
		fd = open("/dev/bintime", 0);
		if (fd < 0)
			panic("Can't open /dev/bintime");
	}

	pread(fd, &t, sizeof(t), 0ULL);
	return t;
}

int
main (int argc, char **argv)
{
	MPI_Init(&argc, &argv);
	
	struct AmRing *amring = amringsetup(20, 0);
	print("Call amrstartup, myproc %d nproc %d\n", myproc, nproc);
	if (myproc < nproc){
		if (myproc)
			torusctl("debug 64", 1);
		amrstartup(amring);	
	} else
		while(1);
	print("amrstartup returns\n");
	return 0;
}

