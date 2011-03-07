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

int logN; /* consider adding to ring later; but we have to change kernel too */

int amringsetup(void *where, int logN, int core)
{
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
}

int
amrsend(void *i, int size, int rank)
{
	memset(pkt, 0, sizeof(*pkt));
	pkt->dst[X] = 0;
	pkt->dst[Y] = 0;
	pkt->dst[Z] = 0;
	res = syscall(669, pkt, data);
	return res;
}


/* return pointer to the set of config packets; set *size to the number 
 * that are there. 
 * later. For now we just return the next. 
 */
void *
amrrecv(int *num)
{
	Tpkt *t = (Tpkt *)amr->base;

	if (amr->con != amr->prod){
		tpkt = &tpkt[amr->prod & logN];
		*num = 1;
		amr->con++;
	} else {
		*num = 0;
	}
	return null;
}

