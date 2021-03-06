/*The Parallel Hello World Program*/
#include "rompi.h"

/* tested may 17 2010 and works */
extern int torusdebug;

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

void
main(int argc, char **argv)
{
	int node, nproc;
	unsigned int *nodes;
	MPI_Status status;
	int i;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &node);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);

	nodes = malloc(nproc * sizeof(*nodes));
	
	print("Hello World from Node %d of %d\n",node, nproc);
	if (! node) {
		nodes[0] = node;
		MPI_Send(nodes, nproc, MPI_INT, 1, 1, MPI_COMM_WORLD);
		MPI_Recv(nodes, nproc, MPI_INT, nproc-1, 1, MPI_COMM_WORLD, &status);
	} else {
		MPI_Recv(nodes, nproc, MPI_INT, node-1, 1, MPI_COMM_WORLD, &status);
		nodes[node] = node;
		MPI_Send(nodes, nproc, MPI_INT, (node+1)%nproc, 1, MPI_COMM_WORLD);
	}
	print("%d: Finalize\n", node);
	MPI_Finalize();
	if (! node) {
		for(i = 0; i < nproc; i++)
			print("%d ", nodes[i]);
		print("\n");
	}
}
