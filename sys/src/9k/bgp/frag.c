#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#include "../ip/ip.h"		/* for hnputs etc */

#include "frag.h"

enum{
	MaxHdrlen=	32,		/* hsize and our header */
};

#define	DPRINT	if(0)print
#define	IPRINT	if(0)iprint

#define	FMSGSIZE(m)	(1<<(((m)->flags&0x1F)+1))

typedef struct Hmsg Hmsg;
struct Hmsg {
	Block*	b;
	ulong	src;		/* 24 bits */
	uint	seq;
	uchar	hdr[MaxHdrlen];
	int	mlen;	/* message length */
	int	needed;	/* number of fragments needed (set only once first packet received) */
	int	arrived;	/* number of unique fragments received */
	int	time;		/* TO DO */
	uchar	*seen;
	Hmsg*	next;
};

enum {
	Hmask=	(256-1),
	Hsize=	Hmask+1,

	Maxseq=	(1<<12)-1,	/* 12 bits for sequence number */
};

struct Frags {
	Hmsg*	msgs[Hsize];
	Lock	msgl[Hsize];	/* ilock per hash bucket */
	int	hsize;	/* medium header size (also offset of sw header) */
	int	hdrlen;	/* header size in packet */
	int	mintu;	/* medium's min packet */
	int	maxtu;	/* medium's max packet */
	int	fragsize;	/* largest payload (excluding f->hdrlen) */
	int	mapsize;	/* size of bitmap to detect duplicate fragments */
	Lock	seql;		/* protect sequence number */
	ushort	seq;
	int	hwfrag;	/* hardware will fragment */
};

Frags*
newfrags(int hsize, int mintu, int maxtu)
{
	Frags *f;

	f = mallocz(sizeof(Frags), 1);
	if(f == nil)
		return nil;
	f->hsize = hsize;
	f->mintu = mintu;
	f->maxtu = maxtu;
	f->hdrlen = f->hsize + M_HDRLEN;
	f->fragsize = maxtu - f->hdrlen;
	f->mapsize = HOWMANY(Maxmsglen, f->fragsize);
	f->seq = 1;
	return f;
}

void
freefrags(Frags *f)
{
	Hmsg *h;
	int i;

	if(f != nil){
		for(i=0; i<nelem(f->msgs); i++){
			while((h = f->msgs[i])!= nil){
				f->msgs[i] = h->next;
				freeb(h->b);
				free(h);
			}
		}
		free(f);
	}
}

void
dumpblock(Block *b)
{
	int i, n;

	n = BLEN(b);
	print("[%3d]: ", n);
	if(n > 64)
		n = 64;
	for(i=0; i<n; i++)
		print(" %.2ux", b->rp[i]);
	if(n == 64)
		print(" ...");
	print("\n");
}

ulong
nhget3(uchar *p)
{
	return (((p[0]<<8)|p[1])<<8)|p[2];
}

void
hnput3(uchar *p, ulong v)
{
	p[0] = v>>16;
	p[1] = v>>8;
	p[2] = v;
}

/*
 * fragment block b as required to fit the network
 * requirements given by f. the packet goes to a
 * network address specified by a header
 * (f->hdrlen bytes) that contains a software header (Hdr)
 * at offset f->hsize.  b will have been freed on return.
 */
void
ttpfragment(Frags *f, Block *ob, void (*send)(Block*,void*), void *arg)
{
	int n, offset, len, seq, pad;
	Block *b;
	Hdr *oh, *h;
	uchar *orp;

	if(ob->next)
		ob = concatblock(ob);	/* TO DO */
	oh = (Hdr*)(ob->rp + f->hsize);
	len = blocklen(ob) - f->hdrlen;
	hnputs(oh->len, len-1);
	if(len <= f->fragsize){
		hnputs(oh->off, 0);
		oh->seq = 0;	/* raw */
		(*send)(ob, arg);
		return;
	}
	if(len > Maxmsglen){
		freeblist(ob);
		error(Etoobig);
	}
	lock(&f->seql);
	seq = ++f->seq;
	if(seq == Maxseq)
		f->seq = 1;
	unlock(&f->seql);

	offset = 0;
	orp = ob->rp;
	ob->rp += f->hdrlen;
	for(; len > 0; len -= n){
		n = len;
		if(n > f->fragsize)
			n = f->fragsize;
		pad = 0;
		if(n+f->hdrlen < f->mintu){
			pad = f->mintu - (n+f->hdrlen);
			b = allocb(f->mintu);
		}else
			b = allocb(n + f->hdrlen);
		/* each fragment has the original media header and a modified fragment header */
		memmove(b->wp, orp, f->hdrlen);
		b->wp += f->hdrlen;
		memmove(b->wp, ob->rp, n);
		b->wp += n + pad;
		h = (Hdr*)(b->rp + f->hsize);
		hnputs(h->off, offset | (seq>>8));
		h->seq = seq;
		ob->rp += n;
		(*send)(b, arg);
		offset += f->fragsize;
	}
	freeblist(ob);	/* TO DO: use ob itself as the last fragment */
}

/*
 * set up and send block b to be fragmented by hardware (eg, torus)
 */
void
ttphwfragment(Frags *f, Block *b, void (*send)(Block*,void*), void *arg)
{
	int len, seq;
	Hdr *h;

	if(b->next)
		b = concatblock(b);	/* TO DO */
	h = (Hdr*)(b->rp + f->hsize);
	len = blocklen(b) - f->hdrlen;
	if(len > Maxmsglen){
		freeblist(b);
		error(Etoobig);
	}
	hnputs(h->len, len-1);
	lock(&f->seql);
	seq = ++f->seq;
	if(seq == Maxseq)
		f->seq = 1;
	unlock(&f->seql);
	h->seq = seq;
	hnputs(h->off, seq>>8);
	/* src set by caller */
	(*send)(b, arg);
}

static Hmsg**
findmsg(Frags *f, ulong src, ulong seq, Lock **hl)
{
	int i;
	Hmsg **h, *e;

	/* could discard old messages here, without needing an ageing kproc */
	i = ((src>>16)^src^seq)&Hmask;
	h = &f->msgs[i];
	*hl = &f->msgl[i];
	ilock(*hl);
	for(; (e = *h) != nil; h = &e->next)
		if(e->src == src && e->seq == seq)
			break;
	return h;
}

/*
 * it's assumed that the caller has provided any locking needed,
 * and the incoming blocks include the headers.
 *
 * TO DO: possibly add software header similar to udphdr
 */
Block*
ttpreassemble(Frags *f, uchar *data, int blen)
{
	Hmsg *msg, **ml;
	Hdr *h;
	int ind, l, off, bit;
	u32int src, seq;
	uchar* map;
	Block *b;
	Lock *hl;

	blen -= f->hdrlen;
	if(blen < 0)
		return nil;
	h = (Hdr*)(data + f->hsize);
	src = nhget3(h->src);
	seq = ((nhgets(h->off) & 0xF)<<8) | h->seq;
	l = nhgets(h->len) + 1;
	if(l <= f->fragsize){
		if(blen < l)
			return nil;
		l += f->hdrlen;	/* include both headers */
		b = iallocb(l);
		if(b != nil){
			memmove(b->wp, data, l);
			b->wp += l;
		}
		return b;
	}
	ml = findmsg(f, src, seq, &hl);
	msg = *ml;
	if(msg == nil){
		msg = mallocz(sizeof(*msg)+f->mapsize, 1);
		msg->b = iallocb(l + f->hdrlen);
		msg->b->wp += f->hdrlen;
		msg->src = src;
		msg->seq = seq;
		msg->needed = HOWMANY(l, f->fragsize);
		msg->mlen = l;
		msg->next = *ml;
		msg->seen = (uchar*)(msg+1);
		*ml = msg;
	}
	off = nhgets(h->off) & ~0xF;
	IPRINT("frag seq=%d off=%d tlen=%d\n", seq, off, l);
	if(off >= l){
		iunlock(hl);
		iprint("frag: invalid offset: %d of %d\n", off, msg->mlen);
		return nil;
	}
	ind = off/f->fragsize;
	IPRINT("ind=%d off=%d size=%d\n", ind, off, msg->mlen);
	map = &msg->seen[ind>>3];
	bit = 1<<(ind & 7);
	if(*map & bit){
		/* duplicate, shouldn't happen */
		iunlock(hl);
		iprint("frag: msg dup: %d off=%d ind=%d\n", seq, off, ind);
		return nil;
	}
	*map |= bit;
	if(off+blen > l)
		blen = l - off;
	b = msg->b;
	memmove(b->wp+off, data+f->hdrlen, blen);
	if(off == 0)
		memmove(b->rp, data, f->hdrlen);
	msg->arrived++;
	IPRINT("needed=%d arrived=%d\n", msg->needed, msg->arrived);
	if(msg->arrived != msg->needed){
		iunlock(hl);
		return nil;
	}
	/* last one needed */
	*ml = msg->next;
	iunlock(hl);
	b->wp += l;
	IPRINT("frag: all in: seq=%d mlen=%d hdrlen=%d\n", msg->seq, msg->mlen, f->hdrlen);
	free(msg);
	if(0)
		dumpblock(b);
	return b;
}
