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
	MPI_Status status;
	int i, fromz, fromrank;
	int torank;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	
	print("%lld Hello World from Node %d of %d\n", nsec(),node, nproc);

	sum[0] = node;
	/* if we have a 'z' address, send to (x,y,0) */
	if (z) {
		torank = xyztorank(x,y,0);
		print("%lld %d:(%d,%d,%d) sends %d\n", nsec(), node, x, y, z, node);
		MPI_Send(sum, 1, MPI_INT, torank, 1, MPI_COMM_WORLD);
	} else {
		/* gather up all our z >0 data */
		for(fromz = 1; fromz < zsize; fromz++){
			fromrank = xyztorank(x, y, fromz);
			MPI_Recv(nsum, 1, MPI_INT, fromrank, 1, MPI_COMM_WORLD, &status);
			sum[0] += nsum[0];
		}
		print("%lld AFTER Z: %d(%d, %d, %d): %d\n", nsec(), node, x, y, z, sum[0]);

		/* y? y o y? */
		if (y) {
			torank = xyztorank(x,0,0);
			/* send to our parent */
			print("%lld %d:(%d,%d,%d) sends %d\n", nsec(), node, x, y, z, sum[0]);
			MPI_Send(sum, 1, MPI_INT, torank, 1, MPI_COMM_WORLD);
		} else {
			/* This is the vector along the X axis. All nodes, including (0,0,0), gather up along 
			 * the y axis 
			 */
			int fromrank;
			int fromy;
			torank = 0;
			for(fromy = 1; fromy < ysize; fromy++){
				fromrank = xyztorank(x, fromy, 0);
				MPI_Recv(nsum, 1, MPI_INT, fromrank, 1, MPI_COMM_WORLD, &status);
				sum[0] += nsum[0];
			}
print("%lld %d:(%d,%d,%d) gets %d accum %d\n", nsec(), node, x, y, z, nsum[0], sum[0]);
			if (x) {
				/* send to our parent */
				print("%lld %d:(%d,%d,%d) sends %d to %d\n", nsec(), node, x, y, z, sum[0], torank);
				MPI_Send(sum, 1, MPI_INT, torank, 1, MPI_COMM_WORLD);
			} else {
					/* sum contains the sum from our y axis above */
					for(i = 1; i < xsize; i++) {
						MPI_Recv(nsum, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &status);
						sum[0] += nsum[0];
					print("%lld %d:(%d,%d,%d) recv %d to %d\n", nsec(), node, x, y, z, sum[0], i);
				}
			}
		}
	}

	if (! node) print("%lld Node %d (%d, %d, %d) computes sum as %d and MPI says sum is %d\n", nsec(), node, x, y, z, ((nproc-1)*nproc)/2, sum[0]);
//	torusstatus(1);
	/* this was simple before the deposit bit. It gets a tad harder when we use it. */
	if (1) {
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

	if (node) print("%lld Node %d (%d, %d, %d) computes sum as %d and MPI says sum is %d\n", nsec(), node, x, y, z, ((nproc-1)*nproc)/2, sum[0]);
	print("%lld %d: Finalize\n", nsec(), node);
	MPI_Finalize();
}
