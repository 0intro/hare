enum
{
	Eaddrlen	= 6,
	ETHERHDRSIZE	= 14,		/* size of an ethernet header */
	
	ETHERMINTU	= 60,		/* minimum transmit size */
	ETHERMAXTU	= 1514,		/* maximum transmit size */	

	MaxEther	= 1,
	Ntypes		= 8,
};

typedef struct Ether Ether;
struct Ether {
	RWlock;				/* TO DO */

	char*	type;			/* hardware info */
	int	port;
	u64int	pa;
	uintptr	reg;
	int	irq;

	int	ctlrno;
	int	tbdf;			/* type+busno+devno+funcno */
	int	minmtu;
	int	maxmtu;
	uchar	ea[Eaddrlen];
	int	encry;

	void	(*attach)(Ether*);	/* filled in by reset routine */
	void	(*closed)(Ether*);
	void	(*detach)(Ether*);
	void	(*transmit)(Ether*);
	void	(*interrupt)(Ureg*, void*);
	long	(*ifstat)(Ether*, void*, long, ulong);
	long	(*ctl)(Ether*, void*, long); /* custom ctl messages */
	void	(*power)(Ether*, int);	/* power on/off */
	void	(*shutdown)(Ether*);	/* shutdown hardware before reboot */
	void	*ctlr;
	int	pcmslot;		/* PCMCIA */
	int	fullduplex;		/* non-zero if full duplex */

	Queue*	oq;

	Netif;
};

typedef struct Etherpkt Etherpkt;
struct Etherpkt
{
	uchar	d[Eaddrlen];
	uchar	s[Eaddrlen];
	uchar	type[2];
	uchar	data[1500];
};

extern Block* etheriq(Ether*, Block*, int);
extern void addethercard(char*, int(*)(Ether*));
extern int archether(int, Ether*);
