#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"

#include	<tos.h>
#include	"ureg.h"

#include	"io.h"

extern void syscall(Ureg*);

static struct {
	ulong off;
	char *name;
} intcause[] = {
	{ INT_CI,	"critical input" },
	{ INT_MCHECK,	"machine check" },
	{ INT_DSI,	"data access" },
	{ INT_ISI,	"instruction access" },
	{ INT_EI,	"external interrupt" },
	{ INT_ALIGN,	"alignment" },
	{ INT_PROG,	"program exception" },
	{ INT_FPU,	"floating-point unavailable" },
	{ INT_DEC,	"decrementer" },
	{ INT_SYSCALL,	"system call" },
	{ INT_TRACE,	"trace trap" },
	{ INT_FPA,	"floating point unavailable" },
	{ INT_APU,	"auxiliary processor unavailable" },
	{ INT_PIT,	"programmable interval timer interrrupt" },
	{ INT_FIT,	"fixed interval timer interrupt" },
	{ INT_WDT,	"watch dog timer interrupt" },
	{ INT_DMISS,	"data TLB miss" },
	{ INT_IMISS,	"instruction TLB miss" },
	{ INT_DEBUG,	"debug interrupt" },
	{ 0,		"unknown interrupt" }
};

static char *excname(ulong, u32int);

char *regname[]={
	"CAUSE","SRR1",
	"PC",	"GOK",
	"LR",	"CR",
	"XER",	"CTR",
	"R0",	"R1",
	"R2",	"R3",
	"R4",	"R5",
	"R6",	"R7",
	"R8",	"R9",
	"R10",	"R11",
	"R12",	"R13",
	"R14",	"R15",
	"R16",	"R17",
	"R18",	"R19",
	"R20",	"R21",
	"R22",	"R23",
	"R24",	"R25",
	"R26",	"R27",
	"R28",	"R29",
	"R30",	"R31",
};

#define	SPR(v)	(((v&0x1f)<<16) | (((v>>5)&0x1f)<<11))

#define SPR_SPRG0	0x110		/* SPR General 0 */
#define SPR_SPRG2	0x112		/* SPR General 2 */
#define SPR_SPRG6W	0x116		/* SPR General 6; supervisor W  */

static void
vecgen(uintptr v, void (*r)(void), int r0spr)
{
	int i;
	long d;
	u32int *vp, o, ra;

	vp = UINT2PTR(v);
	vp[0] = 0x7c0003a6 | SPR(r0spr);	/* MOVW R0, SPR(r0spr) */
	vp[1] = 0x7c0802a6;			/* MOVW LR, R0 */
	vp[2] = 0x7c0003a6 | SPR(SPR_SPRG2);	/* MOVW R0, SPR(SPRG2) */
	d = (uchar*)r - (uchar*)&vp[3];
	o = (u32int)d >> 25;
	i = 3;
	if(o != 0 && o != 0x7F){
		/* a branch too far: running from ROM */
		ra = (u32int)r;
		vp[i++] = (15<<26)|(ra>>16);	/* MOVW $r&~0xFFFF, R0 */
		vp[i++] = (24<<26)|(ra&0xFFFF);	/* OR $r&0xFFFF, R0 */
		vp[i++] = 0x7c0803a6;		/* MOVW	R0, LR */
		vp[i++] = 0x4e800021;		/* BL (LR) */
	}else
		vp[i++] = (18<<26)|(d&0x3FFFFFC)|1;	/* bl (relative) */
	dcflush(PTR2UINT(vp), i*sizeof(u32int));
}

static void
sethvec(int v, void (*r)(void))
{
	vecgen(v, r, SPR_SPRG0);
}

static void
sethvec2(uintptr v, void (*r)(void))
{
	long d;
	u32int *vp;

	vp = UINT2PTR(v);
	d = (uchar*)r - (uchar*)&vp[0];
	vp[0] = (18<<26)|(d & 0x3FFFFFC);	/* b (relative) */
	dcflush(PTR2UINT(vp), sizeof(*vp));
}

static void
faultpower(Ureg *ureg, ulong addr, int read)
{
	int user, insyscall;
	char buf[ERRMAX];

	/*
	 * There must be a user context.
	 * If not, the usual problem is causing a fault during
	 * initialisation before the system is fully up.
	 */
	if(up == nil){
		panic("fault with up == nil; pc %#lux addr %#lux\n",
			ureg->pc, addr);
	}
	user = (ureg->srr1 & MSR_PR) != 0;
	insyscall = up->insyscall;
	up->insyscall = 1;
	if(fault(addr, read) < 0){
		/*
		 * It is possible to get here with !user if, for example,
		 * a process was in a system call accessing a shared
		 * segment but was preempted by another process which shrunk
		 * or deallocated the shared segment; when the original
		 * process resumes it may fault while in kernel mode.
		 * No need to panic this case, post a note to the process
		 * and unwind the error stack. There must be an error stack
		 * (up->nerrlab != 0) if this is a system call, if not then
		 * the game's a bogey.
		 */
		if(!user && (!insyscall || up->nerrlab == 0)){
			dumpregs(ureg);
			panic("fault: %#lux", addr);
		}
		sprint(buf, "sys: trap: fault %s addr=%#lux",
			read? "read": "write", addr);
		postnote(up, 1, buf, NDebug);
		if(insyscall)
			error(buf);
	}
	up->insyscall = insyscall;
}

void
kexit(Ureg*)
{
	uvlong t;
	Tos *tos;

	/* precise time accounting, kernel exit */
	tos = (Tos*)(USTKTOP-sizeof(Tos));
	cycles(&t);
	tos->kcycles += t - up->kentry;
	tos->pcycles = up->pcycles;
	tos->pid = up->pid;
// surely only need to set tos->pid on rfork and exec?
}

void
trap(Ureg *ur)
{
	u32int dear, esr;
	char buf[ERRMAX];
	int ecode, user;

	ur->cause &= 0xFFE0;
	ecode = ur->cause;
	if(ecode < 0 || ecode >= 0x2000)
		ecode = 0x3000;
	esr = getesr();
	putesr(0);

	user = (ur->srr1 & MSR_PR) != 0;
	if(user){
		cycles(&up->kentry);
		up->dbgreg = ur;
	}

	switch(ecode){

	case INT_SYSCALL:
		if(!user)
			panic("syscall in kernel: srr1 %#4.4luX pc %#p\n", ur->srr1, ur->pc);
		if(up->cnk)
			cnksyscall(ur);
 		else
			syscall(ur);
		return;	/* syscall() calls notify itself */

	case INT_PIT:
//if(!(ur->srr1 & (MSR_CE|MSR_EE)))
//    iprint("P!");
		m->intr++;
		clockintr(ur);
		break;

	case INT_MCHECK:
		if(up == nil)
			goto buggery;
		if(esr & ESR_MCI){
			faultpower(ur, ur->pc, 1);
			break;
		}
		dear = dearget();

		print("mcheck %#p esr=%#ux dear=%#ux mcsr=%#ux\n",
			ur->pc, esr, dear, getmcsr());

		ur->pc -= 4;	/* back up to faulting instruction */
		faultpower(ur, dear, !(esr&ESR_DST));
		break;

	case INT_DSI:
	case INT_DMISS:
		if(up == nil)
			goto buggery;

		dear = dearget();
		if(!user && (dear & KSEGM) == KSEG1) {
			kmapfault(ur, dear);
			break;
		}

		faultpower(ur, dear, !(esr&ESR_DST));
		break;

	case INT_ISI:
	case INT_IMISS:
		if(up == nil)
			goto buggery;

		faultpower(ur, ur->pc, 1);
		break;

	case INT_EI:
		m->intr++;
		intr(ur);
		break;

	case INT_FPA:
	case INT_FPU:
		if(fpuavail(ur))
			break;
		esr |= ESR_PIL;
		/* fall through */

	case INT_PROG:
		if(esr & ESR_PFP){	/* floating-point enabled exception */
			fputrap(ur, user);
			break;
		}
		if(esr & ESR_PIL && user){
			if(fpuemu(ur))
				break;
			/* otherwise it's an illegal instruction */
		}
	default:
		if(user){
			spllo();
			sprint(buf, "sys: trap: %s", excname(ecode, esr));
			if(ecode == INT_ALIGN)
				sprint(buf+strlen(buf), " ea=%#ux", dearget());
			postnote(up, 1, buf, NDebug);
			break;
		}
	buggery:
		print("kernel %s pc=%#p\n", excname(ecode, esr), ur->pc);
		dumpregs(ur);
		dumpstack();
		if(m->machno == 0)
			spllo();
		exit(1);
	}
	splhi();

	/* delaysched set because we held a lock or because our quantum ended */
	if(up && up->delaysched && ecode == INT_PIT){
		sched();
		splhi();
	}

	if(user){
		if(up->procctl || up->nnote)
			notify(ur);
		kexit(ur);
	}
}

void
trapinit(void)
{
	int i;
	uintptr vb;

	/*
	 * Hold off on initializing the bic until after print is setup
	 */

	/*
	 * l.s has set up IVPR and the IVORs in aligned space
	 * after end(SB) as part of the Sys structure.
	 */
	vb = PTR2UINT(sys->vectorbase);

	/*
	 * set all exceptions to trap
 	 */
	for(i = 0; i < 0x3000; i += 0x100)
		sethvec(vb+i, trapvec);

	/*
	 * set exception handlers
	 */
	sethvec(vb+INT_CI, critintrvec);
	sethvec(vb+INT_MCHECK, trapmvec);
	sethvec(vb+INT_DSI, trapvec);
	sethvec(vb+INT_ISI, trapvec);
	sethvec(vb+INT_EI, intrvec);
	sethvec(vb+INT_ALIGN, trapvec);
	sethvec(vb+INT_PROG, trapvec);
	sethvec(vb+INT_FPU, trapvec);
	sethvec(vb+INT_SYSCALL, trapvec);
	sethvec(vb+INT_APU, trapvec);
	sethvec(vb+INT_PIT, trapvec);
//	sethvec(vb+INT_FIT, trapvec);
//	vecgen(vb+INT_WDT, critintrvec, SPR_SPRG6W, 0);
	sethvec2(vb+INT_DMISS, dtlbmiss);
	sethvec2(vb+INT_IMISS, itlbmiss);
//	sethvec(vb+INT_DEBUG, trapvec);	/* TO DO: should be critical trap */
}

static char*
excname(ulong ivoff, u32int esr)
{
	int i;

	USED(esr);
	if(ivoff == INT_PROG){
		if(esr & ESR_PIL)
			return "illegal instruction";
		if(esr & ESR_PPR)
			return "privileged";
		if(esr & ESR_PTR)
			return "trap with successful compare";
		if(esr & ESR_PEU)
			return "unimplemented APU/FPU";
		if(esr & ESR_PAP)
			return "APU exception";
		if(esr & ESR_U0F)
			return "data storage: u0 fault";
	}
	for(i=0; intcause[i].off != 0; i++)
		if(intcause[i].off == ivoff)
			break;
	return intcause[i].name;
}

void
dumpstack(void)
{
	uintptr l, v;
	int i;

	if(up == nil)
		return;
	i = 0;
	for(l=(uintptr)&l; l<(uintptr)(up->kstack+KSTACK); l+=4){
		v = *(uintptr*)l;
		if(KTZERO < v && v < (ulong)etext){
			iprint("%#8.8p=%#8.8p, ", l, v);
			if(i++ == 4){
				iprint("\n");
				i = 0;
			}
		}
	}
}

void
dumpregs(Ureg *ureg)
{
	int i;
	uintptr *l;

	if(up != nil)
		iprint("cpu%d: registers for %s %d\n",
			m->machno, up->text, up->pid);
	else
		iprint("cpu%d: registers for kernel\n", m->machno);

	l = &ureg->cause;
	for(i = 0; i < nelem(regname); i += 2, l += 2)
		iprint("%s\t%#8.8p\t%s\t%#8.8p\n", regname[i], l[0], regname[i+1], l[1]);
}

uintptr
userpc(Ureg* ureg)
{
	if(ureg == nil)
		ureg = up->dbgreg;
	return ureg->pc;
}

/*
 * This routine must save the values of registers the user is not
 * permitted to write from devproc and then restore the saved values
 * before returning
 */
void
setregisters(Ureg *ureg, char *pureg, char *uva, int n)
{
	u32int status;

	status = ureg->status;
	memmove(pureg, uva, n);
	ureg->status = status;
}

/*
 * Give enough context in the ureg to produce a kernel stack for
 * a sleeping process
 */
void
setkernur(Ureg* ureg, Proc* p)
{
	ureg->pc = p->sched.pc;
	ureg->sp = p->sched.sp+BY2SE;
}

uintptr
dbgpc(Proc* p)
{
	Ureg *ureg;

	ureg = p->dbgreg;
	if(ureg == 0)
		return 0;

	return ureg->pc;
}
