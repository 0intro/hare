#include "rompi.h"

/* need to resync kernel to this but for now it's ok */
struct AmRing {
	u8int *base;
	int size;
	int prod;
	int logN;
	u8int _116[128-16];
	int con;
	u8int _124[128-4];
};

/* stuff we might as well keep around for the duration. */
static int cfd, ctlfd;

struct AmRing * 
amringsetup(int logN, int core)
{
	char ctlcmd[128];
	int amt;
	/* the wiring is crucial. We only got to about 2.9 microseconds 
     * per send without wiring. With wiring, we got to 1.165 microseconds
     * per send. What is likely going on is inj interrupts taking time
     * and slowing down the rest of the work. 
     */
	char wire[32];
	int wlen;

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
	amr->base = (u8int *)&amr[1];
	amr->size = 1 << logN;
	amr->logN = logN;
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

	return amr;
}

Tpkt *waitamrpacket(struct AmRing *amr, u8int type)
{
	/* eat packets until packet type 'type' is found. */
	do {
	} until (pkt->data[16] == type);
}
void
beacon(struct AmRing *arm, int *nodes)
{
	/* go ahead and send the beacon. */
	/* gather any responses */
	/* return total responses contained in 'nodes' */
}

void
waitbeacon(struct AmRing *amr)
{
	/* receive a packet */
	/* see if first char is 'B' --> beacon */
}

void
respondbeacon(struct AmRing *amr)
{
	/* send a rsponse to a packet -- first char is 'b' */
}

void
intreduce(struct AmRing *amr)
{
	if (! myproc) {
		/* just sweep up the 'i' packets */
		/* second 32-bits is the sum */
		/* return the sum with an 'I' packet */
	} else {
		/* send the packet of type 'i' */
		/* waitpacket 'I' */
	}
}

void
waitbeacon(struct AmRing *amr)
void
amrstartu(struct AmRing *amr)
{
	if (! myproc) {
		/* node 0 beacons once per second. It collects responses. 
		 * once all nodes have checked in it sends a start message. 
		 */
		int numresponded = 0;
		int *nodes = mallocz(nprocs);
		while (numresponded < nprocs) {
			numresponded = beacon(nodes);
			sleep(1);
		}
	} else {
		/* wait for the beacon */
		waitbeacon(amr);
		/* send our response */
		respondbeacon(amr);
	}

	/* amring is now usable for normal work */
}

int
amrsend(struct AmRing *amr, void *data, int size, int rank)
{
	void ranktoxyz(int rank, int *x, int *y, int *z);
    
	int x, y, z;
	ranktoxyz(rank, &x, &y, &z);
	Tpkt pkt;
	int res;
	memset(&pkt, 0, sizeof(pkt));
	pkt.dst[X] = x;
	pkt.dst[Y] = y;
	pkt.dst[Z] = z;
	res = syscall(669, pkt, data);
	return res;
}


/* return pointer to the set of config packets; set *size to the number 
 * that are there. 
 * later. For now we just return the next. 
 */
void *
amrrecv(struct AmRing *amr, int *num)
{
	Tpkt *tpkt = (Tpkt *)amr->base;

	if (amr->con != amr->prod){
		tpkt = &tpkt[amr->prod & (amr->size-1)];
		*num = 1;
		amr->con++;
	} else {
		*num = 0;
		tpkt = NULL;
	}
	return tpkt;
}

