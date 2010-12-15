#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "bgp_personality.h"

#include "../port/error.h"
#include <tos.h>
#include "ureg.h"
/* from bgp
machine BGP
nodename ion-1
release 2.6.19.2
sysname CNK
version 1
*/
/* from linux */

struct iovec {
	void *base;
	int len;
};


struct utsname {
	char sysname[65];
	char nodename[65];
	char release[65];
	char version[65];
	char machine[65];
};


struct timeval
  {
    u32int tv_sec;		/* Seconds.  */
    u32int tv_usec;	/* Microseconds.  */
  };

struct  rusage {
        struct timeval ru_utime;        /* user time used */
        struct timeval ru_stime;        /* system time used */
        long    ru_maxrss;              /* maximum resident set size */
        long    ru_ixrss;               /* integral shared memory size */
        long    ru_idrss;               /* integral unshared data size */
        long    ru_isrss;               /* integral unshared stack size */
        long    ru_minflt;              /* page reclaims */
        long    ru_majflt;              /* page faults */
        long    ru_nswap;               /* swaps */
        long    ru_inblock;             /* block input operations */
        long    ru_oublock;             /* block output operations */
        long    ru_msgsnd;              /* messages sent */
        long    ru_msgrcv;              /* messages received */
        long    ru_nsignals;            /* signals received */
        long    ru_nvcsw;               /* voluntary context switches */
        long    ru_nivcsw;              /* involuntary " */
};


struct utsname cnkutsname = {
	 "1"
};

void
cnkinit(void)
{
	extern u32int cnkbase;
	u32int attr;
	print("CNKINIT: machno %d cnkbase %p\n", m->machno, (void *)cnkbase);
	if(m->machno != 0)
		return;
	if (cnkbase){
		u8int *x;
		attr = TLBUW|TLBUR|TLBWR|TLBM|TLBCI;
		//attr |= TLBI|TLBG;
		print("cnkinit:kmappphys(0, %p, %#x, %#x);\n", (void *)(cnkbase*MiB), 0x10000000 /*n-cnkbase*MiB*/,attr);
		x = kmapphys(0, cnkbase*MiB, 0x10000000/*n-cnkbase*MiB*/,attr);
		print("Did a kmapphys , got %p,  do a store\n", x);
		*x = 0;
	} 
}
void
cnkexit(Ar0*, va_list list)
{
	int val;
	char exitstr[32] = "";
	
	val = va_arg(list, int);
	if (val)
		snprint(exitstr, sizeof(exitstr), "%d", val);
	if (up->cnk & 128) print("%d:cnkexit %d\n", up->pid, val);
	up->cnk = 0;
	pexit(exitstr, 1);
}

void
cnkuname(Ar0*ar, va_list list)
{
	void *va;
	va = va_arg(list, void *);
	if (up->cnk & 128) print("%d:cnkuname va %p\n", up->pid, va);
	validaddr(va, 1, 1);
	memmove(va, &cnkutsname, sizeof(cnkutsname));
	// if this does not work we will need a /proc for bgl 
	memmove(va, "BGP\0plan9\02.6.19.2\0CNK\0 1\0", 26);
	ar->i = 0;
}

void
cnkgetpersonality(Ar0*ar, va_list list)
{
	void *va;
	va = va_arg(list, void *);
	if (up->cnk & 128) print("%d:cnkgetpersonality va %p\n", up->pid, va);
	validaddr(va, sizeof(*personality), 1);
	memmove(va, personality, sizeof(*personality));
	ar->i = 0;
}

/* this was in port/sysseg.c and was copied here. */
/* There are a few special bits for CNK needs. */
/* called with HIGHEST address needed for sbrk */
void
cnksbrk(Ar0* ar0, uintptr addr, int allocate)
{
	extern u32int cnkbase;
	uintptr ibrk(uintptr addr, int seg);
	Segment *heapseg;
	uintptr oldtop;
	int i;

	heapseg = up->heapseg;

	if (! heapseg){
		print("cnksbrk: no heap set up yet\n");
		error("No heap set up yet");
	}

	oldtop = heapseg->top;

	if(addr == 0){
		ar0->p = oldtop;
		return;
	}

	if (cnkbase) {
		uintptr newtop;
		long newsize;
		newtop = PGROUND(addr);
		newsize = (newtop-heapseg->base)/BY2PG;
		heapseg->top = newtop;
		heapseg->size = newsize;
		ar0->p = newtop;
		return;
	}
	if (addr < oldtop){
		/* just ignore it *
		print("cnksbrk: can't shrink heap\n");
		error("can't shrink heap");
		 */
		ar0->p = oldtop;
		return;
	}

	/* FIX ME -- up->heapseg should be up->heapindex -- but up->seg[i] is set in a strange way */
	/* find the index of the heap segment; call ibrk with that segment. */
	/* consider flagging heapseg by base address or p==v, but it's too soon to know 
	 * if that is a universal test and I hate to do a strcmp on each cnksbrk
	 */
	for(i = 0; i < NSEG; i++) {
		if (heapseg == up->seg[i])
			break;
	}
	/* isn't life grand? The heap is already mapped. So just grow the end of heap pointer 
	 * and we need to allocate pages, since you're not allowed to fault on the heap. 
	 */
	//print("CNKSBRK: brk to %#ulx\n", addr);
	if (i < NSEG)
		ar0->p = ibrk(addr, i);
	/* now we might need to fault in all the pages by hand */
	if (allocate) {
		//print("PRE-fault %#ulx to %#ulx\n", oldtop, addr);
		for(;oldtop < addr; oldtop += BY2PG) {
			/* heap is always writeable */
			//print("Fault in %#ulx\n", oldtop);
			/* oh wow this is so gross */
			heapseg->nozfod = 0;
			fault(oldtop, 0);
			heapseg->nozfod = 1;
		}
	}
	if (up->cnk & 128) print("%d:cnksbrk for %p returns %p\n", up->pid, addr, ar0->p);
}

/* special case: interpretation of '0' is done in USER MODE on Plan 9 */
/* but old deprecated sysbrk_ does what we need */
void
cnkbrk(Ar0* ar0, va_list list)
{
	uintptr addr;
	addr = va_arg(list, uintptr);

	if (up->cnk & 128) print("%d:cnkbrk va %#ulx\n", up->pid, addr);
	
	cnksbrk(ar0, addr, 1);
	/* it is possible, though unlikely, that libc wants exactly the value it asked for. Plan 9 is returning rounded-up-to-next-page values. */
	if (addr)	
		ar0->i = addr;

}

void
cnkopen(Ar0 *ar0, va_list list)
{
	char *aname;
	int omode;
	void sysopen(Ar0 *, va_list);
	aname = va_arg(list, char*);
	omode = va_arg(list, int);
	USED(aname,omode);
	sysopen(ar0, list);
}

void
cnkwritev(Ar0 *ar0, va_list list)
{
	void sys_write(Ar0* ar0, va_list list);
	int fd;
	struct iovec *iov;
	int iovcnt;
	int i;
	fd = va_arg(list, int);
	iov = va_arg(list, struct iovec *);
	iovcnt = va_arg(list, int);
	if (up->cnk & 128) print("%d:cnkwritev (%d, %p, %d):", up->pid, fd, iov, iovcnt);
	validaddr(iov, iovcnt * sizeof(*iov), 0);
	/* don't validate all the pointers in the iov; sys_write does this */	
	for(i = 0; i < iovcnt; i++) {
		Ar0 war0;
		uintptr arg[3];
		if (up->cnk & 128) print("[%p,%d],", iov[i].base, iov[i].len);
		arg[0] = fd;
		arg[1] = (uintptr) iov[i].base;
		arg[2] = iov[i].len;
		sys_write(&war0, (va_list) arg);
		if (war0.l < 0)
			break;
		/* if the first one fails, we get 0 */
		ar0->l += war0.l;
	}
	if (up->cnk & 128) print("\n");
}


void
cnksocketcall(Ar0 *ar0, va_list list)
{
	int fd;
	uintptr *args;

	USED(ar0);

	fd = va_arg(list, int);
	args = va_arg(list, uintptr *);
	if (up->cnk & 128) print("%d:cnksocketcall (%d, %p):", up->pid, fd, args);
	validaddr(args,sizeof(*args), 0);
	if (up->cnk & 128) print("\n");
}


void
cnkgeteuid(Ar0 *ar0, va_list)
{
	ar0->i = 0;
}

/* ow this hurts. */
typedef unsigned long int __ino_t;
typedef long long int __quad_t;
typedef unsigned int __mode_t;
typedef unsigned int __nlink_t;
typedef long int __off_t;
typedef unsigned int __uid_t;
typedef unsigned int __gid_t;
typedef long int __blksize_t;
typedef long int __time_t;
typedef long int __blkcnt_t;

typedef unsigned long long int __u_quad_t;

typedef __u_quad_t __dev_t;

struct timespec
  {
    __time_t tv_sec;
    long int tv_nsec;
  };
/*
# 103 "/bgsys/drivers/V1R2M0_200_2008-080513P/ppc/gnu-linux/lib/gcc/powerpc-bgp-linux/4.1.2/../../../../powerpc-bgp-linux/sys-include/sys/stat.h" 3 4
*/
/* how many stat structs does linux have? too many. */
struct stat {
     __dev_t st_dev;
    unsigned short int __pad1;
    __ino_t st_ino;
    __mode_t st_mode;
    __nlink_t st_nlink;
    __uid_t st_uid;
    __gid_t st_gid;
    __dev_t st_rdev;
    unsigned short int __pad2;
    __off_t st_size;
    __blksize_t st_blksize;
    __blkcnt_t st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    unsigned long int __unused4;
    unsigned long int __unused5;
} stupid = {
	.st_blksize = 4096,
	.st_dev = 1,
	.st_gid = 0,
	.st_ino = 0x12345,
	.st_mode = 0664 | 020000,
	.st_nlink = 1,
	.st_rdev = 501
};

void
fstat64(Ar0 *ar0, va_list list)
{
	void *v;
	int fd;
	fd = va_arg(list, int);
	v = va_arg(list, void *);
	validaddr(v, 1, 0);
	switch(fd) {
		case 0:
		case 1:
		case 2: 
			ar0->i = 0;
			memmove(v, &stupid, sizeof(stupid));
			break;
	}

}


/* do nothing, succesfully */
void
returnok(Ar0*, va_list)
{

	return;
}

/* void  *  mmap(void *start, size_t length, int prot , int flags, int fd,
       off_t offset); */

void cnkmmap(Ar0 *ar0, va_list list)
{
	void *v;
	int length, prot, flags, fd;
	ulong offset;
	v = va_arg(list, void *);
	length = va_arg(list, int);
	prot = va_arg(list, int);
	flags = va_arg(list, int);
	fd = va_arg(list, int);
	offset = va_arg(list, ulong);
	if (up->cnk & 128) print("%d:CNK: mmap %p %#x %#x %#x %d %#ulx\n", up->pid, v, length, prot, flags, fd, offset);
	/* TODO: in the case that it is 1M in size, go ahead an 1M align it and
	 * find pages that are physically contiguous
	 */
	if (fd == -1){
		Segment *heapseg = up->heapseg;
 		unsigned char *newv, *oldv;
		uintptr arg;
		arg = PGROUND(heapseg->top);
		cnksbrk(ar0, arg, 0);
		if (up->cnk & 128) print("%d:mmap anon: current is %p\n", up->pid, ar0->v);
		oldv =ar0->v;
		arg += length;
		arg = PGROUND(arg);
		newv =  (uchar *)arg;
		if (up->cnk & 128) print("%d:mmap anon: ask for %p\n", up->pid, newv);
		cnksbrk(ar0, arg, 1);
		if (up->cnk & 128) print("%d:mmap anon: new is %p\n", up->pid, ar0->v);
 		/* success means "return the old pointer" ... */
 		ar0->v = oldv;
 	}
	
}

/* but it's really a no op. And a storage leak. Sorry. */
void cnkmunmap(Ar0 *ar0, va_list list)
{
	u8int *v;
	int length;
	uintptr arg;
	Segment *heapseg = up->heapseg;
	v = va_arg(list, void *);
	length = va_arg(list, int);
	arg = PTR2UINT(v);
	if (up->cnk & 128) print("%d:CNK: munmap %p %#x\n", up->pid, v, length);

	if (arg < heapseg->base || arg + length > heapseg->top){
		print("%d: munmap %p %#x: outsize the range of the heap\n", up->pid, v, length);
		return;
	}

	ar0->i = 0;
}

/*
 * Given a p2p, create an xyz. I/O node differences beckon. All this code basically sucks at this point; note repetition. But Jim is fixing it all anyway. 
 */
static void
p2ptoxyz(int p2paddr, u32int *x, u32int *y, u32int *z)
{
	static int first = 1;
	static	int nnodes, yspan, zspan, psetNum;
	if (first){
		first = 0;
		nnodes = personality->Network_Config.Xnodes
			 * personality->Network_Config.Ynodes
			 * personality->Network_Config.Znodes;
		yspan = personality->Network_Config.Xnodes;
		zspan = yspan * personality->Network_Config.Ynodes;
		psetNum = personality->Network_Config.PSetNum;
		if (up->cnk & 128) iprint("nnodes %d, yspan %d, zspan %d, psetNum %d\n", nnodes, yspan, zspan, psetNum);
	}

	/* From IP layer, we set the second octet to 128 for the IO node which becomes x */
	/* From CNK, x will be -1. So check either one. */
	/* this test breaks if we ever get a BG which has > 2^21 nodes */
	if (p2paddr >= nnodes) {
		/* we don't really have an x,y,z */
		*x = *y = *z = 255;
	} else {
		*z = p2paddr / zspan;
		p2paddr -= *z*zspan;
		*y = p2paddr / yspan;
		p2paddr -= *y*yspan;
		*x = p2paddr;	
	}
	return;
}

/* interesting question. What will we do here? */
/* void  *  rank2coord(int rank, u32 *x, u32 *y, u32 *z, u32 *t) */

void cnkr2c(Ar0 *ar0, va_list list)
{
	u32int *x, *y, *z, *t;
	int rank;

	rank = va_arg(list, int);
	x = va_arg(list, u32int *);
	validaddr(x, sizeof(*x), 1);
	y = va_arg(list, u32int *);
	validaddr(y, sizeof(*y), 1);
	z = va_arg(list, u32int *);
	validaddr(z, sizeof(*z), 1);
	t = va_arg(list, u32int *);
	validaddr(t, sizeof(*t), 1);
	if (up->cnk & 128) print("%d:CNK: rank2coord %d %p %p %p %p\n", up->pid, rank, x, y, z, t);
	/* use p2ptoxyz for now because I don't really know what to do */
	p2ptoxyz(rank, x, y, z);
	*t = 0;
	if (up->cnk & 128) print("%d:CNK: (%d, %d, %d, %d)\n", up->pid, *x, *y, *z, *t);
	/* HACK. Really we need to see if rank is > psetsize of something but
	 * IO nodes can't have coords either. So we have to figure this out but ..
	 * the # threads per cpu comes into it too. Potentially there are 256 
	 * ranks on a 64 node block. 
	 */
	if (*x == 255)
		ar0->i = -1;
	else
		ar0->i = 0;
	
}

/* void  *  Kernel_Coord2Rank(x, y, z, t, &rank2, & size);*/

void cnkc2r(Ar0 *ar0, va_list list)
{
	u32int x, y, z, t;
	u32int *rank2, *size;
	int yspan, zspan;
	x = va_arg(list, int);
	y = va_arg(list, int);
	z = va_arg(list, int);
	t = va_arg(list, int);
	rank2 = va_arg(list, u32int *);
	validaddr(rank2, sizeof(*rank2), 1);
	size = va_arg(list, u32int *);
	validaddr(size, sizeof(*size), 1);
	if (up->cnk & 128) print("%d:CNK: coord2rank %d %d %d %d %p %p\n", up->pid, x, y, z, t, rank2, size);
	*size = personality->Network_Config.Xnodes
			 * personality->Network_Config.Ynodes
			 * personality->Network_Config.Znodes;
	yspan = personality->Network_Config.Xnodes;
	zspan = yspan * personality->Network_Config.Ynodes;
	if (x &128)
		*rank2 = *size + personality->Network_Config.PSetNum;	/* IO node */
	else
		*rank2 = z*zspan + y*yspan +x;
	*rank2 &= 0xFFFFFF;
	if (up->cnk & 128) print("%d:Rank2 %#x size %d\n", up->pid, *rank2, *size);
	ar0->i = 0;

}

void cnkprocid(Ar0 *ar0, va_list)
{
	ar0->i = 0;
}

/*Kernel_Ranks2Coords((kernel_coords_t *)_mapcache, _fullSize);*/
void cnkranks2coords(Ar0 *ar0, va_list list)
{
	u32int *mapcache;
	int fullsize;
	mapcache = va_arg(list, u32int *);
	fullsize = va_arg(list, int);
	validaddr(mapcache, fullsize * sizeof(*mapcache), 1);
	if (up->cnk & 128) print("%d:cnkranks2coords: %p %d\n", up->pid, mapcache, fullsize);
	ar0->i = -1;
}

/* int sigaction(int sig, const struct sigaction *restrict act,
              struct sigaction *restrict oact); */

void sigaction(Ar0 *ar0, va_list list)
{
	void *act, *oact;
	act = va_arg(list, void *);
	oact = va_arg(list, void *);
	if (up->cnk & 128) print("%d:sigaction, %p %p\n", up->pid, act, oact);
	ar0->i = 0;
}

/*long rt_sigprocmask (int how, sigset_t *set, sigset_t *oset, size_t sigsetsize); */

void rt_sigprocmask(Ar0 *ar0, va_list list)
{
	int how;
	void *set, *oset;
	int size;
	how = va_arg(list,int);
	set = va_arg(list, void *);
	oset = va_arg(list, void *);
	size = va_arg(list, int);
	if (up->cnk & 128) print("%d:sigaction, %d %p %p %d\n", up->pid, how, set, oset, size);
	ar0->l = 0;
}

/* task id stuff. Ignored for now */
void
cnksettid(Ar0 *ar0, va_list)
{
	ar0->i = 0;

}

/* damn. Did not want to do futtocks */
#define FUTEX_WAIT              0
#define FUTEX_WAKE              1
#define FUTEX_FD                2
#define FUTEX_REQUEUE           3
#define FUTEX_CMP_REQUEUE       4
#define FUTEX_WAKE_OP           5
#define FUTEX_LOCK_PI           6
#define FUTEX_UNLOCK_PI         7
#define FUTEX_TRYLOCK_PI        8
#define FUTEX_WAIT_BITSET       9
#define FUTEX_WAKE_BITSET       10


void futex(Ar0 *ar0, va_list list)
{
	int *uaddr, op, val;
	int *uaddr2, val3;
	struct timespec *timeout;
	uaddr = va_arg(list,int *);
	op = va_arg(list, int);
	val = va_arg(list, int);
	timeout = va_arg(list, struct timespec *);
	uaddr2 = va_arg(list, int *);
	val3 = va_arg(list, int);
	USED(uaddr);
	USED(op);
	USED(val);
	USED(timeout);
	USED(uaddr2);
	USED(val3);
	if (up->cnk & 128) print("%d:futex, uaddr %p op %x val %x uaddr2 %p timeout  %p val3 %x\n", up->pid, 
			uaddr, op, val, uaddr2, timeout, val);
	switch(op) {
		default: 
			ar0->i = -1;
			break;
		case FUTEX_WAIT: 
			/*
			 * This  operation atomically verifies that the futex address uaddr
			 * still contains the value val, and sleeps awaiting FUTEX_WAKE  on
			 * this  futex  address.   If the timeout argument is non-NULL, its
			 * contents describe the maximum duration of  the  wait,  which  is
			 * infinite  otherwise.  The arguments uaddr2 and val3 are ignored.
			 */
			validaddr(uaddr, sizeof(*uaddr), 0);
			if (up->cnk & 128) print("%d:futex: value at %p is %#x, val is %#x\n", up->pid, uaddr, *uaddr, val);
			if (*uaddr != val) {
				ar0->i = -11;
				return;
			}
			if (timeout) {
				validaddr(timeout, sizeof(*timeout), 0);
				if (timeout->tv_sec == timeout->tv_nsec == 0)
					return;
			}
			if (up->cnk & 128) print("%d:Not going to sleep\n", up->pid);
			break;
	}
}

/* mprotect is used to set a stack red zone. We may want to use
 * segattach for anon page alloc and then use segdetach for the same purpose. 
 */
void cnkmprotect(Ar0 *ar0, va_list list)
{
	u32int addr, len, prot;
	addr = va_arg(list, u32int);
	len = va_arg(list, u32int);
	prot = va_arg(list, u32int);
	if (up->cnk & 128) print("%d:mprotect: %#x %#x %#x\n", up->pid, addr, len, prot);
	ar0->i = 0;
}

/* this is a hack. */
Segment*
cnkdupseg(Segment **seg, int segno, int share)
{
	int i, size;
	Pte *pte;
	Segment *n, *s;

	SET(n);
	s = seg[segno];

	qlock(&s->lk);
	if(waserror()){
		qunlock(&s->lk);
		nexterror();
	}
	switch(s->type&SG_TYPE) {
	case SG_TEXT:		/* New segment shares pte set */
	case SG_SHARED:
	case SG_PHYSICAL:
		goto sameseg;

	case SG_STACK:
		/* linux wants to share the stack. */
		if(share){ if (up->cnk & 128) print("CLONE STACK IS SHARE\n");
			goto sameseg;
		}
		/* that is all the change */
if (up->cnk & 128) print("CLONE STACK IS NEW\n");
		n = newseg(s->type, s->base, s->size);
		break;

	case SG_BSS:		/* Just copy on write */
		if(share)
			goto sameseg;
if (up->cnk & 128) print("CLONE NEW BSS\n");
		n = newseg(s->type, s->base, s->size);
		break;

	case SG_DATA:		/* Copy on write plus demand load info */
		if(segno == TSEG){
			poperror();
			qunlock(&s->lk);
			return data2txt(s);
		}

		if(share)
			goto sameseg;
		n = newseg(s->type, s->base, s->size);

		incref(s->image);
		n->image = s->image;
		n->fstart = s->fstart;
		n->flen = s->flen;
		break;
	}
	size = s->mapsize;
	for(i = 0; i < size; i++)
		if(pte = s->map[i])
			n->map[i] = ptecpy(pte);

	n->flushme = s->flushme;
	if(s->ref > 1)
		procflushseg(s);
	poperror();
	qunlock(&s->lk);
	return n;

sameseg:
	incref(s);
	poperror();
	qunlock(&s->lk);
	return s;
}

/* the big problem here is that linux clone wants to allow the user to set the 
 * stack. It's stupid but it's what they do. Linux NPTL pretty much requires it. 
 * we are going to be dumb here for now and assume we only use 
 * RFPROC|RFMEM. 
 * What do we do about stack longer term? It gets a bit weird. 
 * the child process stack is in the data segment. Should we make the 
 * child process stack segment a DATA segment, share the data segment, 
 * and make it a STACK for the child? If we did things right in cnkclone
 * we could remove our private dupseg. 
 */
void cnkclone(Ar0 *ar0, va_list list)
{
	void cnksysrforkchild(Proc* child, Proc* parent, uintptr newsp);
	u32int flags,  stack;
	Proc *p;
	int flag, i, n, pid;
	Mach *wm;
	flags = va_arg(list, u32int);
	stack = va_arg(list, u32int);
	if (up->cnk & 128) print("%d:CLONE: %#x %#x\n", up->pid, flags, stack);
	if (flags != 0x7d0f00) {
		print("%d:CLONE: don't know what to do with flags %#x\n", up->pid, flags);
		ar0->i = -1;
		return;
	}
	flag = RFPROC | RFMEM;

	p = newproc();

	p->trace = up->trace;
	p->scallnr = up->scallnr;
	memmove(p->arg, up->arg, sizeof(up->arg));
	p->nerrlab = 0;
	p->slash = up->slash;
	p->dot = up->dot;
	incref(p->dot);

	memmove(p->note, up->note, sizeof(p->note));
	p->privatemem = up->privatemem;
	p->noswap = up->noswap;
	p->nnote = up->nnote;
	p->notified = 0;
	p->lastnote = up->lastnote;
	p->notify = up->notify;
	p->ureg = up->ureg;
	p->dbgreg = 0;
	p->cnk = 1;

	/* Make a new set of memory segments */
	n = flag & RFMEM;
	qlock(&p->seglock);
	if(waserror()){
		qunlock(&p->seglock);
		nexterror();
	}
	for(i = 0; i < NSEG; i++)
		if(up->seg[i])
			p->seg[i] = cnkdupseg(up->seg, i, n);
	qunlock(&p->seglock);
	poperror();

	/* File descriptors */
	if(flag & (RFFDG|RFCFDG)) {
		if(flag & RFFDG)
			p->fgrp = dupfgrp(up->fgrp);
		else
			p->fgrp = dupfgrp(nil);
	}
	else {
		p->fgrp = up->fgrp;
		incref(p->fgrp);
	}

	/* Process groups */
	if(flag & (RFNAMEG|RFCNAMEG)) {
		p->pgrp = newpgrp();
		if(flag & RFNAMEG)
			pgrpcpy(p->pgrp, up->pgrp);
		/* inherit noattach */
		p->pgrp->noattach = up->pgrp->noattach;
	}
	else {
		p->pgrp = up->pgrp;
		incref(p->pgrp);
	}
	if(flag & RFNOMNT)
		up->pgrp->noattach = 1;

	if(flag & RFREND)
		p->rgrp = newrgrp();
	else {
		incref(up->rgrp);
		p->rgrp = up->rgrp;
	}

	/* Environment group */
	if(flag & (RFENVG|RFCENVG)) {
		p->egrp = smalloc(sizeof(Egrp));
		p->egrp->ref = 1;
		if(flag & RFENVG)
			envcpy(p->egrp, up->egrp);
	}
	else {
		p->egrp = up->egrp;
		incref(p->egrp);
	}
	p->hang = up->hang;
	p->procmode = up->procmode;

	/* Craft a return frame which will cause the child to pop out of
	 * the scheduler in user mode with the return register zero
	 */
	/* fix the stack for linux semantics */
	cnksysrforkchild(p, up, stack);

	p->parent = up;
	p->parentpid = up->pid;
	if(flag&RFNOWAIT)
		p->parentpid = 0;
	else {
		lock(&up->exl);
		up->nchild++;
		unlock(&up->exl);
	}
	if((flag&RFNOTEG) == 0)
		p->noteid = up->noteid;

	pid = p->pid;
	memset(p->time, 0, sizeof(p->time));
	p->time[TReal] = MACHP(0)->ticks;

	kstrdup(&p->text, up->text);
	kstrdup(&p->user, up->user);
	/*
	 *  since the bss/data segments are now shareable,
	 *  any mmu info about this process is now stale
	 *  (i.e. has bad properties) and has to be discarded.
	 */
	mmuflush();
	p->basepri = up->basepri;
	p->priority = up->basepri;
	p->fixedpri = up->fixedpri;
	p->mp = up->mp;
	wm = up->wired;
	if(wm)
		procwired(p, wm->machno);
	ready(p);
	sched();

	ar0->i = pid;
}

/* get app segment mapping. Not the gasm you think, you dirty-minded person. 
 */
/* we are going to deprecate this call. It was only there for libraries (dcmf, MPI) that needed
 * the huge physical segment. I think nowadays that is a bad idea. 
 */
void gasm(Ar0 *, va_list)
{
#ifdef NOMORE
	void seginfo(int seg, u32int *va, u64int *pa, u32int *len);
	u64int *pa;
	int whichseg;
	int corenum;
	u32int *va;
	u32int *slen;

	whichseg = va_arg(list, int);
	corenum = va_arg(list, int);
	va = va_arg(list, u32int *);
	pa = va_arg(list, u64int *);
	slen = va_arg(list, u32int *);
	validaddr(va, sizeof(*va), 1);
	validaddr(pa, sizeof(*pa), 1);
	validaddr(slen, sizeof(*slen), 1);

	if (up->cnk & 128) print("%d:gasm: %#x %#x %p %p %p\n", up->pid, whichseg, corenum, va, pa, slen);

	/* we can not run any more without devsegment. Sorry. */
	seginfo(whichseg, va, pa, slen);
	if (up->cnk & 128) print("%d:gasm: %#x %#llx %#x\n", up->pid, *va, *pa, *slen);
	ar0->i = 0;
#endif
}

u32int counters[1024];

/* counter group allocate -- takes a start, length, ptr, flags */
void cnkcga(Ar0 *ar0, va_list list)
{
	int start;
	int len;
	u32int **ptr;
	u32int flags;
	int i;

	start = va_arg(list, int);
	len = va_arg(list, int);
	ptr = va_arg(list, u32int **);
	flags= va_arg(list, u32int);
	validaddr(ptr, sizeof(*ptr)*len, 1);

	if (up->cnk & 128) print("%d:cnkcga: %#x %#x %p %#x\n", up->pid, start, len, ptr, flags);
/*
	for(i = start; i < start + len; i++)
		if (counters[i])
			error("Counter in use");
 */
	for(i = start; i < start + len; i++) {
		ptr[i] = &((u32int *)VIRTULOCKBOX)[i*4];
		counters[i] = flags;
	}
	ar0->i = 0;
	
}

void timeconv(ulong l, struct timeval *t)
{
	u32int ms;
	ms = TK2MS(l);
	t->tv_sec += ms / 1000;
	ms %= 1000;
	t->tv_usec += ms * 1000;
}
void getrusage(Ar0 *ar0, va_list list)
{
	int what;
	struct rusage *r;	

	what = va_arg(list, int);
	r = va_arg(list, struct rusage *);
	validaddr(r, sizeof(*r), 1);

	if (up->cnk & 128) print("%d:getrusage: %s %p:",up->pid,  !what? "self" : "kids", r);
	memset(r, 0, sizeof(*r));
	if (what) {
		timeconv(up->time[3], &r->ru_utime);
		timeconv(up->time[4], &r->ru_stime);
	} else {
		timeconv(up->time[0], &r->ru_utime);
		timeconv(up->time[1], &r->ru_stime);
		if (up->cnk & 128) print("%#lx:%#lx, ", up->time[0], up->time[1]);
	}
	if (up->cnk & 128) print("%#x:%#x\n", r->ru_utime.tv_sec, r->ru_stime.tv_sec);

	ar0->i = 0;	
}

void cnkgetpid(Ar0 *ar0, va_list)
{
	if (up->cnk & 128) print("%d:getpid\n",up->pid);
	ar0->i = up->pid;	
}
