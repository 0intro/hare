
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
//#include <asm/io.h>
/* fucking useless include files! */
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

static __inline__ void mb(void)
{
	__asm__ __volatile__ ("mbar\n");
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

 
	vlong start, end;
	int ctlfd;
	char ctlcmd[128];

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
	u8int *coord;
	unsigned long *l;
    
	unsigned char *data;
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
	amr->base = (void *)&amr[1];
	amr->size = 1 << 20;
	for (i = 0, l = (unsigned long *)amr->base; i < amr->size/sizeof(unsigned long); i++)
		l[i] = 0xdeadbeef + role;
		
	coord = malloc(1024);
	data = malloc(1024);
	print("coord %p data %p\n", coord, malloc);
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

  for(i = 0; i < 240; i++)
  data[i] = i + 0x55;

	print("Run until amr->prod is %d\n", 256 * num);
	start = getticks();
	/* we are 1 or 0. 1 sends, 0 eats. */
	if (role == 0) {
		/* suck it up */
		/* do I need an mbar now */
		while (readl(&amr->prod) < 256 * num) {
			mb();
			if (readl(&amr->con) != readl(&amr->prod)){
				if (readl(&amr->con) == 0)	
					start = getticks();
				print("Advance to %d\n", readl(&amr->prod));
				writel(readl(&amr->prod), &amr->con);
			} 
		} 
	} else {
		/* do we need to clear out the packet each time through? */
		for(i = 0; i < num; i++) {
			int res;
			memset(coord, 0, 3);
			if (! i) print("Call with %p and %p\n", coord, data);
			res = syscall(669, coord, data);
			if (! res) 
				print("Fail @ %d\n", i);
		}
	}
	

	end = getticks();
	print("%lld %lld - %d / p\n", end, start, num);
	for (i = 0, l = (unsigned long *)amr->base; i < 8; i++)
		print("l[%d] = 0x%x\n", i, l[i]);
	return 0;
}

