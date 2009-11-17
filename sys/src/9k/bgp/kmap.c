#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "ureg.h"

enum {
	NKMap		= 64,			/* Overestimate? */
	NKMtlb		= 3,

	Minutlb		= NTLB/2,		/* >= this for user processes */
};

static Lock kmlock;
static KMap kmpte[NKMap];
static KMap* kmfree;
static uintptr kmdma = KSEG1 + (NKMap<<PGSHIFT);

static void
kmapdump(void)
{
	int i;
	KMap *k;

	DBG("kmfree %#p (%ld)\n", kmfree, kmfree == nil? -1: kmfree-kmpte);
	DBG("kmtlblo %d %d kmactive %ld\n",
		m->kmtlblo, m->kmtlbhi,
		m->kmactive == nil? -1: m->kmactive-kmpte);
	for(i = 0; i < NKMap; i++){
		k = &kmpte[i];
		//if(k->pa == 0)
		//	continue;
		DBG("kmpte%d: %#p %#p next %ld kmonmach %ld\n",
			i, k->va, k->va, k->next == nil? -1: k->next-kmpte,
			k->kmonmach[0]-kmpte);
		USED(k);
	}
}

void
kmapinit(void)
{
	KMap *k;

	/*
	 * Note: .(km|u)tlbhi are inclusive.
	 */
	m->kmtlbhi = m->utlbhi;
	m->utlbhi -= NKMtlb;
	m->kmtlblo = m->utlbhi+1;
	m->kmtlbnext = m->kmtlblo;

	if(m->machno != 0)
		return;

	lock(&kmlock);
	kmfree = kmpte;
	for(k = kmpte; k < &kmpte[NKMap-1]; k++)
		k->next = k+1;
	k->next = nil;

	unlock(&kmlock);
}

void
kunmap(KMap* k)
{
	Mreg s;

DBG("kunmap@%#p: index %ld km %#p %#p ref %d\n",
    getcallerpc(&k), k-kmpte, k->va, k->pa, k->ref);

	s = splhi();
	if(decref(k) == 0){
		k->va = 0;
		k->pa = 0;
		k->sz = 0;
		k->pg = nil;

		lock(&kmlock);
		k->next = kmfree;
		kmfree = k;
		unlock(&kmlock);
	}
	splx(s);
}

static void
kmapinvalx(int clock)
{
	int machno, i;
	KMap *k, *next;
	uchar *kmtlbx;

	if(m->kmactive == nil)
		return;

if(!clock) kmapdump();
	stidput(0);
	kmtlbx = m->kmtlbx;
DBG("kmapinval: curpid %d *kmtlbx %d\n", pidget(), *kmtlbx);
//this loop is from kmtlblo to kmtlbhi, surely?
	for(i = 0; i < NTLB; i++, kmtlbx++){
		if(*kmtlbx == 0)
			continue;
DBG("kmapinval: zap %d\n", i);
		tlbwrx(i, 0, 0, 0);
		*kmtlbx = 0;
	}
	stidput(pidget());

	machno = m->machno;
	for(k = m->kmactive; k != nil; k = next){
DBG("kmapinval: kunmap km %#p %#p ref %d\n", k->va, k->pa, k->ref);
		next = k->kmonmach[machno];
		kunmap(k);
	}

	m->kmactive = nil;
	m->kmtlbnext = m->kmtlblo;
}

void
kmapinval(void)
{
	/*
	 * Called from hzclock().
	 * Fix to have an 'archhzclock() routine.
	 */
	kmapinvalx(1);
}

KMap*
kmap(Page* pg)
{
	Mreg s;
	int i, x;
	KMap *k;
	u32int hi, md;

	s = splhi();
	lock(&kmlock);

DBG("kmap: kmfree %#p\n", kmfree);
	for(i = 0; kmfree == nil; i++){
		unlock(&kmlock);
		kmapinvalx(0);
		lock(&kmlock);
		if(kmfree == nil){
			unlock(&kmlock);
			splx(s);
			delay(100);
			splhi();
			lock(&kmlock);
			DBG("out of kmap %d\n", i);
		}
	}

	k = kmfree;
	kmfree = k->next;
DBG("kmap: use %ld, head now %ld\n", k-kmpte, kmfree == nil? -1: kmfree-kmpte);

	k->ref = 2;
	k->kmonmach[m->machno] = m->kmactive;
	m->kmactive = k;
	k->pg = pg;
	k->pc = getcallerpc(&pg);

	k->va = KSEG1 + ((k - kmpte)<<PGSHIFT);	/* munge k->sz eventually */
	hi = TLBEPN(k->va)|TLB4K|TLBVALID;	/* munge k->sz eventually */
	md = TLBRPN(pg->pa)/*|TLBERPN(pg->pa)*/;/* botch: pg->pa only 32b */
	k->pa = md;

	if(stidget() != pidget()){
		iprint("kmap mismatch: stid %d pid %d\n", stidget(), pidget());
		stidput(pidget());
	}
	x = m->kmtlbnext;
	tlbwrx(x, hi, md, TLBWL1|TLBU2|TLBM|TLBWR);

	m->kmtlbx[x] = 1;
	if(x++ >= m->kmtlbhi)
		x = m->kmtlblo;
	m->kmtlbnext = x;

	unlock(&kmlock);
	splx(s);
DBG("kmap@%#p: pg %#p %#p km %#p %#p x %d\n",
    k->pc, pg->va, pg->pa, k->va, k->pa, x);

	return k;
}

void
kmapfault(Ureg*, uintptr addr)
{
	int x;
	KMap *k, *f;


	x = (addr & ~KSEGM)/BY2PG;
	if(x >= NKMap)
		panic("kmapfault: %#p", addr);

	k = &kmpte[x];
DBG("kmapfault@%#p: km %#p %#p x %d\n", addr, k->va, k->pa, m->kmtlbnext);
	if(k->va == 0)
		panic("kmapfault: unmapped %#p", addr);

	for(f = m->kmactive; f != nil; f = f->kmonmach[m->machno])
		if(f == k)
			break;
	if(f == nil){
		incref(k);
		k->kmonmach[m->machno] = m->kmactive;
		m->kmactive = k;
	}

	x = m->kmtlbnext;
	tlbwrx(x, TLBEPN(k->va)|TLB4K|TLBVALID, k->pa, TLBWL1|TLBU2|TLBM|TLBWR);

	m->kmtlbx[x] = 1;
	if(x++ >= m->kmtlbhi)
		x = m->kmtlblo;
	m->kmtlbnext = x;
}

/*
 * return required size and alignment to map n bytes in a tlb entry
 */
usize
kmapsize(usize n)
{
	int i;
	usize size;

	size = 1*KiB;
	for(i = 0; i < 10 && size < n; i++)
		size <<= 2;
	if(i >= 10)
		return 0;

	/*
	 * skip sizes not implemented by hardware
	 */
	if(i == 6 || i == 8)
		size <<= 2;

	return size;
}

/*
 * map a physical addresses at pa to va, with the given attributes.
 * the virtual address must not be mapped already (but no check is made...).
 * if va is nil, map it at pa in virtual space.
 */
void*
kmapphys(uintptr va, u64int pa, unsigned nb, unsigned attr)
{
	Mreg s;
	int i;
	Tlb *entry;
	unsigned size;
	u32int hi, md;

	if(va == 0)
		va = pa;	/* simplest is to use a 1-1 map */
	size = 1*KiB;
	for(i = 0; i < 10 && size < nb; i++)
		size <<= 2;
	if(i >= 10)
		return nil;
	if(i == 6 || i == 8)
		i++;	/* skip sizes not implemented by hardware */

	hi = TLBEPN(va)|(i<<4)|TLBVALID;
	md = TLBRPN(pa)|TLBERPN(pa);

	if(m->utlbhi <= Minutlb)
		panic("kmapphys m->utlbhi %d", m->utlbhi);

	lock(&phystlblock);
	if(phystlbhi >= nelem(phystlb)-1){
		unlock(&phystlblock);

		return nil;
	}
	entry = &phystlb[phystlbhi++];
	entry->hi = hi;
	entry->md = md;
	entry->lo = attr;
	unlock(&phystlblock);

	s = splhi();
	stidput(0);
	tlbwrx(m->utlbhi, hi, md, attr);
	m->utlbhi--;
	stidput(pidget());
	splx(s);
//print("cpu%d: phystlhi %d utlbhi %d %#p %#llux %#ux %#ux %#ux\n",
//    m->machno, phystlbhi-1, m->utlbhi+1, va, pa, hi, md, attr);
	if(DBGFLG)
		tlbdump("kmapphys");

	return UINT2PTR(va);
}

void
kunmapphys(void* va)
{
	/*
	 * Stub for now.
	 * How to synchronise the TLBs?
	 */
	USED(va);
}

void*
kmapdma(usize n)
{
	void *a;
	uintptr pa, va;

	if((n = kmapsize(n)) == 0)
		return nil;
	if((a = mallocalign(n, n, 0, 0)) == nil)
		return nil;

	dcflush(PTR2UINT(a), n);

	pa = PADDR(a);
	lock(&kmlock);
	kmdma = ROUNDUP(kmdma, n);
	va = kmdma;
	kmdma += n;
	unlock(&kmlock);
	if(va > KSEG1 + 0x10000000){
		free(a);
		return nil;
	}
//print("kmapdma: va %#p pa %#p n %lud kmdma %#p\n", va, pa, n, kmdma);

	return kmapphys(va, pa, n, TLBI|TLBG|TLBWR);
}
