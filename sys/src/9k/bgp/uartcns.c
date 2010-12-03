/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Based on ppc440/uartppc405.c Copyright (C) 2006-2009 Alcatel-Lucent
 *
 * Description: CNS console interface
 *
 * All rights reserved
 *
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "bgcns.h"

#define MAXLOG 0x4000
struct cnslog {
	unsigned int write;
	char buf[MAXLOG];
};

int chatty = 0;
static Lock cnsconslock;

struct cnslog cnslog;
extern int mbconsputs(char*, int);
extern PhysUart cnsphysuart;

static Uart cnsuart[1] = {
{
	.regs	= nil,
	.name	= "eia0",
	.phys	= &cnsphysuart,
	.special= 0,
	.next	= nil, },
};

static int
cnsgetc(Uart *uart)
{
	char c = 0;
	USED(uart);

	CNS(readFromMailboxConsole, int, &c, 1);

	return (int) c;
}

static void
cnspoll(Uart *uart)
{
	char buf[255];
	int r, c;

if(0)
	if(uart->enabled) {
		while(CNS(testInboxAttention, int, 0)) {
			r = CNS(readFromMailboxConsole, int, buf, 255);
			if(r < 0)
				return;
			for(c = 0;c < r;c++)
				uartrecv(uart, buf[c]);
		}
	}
USED(uart);
}

static void
cnsputc(Uart*, int c)
{
	char cbuf = (char) c;

	if (cnslog.write >= MAXLOG){
		cnslog.write = 0;
	}

	cnslog.buf[cnslog.write] = cbuf;
	cnslog.write++;

	if((cnslog.write >= MAXLOG) || (cbuf=='\n')) {
		if(m->chatty) {
			mbconsputs(cnslog.buf, cnslog.write);
			cnslog.write = 0;
		}
	}
}

static long
cnsstatus(Uart* uart, void* buf, long n, long offset)
{
	char *p = malloc(READSTR);
	USED(uart);
	snprint(buf, READSTR, "CNS UART\n");
	n = readstr(offset, buf, n, p);
	free(p);
	return n;
}

static int
cnsstub(Uart* uart, int ignore)
{
	USED(uart);
	USED(ignore);

	return 0;
}

static void
cnsstub_noret(Uart* uart, int ignore)
{
	USED(uart);
	USED(ignore);
}

static void
cnskick(Uart* uart)
{
	if(uart->cts == 0 || uart->blocked)
		return;

	while(uart->op < uart->oe || uartstageoutput(uart) != 0){
		if(m->chatty)
			mbconsputs((char*)uart->op, (int)(uart->oe - uart->op));
		uart->op = uart->oe;
	}
}

static void
cnsdisable(Uart* uart)
{
	USED(uart);
}

static void
cnsenable(Uart* uart, int ie)
{
	USED(uart);
	USED(ie);
}

static Uart*
cnspnp(void)
{
	extern void mbinit(void);

	mbinit();

	return cnsuart;
}

/* stub functions */

PhysUart cnsphysuart = {
	.name		= "cns",
	.getc		= cnsgetc,
	.putc		= cnsputc,
	.status		= cnsstatus,
	.pnp		= cnspnp,
	.enable		= cnsenable,
	.disable	= cnsdisable,
/* stubs for the rest */
	.kick		= cnskick,
	.dobreak	= cnsstub_noret,
	.baud		= cnsstub,
	.bits		= cnsstub,
	.stop		= cnsstub,
	.parity		= cnsstub,
	.modemctl	= cnsstub_noret,
	.rts		= cnsstub_noret,
	.dtr		= cnsstub_noret,
	.fifo		= cnsstub_noret,
	.poll		= cnspoll,
};

void
cnsconsole(void)
{
	Uart *uart;

	uart = &cnsuart[0];
	consuart = uart;
	uart->console = 1;
}

