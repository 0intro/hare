
/*                  pong.c Generic Benchmark code
 *               Dave Turner - Ames Lab - July of 1994+++
 *
 *  Most Unix timers can't be trusted for very short times, so take this
 *  into account when looking at the results.  This code also only times
 *  a single message passing event for each size, so the results may vary
 *  between runs.  For more accurate measurements, grab NetPIPE from
 *  http://www.scl.ameslab.gov/ .
 */
#include "rompi.h"

typedef struct Ring Ring;
typedef unsigned long u32int;

struct Ring {
	u32int tag[3];
	void *userdata; /* for irecv */
	u32int count; /* number of units; bytecount is computed using datatype*/
	u32int datatype;
	u32int source;
	u32int itag;
	u32int comm;
	u32int status;
	u32int xmit; /* if this was an xmit request. */
	u32int done;
};

typedef uvlong ticks;

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

uvlong
mynsec(void)
{
	uvlong t;
	static int fd = -1;
	extern int pread(int, void *, int, unsigned long long);

	if (fd < 0) {
		fd = open("/dev/bintime", 0);
		if (fd < 0)
			panic("Can't open /dev/bintime");
	}

	pread(fd, &t, sizeof(t), 0ULL);
	return t;
}

int
main (int argc, char **argv)
{
    double bw;
    int myproc, nprocs, done;
	/* weird issue: malloc gets wrong address. Don't use it for now */

    int datablocksize = 32768;
    unsigned char *d = malloc(1024);
printf("d is %p\n", d);

    int fd = open("/dev/mpi", 2);
    int cfd = open("/dev/torusctl", 2);
	 int nring = 512;
	 int niter = 1;
	 Ring *ring;
	 u8int *rings;
	 u32int cmd[3];
	 vlong start, end, totalbytes;
	 int i, j, ringsize;
	char *debugcmd = "debug 48";
	int debuglen = strlen(debugcmd);
	int ctlfd;
	char ctlcmd[128];
	char *wire="wired 1";
	int wlen = strlen(wire);

	 if (fd < 0)
		 panic("no /dev/mpi");
	 if (cfd < 0)
		 panic("no /dev/torusctl");
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &myproc);

   printf("Hello from %d of %d\n", myproc, nprocs);

	if (argc > 1)
		nring = strtoul(argv[1], 0, 0);

	if (argc > 2)
		niter = strtoul(argv[2], 0, 0);

	/* allocate "num" ring structs */
	ringsize = 64 * nring * sizeof(*ring);
	rings = malloc(ringsize);
print("Rings %p\n", rings);
	memset(rings, 0, ringsize);
	/* for this test, just simulate nring 1M  reads */
	for(i = 0; i < nring; i++) {
		ring = (Ring *)&rings[i*64];
		ring->userdata = malloc(datablocksize);
		ring->count = datablocksize/4;
		ring->datatype = 4;
		ring->xmit = myproc;
//		printf("ring %p userdata %p count %ld\n", ring, ring->userdata, ring->count);
	}
	cmd[0] = 'r';
	cmd[1] = (u32int) rings;
	cmd[2] = (u32int) nring;
	printf("Command %ld %#lx %ld\n", cmd[0], cmd[1], cmd[2]);
	/* wire us to core 3 */
	sprintf(ctlcmd, "/proc/%d/ctl", getpid());
	print("ctlcmd is %s\n", ctlcmd);
	ctlfd = open(ctlcmd, 2);
	print("ctlfd  is %d\n", ctlfd);
	if (ctlfd < 0) {
		perror(ctlcmd);
		exit(1);
	}
	if (write(ctlfd, wire, wlen) < wlen) {
		perror(wire);
		exit(1);
	}
   MPI_Barrier(MPI_COMM_WORLD);
	if (myproc > 1) 
		goto done;
//	if (!myproc)
	if (0)
	if (write(cfd, debugcmd, debuglen) < debuglen) {
		printf("Debug %s failed\n", debugcmd);
		goto done;
	}
	//printf("after barrier, proceed\n");
	write(fd, cmd, sizeof(cmd));
	start = getticks();
	cmd[0] = 'g';

	if (myproc)
	for(j = 0; j < nring; j++) {
		int i;
		unsigned long long *u;
		ring = (Ring *)&rings[j*64];
		u = ring->userdata;
		u[2] = getticks();
		for(i = 3; i < 30; i++)
			u[i] = j*30+i;
//printf("%lld\n", u[0]);
	}
	for (i = 0; i < niter; i++){
		if (myproc)
		if (write(fd, cmd, sizeof(cmd[0])) < sizeof(cmd[0])) {
			print("Things did not go so well\n");
		}
		//if (!myproc)
		for(done = 0; done < nring;) {
			for(j = 0; j < nring; j++) {
				if (ring->done) {
					done++;
					/* this did not help. Setting CI on 
					 * TLB did. 
					if (! myproc) 
						__asm__ __volatile__ ("msync");
					 */
				}
			}
		}
		for(j = 0; j < nring; j++) {
			unsigned long long *u;
			ring = (Ring *)&rings[j*64];
			ring->done = 0;
			u = ring->userdata;
			if (! myproc)
				u[5] = getticks();
		}
	}
	end = getticks();
	printf("Start %#llx end %#llx Total nsecs %#llx\n", start, end, end - start);
	printf("total bytes transferred: %dB\n", nring * niter*256);
	totalbytes = nring * niter * 256;
/*	seconds = (end - start);
	seconds /= 1e9;
	bw = totalbytes;
	bw /= seconds;*/
	printf("nring %d niter %d %lld bytes in %lld nanoseconds\n", nring, niter, totalbytes, end-start);
	printf("Bandwidth: %g\n", bw);
	for(j = 0; j < nring; j++) {
		unsigned long long *u;
		ring = (Ring *)&rings[j*64];
		u = ring->userdata;
		printf("%llu %llu %llu %llu\n", u[0], u[1], u[2], u[3]);
	}
	/* DUMP */
	if (1)
	for(j = 0; j < nring; j++) {
		int i;
		unsigned long long *u;
		ring = (Ring *)&rings[j*64];
		u = ring->userdata;
		printf("%p ", u);
		for(i = 0; i < 31; i++) 
			printf("%llu ", u[i]);
		printf("\n");
	}
done:
    /* just spin so we don't get lots of output crap ... */
    while (myproc > 1) ;
    return 0;
}

