/*The Parallel Hello World Program*/
#include <stdio.h>
#include "mpi.h"

/* tested may 17 2010 and works */
extern int torusdebug;

typedef unsigned long long ticks;

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

int
main(int argc, char **argv)
{
	int node, nproc;
	unsigned char data[240];
	MPI_Status status;
	int i;
	int iter = 1000;
	ticks start, end;
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	
	printf("Hello World from Node %d of %d\n",node, nproc);
	start = getticks();
	for(i = 0; i < iter; i++) {
		if (! node) {
			MPI_Send(data, nproc, MPI_INT, 1, 1, MPI_COMM_WORLD);
			MPI_Recv(data, nproc, MPI_INT, nproc-1, 1, MPI_COMM_WORLD, &status);
		} else {
			MPI_Recv(data, nproc, MPI_INT, node-1, 1, MPI_COMM_WORLD, &status);
			MPI_Send(data, nproc, MPI_INT, (node+1)%nproc, 1, MPI_COMM_WORLD);
		}
	}
	end = getticks();
	printf("%lld %lld - p\n", end, start);
	printf("%d: Finalize\n", node);
	MPI_Finalize();
	return 0;
}
