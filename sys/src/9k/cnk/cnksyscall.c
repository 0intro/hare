#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "../port/error.h"
#include "cnksystab.h"

#include <tos.h>
#include "ureg.h"

/* a system call to returnok (i.e. # 0) takes 898 microseconds. */
void
cnksyscall(Ureg* ureg)
{
	char *e;
	Ar0 ar0;
	uintptr sp;
	int i, s, scallnr;
	static Ar0 zar0;
	uintptr linuxargs[6];

	/* with this short circuit in place we're down to 460 ns. Time to start short circuiting assembly code. */
	if (ureg->r0 == 666) {
		kexit(ureg);
		return;
	}

	m->syscall++;
	up->insyscall = 1;
	up->pc = ureg->pc;
	up->dbgreg = ureg;

	if(up->procctl == Proc_tracesyscall){
		up->procctl = Proc_stopme;
		procctl(up);
	}

	scallnr = ureg->r0;
	up->scallnr = scallnr;

	if(scallnr == 120)
		fpusysrfork(ureg);
	spllo();

	sp = ureg->sp;
	up->nerrlab = 0;
	ar0 = zar0;
	if(!waserror()){
		int printarg;
		if(scallnr >= ncnksyscall || cnksystab[scallnr].f == nil){
			pprint("bad CNK sys call number %d pc %#lux max %d\n",
				scallnr, ureg->pc, ncnksyscall);
			postnote(up, 1, "sys: bad sys call", NDebug);
			error(Ebadarg);
		}

		if(sp < (USTKTOP-BY2PG) || sp > (USTKTOP-sizeof(up->arg)-BY2SE))
			validaddr(UINT2PTR(sp), sizeof(up->arg)+BY2SE, 0);

		memmove(up->arg, UINT2PTR(sp+BY2SE), sizeof(up->arg));
		up->psstate = cnksystab[scallnr].n;

		linuxargs[0] = ureg->r3;
		linuxargs[1] = ureg->r4;
		linuxargs[2] = ureg->r5;
		linuxargs[3] = ureg->r6;
		linuxargs[4] = ureg->r7;
		linuxargs[5] = ureg->r8;

		if (up->cnk & 16) {print("%d:CNK: %s: pc %#p ", up->pid, cnksystab[scallnr].n,(void *)ureg->pc);
			for(printarg = 0; printarg < cnksystab[scallnr].narg; printarg++)
				print("%p ", (void *)linuxargs[printarg]);
			print("\n");
		}
		if (up->cnk&32) dumpregs(ureg);
		cnksystab[scallnr].f(&ar0, (va_list)linuxargs);
		if (up->cnk & 64){print("AFTER: ");dumpregs(ureg);}
		poperror();
	}else{
		/* failure: save the error buffer for errstr */
		if (up->cnk & 16) print("Error path in cnksyscall: %s\n", up->syserrstr);
		e = up->syserrstr;
		up->syserrstr = up->errstr;
		up->errstr = e;
		if(0 && vflag && up->pid == 1)
			iprint("%s: CNK syscall %s error %s\n",
				up->text, cnksystab[scallnr].n, up->syserrstr);
		ar0 = cnksystab[scallnr].r;
	}
	if(up->nerrlab){
		print("bad errstack [%d]: %d extra\n", scallnr, up->nerrlab);
		for(i = 0; i < NERR; i++)
			print("sp=%#ux pc=%#ux\n",
				up->errlab[i].sp, up->errlab[i].pc);
		panic("error stack");
	}

	/*
	 * Put return value in frame.
	 */
	ureg->r3 = ar0.p;
	if (up->cnk & 16)print("%d:Ret from syscall %#lx\n", up->pid, (unsigned long) ar0.p);
	if(up->procctl == Proc_tracesyscall){
		up->procctl = Proc_stopme;
		s = splhi();
		procctl(up);
		splx(s);
	}

	up->insyscall = 0;
	up->psstate = 0;

	splhi();

	if(scallnr != 120 && (up->procctl || up->nnote))
		notify(ureg);

	/* if we delayed sched because we held a lock, sched now */
	if(up->delaysched){
		sched();
		splhi();
	}
	kexit(ureg);
}

void*
cnksysexecregs(uintptr entry, ulong ssize, ulong nargs)
{
	int i;
	ulong *l;
	Ureg *ureg;
	uintptr *sp;

	if(!up->cnk)
		panic("cnksysexecregs: up->cnk %d\n", up->cnk);

	sp = (uintptr*)(USTKTOP - ssize);
	*--sp = nargs;

	ureg = up->dbgreg;
	l = &ureg->r7;
	print("Starting CNK proc pc %#lx sp %p nargs %ld\n",
		ureg->pc, sp+1, nargs);

	/* set up registers for CNK */
	/* we are dying in getenv. */
	/* because glibc does not follow the PPC ABI. */
	/* you have to push the env, then the args. */
	/* so to do this, well, we'll push an empty env on stack, i.e. shift
	 * the args down one. stack grows down. We already made space
	 * when we pushed nargs. 
	 */
	memmove(sp, sp+1, nargs * sizeof(*sp));
	sp[nargs] = 0;
	*--sp = nargs;
	ureg->r3 = nargs;
	ureg->r4 = (ulong)(sp+1);
	ureg->r5 = 0;
	ureg->r6 = 0; /* but supposed to be the aux vector */
	for(i = 7; i < 32; i++)
		*l++ = 0xdeadbeef + (i*0x110);

	ureg->usp = PTR2UINT(sp);
	ureg->pc = entry;
	ureg->srr1 &= ~MSR_FP;			/* disable floating point */

	print("Starting CNK proc pc %#lx\n", ureg->pc);

	/*
	 */
	return UINT2PTR(nargs);
}

void
cnksysrforkchild(Proc* child, Proc* parent, uintptr newsp)
{
	Ureg *cureg;

	/* don't clear cnk any more. cnk procs can now fork */
	child->cnkexec = 0;
	/*
	 */
	child->sched.sp = PTR2UINT(child->kstack+KSTACK-(UREGSIZE+2*BY2SE));
	child->sched.pc = PTR2UINT(sysrforkret);

	cureg = (Ureg*)(child->sched.sp+2*BY2SE);
	memmove(cureg, parent->dbgreg, sizeof(Ureg));

	/* Things from bottom of syscall which were never executed */
	child->psstate = 0;
	child->insyscall = 0;

	cureg->r3 = 0;
	cureg->usp = newsp;
	child->hang = 1;

	dumpregs(cureg);
	fpusysrforkchild(child, cureg, parent);
}
