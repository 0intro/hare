typedef struct Hdr Hdr;
typedef struct Frags Frags;

#pragma incomplete Frags

struct Hdr {
	uchar	len[2];	/* byte length - 1 */
	uchar	off[2];	/* offset adjusted by hardware (12 bits); top 4 bits of sequence */
	uchar	seq;		/* low order bits of sequence (zero if raw) */
	uchar	src[3];	/* source node address */
};

enum{
	M_HDRLEN=	8,

	Maxmsglen=	64*1024,
};

Frags*	newfrags(int hsize, int mintu, int maxtu);
void	freefrags(Frags*);
void	ttpfragment(Frags*, Block*, void (*)(Block*,void*), void*);
void	ttphwfragment(Frags*, Block*, void (*)(Block*,void*), void*);
Block*	ttpreassemble(Frags*, uchar*, int);
void	dumpblock(Block*);

/* these don't belong here */
ulong	nhget3(uchar*);
void	hnput3(uchar*, ulong);
