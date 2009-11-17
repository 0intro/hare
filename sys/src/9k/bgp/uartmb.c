/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: primitive JTAG console implementation
 *
 * All rights reserved
 *
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

typedef struct Mb Mb;
typedef struct Md Md;
typedef struct Mm Mm;

/*
 * Mailbox Descriptor.
 * Offset is from 0x7ffff8000ull. Length includes the message
 * header, and may be 0 if the mailbox isn't present.
 * The table of mailbox descriptors must be at 0x7ffffffd0ull.
 * There are 2 descriptors per mailbox, one for input and
 * one for output.
 */
struct Md {
	u16int	offset;
	u16int	length;
};

/*
 * Mailbox Message.
 * A descriptor points to one of these.
 * The MSb of cmd is an acknowledge
 * Result is set by the reader and may be ignored.
 * A crc of 0 means none was calculated.
 */
struct Mm {
	u16int	cmd;
	u16int	len;
	u16int	result;
	u16int	crc;
	u8int	data[];
};

/*
 * Soft mailbox.
 */
struct Mb {
	Mm*	mm;
	usize	len;
};

enum {
	NMb		= 4,

	Mdtable		= VIRTSRAM+0x7fd0,
};

#define BGP_DCR_TEST(x)			(0x400+(x))
#define BGP_DCR_GLOB_ATT_WRITE_SET	BGP_DCR_TEST(0x17)
#define BGP_DCR_GLOB_ATT_WRITE_CLEAR	BGP_DCR_TEST(0x18)
#define BGP_ALERT_OUT(core)	        (0x80000000u>>(24+(core)))
#define BGP_ALERT_IN(core)	        (0x80000000u>>(28+(core)))
#define BGP_JMB_CMD2HOST_PRINT          (0x0002)

static Mb mbi[NMb];
static Mb mbo[NMb];

void
mbinit(void)
{
	int i;
	Mb *mb;
	Md *md;

	md = (Md*)Mdtable;
	for(i = 0; i < NMb; i++){
		mb = &mbi[i];
		if(md->length != 0){
			mb->mm = (Mm*)(VIRTSRAM+md->offset);
			mb->len = md->length - offsetof(Mm, data[0]);
		}
		md++;

		mb = &mbo[i];
		if(md->length != 0){
			mb->mm = (Mm*)(VIRTSRAM+md->offset);
			mb->len = md->length - offsetof(Mm, data[0]);
		}
		md++;
	}
}

static int
mbputs(int core, char* s, int n)
{
	Mb *mb;
	Mm *mm;
	int len, r;

	if(core >= sys->ncores)
		return -1;
	mb = &mbo[core];
	if((mm = mb->mm) == nil)
		return -1;

	r = n;
	while(n > 0){
		if(n > mb->len)
			len = mb->len;
		else
			len = n;
		memmove(mm->data, s, len);
		mm->len = len;
		mm->cmd = BGP_JMB_CMD2HOST_PRINT;
		imb();

		dcrput(BGP_DCR_GLOB_ATT_WRITE_SET, BGP_ALERT_OUT(core));

		do{ 
			dcbi(&mm->cmd);
		}while(!(mm->cmd & 0x8000));

		dcrput(BGP_DCR_GLOB_ATT_WRITE_CLEAR, BGP_ALERT_OUT(core));

		n -= len;
		s += len;
	}

	return r;
}

int
mbconsputs(char* s, int n)
{
	return mbputs(m->machno, s, n);
}
