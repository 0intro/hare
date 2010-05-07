#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "../port/error.h"

#include "/sys/src/libc/9syscall/sys.h"

#include <tos.h>
#include "ureg.h"

typedef struct {
	uintptr	pc;
	Ureg*	arg0;
	char*	arg1;
	char	msg[ERRMAX];
	Ureg*	old;
	Ureg	ureg;
} NFrame;

/*
 *   Return user to state before notify()
 */
static void
noted(Ureg* cur, uintptr arg0)
{
	Ureg *nur;
	Note note;
	NFrame *nf;

	qlock(&up->debug);
	if(arg0 != NRSTR && !up->notified){
		qunlock(&up->debug);
		pprint("suicide: call to noted when not notified\n");
		pexit("Suicide", 0);
	}
	up->notified = 0;
	fpunoted();

	nf = up->ureg;

	/* sanity clause */
	if(!okaddr(PTR2UINT(nf), sizeof(NFrame), 0)){
		qunlock(&up->debug);
		pprint("suicide: bad ureg %#p in noted\n", nf);
		pexit("Suicide", 0);
	}

	/* don't let user change system status */
	nur = &nf->ureg;
	nur->status = cur->status;

	memmove(cur, nur, sizeof(Ureg));

	switch((int)arg0){
	case NCONT:
	case NRSTR:
		if(!okaddr(nur->pc, BY2SE, 0) || !okaddr(nur->usp, BY2SE, 0)){
			qunlock(&up->debug);
			pprint("suicide: trap in noted pc=%#p sp=%#p\n",
				nur->pc, nur->sp);
			pexit("Suicide", 0);
		}
		up->ureg = nf->old;
		qunlock(&up->debug);
		break;

	case NSAVE:
		if(!okaddr(nur->pc, BY2SE, 0) || !okaddr(nur->usp, BY2SE, 0)){
			qunlock(&up->debug);
			pprint("suicide: trap in noted pc=%#p sp=%#p\n",
				nur->pc, nur->sp);
			pexit("Suicide", 0);
		}
		qunlock(&up->debug);

		splhi();
		nf->arg1 = nf->msg;
		nf->arg0 = &nf->ureg;
		cur->r3 = PTR2UINT(nf->arg0);
		nf->pc = 0;
		cur->sp = PTR2UINT(nf);
		break;

	default:
		memmove(&note, &up->lastnote, sizeof(Note));
		qunlock(&up->debug);
		pprint("suicide: bad arg %#p in noted: %s\n", arg0, note.msg);
		pexit(note.msg, 0);
		break;

	case NDFLT:
		memmove(&note, &up->lastnote, sizeof(Note));
		qunlock(&up->debug);
		if(note.flag == NDebug)
			pprint("suicide: %s\n", note.msg);
		pexit(note.msg, note.flag != NDebug);
		break;
	}
}

/*
 *  Call user, if necessary, with note.
 *  Pass user the Ureg struct and the note on his stack.
 */
int
notify(Ureg* ureg)
{
	int l;
	Mreg s;
	Note note;
	uintptr sp;
	NFrame *nf;

	if(up->procctl)
		procctl(up);
	if(up->nnote == 0)
		return 0;

	fpunotify(ureg);

	s = spllo();
	qlock(&up->debug);

	up->notepending = 0;
	memmove(&note, &up->note[0], sizeof(Note));
	if(strncmp(note.msg, "sys:", 4) == 0){
		l = strlen(note.msg);
		if(l > ERRMAX-15)	/* " pc=0x12345678\0" */
			l = ERRMAX-15;
		sprint(note.msg+l, " pc=%#.8lux", ureg->pc);
	}

	if(note.flag != NUser && (up->notified || up->notify == nil)){
		qunlock(&up->debug);
		if(note.flag == NDebug)
			pprint("suicide: %s\n", note.msg);
		pexit(note.msg, note.flag != NDebug);
	}

	if(up->notified){
		qunlock(&up->debug);
		splhi();
		return 0;
	}

	if(up->notify == nil){
		qunlock(&up->debug);
		pexit(note.msg, note.flag != NDebug);
	}
	if(!okaddr(PTR2UINT(up->notify), sizeof(ureg->pc), 0)){
		qunlock(&up->debug);
		pprint("suicide: bad function address %#p in notify\n",
			up->notify);
		pexit("Suicide", 0);
	}

	sp = ureg->usp & ~(BY2V-1);
	sp -= (sizeof(NFrame)+BY2V-1) & ~(BY2V-1);

	if(!okaddr(sp, sizeof(NFrame), 1)){
		qunlock(&up->debug);
		pprint("suicide: bad stack address %#p in notify\n", sp);
		pexit("Suicide", 0);
	}

	nf = UINT2PTR(sp);
	memmove(&nf->ureg, ureg, sizeof(Ureg));
	nf->old = up->ureg;
	up->ureg = nf;
	memmove(nf->msg, note.msg, ERRMAX);
	nf->arg1 = nf->msg;
	nf->arg0 = up->ureg;
	ureg->r3 = PTR2UINT(up->ureg);
	nf->pc = 0;

	ureg->usp = sp;
	ureg->pc = PTR2UINT(up->notify);
	up->notified = 1;
	up->nnote--;
	memmove(&up->lastnote, &note, sizeof(Note));
	memmove(&up->note[0], &up->note[1], up->nnote*sizeof(Note));

	qunlock(&up->debug);
	splx(s);

	return 1;
}

/*
 *  system calls and 'orrible notes
 */

/* TO DO: make this trap a separate asm entry point, as on some other RISC architectures */
void
syscall(Ureg* ureg)
{
	char *e;
	Ar0 ar0;
	Mreg s;
	uintptr sp;
	int i, scallnr;
	static Ar0 zar0;
	vlong startns, stopns;

	m->syscall++;
	up->insyscall = 1;
	up->pc = ureg->pc;
	up->dbgreg = ureg;
	sp = ureg->sp;
	scallnr = ureg->r3;

	if(up->procctl == Proc_tracesyscall){
		up->procctl = Proc_stopme;
		/*
		 * Redundant validaddr. Do we care?
		 * Tracing syscalls is not exactly a fast path...
		 * Beware, validaddr currently does a pexit rather
		 * than an error if there's a problem; that might
		 * change in the future.
		 */
		if(sp < (USTKTOP-BY2PG) || sp > (USTKTOP-sizeof(up->arg)-BY2SE))
			validaddr(UINT2PTR(sp), sizeof(up->arg)+BY2SE, 0);

		syscallfmt(scallnr, (va_list)(sp+BY2SE));
		procctl(up);
		if(up->syscalltrace)
			free(up->syscalltrace);
		up->syscalltrace = nil;
		startns = todget(nil);
	}

	up->scallnr = scallnr;
	if(scallnr == RFORK)
		fpusysrfork(ureg);
	spllo();

	sp = ureg->sp;
	up->nerrlab = 0;
	ar0 = zar0;
	if(!waserror()){
		if(scallnr > nsyscall){
			int nf;
			if (up->fcount) {
				int found = 0;
				nf = up->fcount;
				for (i = 0; i < nf; i++) {
					if (scallnr == up->fc[i].scnum) {
						found = 1;
						up->fc[i].fun(&ar0, up->fc);
						/* what should this be? */
						//up->psstate = up->fc[i].fun;
						up->psstate = "Private";
					}
				}
				if (! found)
					error("Bad private system call number");
			}
		} else {
			if(systab[scallnr].f == nil){
				pprint("bad sys call number %d pc %lux\n",
					scallnr, ureg->pc);
				postnote(up, 1, "sys: bad sys call", NDebug);
				error(Ebadarg);
			}

			if(sp < (USTKTOP-BY2PG) || sp > (USTKTOP-sizeof(up->arg)-BY2SE))
				validaddr(UINT2PTR(sp), sizeof(up->arg)+BY2SE, 0);

			memmove(up->arg, UINT2PTR(sp+BY2SE), sizeof(up->arg));
			up->psstate = systab[scallnr].n;

			systab[scallnr].f(&ar0, (va_list)up->arg);
		}
		poperror();
	}else{
		/* failure: save the error buffer for errstr */
		e = up->syserrstr;
		up->syserrstr = up->errstr;
		up->errstr = e;
		if(0 && vflag && up->pid == 1)
			iprint("%s: syscall %s error %s\n",
				up->text, systab[scallnr].n, up->syserrstr);
		ar0 = systab[scallnr].r;
	}
	if(up->nerrlab){
		print("bad errstack [%d]: %d extra\n", scallnr, up->nerrlab);
		for(i = 0; i < NERR; i++)
			print("sp=%#m pc=%#m\n",
				up->errlab[i].sp, up->errlab[i].pc);
		panic("error stack");
	}

	/*
	 * Put return value in frame.
	 */
	ureg->r3 = ar0.p;

	if(up->procctl == Proc_tracesyscall){
		stopns = todget(nil);
		up->procctl = Proc_stopme;
		sysretfmt(scallnr, (va_list)(sp+BY2SE), &ar0, startns, stopns);
		s = splhi();
		procctl(up);
		splx(s);
		if(up->syscalltrace)
			free(up->syscalltrace);
		up->syscalltrace = nil;
	}

	up->insyscall = 0;
	up->psstate = 0;

	if(scallnr == NOTED)
		noted(ureg, *(uintptr*)(sp+BY2SE));

	splhi();
	if(scallnr != RFORK && (up->procctl || up->nnote))
		notify(ureg);

	/* if we delayed sched because we held a lock, sched now */
	if(up->delaysched){
		sched();
		splhi();
	}
	kexit(ureg);
}

/*
 * called in sysfile.c
 */
void
evenaddr(uintptr addr)
{
	if(addr & (sizeof(uintptr)-1)){
		postnote(up, 1, "sys: misaligned address", NDebug);
		error(Ebadarg);
	}
}

uintptr
sysexecstack(uintptr stack, int argc)
{
	/*
	 * Given a current bottom-of-stack and a count
	 * of pointer arguments to be pushed onto it followed
	 * by an integer argument count, return a suitably
	 * aligned new bottom-of-stack which will satisfy any
	 * hardware stack-alignment contraints.
	 * Rounding the stack down to be aligned with the
	 * natural size of a pointer variable usually suffices,
	 * but some architectures impose further restrictions,
	 * e.g. 32-bit SPARC, where the stack must be 8-byte
	 * aligned although pointers and integers are 32-bits.
	 */
	USED(argc);

	return STACKALIGN(stack);
}

void*
sysexecregs(uintptr entry, ulong ssize, ulong nargs)
{
	uintptr *sp;
	Ureg *ureg;

	up->cnk = up->cnkexec;			/* copy CNK emulation state */
	up->cnkexec = 0;
	if(up->cnk)
		return cnksysexecregs(entry, ssize, nargs);

	sp = (uintptr*)(USTKTOP - ssize);
	*--sp = nargs;

	ureg = up->dbgreg;
	ureg->usp = PTR2UINT(sp);
	ureg->pc = entry;
	ureg->srr1 &= ~MSR_FP;			/* disable floating point */

	/*
	 * return the address of kernel/user shared data
	 * (e.g. clock stuff)
	 */
	return UINT2PTR(USTKTOP-sizeof(Tos));
}

void
sysprocsetup(Proc* p)
{
	fpusysprocsetup(p);
}

void
sysrforkchild(Proc* child, Proc* parent)
{
	Ureg *cureg;

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

	fpusysrforkchild(child, cureg, parent);
}
