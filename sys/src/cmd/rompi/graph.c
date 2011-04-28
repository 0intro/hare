
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

typedef uvlong ticks;
struct AmRing * amringsetup(int logN, int core);
void amrstartup(struct AmRing *amr);
int amrsend(struct AmRing *amr, void *data, int size, int rank);
volatile Tpkt *amrrecv(struct AmRing *amr, int *num, volatile Tpkt *p, int *x, int *y, int *z);

unsigned char *amrrecvbuf(struct AmRing *amr, unsigned char *p, int *rank);
double MPI_Wtime(void);
int MPI_Init(int *argc,char ***argv);
extern int myproc, nproc;

int debug = 0;

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

ticks graph(struct AmRing *amring, unsigned long long num_msgs_global)
{
	ticks start = 0ULL;
	/* send the message around the ring */
	Tpkt *p = (void *)mallocz(1024, 1);
	unsigned char *buf;

	unsigned long long num_msgs_local = (num_msgs_global / nproc), count_msgs_local_sent = 0, 
		count_msgs_local_recv = 0,
		send_checksum_local = 0, recv_checksum_local = 0; 
	if(num_msgs_global % nproc < myproc)
		++num_msgs_local;
	print("===> num_msgs_global %lld num_msgs_local %lld\n", num_msgs_global, num_msgs_local);
	int test_for_termination = 0;
	buf = mallocz(1024, 1);
	do {
		if(count_msgs_local_sent < num_msgs_local) {
			++count_msgs_local_sent;
			//generate random destination rank
			int rank = random() % nproc;
			if (rank == myproc)
			    rank = (rank + 1) % nproc;

			int payload = random();
			if (debug > 2)
				print("--> send to %d cmls %d nml %d\n", rank, count_msgs_local_sent, num_msgs_local);
			buf[0] = 'm';
			memcpy(&buf[4], &payload, sizeof(payload));
			amrsend(amring, buf, 240, rank);
			send_checksum_local += payload;
			if (debug > 2)
				print("Sent\n");
		}
		/* we only get one at a time for now */
		/* this will probably overrun */
		int rank, num = 0, rx, ry, rz;
		amrrecv(amring, &num, p, &rx, &ry, &rz);
		if (num) {
		    rank = xyztorank(rx, ry, rz);
		    if (debug > 4)
			    print("amrrecv gets %d packets from %d\n", num, rank);
		    if (num && p->payload[0] == 'm') {
			if (debug > 2)
				printf("Got something! from %d\n", rank);
			recv_checksum_local += *(unsigned long long *)&p->payload[4];
			count_msgs_local_recv++;
		    }
		}
		test_for_termination = (count_msgs_local_recv + count_msgs_local_sent) > num_msgs_global;
			
	} while(!test_for_termination);
	return start;
}
int
main (int argc, char **argv)
{
	MPI_Init(&argc, &argv);
	
	struct AmRing *amring;
	extern int amrdebug;
	unsigned long long num_msgs_global = 1024;
	amrdebug = 4;
	int iter = 1;
	if (argc > 1)
		num_msgs_global = strtol(argv[1], 0, 0);

	print("Call amrstartup, myproc %d nproc %d\n", myproc, nproc);
	if (myproc < nproc){
		amring = amringsetup(20, 0);
		if (0 && myproc)
			torusctl("debug 64", 1);
		amrstartup(amring);	
	} else
		while(1);
	print("amrstartup returns\n");
	ticks start, end;
	start = graph(amring, num_msgs_global);
	end  = getticks();
	print("%lld %lld - %d / p\n", end, start, iter);
	return 0;
}
