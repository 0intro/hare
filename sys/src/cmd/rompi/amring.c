#include "rompi.h"
//#include <asm/io.h>
typedef uvlong ticks;

/* need to resync kernel to this but for now it's ok */
extern int myproc, nproc;

int amrdebug = 0;
/* stuff we might as well keep around for the duration. */
static int cfd, ctlfd;

/* fucking useless include files! */
unsigned long read8(unsigned char *l)
{
	volatile unsigned char *ll = (volatile unsigned char *)l;
	return *ll;
}
unsigned long readl(unsigned long *l)
{
	volatile unsigned long *ll = (volatile unsigned long *)l;
	return *ll;
}

void 
writel(unsigned long new, unsigned long *l)
{
	volatile unsigned long *ll = (volatile unsigned long *)l;
	*ll = new;
}

static __inline__ void mb(void)
{
	__asm__ __volatile__ ("mbar\n");
}

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
	if (amrdebug > 2)
		print("ctlcmd is %s\n", ctlcmd);
	ctlfd = open(ctlcmd, 2);
	if (amrdebug > 2)
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
	amrbase = (unsigned int) malloc(2*1024*1024);
	if (amrdebug > 2)
		print("amrbase is %#x", amrbase);
	amrbase = (amrbase+255)&(~0xff);
	amr = (void *)amrbase;
	if (amrdebug > 2)
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
	if (amrdebug > 2)
		print("ctlcmd is %s\n", ctlcmd);
	amt = write(cfd, ctlcmd, strlen(ctlcmd));
	if (amrdebug > 2)
		print("ctlcmd write amt %d\n", amt);
	if (amt < 0) {
		print("r cmd failed\n");
		exit(1);
	}
	if (amrdebug > 2)
		print("cmd succeeded\n");
	return amr;
}

int
amrsend(struct AmRing *amr, void *data, int size, int rank)
{
	void ranktoxyz(int rank, int *x, int *y, int *z);
    
	int x, y, z;
	u8int *coord;
	int res;
	ranktoxyz(rank, &x, &y, &z);
	coord = mallocz(3, 0);
	coord[0] = x;
	coord[1] = y;
	coord[2] = z;
	if (amrdebug > 3)
		printf("SEND TO [%d, %d, %d] %p\n", x,y,z,data);
	res = syscall(669, coord, data);
	//res = syscall(666, coord, data);
	/* calling 301 is hanging up the node? 4/11/ 0912 */
	//res = syscall(301, coord, data);
	if (amrdebug > 3)
		printf("res is %d\n", res);
	//free(coord);
	return res;
}


/* return pointer to the set of config packets; set *size to the number 
 * that are there. 
 * later. For now we just return the next. 
 */
volatile Tpkt *
amrrecv(struct AmRing *amr, int *num, volatile Tpkt *p, int *x, int *y, int *z)
{
	volatile Tpkt *tpkt = NULL;
	mb();
	unsigned long con = readl(&amr->con), 
		prod = readl(&amr->prod);
	if (amrdebug > 3)
		printf("con %ld, %ld\n", con, prod);
	*num = 0;

	if (con != prod) {
		tpkt = (volatile Tpkt *) &amr->base[amr->con % amr->size];
		
		{ 
			int i,j; 
			unsigned char * cp = (unsigned char *)&amr->base[amr->con % amr->size];
			for(i = 0; i < 15 && amrdebug > 4; i++) {
				printf("%p: ", &cp[i*16+j]);
				for(j = 0; j < 16; j++)
					printf("%02lx ", read8(&cp[i*16+j]));
				printf("\n");
			}
		}
		if (p){
			unsigned char *cp = (unsigned char *)p;
			int i, j;
			if (amrdebug > 4)
				printf("memcpy(%p, %p, %d);\n", p, &amr->base[amr->con % amr->size], 256);
			memcpy((void *)p, &amr->base[amr->con % amr->size], 256);
			for(i = 0; i < 16 && amrdebug > 4; i++) {
				printf("%p: ", &cp[i*16]);
				for(j = 0; j < 16; j++)
					printf("%02lx ", read8(&cp[i*16+j]));
				printf("\n");
			}
		}

		if (amrdebug > 3)
			printf("Con %ld, tpkt %p, prod %ld, payloadbyte %02x\n", 
			       con, tpkt, prod, p->payload[0]);
		writel(con + 256, &amr->con);
		*x = read8((u8int *)&tpkt->Hdr.src[X]);
		*y = read8((u8int *)&tpkt->Hdr.src[Y]);
		*z = read8((u8int *)&tpkt->Hdr.src[Z]);
		*num = 1;
	}
	if (amrdebug > 3)
		printf("amrrecv returns %p\n", tpkt);
	mb();
	return tpkt;
}

/* just one for now */
unsigned char *amrrecvbuf(struct AmRing *amr, unsigned char *p, int *rank)
{
    int xyztorank(int x, int y, int z);

    int num = 1;
    int x, y, z;
    volatile Tpkt *pkt = amrrecv(amr, &num, NULL, &x, &y, &z);
    if (pkt) {
	memcpy(p, (void *)pkt, 240);
	*rank = xyztorank(x,y,z);
	return p;
    }
    return NULL;
}

void waitamrpacket(struct AmRing *amr, u8int type, volatile Tpkt *pkt, 
		    int *x, int *y, int *z)
{
	int num;

	/* eat packets until packet type 'type' is found. */
	do {
		amrrecv(amr, &num, pkt, x, y, z);
		if (num) {
			if (amrdebug > 4){
				print("waitamrpacket gets %d, wants %d\n", pkt->payload[0], type);
				int i,j; 
				for(i = 0; i < 15 ; i++) 
					for(j = 0; j < 16; j++)
						printf("%02lx ", read8((u8int *)&pkt->payload[i*16+j]));
				
				printf("\n");
			}
		} 	
		sleep(5);
		
	} while (num < 1 || read8((u8int *)&pkt->payload[0]) != type);
	if (amrdebug > 3)
		print("waitamrpacket: returing for %d from (%d, %d, %d)\n", type, x,y,z);
}

int
beacon(struct AmRing *amr, int *nodestatus, int numnodes)
{
	u8int *data;
	Tpkt *pkt;
	int num;
	int x, y, z;
	int rank;
	int i;
	int dontcare;

	pkt = mallocz(1024, 1);
	data = mallocz(256, 1);
	for(i = 0; i < 256; i++)
		data[i] = 'a' + i;
	data[0] = 'B';
	/* go ahead and send the beacon. */
	if (amrdebug > 2)
		print("0: beacon. numnodes %d\n", numnodes);
	for(num = i = 1; i < numnodes; i++) {
		if (! nodestatus[i]){
			amrsend(amr, data, 240, i);
			if (amrdebug > 3)
				print("0: send %d\n", i);
		} else {
			num++;
		}
	}
	if (amrdebug > 2)
		print("all beacons sent\n");
	/* gather any responses */
	while (amrrecv(amr, &dontcare, pkt, &x, &y, &z)) {
	    int xyztorank(int x, int y, int z);
		rank = xyztorank(x, y, z);
		if (amrdebug > 3)
			printf("beacon: one from %d(%d,%d,%d) status %d\n", rank, x,y,z, nodestatus[rank]);
		if (nodestatus[rank] == 0) {
			num++;
			if (amrdebug > 3) print("num %d\n", num);
		}
		nodestatus[rank]++;
	}
	free(data);
	/* return total responses contained in 'nodes' */
	if (amrdebug > 2)
		print("beacon: return %d of %d needed\n", num, numnodes);
	return num;
}

void
waitbeacon(struct AmRing *amr)
{
	int x, y, z;
	static Tpkt *pkt = NULL;
	if (! pkt)
		pkt = mallocz(1024, 1);
	waitamrpacket(amr, 'B', pkt, &x, &y, &z);
	if (amrdebug > 2)
		printf("Waitbeacon gets it from (%d,%d,%d)\n", x, y, z);
}

void
respondbeacon(struct AmRing *amr)
{
	unsigned char *data;
	data = mallocz(256, 1);
	data[0] = 'b';
	if (amrdebug > 2)
		printf("Respondbeacon to 0\n");
	/* send a response to a packet -- first char is 'b' */
	amrsend(amr, data, 240, 0);
	free(data);
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
amrstartup(struct AmRing *amr)
{
	if (! myproc) {
		/* node 0 beacons once per second. It collects responses. 
		 * once all nodes have checked in it sends a start message. 
		 */
		int numresponded = 0;
		int *nodes = mallocz(nproc*sizeof(*nodes), 1);
		if (amrdebug > 2)
			printf("amrstartup: time for node 0 to tell %d nodes\n", nproc);
		while (numresponded < nproc) {
			numresponded = beacon(amr, nodes, nproc);
			if (amrdebug > 3)
				printf("amrstartup: %d to date\n", numresponded);
			sleep(5);
			if (amrdebug > 3)
				printf("amrstartup: node 0 starts again\n");
			//exit(1);
		}
	} else {
		/* wait for the beacon */
		waitbeacon(amr);
		/* send our response */
		respondbeacon(amr);
		if (amrdebug > 2)
			printf("responded\n");
	}

	/* amring is now usable for normal work */
}

