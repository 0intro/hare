/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: BG/P Interrupt Controller
 *
 * All rights reserved
 *
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"ureg.h"
#include	"io.h"
#include	"../port/error.h"

#define DEBUG 0

enum
{
	Maxhandler=	MaxVector,	/* max number of interrupt handlers, assuming none shared */
};

#define BGP_MAX_GROUP		(10)
#define BGP_MAX_CORE		4

#define IRQ_TO_GROUP(irq)	((irq >> 5) & 0xf)
#define GROUP_TO_IRQ(group)	(group << 5)
#define IRQ_OF_GROUP(irq)	((irq & 0x1f))

#define BGP_IRQCTRL_TARGET_DISABLED		(0x0)
#define BGP_IRQCTRL_TARGET_BCAST_NONCRIT	(0x1)
#define BGP_IRQCTRL_TARGET_BCAST_CRIT		(0x2)
#define BGP_IRQCTRL_TARGET_BCAST_MCHECK		(0x3)
#define BGP_IRQCTRL_TARGET_NONCRIT(cpu)		(0x4 | (cpu & 0x3))
#define BGP_IRQCTRL_TARGET_CRIT(cpu)		(0x8 | (cpu & 0x3))
#define BGP_IRQCTRL_TARGET_MCHECK(cpu)		(0xc | (cpu & 0x3))

struct bg_irqctrl_group {
	u32int status;		/* status (read and write) 0 */
	u32int rd_clr_status;	/* status (read and clear) 4 */
	u32int status_clr;	/* status (write and clear)8 */
	u32int status_set;	/* status (write and set) c */

	/* 4 bits per IRQ */
	u32int target_irq[4];	/* target selector 10-20 */

	u32int noncrit_mask[BGP_MAX_CORE];/* mask 20-30 */
	u32int crit_mask[BGP_MAX_CORE];	/* mask 30-40 */
	u32int mchk_mask[BGP_MAX_CORE];	/* mask 40-50 */

	unsigned char __align[0x80 - 0x50];
};

struct bg_irqctrl {
	struct bg_irqctrl_group groups[15];
	u32int core_non_crit[BGP_MAX_CORE];
	u32int core_crit[BGP_MAX_CORE];
	u32int core_mchk[BGP_MAX_CORE];
};

/*
 * the device does not share its many interrupt vectors,
 * so we support just one handler per index
 */

typedef struct Handler	Handler;
struct Handler
{
	void	(*r)(Ureg*, void*);
	void	*arg;
	char	name[KNAMELEN];
	ulong	nintr;
	uvlong	ticks;
	uvlong	maxtick;
	uvlong	mintick;
};

static Lock	veclock;
static Handler	ivec[Maxhandler];
static struct bg_irqctrl *bg_irqctrl = (struct bg_irqctrl*) VIRTBIC;

void
dump_irq(void)
{
	int group;
	u32int *reg;

	print("bg_irqctrl: %#p\n", bg_irqctrl);

	for(group = 0; group < BGP_MAX_GROUP; group++) {
		reg = &bg_irqctrl->groups[group].target_irq[0];
		print("Group #:%d start: %#p\n", group, &bg_irqctrl->groups[group]);
		print("   %#8.8ux %#8.8ux %#8.8ux %#8.8ux",
					reg[0], reg[1], reg[2], reg[3]);
		print("	  Status Location: %#p   Clear Location: %#p\n",
				&bg_irqctrl->groups[group].status,
				&bg_irqctrl->groups[group].status_clr);
		print("   Status: %#8.8ux  Mask: %#8.8ux\n",
				bg_irqctrl->groups[group].status,
				bg_irqctrl->groups[group].noncrit_mask[0]);
	}
	print("Core_Non_Crit: %#ux\n",bg_irqctrl->core_non_crit[0]);
}

void
intrinit(void)
{
	int group;

	bg_irqctrl = UINT2PTR(VIRTBIC);

	/* mask everything */
	for(group = 0; group < BGP_MAX_GROUP; group++) {
		u32int *reg = &bg_irqctrl->groups[group].target_irq[0];
		reg[0] = 0;
		reg[1] = 0;
		reg[2] = 0;
		reg[3] = 0;

		bg_irqctrl->groups[group].status_clr = 0xffffffff;
	}

	mb();
}

static void
bg_irqctrl_set_target(unsigned int irq, unsigned int target)
{
	ulong offset = ((7 - (irq & 0x7)) * 4);

	u32int *reg =
		&bg_irqctrl->groups[IRQ_TO_GROUP(irq)].target_irq[IRQ_OF_GROUP(irq) / 8];

	if(DEBUG)
		iprint("set_target i=%d t=%d group=%d target_irq=%d offset=%lud addr: %#p value: %8.8ux\n",
							irq, target,
							IRQ_TO_GROUP(irq),
							IRQ_OF_GROUP(irq) / 8,
							offset, reg, *reg);

/* TODO: Do we need a IB() here? */
	*reg = ((*reg & ~(0xf << offset)) | ((target & 0xf) << offset));

	if(DEBUG)
		iprint("reg = %#8.8ux\n", *reg);
}

static void
bg_irqctrl_ack_irq(unsigned int irq)
{
	unsigned mask = 1U << (31 - IRQ_OF_GROUP(irq));
	bg_irqctrl->groups[IRQ_TO_GROUP(irq)].status_clr = mask;
}

void
intrenable(int v, void (*f)(Ureg*, void*), void *arg, char *name, int c)
{
	Handler *h;

	if(DEBUG)
		iprint("intrenable %d\n", v);

	if(v < 0 || v >= nelem(ivec))
		panic("intrenable(%d)", v);
	h = &ivec[v];
	if(h->r != nil && h->r != f)	/* it's safe to test this outside lock */
		panic("intrenable(%d) duplicate %#p %#p", v, h->r, f);
	ilock(&veclock);
	h->r = f;
	h->arg = arg;
	strncpy(h->name, name, KNAMELEN-1);
	h->name[KNAMELEN-1] = 0;

	/* enable bit in the BIC (hardcoded to core 0 for now) */
	bg_irqctrl_set_target(v, BGP_IRQCTRL_TARGET_NONCRIT(c));

	mb();
	iunlock(&veclock);
}

void
intrdisable(int v, void (*r)(Ureg*, void*), void *arg, char *name)
{
	Handler *h;

	if(DEBUG)
		iprint("intrdisable %d\n", v);

	if(v < 0 || v >= nelem(ivec))
		panic("intrdisable(%d)", v);
	h = &ivec[v];
	ilock(&veclock);
	if(h->r == r && h->arg == arg && strcmp(h->name, name) == 0){
		h->r = nil;
		bg_irqctrl_set_target(v, BGP_IRQCTRL_TARGET_DISABLED);
		mb();
	}
	iunlock(&veclock);
}

void
intrstats(char *buf, int size)
{
	int count;
	int o = 0;

	for(count=0;count<Maxhandler;count++) {
		if((ivec[count].r != nil)&&(ivec[count].nintr)) {
			o += snprint(buf+o, size-o, "%d %lud %llud %llud %llud\n",count, ivec[count].nintr, ivec[count].ticks, ivec[count].mintick, ivec[count].maxtick);
		}
	}
	buf[o] = 0;
}

void
intr(Ureg *ur)
{
	int cpu = m->machno;
	Proc *oup;
	int group, v, bno;
	u32int b, hierarchy, mask;
	Handler *h;
	uvlong t0,t1, td;

	oup = up;
	up = nil;	/* no process at interrupt level */

	ilock(&veclock);
	hierarchy = bg_irqctrl->core_non_crit[cpu];

	if(DEBUG)
		iprint("intr: %#8.8ux\n", hierarchy);

	/* handle spurious interrupts... */
	if(hierarchy == 0)
		goto out;

	/* find first group */
	for(group = 0; group < BGP_MAX_GROUP; group++) {
		mask = bg_irqctrl->groups[group].noncrit_mask[cpu];

		for(bno = 0; mask != 0 && bno < 32; bno++) {
			b = IB(bno);
			if((mask & b) == 0)
				continue;
			mask &= ~b;
			v = GROUP_TO_IRQ(group) + bno;
			ur->cause = v;
			h = &ivec[v];
			if(h->r == nil){
				iprint("unknown interrupt %d pc=%#p\n", v, ur->pc);
				bg_irqctrl_set_target(v, BGP_IRQCTRL_TARGET_DISABLED);
				continue;
			}

			/*
			 *  call the interrupt handler
			 */
			m->intr++;
			h->nintr++;
			cycles(&t0);
			(*h->r)(ur, h->arg);
			cycles(&t1);
			td=t1-t0;
			h->ticks += td;
			if(h->maxtick < td)
				h->maxtick = td;
			if((h->mintick > td) || (h->mintick == 0))
				h->mintick = td;	
			bg_irqctrl_ack_irq(v);
		}
	}


out:
	iunlock(&veclock);
	up = oup;
	if(up != nil)
		preempted();
}
