/*#include <stdio.h>
#include <stdlib.h>
#include <string.h>
*/
#include <u.h>
#include <libc.h>
#include "mpi.h"

enum {
	TAGcomm = 0,
	TAGtag,
	TAGsource,
	TAG
};

/* simple MPI for Plan 9 torus driver. This is a first cut. */
/* rank: rather than have to ask a daemon for rank, we're going to 
 * require that processes be started as follows: 
 * rank = z * (xsize + ysize) + y * xsize + x
 * Thus, rank0 is (0,0,0)
 * This disallows the dynamic stuff but nobody in their right mind uses
 * that anyway. We're going to avoid the DCMF entirely and C++ of course.
 */
struct bufpacket {
	struct  bufpacket *next, *prev;
	unsigned long tag[TAG];
	unsigned char data[];
};


int myproc = -1, nproc = -1, maxrank = -1;
char *myname = nil;
static int torusfd = -1;
struct bufpacket *buffered = nil;

void torussend(void *buf, int length, int rank, void *tag, int taglen);
int torusrecv(MPI_Status **status, void *buf, long length, void *tag, long taglen);
void torusinit(int *pmyproc, const int pnprocs);

void
panic(char *s)
{
	char *m = smprint("Fatal: %s", s);
	print("%s\n", m);
	exits(m);
}

/* we require that myproc be the first arg and nproc the second. */
int
MPI_Init(int *argc, char ***argv)
{
	int xyztorank(int x, int y, int z);
	extern int x, y, z;
	char **av = *argv;
	myname = av[0];
print( "myname %s av[1] %s\n", myname, av[1]);
	nproc = strtoul(av[1], 0, 10);
	*argc = *argc - 2;
	*argv = av + 2;
	torusinit(&myproc, nproc);
print( "x %d y %d z %d myproc %d xyztorank() %d\n", x, y, z, myproc, xyztorank(x,y,z));

	maxrank = nproc - 1;

	/* test */
	if (myproc != xyztorank(x,y,z)) {
		print( "myproc %d != xyztorank %d ", myproc, xyztorank(x,y,z));
		panic("bail");
	}
	return MPI_SUCCESS;
}

int
MPI_Finalize(void)
{
	return MPI_SUCCESS;
}

int
MPI_Comm_size(MPI_Comm x, int *s)
{
	switch(x) {
		case MPI_COMM_WORLD:
			*s = nproc;
			break;
		default:
			panic("comm size only implements MPI_COMM_WORLD\n");
	}
	return MPI_SUCCESS;
}

int
MPI_Comm_rank(MPI_Comm x, int *s)
{
	switch(x) {
		case MPI_COMM_WORLD:
			*s = myproc;
			break;
		default:
			panic("comm rank only implements MPI_COMM_WORLD\n");
	}
	return MPI_SUCCESS;
}

int
MPI_Barrier(MPI_Comm x)
{
	switch(x) {
		case MPI_COMM_WORLD:
			break;
		default:
			panic("MPI_Barrier only implements MPI_COMM_WORLD\n");
	}

	
	return MPI_SUCCESS;
}

double
MPI_Wtime(void)
{
	unsigned long long t;
	double f;
	static int fd = -1;

	if (fd < 0) {
		fd = open("/dev/bintime", 0);
		if (fd < 0)
			panic("Can't open /dev/bintime");
	}

	read(fd, &t, sizeof(t));
	f = t / 1000;
	return f;
}

/* the size of an mpi data type is the data type value >> 8 & 0xff. Just assign to u8int after the >> */
int
MPI_Send( void *buf, int num, MPI_Datatype datatype, int dest, 
              int tag, MPI_Comm comm )
{
	unsigned char nbytes = datatype >> 8;
	unsigned long torustag[TAG];
	int count;
	count = num * nbytes;
	switch(comm) {
		case MPI_COMM_WORLD:
			break;
		default:
			panic("MPI_Send only implements MPI_COMM_WORLD\n");
	}

	torustag[TAGcomm] = comm;
	torustag[TAGtag] = tag;
	torustag[TAGsource] = myproc;
	torussend(buf, count * nbytes, dest, torustag, sizeof(torustag));

	return MPI_SUCCESS;
}

int
MPI_Recv( void *buf, int num, MPI_Datatype datatype, int source, 
              int tag, MPI_Comm comm, MPI_Status *status )
{
	unsigned char nbytes = datatype >> 8;
	MPI_Status *s;
	int count;
	int size;
	count = num * nbytes;
	/* just allocate a bufpacket with room for headers and data and such  -- make it a nice page-multiple*/
	struct bufpacket *b;
	switch(comm) {
		case MPI_COMM_WORLD:
			break;
		default:
			panic("MPI_Recv only implements MPI_COMM_WORLD\n");
	}

	/* Well, first, let's see if it's here somewhere. */
	for(b = buffered; b; b = b->next) {
		if ((b->tag[TAGcomm] == comm) && (b->tag[TAGtag] == tag) && (b->tag[TAGsource] == source))
			goto got;
	}
print("GET IT!\n");
	/* get it */
	while (1) {
		b = calloc(1048576 + 4096, 1);
		/* for MPI, you pretty much have to take the worst case. It might be Some Other Packet */
		size = torusrecv(&s, b->data, 1048576, b->tag, sizeof(b->tag));
print("Torusrecv says %d bytes\n", size);
		if (size < 0)
			panic("torusrecv -1: %r");
		/* tag matching. */
print("Want (%x,%d,%d): REcv comm %lx tag %ld source %ld\n", comm, tag, source, b->tag[TAGcomm],b->tag[TAGtag], b->tag[TAGsource]);
		if ((b->tag[TAGcomm] == comm) && (b->tag[TAGtag] == tag) && (b->tag[TAGsource] == source))
			break;
		b->next = buffered;
		if (buffered)
			buffered->prev = b;
		buffered = b;
	}
		

got:
print("MPI recv: got it\n");
	if (b->next)
		b->next->prev = b->prev;
	if (b->prev)
		b->prev->next = b->next;
	status->count = count;
	status->cancelled = 0;
	status->MPI_SOURCE = b->tag[TAGsource];
	status->MPI_TAG = b->tag[TAGtag];
	status->MPI_ERROR = 0;
	memcpy(buf, b->data, count);
	free(b);
	return MPI_SUCCESS;
}
