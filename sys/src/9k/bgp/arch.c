/*
 * EPISODE 12B
 * How to recognise different types of trees from quite a long way away.
 * NO. 1
 * THE LARCH
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#ifndef incref
int
incref(Ref *r)
{
	int x;

	lock(r);
	x = ++r->ref;
	unlock(r);
	return x;
}
#endif /* incref */

#ifndef decref
int
decref(Ref *r)
{
	int x;

	lock(r);
	x = --r->ref;
	unlock(r);
	if(x < 0)
		panic("decref pc=%#p", getcallerpc(&r));

	return x;
}
#endif /* decref */

/*
 *  Save the mach dependent part of the process state.
 */
void
procsave(Proc *p)
{
	fpuprocsave(p);
}

static void
linkproc(void)
{
	spllo();
	up->kpfun(up->kparg);
	pexit("", 0);
}

void
kprocchild(Proc* p, void (*func)(void*), void* arg)
{
	p->sched.pc = PTR2UINT(linkproc);
	p->sched.sp = PTR2UINT(p->kstack+KSTACK);
	p->sched.sp = STACKALIGN(p->sched.sp);

	p->kpfun = func;
	p->kparg = arg;
}

/*
 * power-saving wait for interrupt when nothing ready.
 * ../port/proc.c should really call this splhi itself to avoid the race avoided here by the call to anyready
 */
void
idlehands(void)
{
	if(conf.nmach > 1)
		return;

	splhi();
	if(!anyready())
		putmsr(getmsr() | MSR_WE | MSR_EE | MSR_CE);	/* MSR_DE as well? */
	spllo();
}
