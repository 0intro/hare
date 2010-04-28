/*The Parallel Hello World Program*/
/*#include <stdio.h>*/
#include <u.h>
#include <libc.h>
#include "mpi.h"

void
main(int argc, char **argv)
{
	int node;
	
	print("Hello from hello arg %d argv %p\n", argc, argv);
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	
	print("Hello World from Node %d\n",node);
	
	MPI_Finalize();
}
