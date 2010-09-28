/*
	stats - statistics gathering API

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

	Description:
		This stats.c provides an API interface to system statistics
		and helper functions to provide average values and what not.

	Based almost entirely on /sys/src/cmd/stats.c

*/

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <9p.h>
#include <ctype.h>

#define	MAXNUM	10	/* maximum number of numbers on data line */

/* TODO: we probably need some sort of 

typedef struct Machine	Machine;
struct Machine
{
	char	*name;
	uvlong	lastupdate;	/* last time this was updated */

	int		nproc;
	int		nchild;
	int		njobs;

	int		statsfd;
	int		swapfd;

	uvlong	devswap[4];
	uvlong	devsysstat[10];

	/* big enough to hold /dev/sysstat even with many processors */
	char	buf[8*1024];
	char	*bufp;
	char	*ebufp;

	Machine	*next;	/* next Machine in list */
	Machine	*child;	/* linked list of children */
};

static Avg		*avgmach;	/* composite statistics */
static Machine	*mach;		/* my statistics */

static int
loadbuf(Machine *m, int *fd)
{
	int n;

	if(*fd < 0)
		return 0;
	seek(*fd, 0, 0);
	n = read(*fd, m->buf, sizeof m->buf-1);
	if(n <= 0){
		close(*fd);
		*fd = -1;
		return 0;
	}
	m->bufp = m->buf;
	m->ebufp = m->buf+n;
	m->buf[n] = 0;
	return 1;
}

/* read one line of text from buffer and process integers */
int
readnums(Machine *m, int n, uvlong *a, int spanlines)
{
	int i;
	char *p, *ep;

	if(spanlines)
		ep = m->ebufp;
	else
		for(ep=m->bufp; ep<m->ebufp; ep++)
			if(*ep == '\n')
				break;
	p = m->bufp;
	for(i=0; i<n && p<ep; i++){
		while(p<ep && (!isascii(*p) || !isdigit(*p)) && *p!='-')
			p++;
		if(p == ep)
			break;
		a[i] = strtoull(p, &p, 10);
	}
	if(ep < m->ebufp)
		ep++;
	m->bufp = ep;
	return i == n;
}

int
readswap(Machine *m, uvlong *a)
{
	if(strstr(m->buf, "memory\n")){
		/* new /dev/swap - skip first 3 numbers */
		if(!readnums(m, 7, a, 1))
			return 0;
		a[0] = a[3];
		a[1] = a[4];
		a[2] = a[5];
		a[3] = a[6];
		return 1;
	}
	return readnums(m, nelem(m->devswap), a, 0);
}

int
initmach(Machine *m)
{
	int n, fd;
	uvlong a[MAXNUM];
	char *p, mpt[256], buf[256];

	m->name = estrdup9p(getenv("sysname"));

	m->remote = (strcmp(p, mysysname) != 0);
	if(m->remote == 0)
		strcpy(mpt, "");

	snprint(buf, sizeof buf, "%s/dev/swap", mpt);
	m->swapfd = open(buf, OREAD);
	if(loadbuf(m, &m->swapfd) && readswap(m, a))
		memmove(m->devswap, a, sizeof m->devswap);
	else{
		m->devswap[Maxswap] = 100;
		m->devswap[Maxmem] = 100;
	}

	snprint(buf, sizeof buf, "%s/dev/sysstat", mpt);
	m->statsfd = open(buf, OREAD);
	if(loadbuf(m, &m->statsfd)){
		for(n=0; readnums(m, nelem(m->devsysstat), a, 0); n++)
			;
		m->nproc = n;
	}else
		m->nproc = 1;

	return 1;
}

void
statinit(void)
{
	nmach = 1;
	mysysname = ;
	if(mysysname == nil){
		fprint(2, "stats: can't find $sysname: %r\n");
		exits("sysname");
	}
	mysysname = estrdup(mysysname);

	mach = emalloc(nmach*sizeof(Machine));
	initmach(&mach[0], mysysname);
	readmach(&mach[0], 1);

	for(;;){
		for(i=0; i<nmach; i++)
			readmach(&mach[i], 0);

		sleep(sleeptime);
	}
}
