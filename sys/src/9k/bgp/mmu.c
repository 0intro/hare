#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"

/*
 * To do: revise the following comment to reflect the truth, tidy.
 */

/*
 * 450 mmu
 *
 * l.s has set some global TLB entries for the kernel at
 * the top of the tlb (NTLB-1 down), partly to account for
 * the way the firmware sets things up on some platforms (eg, 440).
 * The first entry not used by the kernel (eg, by kmapphys) is m->utlbhi.
 * User tlbs are assigned indices from 0 to m->utlbhi, round-robin.
 * m->utlbnext is the next to use.  Kernel tlbs are added at utlbhi, which then moves.
 *
 * The kernel entries are PID 0 (global)
 * so they are usable as the PID changes
 * (0=no user; non-zero=user process mapped).
 *
 * STID is left set to PID after each primitive, so that the [id]tlbmiss
 * code need not load STID each fault.
 */

#define	DEBUG	0

Softtlb	softtlb[MAXMACH][STLBSIZE];

static int newtlbpid(Proc*);
static void purgetlb(int);
static void putstlb(int, u32int, u32int, u32int);

static int
lookstlb(int pid, u32int va)
{
	int i;

	pid <<= 2;
	i = ((va>>12)^pid)&(STLBSIZE-1);
	if(m->stlb[i].hi == (va|pid))
		return i;

	return -1;
}

void
tlbdump(char* s)
{
	u32int hi, md, lo, stid;
	int i, p;

	p = pidget();
	iprint("cpu%d:\ntlb[%s] pid %d lastpid %d utlbnext %d\n",
		m->machno, s, p, m->lastpid, m->utlbnext);
	for(i=0; i<NTLB; i++){
		hi = tlbrehi(i);
		if(hi & TLBVALID){
			stid = stidget();
			lo = tlbrelo(i);
			md = tlbremd(i);
			iprint("%.2d %8.8ux %8.8ux %8.8ux %3d %2d\n",
				i, hi, md, lo, stid, lookstlb(i, TLBEPN(hi)));
		}
		if(i == m->utlbhi)
			print("-----\n");
	}
	stidput(p);	/* tlbrehi changes STID */
}

void
alltlbdump(void)
{
	Mreg s;
	Proc *p;
	int i, machno;

	machno = m->machno;
	s = splhi();
	iprint("cpu%d pidproc:\n", machno);
	for(i=0; i<nelem(m->pidproc); i++){
		p = m->pidproc[i];
		if(p != nil)
			iprint("%d -> %d [%d]\n", i, p->pid, p->mmupid[machno]);
	}
	tlbdump("final");
	splx(s);
}

/*
 * l.s provides a set of tlb entries for the kernel, allocating
 * them starting at the highest index down.  user-level entries
 * will be allocated from index 0 up.
 * PID and STID should be 0 on entry and exit.
 */
void
mmuinit(void)
{
	int i;
	Tlb *entry;

	m->lastpid = 0;
	pidput(0);
	stidput(0);
	m->stlb = softtlb[m->machno];
	m->utlbhi = 0;
	if(DEBUG)
		tlbdump("init0");

	for(i = 0; i < NTLB; i++){
		if(tlbrehi(i) & TLBVALID)
			break;
		m->utlbhi = i;
		tlbwrx(i, 0, 0, 0);
	}
	if(DEBUG)
		tlbdump("init1");

	lock(&phystlblock);
	for(i = 0; i < phystlbhi; i++){
		entry = &phystlb[i];
		tlbwrx(m->utlbhi, entry->hi, entry->md, entry->lo);
		m->utlbhi--;
	}
	unlock(&phystlblock);

	if(DEBUG)
		tlbdump("init2");
}

void
mmuflush(void)
{
	Mreg s;

	s = splhi();
	up->newtlb = 1;
	mmuswitch(up);
	splx(s);
}

/*
 * called with splhi
 */
void
mmuswitch(Proc *p)
{
	int tp;

	if(p->newtlb){
		memset(p->mmupid, 0, sizeof(p->mmupid));
		p->newtlb = 0;
	}
	tp = p->mmupid[m->machno];
	if(tp == 0 && !p->kp)
		tp = newtlbpid(p);
	pidput(tp);
	stidput(tp);
}

void
mmurelease(Proc* p)
{
	memset(p->mmupid, 0, sizeof(p->mmupid));
	pidput(0);
	stidput(0);
}

void
mmuput(uintptr va, uintptr pa, Page* page)
{
	Mreg s;
	int x, tp;
	char *ctl;
	u32int tlbhi, tlblo, tlbmi;

	if(va >= KZERO)
		panic("mmuput");

	tlbhi = TLBEPN(va) | TLB4K | TLBVALID;
	tlbmi = TLBRPN(pa);		/* TO DO: 36-bit physical memory */
	tlblo = TLBUR | TLBUX | TLBSR;	/* shouldn't need TLBSX */
	if(pa & PTEWRITE)
		tlblo |= TLBUW | TLBSW;
	if(pa & PTEUNCACHED)
		tlblo |= TLBI | TLBG;
	else
		tlblo |= TLBWL1|TLBU2|TLBM;

	s = splhi();
	tp = up->mmupid[m->machno];
	if(tp == 0){
		if(up->kp)
			panic("mmuput kp");
		tp = newtlbpid(up);
		pidput(tp);
		stidput(tp);
	}
	else if(pidget() != tp || stidget() != tp){
		print("mmuput: cpu%d pid up->mmupid=%#ux pidget=%#ux stidget=%#ux s=%#ux\n",
			m->machno, tp, pidget(), stidget(), s);
		alltlbdump();
		panic("mmuput: cpu%d pid %#ux %#ux s=%#ux",
			m->machno, tp, pidget(), s);
	}

	/* see if it's already there: note that tlbsx[cc] and tlbwrx use STID */
	x = tlbsxcc(va);
	if(x < 0){
		if(m->utlbnext > m->utlbhi)
			m->utlbnext = 0;
		x = m->utlbnext++;
	}else if(x > m->utlbhi)
		panic("mmuput index va=%#p x=%d klo=%d", va, x, m->utlbhi);	/* shouldn't touch kernel entries */
	if(DEBUG)
		print("put %#p %#p-> %d: %#8.8ux %#8.8ux %#8.8ux\n",
			va, pa, x, tlbhi, tlbmi, tlblo);
	if(stidget() != tp)
		panic("mmuput stid:pid mismatch");
	stidput(tp);
	tlbwrx(x, tlbhi, tlbmi, tlblo);
	putstlb(tp, TLBEPN(va), tlbmi, tlblo);
	splx(s);

	ctl = &page->cachectl[m->machno];
	switch(*ctl){
	case PG_TXTFLUSH:
		dcflush(page->va, BY2PG);
		icflush(page->va, BY2PG);
		*ctl = PG_NOFLUSH;
		break;
	case PG_DATFLUSH:
		dcflush(page->va, BY2PG);
		*ctl = PG_NOFLUSH;
		break;
	case PG_NEWCOL:
//		cleancache();		/* expensive, but fortunately not needed here */
		*ctl = PG_NOFLUSH;
		break;
	}
}

/*
 * Process must be splhi
 */
static int
newtlbpid(Proc *p)
{
	int i, s;
	Proc **h;

	i = m->lastpid;
	h = m->pidproc;
	for(s = 0; s < NTLBPID; s++) {
		i++;
		if(i >= NTLBPID)
			i = 1;
		if(h[i] == nil)
			break;
	}

	if(h[i] != nil){
		purgetlb(i);
		if(h[i] != nil)
			panic("newtlb");
	}

	m->pidproc[i] = p;
	p->mmupid[m->machno] = i;
	m->lastpid = i;

	return i;
}

static void
purgetlb(int pid)
{
	int i, machno;
	Proc *sp, **pidproc;
	Softtlb *entry, *etab;
	u32int hi;

	m->tlbpurge++;

	/*
	 * find all pid entries that are no longer used by processes
	 */
	machno = m->machno;
	pidproc = m->pidproc;
	for(i=1; i<NTLBPID; i++) {
		sp = pidproc[i];
		if(sp != nil && sp->mmupid[machno] != i)
			pidproc[i] = nil;
	}

	/*
	 * shoot down the one we want
	 */
	sp = pidproc[pid];
	if(sp != nil)
		sp->mmupid[machno] = 0;
	pidproc[pid] = nil;

	/*
	 * clean out all dead pids from the stlb;
	 */
	entry = m->stlb;
	for(etab = &entry[STLBSIZE]; entry < etab; entry++)
		if(pidproc[(entry->hi>>2)&0xFF] == nil){
			entry->hi = 0;
			entry->mid = 0;
			entry->lo = 0;
		}

	/*
	 * clean up the hardware
	 */
	for(i = 0; i <= m->utlbhi; i++){
		hi = tlbrehi(i);
		if((hi & TLBVALID) && pidproc[stidget()] == nil)
			tlbwrx(i, 0, 0, 0);
	}
	stidput(pidget());
	iccci();
}

static void
putstlb(int pid, u32int va, u32int tlbmid, u32int tlblo)
{
	Softtlb *entry;

	pid <<= 2;
	entry = &m->stlb[((va>>12)^pid)&(STLBSIZE-1)];
	entry->hi = va | pid;
	entry->mid = tlbmid;
	entry->lo = tlblo;
}
