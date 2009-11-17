#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "ureg.h"

/*
 * hardware floating-point
 */

enum {						/* PFPU.state */
	Init		= 0,			/* The FPU has not been used */
	Busy		= 1,			/* The FPU is being used */
	Idle		= 2,			/* The FPU has been used */

	Hold		= 1<<2,			/* Handling an FPU note (or'd in) */

	FPSsize	= sizeof(((PFPU*)0)->fpusave),
	FPEXPMASK	= 0xfff80f00,		/* Floating exception bits in fpscr */
	FPenables	= MSR_FP | MSR_FE0 | MSR_FE1,	/* enable FP and (precise) FP exceptions in machine status */
};

static PFPU	initfpustate;	/* could be in Mach but they're all the same */

int
fpudevprocio(Proc* proc, void* a, long n, uintptr offset, int write)
{
	uchar *p;

	/*
	 * Called from procdevtab.read and procdevtab.write
	 * allow user process access to the FPU registers.
	 * This is the only FPU routine which is called directly
	 * from the port code; it would be nice to have dynamic
	 * creation of entries in the device file trees...
	 */
	if(offset >= FPSsize)
		return 0;
	p = proc->fpusave;
	switch(write){
	default:
		if(offset+n > FPSsize)
			n = FPSsize - offset;
		memmove(p+offset, a, n);
		break;
	case 0:
		if(offset+n > FPSsize)
			n = FPSsize - offset;
		memmove(a, p+offset, n);
		break;
	}

	return n;
}

void
fpunotify(Ureg *ur)
{
	/*
	 * Called when a note is about to be delivered to a
	 * user process, usually at the end of a system call.
	 * Note handlers are not allowed to use the FPU so
	 * the state is marked (after saving if necessary) and
	 * checked in the FP Unavailable handler.
	 */
	if(up->fpustate == Busy){
		savefpregs(up->fpusave);
		up->fpustate = Idle;
	}
	ur->status &= ~FPenables;
	up->fpustate |= Hold;
}

void
fpunoted(void)
{
	/*
	 * Called from sysnoted() via the machine-dependent
	 * noted() routine.
	 * Clear the flag set above in fpunotify().
	 */
	up->fpustate &= ~Hold;
}

void
fpusysprocsetup(Proc* p)
{
	/*
	 * Disable the FPU.
	 * Called from sysexec() via sysprocsetup() to
	 * set the FPU for the new image.
	 */
	p->fpscr = initfpustate.fpscr;
	p->fpustate = Init;
	if(p->dbgreg != nil)
		((Ureg*)p->dbgreg)->status &= ~FPenables;
}

void
fpusysrfork(Ureg *ur)
{
	/*
	 * Called early in the non-interruptible path of
	 * sysrfork() via the machine-dependent syscall() routine.
	 * Save the state so that it can be easily copied
	 * to the child process later.
	 * If there isn't a pending exception, just save the
	 * initial state and current status, because registers
	 * were killed by the system call.
	 */
	if(up->fpustate == Busy){
		up->fpscr = getfpscr();
		if(up->cnk || (up->fpscr & FPSEX)){
			savefpregs(up->fpusave);
			up->fpustate = Idle;
		}else
			up->fpustate = Init;
	}
	ur->status &= ~FPenables;
}

void
fpusysrforkchild(Proc* child, Ureg* cureg, Proc* parent)
{
	/*
	 * Called later in sysrfork() via the machine-dependent
	 * sysrforkchild() routine.
	 * Copy the parent FPU state to the child.
	 */
	child->fpustate = parent->fpustate;
	cureg->status &= ~FPenables;
	if(child->fpustate == Init)
		return;

	memmove(child->fpusave, parent->fpusave, sizeof(child->fpusave));
}

void
fpuprocsave(Proc* p)
{
	/*
	 * Called from sched() and sleep() via the machine-dependent
	 * procsave() routine.
	 * About to go in to the scheduler.
	 * If the process wasn't using the FPU
	 * there's nothing to do.
	 */

	if(p->fpustate != Busy)
		return;

	((Ureg*)p->dbgreg)->status &= ~FPenables;

	/*
	 * The process is dead so clear and disable the FPU
	 * and set the state for whoever gets this proc struct
	 * next.
	 */
	if(p->state == Moribund){
		p->fpscr = initfpustate.fpscr;
		p->fpustate = Init;
		return;
	}

	/*
	 * On a system call, all fp registers are dead (it's a function call),
	 * so retain just the floating-point status, unless there's an exception.
	 */
	if(!up->cnk && p->insyscall){
		p->fpscr = getfpscr();
		if((p->fpscr & FPSEX) == 0){
			p->fpustate = Init;
			return;
		}
	}

	/*
	 * Save the FPU state without handling pending
	 * unmasked exceptions and disable. Postnote() can't
	 * be called here as sleep() already has up->rlock,
	 * so the handling of pending exceptions is done either
	 * at the start of trap or syscall, or deferred until the
	 * process runs again and handles `floating point unavailable'.
	 */
	savefpregs(p->fpusave);
	p->fpustate = Idle;
}

int
fpuavail(Ureg *ur)
{
	uchar *state;

	if(up == nil)
		return 0;
	if(up->fpustate & Hold){
		postnote(up, 1, "sys: floating point in note handler", NDebug);
		return 1;
	}
	switch(up->fpustate){
	case Busy:
	default:
		panic("fpuavail: state %d pc %#p", up->fpustate, ur->pc);
		return 1;
	case Init:
		state = initfpustate.fpusave;
		break;
	case Idle:
		if(up->fpscr & FPSEX){	/* might as well note enabled exceptions now */
			fputrap(ur, 1);
			return 1;
		}
		state = up->fpusave;
		break;
	}
	restfpregs(state, up->fpscr);
	up->fpustate = Busy;
	ur->status |= FPenables;
	return 1;
}

static char*
fpexcname(u32int fpscr)
{
	u32int e;

	e = fpscr & ((fpscr & 0xF8)<<22);	/* include only enabled exceptions */
	if(e & FPSVX){
		if(fpscr & FPSVXVC)
			return "invalid compare";
		return "invalid operation";	/* could subdivide further */
	}
	if(e & FPSOX)
		return "numeric overflow";
	if(e & FPSUX)
		return "numeric underflow";
	if(e & FPSZX)
		return "divide-by-zero";
	if(e & FPSXX)
		return "inexact operation";
	return "unknown";
}

void
fputrap(Ureg *ur, int user)
{
	char *s, buf[128];
	u32int fpscr;

	fpscr = up->fpscr;
	if(!user)
		panic("kernel fp trap: fpscr=%#ux fppc=%#p", fpscr, ur->pc);
	up->fpscr &= ~FPEXPMASK;
	s = fpexcname(fpscr);
	snprint(buf, sizeof(buf), "sys: fp: %s fpscr=%#ux fppc=%#p", s, fpscr, ur->pc);
	//spllo();
	postnote(up, 1, buf, NDebug);
}

int
fpuemu(Ureg*)
{
	return 0;	/* never emulated */
}

void
fpuinit(void)
{
	fpreginit();
	savefpregs(initfpustate.fpusave);
}
