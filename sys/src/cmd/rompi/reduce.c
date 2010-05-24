/*The Parallel Hello World Program*/
/*#include <stdio.h>*/
#include <u.h>
#include <libc.h>
#include "mpi.h"

/* tested may 17 2010 and works */
extern int torusdebug, rompidebug;
extern int x, y, z, xsize, ysize, zsize; 
int xyztorank(int x, int y, int z);
int reduce_end ( void *buf, int num, MPI_Datatype datatype, int root, 
               MPI_Comm comm,  MPI_Status *status);
void torusstatus(int fd);
void
main(int argc, char **argv)
{
	int node, nproc;
	unsigned int sum[1], nsum[1];

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	
	print("%lld Hello World from Node %d of %d\n", nsec(),node, nproc);

	sum[0] = node;

	if (0 && node) 
		rompidebug |= 4;
	MPI_Reduce (sum, nsum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

	print("%lld Node %d (%d, %d, %d) computes sum as %d and MPI says sum is %d\n", nsec(), node, x, y, z, ((nproc-1)*nproc)/2, nsum[0]);
	print("%lld %d: Finalize\n", nsec(), node);
	MPI_Finalize();
}
