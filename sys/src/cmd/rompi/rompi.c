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
int rompidebug = 0; /* 2: recv, 1: send */
char *myname = nil;
static int torusfd = -1;
struct bufpacket buffered = {&buffered, &buffered};

void torussend(void *buf, int length, int x, int y, int z, int deposit, void *tag, int taglen);
int torusrecv(void *buf, long length, void *tag, long taglen);
void torusinit(int *pmyproc, const int pnprocs);
void ranktoxyz(int rank, int *x, int *y, int *z);
int xyztorank(int x, int y, int z);
extern int x, y, z, xsize, ysize, zsize;

void
buf_add(struct bufpacket *new)
{
//print("Add %p: %p %p %p %p: ", new, buffered.next->prev, buffered.next, &buffered, buffered.next);
	buffered.next->prev = new;
	new->next = buffered.next;
	new->prev = &buffered;
	buffered.next = new;
//print("Added: %p %p %p %p\n", buffered.next->prev, buffered.next, &buffered, buffered.next);
}

void
buf_del(struct bufpacket *r)
{
//print("DEL: %p, %p %p\n", r, r->next, r->prev);
//print("DEL %p: %p %p: ", r, r->next->prev, r->prev->next);
	r->next->prev = r->prev;
	r->prev->next = r->next;
//print(": %p %p\n", r->next->prev, r->prev->next);
}

int
buf_empty(void)
{
	return buffered.next == &buffered;
}

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
//print( "myname %s av[1] %s\n", myname, av[1]);
	nproc = strtoul(av[1], 0, 10);
	*argc = *argc - 2;
	*argv = av + 2;
	torusinit(&myproc, nproc);
//print( "x %d y %d z %d myproc %d xyztorank() %d\n", x, y, z, myproc, xyztorank(x,y,z));

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
	int x, y, z;
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
	ranktoxyz(dest, &x, &y, &z);
	torussend(buf, count * nbytes, x, y, z, 0, torustag, sizeof(torustag));

	return MPI_SUCCESS;
}

int
MPI_Recv( void *buf, int num, MPI_Datatype datatype, int source, 
              int tag, MPI_Comm comm, MPI_Status *status )
{
	unsigned char nbytes = datatype >> 8;
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

//print("WANT %d, buffer %s\n", source, buf_empty() ? "empty":"");
	/* Well, first, let's see if it's here somewhere. */
	for(b = buffered.next; b != &buffered; b = b->next) {
		if (rompidebug &2) print("Check %p tag %ld source %ld\n", b, b->tag[TAGtag] , b->tag[TAGsource]);
		if ((b->tag[TAGcomm] == comm) && (b->tag[TAGtag] == tag) && (b->tag[TAGsource] == source)){
			buf_del(b);
			goto got;
			}
	}
//print("GET IT!\n");
	/* get it */
	while (1) {
		b = calloc(1048576 + 4096, 1);
		/* for MPI, you pretty much have to take the worst case. It might be Some Other Packet */
//print("IR %p\n", b);
		size = torusrecv(b->data, 1048576, b->tag, sizeof(b->tag));
///print("Torusrecv says %d bytes\n", size);
		if (size < 0)
			panic("torusrecv -1: %r");
		/* tag matching. */
		if (rompidebug &2){
			int rx, ry, rz;
			ranktoxyz(b->tag[TAGsource], &rx, &ry, &rz);
			print("Want (%x,%d,%d): REcv comm %lx tag %ld source %ld(%d, %d, %d)\n", comm, tag, source, b->tag[TAGcomm],b->tag[TAGtag], b->tag[TAGsource], rx, ry, rz);
		}
		if ((b->tag[TAGcomm] == comm) && (b->tag[TAGtag] == tag  || tag == MPI_ANY_TAG) && (b->tag[TAGsource] == source || source == MPI_ANY_SOURCE))
			break;
		/* for someone else ... */
//print("Add %d: ", b->tag[TAGsource]);
		buf_add(b);
	}
		

got:
	if (rompidebug &2)print("MPI recv: got it\n");
	status->count = count;
	status->cancelled = 0;
	status->MPI_SOURCE = b->tag[TAGsource];
	status->MPI_TAG = b->tag[TAGtag];
	status->MPI_ERROR = 0;
	memcpy(buf, b->data, count);
	free(b);
	return MPI_SUCCESS;
}

/* the size of an mpi data type is the data type value >> 8 & 0xff. Just assign to u8int after the >> */
/* ah, MPI
sendbuf	address of send buffer (choice) 
count	number of elements in send buffer (integer) 
datatype	data type of elements of send buffer (handle) 
op	reduce operation (handle) 
root	rank of root process (integer) 
comm	communicator (handle)  */
int reduce_end ( void *buf, int num, MPI_Datatype datatype, int /*root*/, 
               MPI_Comm comm,  MPI_Status *status)
{

	unsigned char nbytes = datatype >> 8;
	unsigned long torustag[TAG];
	int count;
	int fromx, fromy, fromz, fromrank;
	count = num * nbytes;
	int reducetag = 0xaa55;
	switch(comm) {
		case MPI_COMM_WORLD:
			break;
		default:
			panic("MPI_Send only implements MPI_COMM_WORLD\n");
	}

	torustag[TAGcomm] = comm;
	torustag[TAGtag] = reducetag;
	torustag[TAGsource] = myproc;

	if(rompidebug&4) print("%lld reduce_end: %d(%d, %d, %d)\n", nsec(), myproc, x, y, z);
	if (myproc) {
		/* who we get it from depends on who we are. */
		/* this shows some bad design. We compute x,y,z, turn it into rank, 
		 * call mpirecv, it turns our rakn into x,y,z.
		 */
		fromz = z;
		fromy = y;
		fromx =x;
		if (z)
			fromz = 0;
		else if (y) 
			fromz = fromy = 0;
		else if (x)
			fromz = fromy = fromx = 0;
		fromrank = xyztorank(fromx, fromy, fromz);
	if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): wait from (%d, %d, %d)\n", nsec(), myproc, x, y, z, fromx, fromy, fromz);
		MPI_Recv(buf, 1, MPI_INT, fromrank, reducetag, MPI_COMM_WORLD, status);
	if(rompidebug&4) print("%lld reduce_end: %d: Got from %d\n", nsec(), myproc, fromrank);
	}

	/* OK, we got it, we just need to do one send to our "axis" (of evil?) */
	/* do me a favor here. You could rewrite this stuff to be "more optimal". 
	 * optimization is what compilers do. I've made an effort to make this easy for
	 * someone to read if they come along later. Please avoid any temptation
	 * to clever-ize this code unless it also makes it easier to follow. 
	 */
	if (z) {
	} else if (y) {
	if(rompidebug&4) print("%lld reduce_end %d(%d, %d, %d): send to (%d, %d, %d)\n", nsec(), myproc, x, y, z, x, y, zsize-1);
		torustag[TAGsource] = myproc;
		torussend(buf, count, x, y, zsize-1, 1, torustag, sizeof(torustag));
	}
	else if (x) {
		/* send down BOTH our y and z axis */
		torustag[TAGsource] = myproc;
	if(rompidebug&4) print("%lld reduce_end %d(%d, %d, %d): send to (%d, %d, %d)\n", nsec(), myproc, x, y, z, x, ysize-1, 0);
		torussend(buf, count, x, ysize-1, 0, 1, torustag, sizeof(torustag));
	if(rompidebug&4) print("%lld reduce_end %d(%d, %d, %d): send to (%d, %d, %d)\n", nsec(), myproc, x, y, z, x, y, zsize-1);
		torussend(buf, count, x, y, zsize-1, 1, torustag, sizeof(torustag));
	} else {
		/* send down x, y, z */
	if(rompidebug&4) print("%lld reduce_end %d(%d, %d, %d): send to (%d, %d, %d)\n", nsec(), myproc, x, y, z, xsize-1, 0, 0);
		torussend(buf, count, xsize-1, 0, 0, 1, torustag, sizeof(torustag));
	if(rompidebug&4) print("%lld reduce_end %d(%d, %d, %d): send to (%d, %d, %d)\n", nsec(), myproc, x, y, z, x, ysize-1, 0);
		torussend(buf, count, x, ysize-1, 0, 1, torustag, sizeof(torustag));
	if(rompidebug&4) print("%lld reduce_end %d(%d, %d, %d): send to (%d, %d, %d)\n", nsec(), myproc, x, y, z, x, y, zsize-1);
		torussend(buf, count, x, y, zsize-1, 1, torustag, sizeof(torustag));
	}

	return MPI_SUCCESS;
}