#include "rompi.h"

enum {
	Sk		= 0x01,			/* Skip Checksum */

	Pid0		= 0x01,			/* Destination Group FIFO MSb */
	Dp		= 0x02,			/* Multicast Deposit */
	Hzm		= 0x04,			/* Z- Hint */
	Hzp		= 0x08,			/* Z+ Hint */
	Hym		= 0x10,			/* Y- Hint */
	Hyp		= 0x20,			/* Y+ Hint */
	Hxm		= 0x40,			/* X- Hint */
	Hxp		= 0x80,			/* X+ Hint */

	Vcd0		= 0x00,			/* Dynamic 0 VC */
	Vcd1		= 0x01,			/* Dynamic 1 VC */
	Vcbn		= 0x02,			/* Deterministic Bubble VC */
	Vcbp		= 0x03,			/* Deterministic Priority VC */
	Dy		= 0x04,			/* Dynamic Routing */
	Dm		= 0x08,			/* DMA Mode */
	Pid1		= 0x10,			/* Destination Group FIFO LSb */
};

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
	void *userdata; /* for irecv */
	int count; /* for irecv */
	MPI_Datatype datatype;
	int num; /* redundant, but we'll figure it out later. */
	int source;
	int itag;
	MPI_Comm comm;
	MPI_Status status;
	int xmit; /* if this was an xmit request. */
	unsigned char data[];
};


int myproc = -1, nproc = -1, maxrank = -1;
int rompidebug = 0; /* 16: print # packets read, 2: recv, 1: send */
char *myname = nil;
extern int torusfd;
struct bufpacket buffered = {&buffered, &buffered};

void torussend(void *buf, int length, int x, int y, int z, int deposit, void *tag, int taglen);
int torusrecv(void *buf, long length, void *tag, long taglen);
void torusinit(int *pmyproc, const int pnprocs);
void ranktoxyz(int rank, int *x, int *y, int *z);
int xyztorank(int x, int y, int z);
extern int x, y, z, xsize, ysize, zsize;
u64int	tbget(void);

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
    void exits(char *);
	char *m = smprint("Fatal: %s", s);
	print("%s\n", m);
	exits(m);
}

int MPI_Abort( MPI_Comm comm, int errorcode )
{
	panic("MPI_Abort: aborting as abort is not implemented\n");
	exit(errorcode);
}

int MPI_Comm_dup ( 
        MPI_Comm comm, 
        MPI_Comm *comm_out )
{
	/* yes well ... we have no real use for this yet. Maybe never. 
	 * we can hope so anyway.
	 */
	*comm_out = comm;
	return MPI_SUCCESS;
}
int MPI_Comm_split ( MPI_Comm comm, int color, int key, MPI_Comm *comm_out )
{
	panic("no split yet. Or ever. \n");
	return -1;
}

int MPI_Comm_free ( MPI_Comm *commp )
{
	/* that was easy */
	return MPI_SUCCESS;
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
			*s = nproc;
//			panic("comm size only implements MPI_COMM_WORLD\n");
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

double
MPI_Wtime(void)
{
	unsigned long long t;
	double f;
	static int fd = -1;
	extern int pread(int, void *, int, unsigned long long);

	if (fd < 0) {
		fd = open("/dev/bintime", 0);
		if (fd < 0)
			panic("Can't open /dev/bintime");
	}

	pread(fd, &t, sizeof(t), 0ULL);
	f = t / 1000000000;
	return f;
}

/* the size of an mpi data type is the data type value >> 8 & 0xff. Just assign to u8int after the >> */

/* if we like this function merge it in with the one below later. */
int
MPI_Sendxyz( void *buf, int num, MPI_Datatype datatype, int x, int y, int z, int hint,
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
	torussend(buf, count, x, y, z, hint, torustag, sizeof(torustag));

	return MPI_SUCCESS;
}

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
	torussend(buf, count, x, y, z, 0, torustag, sizeof(torustag));

	return MPI_SUCCESS;
}

/* we don't do anything with the request yet. Sorry. */
int MPI_Isend( void *buf, int num, MPI_Datatype datatype, int dest, int tag,
               MPI_Comm comm, MPI_Request *request )
{
	unsigned char nbytes = datatype >> 8;
	unsigned long torustag[TAG];
	int count;
	int x, y, z;
	struct bufpacket *b;

	b = calloc(sizeof(*b), 1);
	if (! b) {
		panic("calloc in recv returned 0");
	}
	count = num * nbytes;
	torustag[TAGcomm] = comm;
	torustag[TAGtag] = tag;
	torustag[TAGsource] = myproc;
	ranktoxyz(dest, &x, &y, &z);
	torussend(buf, count, x, y, z, 0, torustag, sizeof(torustag));

	b->xmit = 1;
	*request = (MPI_Request )b;	
	return MPI_SUCCESS;
}
/* the kernel is catching these things. 
 * Just create the descriptors and save them in the request array. 
 */
int
MPI_Irecv( void *buf, int num, MPI_Datatype datatype, int source, 
              int tag, MPI_Comm comm, MPI_Request *request )
{
	unsigned char nbytes = datatype >> 8;
	int count;

	count = num * nbytes;
	struct bufpacket *b;

	b = calloc(sizeof(*b), 1);
	if (! b) {
		panic("calloc in recv returned 0");
	}
	b->userdata = buf;
	b->count = count;
	b->num = num;
	b->source = source;
	b->itag = tag;
	b->comm = comm;

	*request = (MPI_Request )b;	
	return MPI_SUCCESS;
}

int MPI_Waitall(
        int count, 
        MPI_Request array_of_requests[], 
        MPI_Status array_of_statuses[] )
{

	/* just allocate a bufpacket with room for headers and data and such  -- make it a nice page-multiple*/
	struct bufpacket *req;
	MPI_Request *curreq;
	for(curreq = array_of_requests; curreq - array_of_requests < count; curreq++) {
		req = (struct bufpacket *) *curreq;
		/* if it's a send, we're done; no way to check yet */
		if (req->xmit)
			continue;
		MPI_Recv(req->userdata, req->num, req->datatype, 
				req->source, req->itag, req->comm, &req->status);
		free(req);
	}

	if (rompidebug &2)print("MPI WaitAll: done\n");
	return MPI_SUCCESS;
}

/* todo: Should have implemented recv as a special kind of irecv+waitall. 
 * reason is that irecv is the common case. OH well, live and learn.
 */
int
MPI_Recv( void *buf, int num, MPI_Datatype datatype, int source, 
              int tag, MPI_Comm comm, MPI_Status *status )
{
	static int nthpacket = 0;
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
		if ((b->tag[TAGcomm] == comm) && (b->tag[TAGtag] == tag  || tag == MPI_ANY_TAG) && (b->tag[TAGsource] == source || source == MPI_ANY_SOURCE)){
			buf_del(b);
			goto got;
			}
	}
//print("GET IT!\n");
	/* get it */
	while (1) {
		b = calloc(1048576 + 4096, 1);
		if (! b) {
			panic("calloc in recv returned 0");
		}

		/* for MPI, you pretty much have to take the worst case. It might be Some Other Packet */
//print("IR %p\n", b);
		size = torusrecv(b->data, 1048576, b->tag, sizeof(b->tag));
		if (size < 0)
			panic("torusrecv -1: %r");
		nthpacket++;
		if (rompidebug & 16)
			print("%d'th packet\n", nthpacket);
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

int
reduce_end_slow ( void *buf, int num, MPI_Datatype datatype, int root, 
               MPI_Comm comm,  MPI_Status *status)
{
	int tox, toy, toz;
	int torank, fromrank;


	unsigned long torustag[TAG];
	int reducetag = 0xcafebabe;

	torustag[TAGcomm] = comm;
	torustag[TAGtag] = reducetag;
	torustag[TAGsource] = myproc;

	if(rompidebug&8) print("%lld reduce_end: %d(%d, %d, %d)\n", tbget(), myproc, x, y, z);

	/* the easy cases: z > 0 and the origin */
	if (z) {
		/* get the sum from rank x,y,0 */
		fromrank = xyztorank(x,y,0);
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): wait from %d(%d, %d, %d)\n", tbget(), myproc, x, y, z, fromrank, x, 0, 0);
		if (rompidebug&128) print("DOT:%lld b%d->b%d;\n", tbget(), fromrank, myproc);
		MPI_Recv(buf, num, datatype, fromrank, reducetag, MPI_COMM_WORLD, status);
	} else if (! myproc) {
		for(tox = 1; tox < xsize; tox++){
			torank = xyztorank(tox, 0, 0);
			MPI_Send(buf, num, datatype, torank, 1, comm);
		}
		for(toy = 1; toy < ysize; toy++){
			torank = xyztorank(0, toy, 0);
			MPI_Send(buf, num, datatype, torank, 1, comm);
		}
		for(toz = 1; toz < zsize; toz++){
			torank = xyztorank(0, 0, toz);
			MPI_Send(buf, num, datatype, torank, 1, comm);
		}
	} else {
		if (! y) {
			/* get the sum from rank 0 */
			fromrank=xyztorank(0,0,0);
			if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): wait from %d(%d, %d, %d)\n", tbget(), myproc, x, y, z, fromrank, x, 0, 0);
			if (rompidebug&128) print("DOT:%lld b%d->b%d;\n", tbget(), fromrank, myproc);
			MPI_Recv(buf, num, datatype, fromrank, reducetag, MPI_COMM_WORLD, status);
			/* send out our y >0 data */
			for(toy = 1; toy < ysize; toy++){
				torank = xyztorank(x, toy, 0);
				MPI_Send(buf, num, datatype, torank, 1, comm);
			}
			for(toz = 1; toz < zsize; toz++){
				torank = xyztorank(x, 0, toz);
				MPI_Send(buf, num, datatype, torank, 1, comm);
			}
		} else {
			/* get the sum from rank x,0,0 */
			fromrank=xyztorank(x,0,0);
			if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): wait from %d(%d, %d, %d)\n", tbget(), myproc, x, y, z, fromrank, x, 0, 0);
			if (rompidebug&128) print("DOT:%lld b%d->b%d;\n", tbget(), fromrank, myproc);
			MPI_Recv(buf, num, datatype, fromrank, reducetag, MPI_COMM_WORLD, status);
			/* send out our z >0 data */
			for(toz = 1; toz < zsize; toz++){
				torank = xyztorank(x, y, toz);
				MPI_Send(buf, num, datatype, torank, 1, comm);
			}
		}
	}
	return MPI_SUCCESS;

}
int reduce_end ( void *buf, int num, MPI_Datatype datatype, int root, 
               MPI_Comm comm,  MPI_Status *status)
{
	int fromrank;
	unsigned long torustag[TAG];
	int reducetag = 1;

	torustag[TAGcomm] = comm;
	torustag[TAGtag] = reducetag;
	torustag[TAGsource] = myproc;

	if(rompidebug&8) print("%lld reduce_end: %d(%d, %d, %d)\n", tbget(), myproc, x, y, z);

	//rompidebug |= 2;
	/* the easy cases: z > 0 and the origin */
	if (z) {
		/* get the sum from rank x,y,0 */
		fromrank = xyztorank(x,y,0);
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): wait from %d(%d, %d, %d)\n", tbget(), myproc, x, y, z, fromrank, x, 0, 0);
		if (rompidebug&128) print("DOT:%lld b%d->b%d;\n", tbget(), fromrank, myproc);
		MPI_Recv(buf, num, datatype, fromrank, reducetag, MPI_COMM_WORLD, status);
	} else if (! myproc) {
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): send to (%d, %d, %d)\n", tbget(), myproc, x, y, z, xsize-1, 0, 0);
		MPI_Sendxyz(buf, num, datatype, xsize-1, 0, 0, Hxp|Dp, 1, comm);
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): send to (%d, %d, %d)\n", tbget(), myproc, x, y, z, 0, ysize-1, 0);
		MPI_Sendxyz(buf, num, datatype, 0, ysize-1, 0, Hyp|Dp, 1, comm);
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): send to (%d, %d, %d)\n", tbget(), myproc, x, y, z, 0, 0, zsize-1);
		MPI_Sendxyz(buf, num, datatype, 0, 0, zsize-1, Hzp|Dp, 1, comm);
	} else {
		if (! y) {
			/* get the sum from rank 0 */
			fromrank=xyztorank(0,0,0);
			if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): wait from %d(%d, %d, %d)\n", tbget(), myproc, x, y, z, fromrank, 0, 0, 0);
			if (rompidebug&128) print("DOT:%lld b%d->b%d;\n", tbget(), fromrank, myproc);
			MPI_Recv(buf, num, datatype, fromrank, reducetag, MPI_COMM_WORLD, status);
			/* send out our y >0 data */
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): send to (%d, %d, %d)\n", tbget(), myproc, x, y, z, x, ysize-1, 0);
			MPI_Sendxyz(buf, num, datatype, x, ysize-1, 0, Hyp|Dp, 1, comm);
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): send to (%d, %d, %d)\n", tbget(), myproc, x, y, z, x, 0, zsize-1);
			MPI_Sendxyz(buf, num, datatype, x, 0, zsize-1, Hzp|Dp, 1, comm);
		} else {
			/* get the sum from rank x,0,0 */
			fromrank=xyztorank(x,0,0);
			if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): wait from %d(%d, %d, %d)\n", tbget(), myproc, x, y, z, fromrank, x, 0, 0);
			if (rompidebug&128) print("DOT:%lld b%d->b%d;\n", tbget(), fromrank, myproc);
			MPI_Recv(buf, num, datatype, fromrank, reducetag, MPI_COMM_WORLD, status);
			/* send out our z >0 data */
		if(rompidebug&4) print("%lld reduce_end: %d(%d,%d,%d): send to (%d, %d, %d)\n", tbget(), myproc, x, y, z, x, y, zsize-1);
			MPI_Sendxyz(buf, num, datatype, x, y, zsize-1, Hzp|Dp, 1, comm);
		}
	}
	return MPI_SUCCESS;
}

typedef void (*mpi_op)(void *, void *, int, MPI_Datatype);

/* int sum -- length matters not */
void
opsum(void *dst, void *src, int len, MPI_Datatype datatype)
{
	switch(datatype) {
	    case MPI_INT:{
		int *is = src;
		int *id = dst;
		int tmp = *is;
		if (rompidebug & 4) print("%lld %d(%d,%d,%d): ISUM %d+%d=%d\n", tbget(), myproc, x,y,z,*id, *is, *id+tmp);
		*id += tmp;
		    break;
	    }
	case MPI_DOUBLE:{
		double *is = src;
		double *id = dst;
		double tmp = *is;
		if (rompidebug & 4) print("%g %d(%d,%d,%d): DSUM %g+%g=", tbget(), myproc, x,y,z,*id, tmp);
		*id += tmp;
		if (rompidebug & 4) print("=%g\n", *id);
		     break;
	     }
	}
}

mpi_op 
findop(MPI_Op op)
{
	switch((int) op){
	case MPI_SUM    : return opsum;
	default: 
	case MPI_MAX    :
	case MPI_MIN    :
	case MPI_PROD   :
	case MPI_LAND   :
	case MPI_BAND   :
	case MPI_LOR    :
	case MPI_BOR    :
	case MPI_LXOR   :
	case MPI_BXOR   :
	case MPI_MINLOC :
	case MPI_MAXLOC :
	case MPI_REPLACE:
		return nil;
	}
}

/* we're careful not to modify sendbuf, though it is unknown if that matters. */
int MPI_Reduce ( void *sendbuf, void *recvbuf, int count, 
                MPI_Datatype datatype, MPI_Op iop, int root, MPI_Comm comm )
{
	MPI_Status status;
	unsigned char typesize = datatype >> 8;
	int i, fromz, fromrank;
	int torank;
	mpi_op op;
	void *tmp;
	int nbytes = count * typesize;
	int *sum, *nsum; // hack 

	op = findop(iop);
	if (!iop)
		panic("Bad op in MPI_Reduce");

	tmp = malloc(nbytes);
	sum = tmp;
	nsum = recvbuf;
	memmove(tmp, sendbuf, nbytes);

	/* if we have a 'z' address, send to (x,y,0) */
	if (z) {
		torank = xyztorank(x,y,0);
		if (rompidebug&4)
			print("%lld %d:(%d,%d,%d) Z != 0 sends %d to %d\n", tbget(), myproc, x, y, z, myproc, torank);
		if (rompidebug&128) print("DOT:%lld a%d->a%d;\n", tbget(), myproc, torank);
		MPI_Send(tmp, 1, datatype, torank, 1, comm);
	} else {
		/* gather up all our z >0 data */
		for(fromz = 1; fromz < zsize; fromz++){
			fromrank = xyztorank(x, y, fromz);
			MPI_Recv(recvbuf, 1, datatype, fromrank, 1, comm, &status);
			op(tmp, recvbuf, count, datatype);
		}
		if (rompidebug&4)
			print("%lld RECV Z==0 : %d(%d, %d, %d): %d\n", tbget(), myproc, x, y, z, sum[0]);

		/* y? y o y? */
		if (y) {
			torank = xyztorank(x,0,0);
			/* send to our parent */
			if (rompidebug&4)
				print("%lld %d:(%d,%d,%d) Y != 0 sends %d to %d\n", tbget(), myproc, x, y, z, sum[0], torank);
			if (rompidebug&128) print("DOT:%lld a%d->a%d;\n", tbget(), myproc, torank);
			MPI_Send(tmp, 1, datatype, torank, 1, comm);
		} else {
			/* This is the vector along the X axis. All myprocs, including (0,0,0), gather up along 
			 * the y axis 
			 */
			int fromrank;
			int fromy;
			torank = 0;
			for(fromy = 1; fromy < ysize; fromy++){
				fromrank = xyztorank(x, fromy, 0);
				MPI_Recv(recvbuf, 1, datatype, fromrank, 1, comm, &status);
				op(tmp, recvbuf, count, datatype);
			}
			if (rompidebug&4)
				print("%lld %d RECV Y==0:(%d,%d,%d) gets %d accum %d\n", tbget(), myproc, x, y, z, nsum[0], sum[0]);
			if (x) {
				/* send to our parent */
				if (rompidebug&4)
					print("%lld %d X != 0 :(%d,%d,%d) sends %d to %d\n", tbget(), myproc, x, y, z, sum[0], torank);
				if (rompidebug&128) print("DOT:%lld a%d->a%d;\n", tbget(), myproc, torank);
				MPI_Send(tmp, 1, datatype, torank, 1, comm);
			} else {
					/* sum contains the sum from our y axis above */
					for(i = 1; i < xsize; i++) {
						MPI_Recv(recvbuf, 1, datatype, i, 1, comm, &status);
						op(tmp, recvbuf, count, datatype);
						if (rompidebug&4)
							print("%lld %d X == 0 :(%d,%d,%d) recv %d to %d\n", tbget(), myproc, x, y, z, sum[0], i);
				}
			} /* X == Y == Z == 0 */
		} /* Y == Z == 0 */
	} /* Z == 0 */

	if (! myproc){
		memmove(recvbuf, tmp, nbytes);
		if (rompidebug&4)
			print("%lld reduce: %d(%d,%d,%d): gather done, send SUM %d\n", tbget(), myproc, x, y, z, sum[0]);
	}

	return reduce_end (recvbuf, count, datatype, root, comm, &status);
}

int MPI_Allreduce ( void *sendbuf, void *recvbuf, int count, 
                   MPI_Datatype datatype, MPI_Op op, MPI_Comm comm )
{
	return MPI_Reduce ( sendbuf, recvbuf, count, datatype, op, 0, comm);
}
/* use a reduce, it's pretty good. Our barrier driver is broken at this point */
int
MPI_Barrier (MPI_Comm comm )
{
	unsigned long sum[1], nsum[1];
	MPI_Allreduce (sum, nsum, 1, MPI_INT, MPI_SUM, comm);
	return MPI_SUCCESS;
}

int MPI_Bcast ( void *buf, int count, MPI_Datatype datatype, int root, 
               MPI_Comm comm )
{
	MPI_Status status;
	reduce_end(buf, count, datatype, root, comm, &status);
	return MPI_SUCCESS;
}

