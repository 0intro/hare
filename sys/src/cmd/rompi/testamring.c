
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
enum {
        X               = 0,                    /* dimension */
        Y               = 1,
        Z               = 2,
        N               = 3,

        Chunk           = 32,                   /* granularity of FIFO */
        Pchunk          = 8,                    /* Chunks in a packet */

        Quad            = 16,
        Tpkthdrlen      = 16,
};



typedef uvlong ticks;

typedef struct Tpkt Tpkt;
struct Tpkt {
	/* hardware header */
	u8int	sk;					/* Skip Checksum Control */
	u8int	hint;				/* Hint|Dp|Pid0 */
	u8int	size;				/* Size|Pid1|Dm|Dy|VC */
	u8int	dst[N];				/* Destination Coordinates */
	u8int	_6_[2];				/* reserved */

	/* hardware and software headers */
	union{
		struct{	/* direct-put DMA mode */
			u8int	paoff[4];		/* put address offset (updated by hw for large packets) */
			u8int	rdmaid;		/* RDMA counter ID */
			u8int	valid;		/* valid bytes in payload (updated by hw) */
			u8int	rgflags;		/* remote get flags */
			u8int	idmaid;		/* IDMA FIFO ID */
		};
		struct{	/* memory FIFO mode */
			u8int	putoff[4];		/* put offset (updated by hw) */
			u8int	other[4];		/* software */
		};
	};
	u8int	payload[];
};

struct AmRing {
	u8int *base;
	int size;
	int prod;
	u8int _116[128-12];
	int con;
	u8int _124[128-4];
};


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

/* just test the kernel-based fast inject mechanism. Let's hope 
 * it's not going to require we drain it all. Because we're not going to. 
 */
int
main (int argc, char **argv)
{
    int cfd;

    double bw;
    int myproc, nprocs, done;
    /* weird issue: malloc gets wrong address. Don't use it for now */
    unsigned char *b = (void *) 0x20000000;
    int datablocksize = 32768;
    vlong start, end;
    int ctlfd;
    char ctlcmd[128];
    volatile vlong *result, *resultbase;
    unsigned long ptr;
    int amt;
    /* the wiring is crucial. We only got to about 2.9 microseconds 
     * per send without wiring. With wiring, we got to 1.165 microseconds
     * per send. What is likely going on is inj interrupts taking time
     * and slowing down the rest of the work. 
     */
    char wire[32];
    int wlen;
    int core = 0;
    int num = 1024, i;
    int role;
    
    Tpkt *pkt = (Tpkt *) b;
    unsigned char *data = &b[1024];
    result = (vlong *)(&b[128*1024]);
    struct AmRing *amr;
	unsigned long amrbase;
	
    
	/* reguire our ID */
	if (argc < 2) {
		print("%s 0-or-1 [core (default %d) [num-iter (default %d)]]\n", argv[0], core, num);
		exit(1);
	}
	role = strtol(argv[1], 0, 0);
	if (argc > 2) {
		core = strtoul(argv[2], 0, 0);
	}
	if (argc > 3) {
		num = strtoul(argv[3], 0, 0);
	}

        /* wire us to core 'core' */
        sprintf(ctlcmd, "/proc/%d/ctl", getpid());
        print("ctlcmd is %s\n", ctlcmd);
        ctlfd = open(ctlcmd, 2);
        print("ctlfd  is %d\n", ctlfd);
        if (ctlfd < 0) {
                perror(ctlcmd);
                exit(1);
        }
	sprintf(wire, "wired %d", core);
	wlen = strlen(wire);
        if (write(ctlfd, wire, wlen) < wlen) {
                perror(wire);
                exit(1);
        }

	/* set up amr  ... just make the base the next page. */
	amrbase = (unsigned int) malloc(2*1024*1024);
	print("amrbase is %#x", amrbase);
	amrbase = (unsigned int) malloc(2*1024*1024);
	print("amrbase is %#x", amrbase);
	amrbase = (amrbase+255)&(~0xff);
	amr = (void *)amrbase;
	print("amr is %p\n", amr);
	memset(amr, 0, sizeof(*amr));
	amr->base = &amr[1];
	amr->size = 1 << 20;
	/* moved here so we can easily test some things on IO nodes
	 * after CN have all crashed :-)
	 */
    	cfd = open("/dev/torusctl", 2);
    if (cfd < 0) {
	    perror("cfd");
	    exit(1);
    }
	sprintf(ctlcmd, "r %p", amr);
	print("ctlcmd is %s\n", ctlcmd);
	amt = write(cfd, ctlcmd, strlen(ctlcmd));
	print("ctlcmd write amt %d\n", amt);
	if (amt < 0) {
		print("r cmd failed\n");
		exit(1);
	}

/*
	for(i = 0; i < 240; i++)
		data[i] = i + 1;
 */

	start = getticks();
	/* we are 1 or 0. 1 sends, 0 eats. */
	if (role == 0) {
		/* suck it up */
		while (amr->prod < 256 * num) 
			if (amr->con != amr->prod){
				if (amr->con == 0)	
					start = getticks();
				//print("Advance to %d\n", amr->prod);
				amr->con = amr->prod;
			}
	} else {
		/* do we need to clear out the packet each time through? */
		for(i = 0; i < num; i++) {
			int res;
			memset(pkt, 0, sizeof(*pkt));
			pkt->dst[X] = 0;
			pkt->dst[Y] = 0;
			pkt->dst[Z] = 0;
			res = syscall(669, pkt, data);
			if (! res) 
				print("Fail @ %d\n", i);
		}
	}
	end = getticks();
	print("%lld %lld - %d / p\n", end, start, num);

}

