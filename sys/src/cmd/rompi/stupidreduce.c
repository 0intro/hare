/*The Parallel Hello World Program*/
/*#include <stdio.h>*/
#include <u.h>
#include <libc.h>
#include "mpi.h"

/* tested may 17 2010 and works */
extern int torusdebug;

void
main(int argc, char **argv)
{
	int node, nproc;
	unsigned int sum[1];
	MPI_Status status;
	int i;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	
	print("Hello World from Node %d of %d\n",node, nproc);

	if (node) {
		sum[0] = node;
		MPI_Send(sum, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
		MPI_Recv(sum, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
	} else {
		int nodeval[1];
		sum[0] = 0;
		for(i = 1; i < nproc; i++) {
			MPI_Recv(nodeval, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &status);
			sum[0] += nodeval[0];
		}
		for(i = 1; i < nproc; i++) {
			MPI_Send(sum, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
		}
	}
	print("%d: Finalize\n", node);
	MPI_Finalize();
	print("Node %d computes sum as %d and MPI says sum is %d\n", node, ((nproc-1)*nproc)/2, sum[0]);
}
