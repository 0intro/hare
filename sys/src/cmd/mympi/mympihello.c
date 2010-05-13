/*The Parallel Hello World Program*/
/*#include <stdio.h>*/
#include <u.h>
#include <libc.h>
#include "mpi.h"

extern int torusdebug;

void
main(int argc, char **argv)
{
	int node, nproc;
	unsigned int buf[1];
	MPI_Status status;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	
	print("Hello World from Node %d of %d\n",node, nproc);
	
	if (node) {
		MPI_Send(buf, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
		print("%d: Sent a message to rank 0\n", node);
	} else {
		int i;
		torusdebug |= 2;
		for(i = 1; i < nproc; i++) {
			MPI_Recv(buf, 	1, MPI_INT, i, 1, MPI_COMM_WORLD, &status);
			print("0: Recv'd from %d\n", i);
		}
		print("0: All %d nodes checked in\n", nproc-1);
	}
	print("%d: Finalize\n", node);
	MPI_Finalize();
}
