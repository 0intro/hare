#include <u.h>
#include <libc.h>
#include <tos.h>

typedef struct Ring Ring;

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

void
panic(char *str)
{
	print("panic: %s: %r\n", str);
	exits("bad");
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

main(int argc, char *argv[])
{
	int fd = open("/dev/mpi", ORDWR);
	int nring = 4;
	int niter = 128;
	Ring *ring;
	u8int *rings;
	u32int cmd[3];
	vlong start, end, totalbytes;
	int i, j;
	u32int *up;
	double bw, seconds;

	if (fd < 0)
		panic("no /dev/mpi");

	if (argc > 1)
		nring = strtoul(argv[1], 0, 0);

	if (argc > 2)
		niter = strtoul(argv[2], 0, 0);

	/* allocate "num" ring structs */
	rings = malloc(64 * nring);
	/* for this test, just simulate nring 1M  reads */
	for(i = 0; i < nring; i++) {
		ring = (Ring *)&rings[i*64];
		ring->userdata = malloc(1048576);
		ring->count = 1048576/4;
		ring->datatype = 4;
//		print("ring %p userdata %p count %d\n", ring, ring->userdata, ring->count);
	}
	cmd[0] = 'r';
	cmd[1] = (u32int) rings;
	cmd[2] = (u32int) nring;
	print("Command %c %#lx %d\n", cmd[0], cmd[1], cmd[2]);
	write(fd, cmd, sizeof(cmd));
	cmd[0] = 'g';
	start = mynsec();
	for (i = 0; i < niter; i++){
		write(fd, cmd, sizeof(cmd[0]));
		for(j = 0; j < nring; j++) {
			ring = (Ring *)&rings[j*64];
			ring->done = 0;
		}
	}
	end = mynsec();
	print("Start %ullx end %ullx Total nsecs %ullx\n", start, end, end - start);
	print("total bytes transferred: %dMB\n", nring * niter);
	totalbytes = nring * niter * 1048576;
/*	seconds = (end - start);
	seconds /= 1e9;
	bw = totalbytes;
	bw /= seconds;*/
	print("nring %d niter %d %ulld bytes in %ulld nanoseconds\n", nring, niter, totalbytes, end-start);
//	print("Bandwidth: %g\n", bw);
}

/* qc testring.c&& ql -o testring testring.q&&date&&ls -l testring
/usr/rminnich//bin/386/scp testring surveyor.alcf.anl.gov:rompi
*/
