#ifdef cnk
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#define nil NULL
#define OREAD 0
#define ORDWR 2
typedef unsigned char u8int;
typedef unsigned char uchar;
typedef unsigned long long u64int;
typedef long long vlong;
typedef unsigned long long uvlong;
char *smprint(const char *fmt, ...);
void fprint(int fd, const char *fmt, ...);
void print(const char *fmt, ...);
void *mallocz(size_t size, int zero);
void panic(char *s);
void exits(char *s);
void torusctl(char *cmd, int debug);

int xyztorank(int x, int y, int z);

#define F(v, o, w)	(((v) & ((1<<(w))-1))<<(o))
enum {
	X		= 0,			/* dimension */
	Y		= 1,
	Z		= 2,
	N		= 3,

	Chunk		= 32,			/* granularity of FIFO */
	Pchunk		= 8,			/* Chunks in a packet */

	Quad		= 16,
	Tpkthdrlen	= 16,
};

/*
 * SKIP is a field in .sk giving the number of 2-bytes
 * to skip from the top of the packet before including
 * the packet bytes into the running checksum.
 * SIZE is a field in .size giving the size of the
 * packet in 32-byte 'chunks'.
 */
#define SKIP(n)		F(n, 1, 7)
#define SIZE(n)		F(n, 5, 3)

enum {
	Sk		= 0x01,			/* Skip Checksum */

	Pid0		= 0x01,			/* Destination Group FIFO MSb */
	Dp		= 0x02,			/* Multicast Deposit */
	Hzm		= 0x04,			/* Z- Hint */
	Hzp		= 0x08,			/* Z+ Hint */
	Hym		= 0x10,			/* Y- Hint */
	Hyp		= 0x20,			/* Y+ Hint */
	Hxm		= 0x40,			/* X- Hint */
	Hxp		= 0x80,			/* X+ Hint */

	Vcd0		= 0x00,			/* Dynamic 0 VC */
	Vcd1		= 0x01,			/* Dynamic 1 VC */
	Vcbn		= 0x02,			/* Deterministic Bubble VC */
	Vcbp		= 0x03,			/* Deterministic Priority VC */
	Dy		= 0x04,			/* Dynamic Routing */
	Dm		= 0x08,			/* DMA Mode */
	Pid1		= 0x10,			/* Destination Group FIFO LSb */
};

/*
 * Packet header. The hardware requires an 8-byte header
 * of which the last two are reserved (they contain a sequence
 * number and a header checksum inserted by the hardware).
 * The hardware also requires the packet to be aligned on a
 * 128-bit boundary for loading into the HUMMER.
 */
typedef struct Tpkt Tpkt;
typedef struct Hdr Hdr;

struct Hdr {
	uchar	len[2];	/* byte length - 1 */
	uchar	off[2];	/* offset adjusted by hardware (12 bits); top 4 bits of sequence */
	uchar	seq;		/* low order bits of sequence (zero if raw) */
	uchar	src[3];	/* source node address */
};

struct Tpkt {
	u8int	sk;				/* Skip Checksum Control */
	u8int	hint;				/* Hint|Dp|Pid0 */
	u8int	size;				/* Size|Pid1|Dm|Dy|VC */
	u8int	dst[N];				/* Destination Coordinates */
	u8int	_6_[2];				/* reserved */
	Hdr     Hdr;
	u8int	payload[];
};


struct AmRing {
	unsigned char *base;
	unsigned long size;
	unsigned long prod;
	unsigned long logN;
	unsigned char _116[128-16];
	unsigned long con;
	unsigned char _124[128-4];
};



void waitamrpacket(struct AmRing *amr, u8int type, volatile Tpkt *pkt, 
		   int *x, int *y, int *z, int secondstimeout);

static __inline__ u64int tbget(void)
{
     unsigned int tbl, tbu0, tbu1;

     do {
          __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
          __asm__ __volatile__ ("mftb %0" : "=r"(tbl));
          __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
     } while (tbu0 != tbu1);

     return (((unsigned long long)tbu0) << 32) | tbl;
}

#endif
#ifndef cnk
#include <u.h>
#include <libc.h>
#endif
#include "mpi.h"

