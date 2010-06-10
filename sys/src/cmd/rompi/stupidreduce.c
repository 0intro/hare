/*The Parallel Hello World Program*/
/*#include <stdio.h>*/
#include <u.h>
#include <libc.h>
#include "mpi.h"

/* tested may 17 2010 and works */
extern int torusdebug;
u64int	tbget(void);
int node, nproc;

void
reallystupidreduce(unsigned int *sum)
{
	MPI_Status status;
	int i;
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
}

void
stupidreduce(unsigned int *sum)
{
	MPI_Status status;
	int i;
	if (node) {
		sum[0] = node;
		MPI_Send(sum, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
		MPI_Recv(sum, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
	} else {
		int nodeval[1];
		sum[0] = 0;
		for(i = 1; i < nproc; i++) {
			MPI_Recv(nodeval, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
			sum[0] += nodeval[0];
		}
		for(i = 1; i < nproc; i++) {
			MPI_Send(sum, 1, MPI_INT, i, 1, MPI_COMM_WORLD);
		}
	}
}

void
main(int argc, char **argv)
{
	unsigned int sum[1];
	int i;
	vlong startnsec, stopnsec;
	int iter = 100;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	
	print("Hello World from Node %d of %d\n",node, nproc);

	for(i = 0; i < iter; i++){
		if (i == 1)
			startnsec = tbget();
		sum[0] = node;
		stupidreduce(sum);
	}
	stopnsec = tbget();
		
	print("%d: Finalize\n", node);
	if (! node) 
		print("%lld nsec for %d iterations\n", stopnsec-startnsec, iter-1);
	MPI_Finalize();
	print("Node %d computes sum as %d and MPI says sum is %d\n", node, ((nproc-1)*nproc)/2, sum[0]);
}
