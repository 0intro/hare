/*
 * malloc
 *
 *	Uses Quickfit (see SIGPLAN Notices October 1988)
 *	with allocator from Kernighan & Ritchie
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

typedef double Align;

typedef union Header Header;

union Header {
	struct {
		Header*	next;
		uint	size;
	} s;
	Align	al;
};
#define	NUNITS(n) (((n)+sizeof(Header)-1)/sizeof(Header) + 1)

#define	NQUICK	((512/sizeof(Header))+1)

static	Header	*quicklist[NQUICK+1];
static	Header	misclist;
static	Header	*rover;
static	unsigned tailsize;
static	Header	*tailptr;
static	Header	checkval;
static	int	morecore(unsigned);

static	void	qfreeinternal(void*);
static	int	qstats[32];

static Lock mainlock;
#define	LOCK	ilock(&mainlock)
#define	UNLOCK	iunlock(&mainlock)

#define	tailalloc(p,n) ((p)=tailptr, tailsize -= (n), tailptr+=(n),\
			 (p)->s.size=(n), (p)->s.next = &checkval)

#define ISPOWEROF2(x)	(/*((x) != 0) && */!((x) & ((x)-1)))
#define ALIGNHDR(h, a)	(Header*)((((uintptr)(h))+((a)-1)) & ~((a)-1))

static void*
qmallocalign(ulong nbytes, ulong align, long offset, ulong span)
{
	int aligned, naligned;
	Header **pp, *p, *q, *r;
	uint nunits, n;

	if(offset != 0 || span != 0)
		return nil;

	if(!ISPOWEROF2(align) || align < sizeof(Header))
		return nil;

	LOCK;
	qstats[5]++;
	nunits = NUNITS(nbytes);
	if(nunits <= NQUICK){
		/*
		 * Look for a conveniently aligned block
		 * on one of the quicklists.
		 */
		pp = &quicklist[nunits];
		for(p = *pp; p != nil; p = p->s.next){
			if((((uintptr)(p+1)) & (align-1)) == 0){
				*pp = p->s.next;
				p->s.next = &checkval;
				qstats[6]++;
				UNLOCK;
				return p+1;
			}
			pp = &p->s.next;
		}
	}

	if(nunits > tailsize) {
		/* hard way */
		if((q = rover) != nil){
			do {
				p = q->s.next;
				if(p->s.size < nunits)
					continue;
				aligned = !(((uintptr)(p+1)) & (align-1));
				naligned = NUNITS(align)-1;
				if(!aligned && p->s.size < nunits+naligned)
					continue;

				/*
				 * This block is big enough, remove it
				 * from the list.
				 */
				q->s.next = p->s.next;
				rover = q;
				qstats[7]++;

				/*
				 * Free any runt in front of the alignment.
				 */
				if(!aligned){
					r = p;
					p = ALIGNHDR(p+1, align) - 1;
					n = p - r;
					p->s.size = r->s.size - n;

					r->s.size = n;
					r->s.next = &checkval;
					qfreeinternal(r+1);
					qstats[8]++;
				}

				/*
				 * Free any residue after the aligned block.
				 */
				if(p->s.size > nunits){
					r = p+nunits;
					r->s.size = p->s.size - nunits;
					r->s.next = &checkval;
					qstats[9]++;
					qfreeinternal(r+1);

					p->s.size = nunits;
				}

				p->s.next = &checkval;
				UNLOCK;
				return p+1;
			} while((q = p) != rover);
		}
		if(!morecore(nunits)){
			UNLOCK;
			return nil;
		}
	}

	q = ALIGNHDR(tailptr+1, align);
	if(q == tailptr+1){
		tailalloc(p, nunits);
		qstats[10]++;
	}
	else{
		/*
		 * Save the residue before the aligned allocation
		 * and free it after the tail pointer has been bumped
		 * for the main allocation.
		 */
		n = q-tailptr - 1;
		tailalloc(r, n);
		tailalloc(p, nunits);
		qstats[11]++;
		qfreeinternal(r+1);
	}

	UNLOCK;
	return p+1;
}

static void*
qmalloc(ulong nbytes)
{
	Header *p, *q;
	uint nunits;

	LOCK;
	qstats[0]++;
	nunits = NUNITS(nbytes);
	if(nunits <= NQUICK && (p = quicklist[nunits]) != nil) {
		quicklist[nunits] = p->s.next;
		p->s.next = &checkval;
		qstats[1]++;
		UNLOCK;
		return p+1;
	}
	if(nunits > tailsize) {
		/* hard way */
		if((q = rover) != nil){
			do {
				p = q->s.next;
				if(p->s.size >= nunits) {
					if(p->s.size > nunits) {
						p->s.size -= nunits;
						p += p->s.size;
						p->s.size = nunits;
					} else
						q->s.next = p->s.next;
					p->s.next = &checkval;
					rover = q;
					qstats[2]++;
					UNLOCK;
					return p+1;
				}
			} while((q = p) != rover);
		}
		if(!morecore(nunits)){
			UNLOCK;
			return nil;
		}
	}
	qstats[3]++;
	tailalloc(p, nunits);
	UNLOCK;
	return p+1;
}

static int
morecore(uint nunits)
{
	/*
	 * To be done.
	 */
	iprint("morecore %ud\n", nunits);

	return 0;
}

static void
qfreeinternal(void *ap)
{
	Header *p, *q;
	uint nunits;

	if(ap == nil)
		return;
	qstats[16]++;

	p = (Header*)ap - 1;
	if((nunits = p->s.size) == 0 || p->s.next != &checkval)
		panic("malloc: corrupt allocation arena\n");
	if(tailptr != nil && p+nunits == tailptr) {
		/* block before tail */
		tailptr = p;
		tailsize += nunits;
		qstats[18]++;
		return;
	}
	if(nunits <= NQUICK) {
		q = quicklist[nunits];
		p->s.next = q;
		quicklist[nunits] = p;
		qstats[17]++;
		return;
	}
	if((q = rover) == nil) {
		q = &misclist;
		q->s.size = 0;
		q->s.next = q;
	}
	for(; !(p > q && p < q->s.next); q = q->s.next)
		if(q >= q->s.next && (p > q || p < q->s.next))
			break;
	if(p+p->s.size == q->s.next) {
		p->s.size += q->s.next->s.size;
		p->s.next = q->s.next->s.next;
		qstats[19]++;
	} else
		p->s.next = q->s.next;
	if(q+q->s.size == p) {
		q->s.size += p->s.size;
		q->s.next = p->s.next;
		qstats[20]++;
	} else
		q->s.next = p;
	rover = q;
}

ulong
msize(void* ap)
{
	Header *p;
	uint nunits;

	if(ap == nil)
		return 0;

	p = (Header*)ap - 1;
	if((nunits = p->s.size) == 0 || p->s.next != &checkval)
		panic("malloc: corrupt allocation arena\n");

	return (nunits-1) * sizeof(Header);
}

static void
mallocdump(void)
{
	int i, n, t;
	Header *q;

	for(i = 0; i <= NQUICK; i++) {
		n = 0;
		for(q = quicklist[i]; q != nil; q = q->s.next){
			if(q->s.size != i)
				iprint("q%d\t%#p\t%ud\n", i, q, q->s.size);
			n++;
		}
		if(n != 0)
			iprint("q%d %d\n", i, n);
	}
	if((q = rover) != nil){
		i = t = 0;
		do {
			t += q->s.size;
			i++;
			iprint("m%d\t%#p\n", q->s.size, q);
		} while((q = q->s.next) != rover);

		iprint("rover: %d blocks total %d\n", i, t);
	}

	for(i = 0; i < 32; i++){
		if(qstats[i] == 0)
			continue;
		iprint("qstats[%d] %ud\n", i, qstats[i]);
	}
}

void
mallocsummary(void)
{
	mallocdump();
}

void
free(void *ap)
{
	LOCK;
	qfreeinternal(ap);
	UNLOCK;
}

void*
malloc(ulong size)
{
	void* v;

	if((v = qmalloc(size)) != nil)
		memset(v, 0, size);

	return v;
}

void*
mallocz(ulong size, int clr)
{
	void *v;

	if((v = qmalloc(size)) != nil && clr)
		memset(v, 0, size);

	return v;
}

void*
mallocalign(ulong nbytes, ulong align, long offset, ulong span)
{
	return qmallocalign(nbytes, align, offset, span);
}

void*
smalloc(ulong size)
{
	void *v;

	while((v = malloc(size)) == nil)
		tsleep(&up->sleep, return0, 0, 100);
	memset(v, 0, size);

	return v;
}

void
setmalloctag(void*, ulong)
{
}

void
mallocinit(void)
{
	int i, n, upages, kpages;
	ulong maxkpa;
	Confmem *m;
	Pallocmem *pm;

	upages = conf.upages;
	kpages = conf.npage - upages;
	pm = palloc.mem;
	maxkpa = -KZERO;
	for(i=0; i<nelem(conf.mem); i++){
		m = &conf.mem[i];
		n = m->npage;
		if(n > kpages)
			n = kpages;
		if(m->base >= maxkpa)
			n = 0;
		else if(n > 0 && m->base+n*BY2PG >= maxkpa)
			n = (maxkpa - m->base)/BY2PG;
		/* first give to kernel */
		if(n > 0){
			m->kbase = PTR2UINT(KADDR(m->base));
			m->klimit = PTR2UINT(KADDR(m->base+n*BY2PG));
//			xhole(m->base, n*BY2PG);
if(tailptr == nil){
	tailptr = KADDR(m->base);
	tailsize = NUNITS(n*BY2PG);
}
			kpages -= n;
		}
		/* if anything left over, give to user */
		if(n < m->npage){
			if(pm >= palloc.mem+nelem(palloc.mem)){
				print("xinit: losing %lud pages\n", m->npage-n);
				continue;
			}
			pm->base = m->base+n*BY2PG;
			pm->npage = m->npage - n;
			pm++;
		}
	}
iprint("tailptr %#p tailsize %ud\n", tailptr, tailsize);
}
