/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Based on ppc440/io.h
 *
 * Description: IO support definitions and data structures
 *
 * All rights reserved
 *
 */

enum
{	/* BIC non-critical interrupts */
	VectorNCGRP0=0*32,	/* Group 0: non-critical, core to core */
		VectorC2C=VectorNCGRP0,	/* non-critical, core to core */
	VectorNCGRP1=1*32,	/* Group 1: DMA Fata Interrupts */
		VectorTorusFatal=VectorNCGRP1,
	VectorNCGRP2=2*32,	/* Group 2: DMA Non-Fatal Interrupts */
		VectorTorusWatermark=VectorNCGRP2,
	VectorNCGRP3=3*32,	/* Group 3: DMA Non-Fatal Interrupts */
		VectorTorusCount=VectorNCGRP3,
	VectorNCGRP4=4*32,	/* Group 4 */
		VectorGlobalInt0=VectorNCGRP4+12,	/* GIB 12:21 */
	VectorNCGRP5=5*32,
		VectorTree0=VectorNCGRP5,
	VectorNCGRP6=6*32,
	VectorNCGRP7=7*32,
	VectorNCGRP8=8*32,
		VectorTOMAL0=VectorNCGRP8,
		VectorTOMAL1=VectorNCGRP8+1,
	VectorNCGRP9=9*32,
		VectorXEMAC0=VectorNCGRP9,
	MaxVector=	10*32,
};

/*
 * IBM bit-ordering is used throughout for 32-bit registers,
 * i.e the MSb is numbered 0 and the LSb is numbered 31.
 * In IB(o), 'o' is the IBM bit offset in the register. For
 * multi-bit fields use IF(v, o, w) where 'v' is the value
 * of the bit-field of width 'w' with LSb at bit offset 'o'.
 *
 * A pox on you, muddle-endian.
 */
#define IB(o)		((1<<(31-(o))))
#define IF(v, o, w)	(((v) & ((1<<(w))-1))<<(32-(o)-(w)))

/*
 * And back in the real world we have...
 */
#define F(v, o, w)	(((v) & ((1<<(w))-1))<<(o))

/*
 * low-level interface to tree network
 */
enum {
	/* injection header bits */
	PIH_P2P			=	IB(4),
	PIH_INTR			=	IB(5),

	/* variant 1: opcode, size and tag */
	PIH_OPCODEMASK		=	IF(~0, 6, 3),
	PIH_NONE		=	IF(0, 6, 3),
	PIH_OR			=	IF(1, 6, 3),
	PIH_AND			=	IF(2, 6, 3),
	PIH_XOR			=	IF(3, 6, 3),
	PIH_MAX			=	IF(5, 6, 3),
	PIH_ADD			=	IF(6, 6, 3),
	PIH_SIZEMASK	=	IF(~0, 9, 7),	/* size of global operand in bits */
	PIH_TAGMASK		=	IF(~0, 16, 14),

	/* variant 2: point-to-point address */
	PIH_TARGETMASK		=	IF(~0, 6, 24),

	/* both variants */
	PIH_CSUM_NONE		=	IF(0, 30, 2),
	PIH_CSUM_HDR_D16	=	IF(1, 30, 2),
	PIH_CSUM_HDR_VCFG	=	IF(2, 30, 2),
	PIH_CSUM_ALL		=	IF(3, 30, 2),
};
#define	MKP2P(class, addr)	(IF((class),0,4) | PIH_P2P | IF((addr),6,24))
#define	MKTAG(class,irq,tag,op)	(IF((class),0,4) | IF((irq),5, 1) | IF((tag),16,15) | (op))
#define	TAGOFPK(hdr)	(((hdr)>>2)&0x3FFF)

void	treenetinit(void);
void	treenetreset(void);
long	treenetread(int, void*, long);

long	treenetsend(int, Block*);
Block*	treenetbread(int, long);
Block*	treep2p(int, int);
Block*	treetag(int, int, int, int);
void	treedumpblock(Block*, char*);
long	treenetstats(int, char*, long);
int	treenetroutes(int, u32int*, int);

extern u32int xyztop2p(u8int, u8int, u8int);

/* BG/[LP] specific drivers */
void	chunk2fifo(void*, void*, int);
void	fifo2chunk(void*, void*, int);
void	fifo2quad(void*, void*, int);
void	quad2fifo(void*, void*, int);

/*
 * kernel barriers
 */
void	claimbarrier(int);
void	releasebarrier(int);
int	globalbarrier(int, ulong);

/*
 * Toe-puppet.
 */
typedef struct Dd Dd;
typedef struct Mal Mal;
typedef struct Toe Toe;

typedef struct Dd {				/* Device Descriptor */
	u8int	code;
	u8int	command;
	u16int	posted;				/* Posted Length */
	u16int	status;
	u16int	length;
	u64int	address;			/* maybe u32int[2] */
} Dd;

enum {
	LastTx		= 0x01,			/* Last Tx buffer in frame */
	NotifyReq	= 0x02,			/* Interrupt when done */
	Signal		= 0x04,			/* Update .status when done */
	BdCode		= 0x20,			/* Branch Descriptor Code */
	PdCode		= 0x60,			/* Pointer Descriptor Code */

	ReplTag		= 0x01,			/* Replace VLAN Tag */
	InsTag		= 0x02,			/* Insert VLAN Tag */
	ReplSA		= 0x04,			/* Replace Source Address */
	InsSA		= 0x08,			/* Insert Source Address */
	GenPad		= 0x10,			/* Generate Padding */
	GenFCS		= 0x20,			/* Generate FCS */
	CkEnable	= 0x40,			/* Enable Hardware Checksum */

	TUIok		= 0x0002,		/* TCP/UDP/IP Checksum Valid */
	Iok		= 0x0004,		/* IP Checksum Valid */
	TUok		= 0x0008,		/* TCP/UDP Checksum Valid */
	LastRx		= 0x8000,		/* Last Rx buffer in frame */
};

typedef struct Mal {
	uintptr	reg;
	void*	ctlr;
	int	chno;				/* channel number */
	int	fl;				/* frame length */

	int	nrd;
	int	ntd;
	Dd*	dd;

	Dd*	rd;				/* descriptor ring */
	Block**	rb;
	int	ri;				/* index of current frame */
	u32int	rf;				/* number of received frames */

	Lock	tlock;
	Dd*	td;				/* descriptor ring */
	Block**	tb;
	int	tci;				/* index of current frame */
	int	tpi;
	int	tpf;				/* number of posted frames */
} Mal;

extern int malnpn(Mal*);
extern Mal* malpnp(int, int, int, int);
extern Block* malallocb(Mal*);
extern int malattach(Mal*);
extern int maldetach(Mal*);
extern int toenpn(void);
extern int toepnp(void);
extern int xgmiinpn(void*);
extern void* xgmiipnp(void);
extern int xgmiireset(void*);

extern void maltxstart(Mal*);
