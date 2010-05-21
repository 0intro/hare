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
	MPI_Reduce (sum, nsum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

#ifdef NOT
	if (! node) print("%lld Node %d (%d, %d, %d) computes sum as %d and MPI says sum is %d\n", nsec(), node, x, y, z, ((nproc-1)*nproc)/2, sum[0]);
//	torusstatus(1);
	/* this was simple before the deposit bit. It gets a tad harder when we use it. */
	if (0) {
	if (node == 0) {
		print("%lld %d:(%d,%d,%d) sends %d\n", nsec(), node, x, y, z, node);
		for(i = 1; i < nproc; i++) {
			MPI_Send(sum, 1, MPI_INT, i, 2, MPI_COMM_WORLD);
		}
	} else {
		rompidebug |= 2;
		torusdebug |= 2;
		MPI_Recv(sum, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);
	}
	} else {
		if (node) rompidebug |= 2;
		reduce_end(sum, 1, MPI_INT, 0, MPI_COMM_WORLD, &status);
	}
#endif
	if (node) print("%lld Node %d (%d, %d, %d) computes sum as %d and MPI says sum is %d\n", nsec(), node, x, y, z, ((nproc-1)*nproc)/2, sum[0]);
	print("%lld %d: Finalize\n", nsec(), node);
	MPI_Finalize();
}
