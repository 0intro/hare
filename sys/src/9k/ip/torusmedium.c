#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#include "ip.h"

enum {
	X		= 0,			/* dimension */
	Y		= 1,
	Z		= 2,
	N		= 3,

	Chunk		= 32,			/* granularity of FIFO */
	Pchunk		= 8,			/* Chunks in a packet */
};

typedef struct Th Th;
struct Th {
	u8int	_0_[1];				/* Tpkt: 8 bytes */
	u8int	hint;					/* may be useful some day if we want to broadcast. */
	u8int	_1_[1];
	u8int	dst[N];				/* Destination Coordinates */
	u8int	_6_[2];
	u8int	_8_[8];				/* Tpkt: padding */

	u8int	_16_[16];			/* Ipv4 header: 20 bytes */
	u8int	ipv4dst[4];
};

typedef struct Rock Rock;
struct Rock {
	Fs*	fs;				/* file system we belong to */
	Proc*	read4p;				/* v4 read process */
	Chan*	mchan4;				/* v4 data channel */

	u8int	size[N];
};

/*
 * Called by ipoput with a single block to write.
 */
static void
torusbwrite(Ipifc* ifc, Block* b, int, uchar*)
{
	Th *th;
	Rock *r;
	int i, n;

	r = ifc->arg;

	b = padblock(b, ifc->medium->hsize);
	if(b->next)
		b = concatblock(b);
	n = ROUNDUP(BLEN(b), Chunk);
	if(BLEN(b) < n)
		b = adjustblock(b, n);

	/*
	 * The naïve mapping of IP address to torus address is
	 * to assign a Class A network to the torus and set the
	 * 3 octets of the host number to x.y.z, with z being
	 * the least significant. However, there will be a root
	 * compute node with an all zero torus address and that
	 * is reserved for use as a network number. Therefore
	 * the mapping is modified by adding 1 to the torus z
	 * coordinate to create the least significant octet of
	 * the IP address.
	 * Silently toss packets with a host address out of range.
	 * The torus will generate an unrecoverable error if it
	 * sees an address outwith its dimensions.
	 * Should there be a some ICMP notification thingy here
	 * (address unreachable)?
	 */
	th = (Th*)b->rp;
	th->hint = 0;
	th->dst[X] = th->ipv4dst[1];
	th->dst[Y] = th->ipv4dst[2];
	th->dst[Z] = th->ipv4dst[3]-1;
	if(th->dst[Z] >= 254){				/* IP was 255 or 0 */
		freeb(b);
		return;
	}
	for(i = 0; i < N; i++){
		if(th->dst[i] >= r->size[i]){
			freeb(b);
			return;
		}
	}

	r->mchan4->dev->bwrite(r->mchan4, b, 0);
	ifc->out++;
}

/*
 * Process to read from the device.
 */
static void
torusread4(void* a)
{
	Rock *r;
	Block *b;
	Ipifc *ifc;

	ifc = a;
	r = ifc->arg;
	r->read4p = up;
	if(waserror()){
		r->read4p = nil;
		pexit("hangup", 1);
	}
	for(;;){
		b = r->mchan4->dev->bread(r->mchan4, ifc->maxtu, 0);
		if(!canrlock(ifc)){
			freeb(b);
			continue;
		}
		if(waserror()){
			runlock(ifc);
			nexterror();
		}
		ifc->in++;
		b->rp += ifc->medium->hsize;
		if(ifc->lifc == nil)
			freeb(b);
		else
			ipiput4(r->fs, ifc, b);
		runlock(ifc);
		poperror();
	}
}

/*
 * Called with ifc wlock'd.
 */
static void
torusunbind(Ipifc* ifc)
{
	Rock *r;

	r = ifc->arg;
	if(r->read4p != nil)
		postnote(r->read4p, 1, "unbind", NUser);

	/* wait for readers to die */
	while(r->read4p != nil)
		tsleep(&up->sleep, return0, 0, 300);

	if(r->mchan4 != nil)
		cclose(r->mchan4);

	free(r);
}

static int
torusparse(u8int d[3], char* item, char* buf)
{
	int n;
	char *p;

	if((p = strstr(buf, item)) == nil || (p != buf && *(p-1) != '\n'))
		return -1;
	n = strlen(item);
	if(strlen(p) < n+sizeof(": x 0 y 0 z 0"))
		return -1;
	p += n+sizeof(": x ")-1;
	if(strncmp(p-4, ": x ", 4) != 0)
		return -1;
	if((n = strtol(p, &p, 0)) > 255 || *p != ' ' || *(p+1) != 'y')
		return -1;
	d[0] = n;
	if((n = strtol(p+2, &p, 0)) > 255 || *p != ' ' || *(p+1) != 'z')
		return -1;
	d[1] = n;
	if((n = strtol(p+2, &p, 0)) > 255 || (*p != '\n' && *p != '\0'))
		return -1;
	d[2] = n;

	return 0;
}

/*
 * Called to bind an Ipifc to a torus device.
 * Ifc is qlock'd.
 */
static void
torusbind(Ipifc* ifc, int argc, char* argv[])
{
	int n;
	Rock *r;
	Chan *mchan4, *schan;
	char *buf, stats[Maxpath];

	if(argc < 2)
		error(Ebadarg);

	mchan4 = nil;
	buf = nil;
	if(waserror()){
		if(mchan4 != nil)
			cclose(mchan4);
		if(buf != nil)
			free(buf);
		nexterror();
	}

	mchan4 = namec(argv[2], Aopen, ORDWR, 0);

	/*
	 * Get the dimensions of the torus/mesh from the device
	 * 'stats' file so attempts to send outside can be detected
	 * in torusbwrite.
	 */
	snprint(stats, sizeof(stats), "%sstatus", argv[2]);
	buf = smalloc(2*READSTR);
	schan = namec(stats, Aopen, OREAD, 0);
	if(waserror()){
		cclose(schan);
		nexterror();
	}
	n = schan->dev->read(schan, buf, 511, 0);
	cclose(schan);
	poperror();
	buf[n] = 0;

	r = smalloc(sizeof(*r));
	if(torusparse(r->size, "size", buf) < 0){
		free(buf);
		error(Eio);
	}

	r->mchan4 = mchan4;
	r->fs = ifc->conv->p->f;
	ifc->arg = r;
	ifc->mbps = 1001;

	free(buf);
	poperror();

	kproc("torusread4", torusread4, ifc);
}

static Medium torusmedium = {
	.name		= "torus",
	.hsize		= 16,
	.mintu		= Chunk,
	.maxtu		= Chunk*Pchunk,
	.maclen		= N,
	.bind		= torusbind,
	.unbind		= torusunbind,
	.bwrite		= torusbwrite,
};

void
torusmediumlink(void)
{
	addipmedium(&torusmedium);
}
