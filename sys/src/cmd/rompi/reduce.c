/*The Parallel Hello World Program*/
/*#include <stdio.h>*/
#include <u.h>
#include <libc.h>
#include "mpi.h"

/* tested may 17 2010 and works */
u64int	tbget(void);
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
	int i;
	vlong startnsec, stopnsec;
	int iter = 100;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	
	print("%lld Hello World from Node %d of %d\n", tbget(),node, nproc);

	if (0 && node) 
		rompidebug |= 4;
	for (i = 0; i < iter; i++){
		if (i == 1)
			startnsec = tbget();
		sum[0] = node;
		MPI_Reduce (sum, nsum, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
//		if (! node)
//			print("%d\n", i);
	}
	stopnsec = tbget();

	print("%lld Node %d (%d, %d, %d) computes sum as %d and MPI says sum is %d\n", tbget(), node, x, y, z, ((nproc-1)*nproc)/2, nsum[0]);
	if (! node) 
		print("%lld nsec for %d iterations\n", stopnsec-startnsec, iter);
	print("%lld %d: Finalize\n", nsec(), node);
	MPI_Finalize();
}
