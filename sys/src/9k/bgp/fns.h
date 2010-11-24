#include "../port/portfns.h"

void	addconf(char*, char*);
uint	archmiiport(int);
void	archpersonalityfmt(char*, char*);
void	archprint(void);
void	archreset(void);
ulong	archuartclock(int, int);
u32int	ccr0get(void);
u32int	ccr1get(void);
void	clockinit(void);
void	clockintr(Ureg*);
void cnkinit(void);
#define coherence()	imb()
void	cpuidprint(void);
void	critintrvec(void);
void	dcbi(void*);
void	dcflush(uintptr, usize);
u32int	dcrget(int);
void	dcrput(int, u32int);
#define decref(r)	adec(&(r)->ref)
u32int	dearget(void);
void	delay(int);
void	dtlbmiss(void);
void	dumpregs(Ureg*);
void	delayloopinit(void);
void	evenaddr(uintptr);
void	firmware(int);

void	fpreginit(void);
int	fpuavail(Ureg*);
int	fpudevprocio(Proc*, void*, long, uintptr, int);
int	fpuemu(Ureg*);
void	fpuinit(void);
void	fpunoted(void);
void	fpunotify(Ureg*);
void	fpuprocsave(Proc*);
void	fpusysprocsetup(Proc*);
void	fpusysrfork(Ureg*);
void	fpusysrforkchild(Proc*, Ureg*, Proc*);
void	fputrap(Ureg*, int);
char*	getconf(char*);
u32int	getdar(void);
u32int	getdec(void);
u32int	getesr(void);
u32int	getmcsr(void);
u32int	getmsr(void);
u32int	gettbl(void);
u32int	gettbu(void);
u32int	gettsr(void);
void	gotopc(uintptr);
int	havetimer(void);
void	iccci(void);
void	icflush(uintptr, usize);
void	idlehands(void);
void	imb(void);
#define incref(r)	ainc(&(r)->ref)
void	intr(Ureg*);
void	intrenable(int, void (*)(Ureg*, void*), void*, char*, int);
void	intrdisable(int, void (*)(Ureg*, void*), void*, char*);
void	intrgrpdisable(int, u32int);
void	intrgrpenable(int, u32int);
char*	intrgrpstatus(int, char*, char*);
void	intrinit(void);
void	intrvec(void);
int	iprint(char*, ...);
void	itlbmiss(void);
void	cnsconsole(void);
void	kexit(Ureg*);
KMap*	kmap(Page*);
void*	kmapdma(usize);
void	kmapfault(Ureg*, uintptr);
void	kmapinit(void);
void	kmapinval(void);
void*	kmapphys(uintptr, u64int, unsigned, unsigned);
usize	kmapsize(usize);
void	kunmap(KMap*);
void	kunmapphys(void*);
void	links(void);
void	mach0init(void);
void	mb(void);
void	meminit(unsigned);
void	mmuinit(void);
int	newmmupid(void);
int	notify(Ureg*);
u32int	pidget(void);
void	pidput(u32int);
u32int	pitget(void);
void	pitput(u32int);
#define procrestore(p)
void	procsave(Proc*);
void	putdcr(int, u32int);
void	putdec(ulong);
void	putesr(ulong);
void	putevpr(ulong);
void	evprput(ulong);
void	putmsr(u32int);
void	putsdr1(u32int);
void	puttcr(u32int);
void	puttsr(u32int);
u32int	pvrget(void);
void 	sendRAS(int, int, int, int, int, int);
void	spldone(void);
Mreg	splhi(void);
Mreg	spllo(void);
void	splx(Mreg);
void	splxpc(Mreg);
u32int	stidget(void);
void	stidput(u32int);
void	sysprocsetup(Proc*);
void*	sysexecregs(uintptr, ulong, ulong);
uintptr	sysexecstack(uintptr, int);
u64int	tbget(void);
void	tbput(u64int);
void	touser(void*);
void	trapinit(void);
void	trapvec(void);
void	trapmvec(void);
void	tlbdump(char*);
u32int	tlbrehi(int);
u32int	tlbrelo(int);
u32int	tlbremd(int);
int	tlbsxcc(uintptr);
void	tlbwrx(int, u32int, u32int, u32int);
#define	userureg(ur)	(((ur)->status & MSR_PR) != 0)
#define	waserror()	(up->nerrlab++, setlabel(&up->errlab[up->nerrlab-1]))
void	watchreset(void);

int	cas32(void*, u32int, u32int);
int	tas32(void*);

#define CASU(p, e, n)	cas32((p), (u32int)(e), (u32int)(n))
#define CASV(p, e, n)	cas32((p), (u32int)(e), (u32int)(n))
#define CASW(addr, exp, new)	cas32((addr), (exp), (new))
#define TAS(addr)	tas32(addr)

void	sysrforkret(void);

#define PTR2UINT(p)	((uintptr)(p))
#define UINT2PTR(i)	((void*)(i))

#define	isphys(a)	(((ulong)(a)&KSEGM)!=KSEG0 && ((ulong)(a)&KSEGM)!=KSEG1)
#define KADDR(a)	((void*)((uintptr)(a)|KZERO))
#define PADDR(a)	(isphys(a)?(uintptr)(a):((uintptr)(a)&~KSEGM))

/* to be renamed */
void	savefpregs(uchar*);
void	restfpregs(uchar*, u32int);
u32int	getfpscr(void);

/* CNK emulation */
void	cnksyscall(Ureg*);
void*	cnksysexecregs(uintptr, ulong, ulong);
