/*
	gangfs - aggregated execution management

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

	Description:
		This provides a file system which interacts with execfs(4)
		to provide a gang execution environment.  This initial version
		will be single-node, but the intention is to support multinode
		environments

	Based in part on Pravin Shinde's devtask.c

	TODO:
		* gang->gang job deployment
		* per stdio aggregation mode
		* non-aggregation mode for stdio(s)
		* push helper: (launch local stage1 and stage3 with fanout stage2)
		* session level status file
		* MAYBE: aggregated status via multipipes
		* nextaggregated wait via multipipe barrier (todo in multipipe)
*/

#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>
#include <stdio.h>
#include "debug.h"

static char defaultpath[] =	"/proc";
static char *procpath;
static char *gangpath;	/* our view of the gang path */
static char *xgangpath; /* external view of the gang path */
static char *mysysname;
static char *srvctl;
static Channel *iochan;
static Channel *clunkchan;
static int updateinterval; /* in seconds */
static char *amprefix = "/n"; /* mount prefix */

char Enotdir[] = "Not a directory";
char Enotfound[] = "File not found";
char Eperm[] = "Permission denied";
char Enogang[] = "ctl file has no gang";
char Ebusy[] = "gang already has subsessions reserved";
char Ebadimode[] = "unknown input mode";
char Ebadpid[] = "stupid pid problem";
char Ebadctl[] = "problemn reading session ctl";
char Estdin[] = "problem binding gang stdin";
char Estdout[] = "problem binding gang stdout";
char Estderr[] = "problem binding gang stderr";
char Enores[] = "insufficient resources";
char Echeckmount[] = "could not check mount";
char Eclosing[] = "gang is closing or broken";

enum {	/* DEBUG LEVELS */
	DERR = 0,	/* error */
	DCUR = 1,	/* current - temporary trace */
	DFID = 2,	/* fid tracking */
	DGAN = 3,	/* gang tracking */
	DEXE = 4,	/* execfs interactions */
	DBCA = 5,	/* broadcast operations */
	DCLN = 7,	/* cleanup tracking */
	DPIP = 8,	/* interactions with multipipes */
	DREF = 9,	/* ref count tracking */
	DARG = 10,	/* arguments */
};

enum
{
	STACK = (8 * 1024),
	NAMELEN = 255,
	NUMSIZE = 12,
};

/* 
  qid.path format:
	[MAGIC(16)][CONV(16)][TYPE(16)] where low 00ff bytes are type
*/
#define MAGIC		 ((uvlong)0x6AF5<<32)
#define TYPE(x) 	 ((ulong) (x).path & 0xff)
#define CONVP(x) 	 ((((ulong) (x).path >> 16)&0xffff) -1)
#define CONV(x) 	 ((((ulong) (x)->path >> 16)&0xffff) -1)
#define CONVQIDP(c, y) ((uvlong)(MAGIC|((((c+1) << 16)|(y)))))

typedef struct Session Session;
typedef struct Gang Gang;
struct Session
{
	Gang *g;		/* gang? necessary? */
	char *path;		/* path to exec gang */
	int fd;			/* fd to ctl file */
	Req *r;			/* outstanding ctl requests */
	Channel *chan;	/* status channel */
	int pid;		/* Session's actual pid */
	int index;		/* session index in gang */
	int remote;		/* flag marking whether session is local or remote */
};

enum {
	GANG_NEW,		/* allocated */
	GANG_RESV,		/* reserved */
	GANG_EXEC,		/* executing */
	GANG_CLOSING,	/* closing */
	GANG_CLOSED,	/* closed */
	GANG_BROKE,		/* broken */
};

/* NOTE:
   we use the structural reference count for clean up
   of the struct and the ctl ref count for the clean
   up of the directory
*/
struct Gang
{
	char *path;		/* gang's path */
	int refcount;	/* structural reference count */
	int ctlref;		/* control file reference count */
	int status;		/* current status of gang */

	int index;		/* gang id */
	int size;		/* number of subsessions */
	Session *sess;	/* array of subsessions */
	int imode;		/* input mode (enum/bcast/raw) */

	Channel *chan;	/* channel for sync/error reporting */

	Gang *next;		/* primary linked list */
};

RWLock glock; 
Gang *glist;

typedef struct Dirtab Dirtab;
struct Dirtab
{
	char name[NAMELEN];
	Qid qid;
	long perm;
};

enum
{
	Qroot = 0,
	Qclone,
	Qgstat,
	Qrootend,
	Qconv = 0,
	Qctl,
	Qenv,
	Qns,
	Qargs,
	Qstatus,
	Qwait,
	Qconvend,
	Qsubses = Qconvend,
};

/* these dirtab entries form the base */
static Dirtab rootdir[] = {
	"gclone", 	{Qclone},				0666,
	"status",	{Qgstat},				0666,
};

static Dirtab convdir[] = {
	"ctl",		{Qctl},					0666,
	"env",		{Qenv},					0444,
	"ns",		{Qns},					0444,
	"args",		{Qargs},				0444,
	"status",	{Qstatus},				0444,
	"wait",		{Qwait},				0444,
};

enum
{
	CMenum,			/* set input to enum (must be before res) */
	CMbcast,		/* set input to broadcast mode (must be before res) */
	CMres,			/* reserve a set of nodes */
	CMmount,		/* backmount peer */
};

Cmdtab ctlcmdtab[]={
	CMenum, "enum", 1,
	CMbcast, "bcast", 1,
	CMres, "res", 0,
	CMmount, "checkmount", 2,
};

#define	MAXNUM	10	/* maximum number of numbers on data line */

enum
{
	/* old /dev/swap */
	Mem		= 0,
	Maxmem,
	Swap,
	Maxswap,

	/* /dev/sysstats */
	Procno	= 0,
	Context,
	Interrupt,
	Syscall,
	Fault,
	TLBfault,
	TLBpurge,
	Load,
	Idle,
	InIntr,
};

/* TODO: we'll need a lock to protect the linked list */
typedef struct Status Status;
struct Status
{
	char	*name;
	long	lastup;	/* last time this was updated */

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

	Status	*next;	/* linked list of children */
};

static Status avgstats;	/* composite statistics */
static Status mystats;	/* my statistics */
static char *canonpath; /* canonical path */

static int
loadbuf(Status *m, int *fd)
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
static int
readnums(Status *m, int n, uvlong *a, int spanlines)
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
readnum(ulong off, char *buf, ulong n, ulong val, int size)
{
	char tmp[64];

	snprint(tmp, sizeof(tmp), "%*lud", size-1, val);
	tmp[size-1] = ' ';
	if(off >= size)
		return 0;
	if(off+n > size)
		n = size-off;
	memmove(buf, tmp+off, n);
	return n;
}

static int
readswap(Status *m, uvlong *a)
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

static int
refreshstatus(Status *m)
{
	int n;
	int i;
	uvlong a[nelem(m->devsysstat)];

	if(m->next) { /* aggregation node */
		Status *c;
		/* go through all the children and add the important bits up */
		m->nproc = 0;
		m->nchild = 0;
		m->njobs = 0; /* MAYBE: do we really want to do this? */
		m->devswap[Mem] = 0;
		m->devswap[Maxmem] = 0;
		m->devsysstat[Load] = 0;
		m->devsysstat[Idle] = 0;
		c=m->next;
		while(c != nil) {
			m->nproc += c->nproc;
			/* if child is an aggregation node only count its children */
			if(c->nchild) { 
				m->nchild += c->nchild;
			} else {			
				m->nchild++;
			}

			m->njobs += c->njobs;
			m->devswap[Mem] += c->devswap[Mem];
			m->devswap[Maxmem] += c->devswap[Maxmem];
			m->devsysstat[Load] += c->devsysstat[Load];
			m->devsysstat[Idle] =+ c->devsysstat[Idle];
			c = c->next;
		}

		/* MAYBE: average load and idle when done? */
	} else { /* child node */
		if(loadbuf(m, &m->swapfd) && readswap(m, a))
			memmove(m->devswap, a, sizeof m->devswap);

		if(loadbuf(m, &m->statsfd)){
			memset(m->devsysstat, 0, sizeof m->devsysstat);
			for(n=0; n<m->nproc && readnums(m, nelem(m->devsysstat), a, 0); n++)
				for(i=0; i<nelem(m->devsysstat); i++)
					m->devsysstat[i] += a[i];
		}

		m->lastup = time(0);
	}
	return 1;
}

static void
fillstatbuf(Status *m, char *statbuf)
{
	char *bp;
	ulong t;

	bp = statbuf;
	t = time(0);
	if(t-m->lastup > updateinterval)
		refreshstatus(m);

	bp+=snprint(bp, 16, "%15s ", mystats.name);
			
	readnum(0, bp, NUMSIZE, m->nproc, NUMSIZE);
	bp+=NUMSIZE;
	readnum(0, bp, NUMSIZE, m->nchild, NUMSIZE);
	bp+=NUMSIZE;
	readnum(0, bp, NUMSIZE, m->njobs, NUMSIZE);
	bp+=NUMSIZE;	
	readnum(0, bp, NUMSIZE, m->devsysstat[Load], NUMSIZE);
	bp+=NUMSIZE;
	readnum(0, bp, NUMSIZE, m->devsysstat[Idle], NUMSIZE);
	bp+=NUMSIZE;
	readnum(0, bp, NUMSIZE, m->devswap[Mem], NUMSIZE);
	bp+=NUMSIZE;
	readnum(0, bp, NUMSIZE, m->devswap[Maxmem], NUMSIZE);
	bp+=NUMSIZE;
	*bp = 0;	
}

static Status *
findchild(Status *m, char *name)
{	
	Status *current = m;

	while(current != nil) {
		if(strcmp(name, current->name) == 0) 
			break;
		current = current->next;
	}

	return current;
}

/* find lowest loaded child for next workload */
/* NOTE: right now we just base this on number of jobs */
static Status *
idlechild(Status *m)
{
	Status *current = m->next;
	Status *lowest = current;

	while(current != nil) {
		if(current->njobs < lowest->njobs)
			lowest = current;
		current = current->next;
	}

	return lowest;
}

static void
statuswrite(char *buf)
{
	char name[16];
	Status *m;

	/* grab name and match it to our children */
	sscanf(buf, "%15s", name);
	m = findchild(&mystats, name);
	if(m == nil) {
		m = emalloc9p(sizeof(Status));
		memset(m, 0, sizeof(Status));
		/* TODO: lock */
		m->next = mystats.next;
		mystats.next = m;
		mystats.nchild++;
		m->name = estrdup9p(name);
		/* TODO: unlock */
	}
	assert(m != nil);
	sscanf(buf, "%15s %12d %12d %12d %12llud %12llud %12llud %12llud", 
			m->name, &m->nproc, &m->nchild, &m->njobs, &m->devsysstat[Load], 
			&m->devsysstat[Idle], &m->devswap[Mem], &m->devswap[Maxmem]);
	m->lastup = time(0); /* MAYBE: pull timestamp from child and include it in data */
	xgangpath = smprint("/n/%s%s", mysysname, procpath);	
}

static void
updatestatus(void *arg)
{
	int n;
	char *parent = (char *) arg;
	char *statbuf=emalloc9p((NUMSIZE*11+1) + 1);
	char *parentpath = smprint("%s/%s/proc/status", amprefix, parent);
	int parentfd = open(parentpath, OWRITE);
			
	if(parentfd < 0) {
		DPRINT(DERR, "*ERROR*: couldn't open parent %s on path %s\n", parent, parentpath);
		return;
	}
	
	/* TODO: set my priority low */ 

	for(;;) {
		fillstatbuf(&mystats, statbuf);
		n = fprint(parentfd, "%s\n", statbuf);
		if(n < 0)
			break;
		sleep(updateinterval*1000);
	}

	free(parentpath);
	free(statbuf);
}

static void
initstatus(Status *m)
{
	int n;
	uvlong a[nelem(m->devsysstat)];

	memset(m, 0, sizeof(Status));
	m->name = mysysname;
	m->swapfd = open("/dev/swap", OREAD);
	assert(m->swapfd > 0);
	m->statsfd = open("/dev/sysstat", OREAD);
	assert(m->swapfd > 0);
	if(loadbuf(m, &m->statsfd)){
		for(n=0; readnums(m, nelem(m->devsysstat), a, 0); n++)
			;
		m->nproc = n;
	}else
		m->nproc = 1;
	m->lastup = time(0);
}

static void
closestatus(Status *m)
{
	close(m->statsfd);
	close(m->swapfd);
}

/* remote support */
static void
startimport(void *arg)
{
	char *addr = (char *) arg;

	threadsetname("gangfs-startimport");
	
	procexecl(nil, "/bin/import", "import", arg, "/", smprint("/n/%s", addr), nil);

	DPRINT(DERR, "*ERROR*: startimport: procexecl returned %r\n");
	threadexits(nil);
}

/* checkmount: check to see if a remote system is already, if not mount it */
static int
checkmount(char *addr)
{
	char *dest = estrdup9p(addr);
	char *mtpt = smprint("/n/%s/proc", addr);
	Dir *tmp;
	int retries = 0;
	int err = 0;

	if((tmp = dirstat(mtpt)) != nil) /* dir exists, sweet */
		goto out;
	else {
		char *srvpt = smprint("/srv/%s", addr);
		char *srvtg = smprint("/n/%s", addr);
		int fd;
		DPRINT(DCUR, "attempting to mount %s on %s\n", srvpt, srvtg);
		if(fd = open(srvpt, ORDWR)) { /* srv exists, sweet */
			free(srvpt);
			if(mount(fd, -1, srvtg, MREPL, "") < 0) {
				free(srvtg);
				close(fd);
				DPRINT(DERR, "srv mount failed: %r\n");
			} else {
				free(srvtg);
				close(fd);
				DPRINT(DERR, "srv mount succeeded\n");
				goto out;
			}
		}
	}

	/* no dir, spawn off an import */
	proccreate(startimport, dest, STACK);

	/* we need to wait for this to be done */
	while( (tmp = dirstat(mtpt)) == nil) {
		sleep(100);
		if(retries++ > 100) {
			DPRINT(DERR, "*ERROR*: checkmount: import not complete after 10 seconds, giving up\n");

			err = -1;
			break;
		}
	}

out:
	free(tmp);
	free(dest);
	free(mtpt);
	return err;
}	

static void
usage(void)
{
	fprint(2, "gangfs [-D] [mtpt]\n");
	exits("usage");
}

static void
gangrefinc(Gang *g, char *where)
{
	g->refcount++;
	DPRINT(DREF, "\t\tGang(%p).refcount++ = %d [%s]\n", g, g->refcount, where);
}

static void
gangrefdec(Gang *g, char *where)
{
	g->refcount--;
	DPRINT(DREF, "\t\tGang(%p).refcount-- = %d [%s]\n", g, g->refcount, where);
}

static int
mpipe(char *path, char *name)
{
	int fd, ret;
	fd = open("/srv/mpipe", ORDWR);
	if(fd<0) {
		DPRINT(DERR, "couldn't open /srv/mpipe: %r\n");
		return -1;
	}
	
	ret = mount(fd, -1, path, MBEFORE, name);
	if(ret < 0) {
		DPRINT(DERR, "mount of multipipe failed: %r\n");
	}
	close(fd);
	return ret;
}

static int
flushmp(char *src)
{
	int fd = open(src, OWRITE);
	int n; 
	char hdr[255];
	ulong tag = ~0; /* header byte is at offset ~0 */
	char pkttype='.';

	if(fd < 0) {
		DPRINT(DPIP, "flushmp: open failed: %r\n");
		return fd;
	}

	n = snprint(hdr, 255, "%c\n%lud\n%lud\n%s\n", pkttype, (ulong)0, (ulong)0, "");
	n = pwrite(fd, hdr, n, tag);
	close(fd);
	return n;
}

static int
spliceto(char *src, char *dest)
{
	int fd = open(src, OWRITE);
	int n; 
	char hdr[255];
	ulong tag = ~0; /* header byte is at offset ~0 */
	char pkttype='>';

	if(fd < 0) {
		DPRINT(DPIP, "spliceto: open %s failed: %r\n", src);
		return fd;
	}

	n = snprint(hdr, 255, "%c\n%lud\n%lud\n%s\n", pkttype, (ulong)0, (ulong)0, dest);
	n = pwrite(fd, hdr, n, tag);
	close(fd);
	return n;
}

static int
splicefrom(char *dest, char *src)
{
	int fd = open(dest, OWRITE);
	int n; 
	char hdr[255];
	ulong tag = ~0; /* header byte is at offset ~0 */
	char pkttype='<';

	if(fd < 0) {
		DPRINT(DERR, "*ERROR*: splicefrom: open %s failed: %r\n", dest);
		return fd;
	}

	n = snprint(hdr, 255, "%c\n%lud\n%lud\n%s\n", pkttype, (ulong)0, (ulong)0, src);
	n = pwrite(fd, hdr, n, tag);
	close(fd);
	return n;
}

static Gang *
newgang(void) 
{
	Gang *mygang = emalloc9p(sizeof(Gang));

	if(mygang == nil)
		return nil;
	memset(mygang, 0, sizeof(Gang));
	mygang->chan = chancreate(sizeof(void *), 0);

	wlock(&glock);
	if(glist == nil) {
		glist = mygang;
		mygang->index = 0;
	} else {
		int last = -1;
		Gang *current;
		for(current=glist; current->next != nil; current = current->next) {
			if(current->index != last+1) {
				break;
			}
			last++;
		}
		mygang->next = current->next;
		current->next = mygang;
		mygang->index = current->index+1;
	}

	mygang->ctlref = 1;
	mygang->refcount = 1;
	mygang->imode = CMbcast;	/* broadcast mode default */
	mygang->status = GANG_NEW;
	wunlock(&glock);

	return mygang;
}

static Gang *
findgang(int index)
{
	Gang *current = nil;

	rlock(&glock);
	if(glist == nil) {
		goto out;
	} else {
		for(current=glist; current != nil; current = current->next) {
			if((current->index == index)&&(current->status != GANG_CLOSED)) {
				gangrefinc(current, "findgang");
				goto out;
			}
		}
	}
out:
	runlock(&glock);
	return current;
}

static Gang *
findgangnum(int which)
{
	Gang *current = nil;
	rlock(&glock);
	if(glist == nil) {
		goto out;
	} else {
		int count = 0;
		for(current=glist; current != nil; current = current->next) {
			if(current->status == GANG_CLOSED)
				continue;
			if(count == which) {
				gangrefinc(current, "findgangnum");
				goto out;
			}
			count++;
		}
	}
out:
	runlock(&glock);
	return current;
}

static void
releasesessions(Gang *g)
{
	int count;
	
	for(count = 0; count < g->size; count++) {
		free(g->sess[count].path);
		chanclose(g->sess[count].chan);
		chanfree(g->sess[count].chan);
		close(g->sess[count].fd);
	}

	free(g->sess);
}

static void
releasegang(Gang *g)
{
	wlock(&glock);
	gangrefdec(g, "releasegang");
	if(g->refcount == 0) {	/* clean up */
		Gang *c;
		Gang *p = glist;

		if(glist == g) {
			glist = g->next;
		} else {
			for(c = glist; c != nil; c = c->next) {
				if(c == g) {
					p->next = g->next;
					break;
				}
				p = c;
			}
		}
		
		releasesessions(g);
		chanfree(g->chan);
		free(g);
	}

	wunlock(&glock);
}

static void
cleanup(Srv *)
{
	nbsendp(iochan, 0); /* flush any blocked readers */
	chanclose(iochan);
	closestatus(&mystats);
	sleep(5);
	threadexitsall("done");
}

static void
fsattach(Req *r)
{
	if(r->ifcall.aname && r->ifcall.aname[0]){
		respond(r, "invalid attach specifier");
		return;
	}
	r->fid->qid.path = Qroot;
	r->fid->qid.type = QTDIR;
	r->fid->qid.vers = 0;
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

static int
dirgen(int n, Dir *d, void *aux)
{
	Fid *f = aux;
	Gang *g = f->aux;
	Qid *q = &f->qid;
	Dirtab *dt;
	int ne = 0;
	int s = 0;

	USED(s);
	USED(ne);

	d->atime = time(nil);
	d->mtime = d->atime;
	d->uid = estrdup9p("gangfs");
	d->gid = estrdup9p("gangfs");
	d->muid = estrdup9p("gangfs");
	d->length = 0;

	if(n==-1) {
		if(CONV(q) == -1) { /* root directory */
			d->qid.path=CONVQIDP(-1, Qroot);
			d->qid.type=QTDIR;
			d->mode = 0555|DMDIR;
			d->name = smprint("#gtaskfs");
			return 0;			
		} else {			/* conv directory */
			d->qid.path=CONVQIDP(CONV(q), Qconv);
			d->qid.type=QTDIR;
			d->mode = 0555|DMDIR;
			d->name = smprint("g%lud", CONV(q)); 
			return 0;			
		}
	}

	if(CONV(q) == -1) { /* root directory */
		dt = rootdir;
		ne = Qrootend-1;
		s = n-ne;
		if(s >= 0) {
			g = findgangnum(s);
			if(g != nil) {
				d->qid.path=CONVQIDP(g->index, Qconv);
				d->qid.type=QTDIR;
				d->mode = 0555|DMDIR; /* FUTURE: set perms */
				d->name = smprint("g%d", g->index);
				releasegang(g);
				return 0;
			}
		}
	} else {				/* session directory */
		dt = convdir;
		ne = Qconvend-1;
		s = n-ne;

		if(g && (s >= 0) && (s < g->size)) {
			d->qid.path=CONVQIDP(CONV(q), Qsubses+s);
			d->qid.vers=0;
			d->qid.type=QTDIR;
			d->mode = DMDIR;	/* mount points only! */
			d->name = smprint("%d", s);
			return 0;
		}
	}

	if(n < ne) {	/* core synthetics */
		d->qid.path = CONVQIDP(CONV(q), dt[n].qid.path);
		d->mode = dt[n].perm;
		d->name = estrdup9p(dt[n].name);
		return 0;
	}

	return -1;
}

static int
isanumber(char *thingy)
{
	if(thingy == nil)
		return 0;
	if((thingy[0] >=  '0')&&(thingy[0] <= '9'))
		return 1;
	return 0;
}

static void
qidcopy(Qid *src, Qid *dst)
{
	dst->path = src->path;
	dst->vers = src->vers;
	dst->type = src->type;
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	Dirtab *dt;
	Gang *g = fid->aux;
	int j;

	if(!(fid->qid.type&QTDIR))
		return Enotdir;

	if(strcmp(name, "..") == 0){
		if((CONV(&fid->qid) == -1) || (TYPE(fid->qid) == Qconv)) {
			qid->path = Qroot;
			qid->type = QTDIR;
			qidcopy(qid, &fid->qid);
			return nil;
		} else {
			qid->path = CONVQIDP(CONV(&fid->qid), Qconv);
			qid->type = QTDIR;
			qidcopy(qid, &fid->qid);
			return nil;
		}	
	}
	/* depending on path do something different */
	if(CONV(&fid->qid) == -1) {
		dt = rootdir;
		for(j = 0; j < Qrootend; j++)
			if(strcmp(dt[j].name, name) == 0) {
				qidcopy(&dt[j].qid, qid);
				qidcopy(qid, &fid->qid);
				return nil;
			}
		if( (name[0] == 'g') && (isanumber(name+1))) { /* session  */
			int s = atoi(name+1);
			Gang *g = findgang(s);
			if(g != nil) {
				qid->path = CONVQIDP(g->index, Qconv);
				qid->vers = 0;
				qid->type = QTDIR;
				qidcopy(qid, &fid->qid);
				fid->aux = g;
				return nil;
			}
		}
	} else {
		dt = convdir;
		for(j = Qconv; j < Qconvend; j++)
			if(strcmp(dt[j].name, name) == 0) {
				qidcopy(&dt[j].qid, qid);
				qid->path = CONVQIDP(CONV(&fid->qid), qid->path);
				qidcopy(qid, &fid->qid);
				return nil;
			}
		if(isanumber(name)) {
			int s = atoi(name);
			if(g && (s < g->size)) {
				qid->path = CONVQIDP(CONV(&fid->qid), s+Qsubses);
				qid->vers = 0;
				qid->type = QTDIR;
				qidcopy(qid, &fid->qid);
				return nil;
			}		
		}	
	}
		
	return Enotfound;
}

static char *
fsclone(Fid *oldfid, Fid *newfid)
{
	Gang *g;

	if((oldfid == nil)||(newfid==nil)) {
		fprint(2, "fsclone: bad fids\n");
		return nil;
	}

	if(oldfid->aux == nil)
		return nil;
	if(newfid->aux != nil) {
		fprint(2, "newfid->aux is bit nil when oldfid->aux was not\n");
		return nil;
	}
	/* this will only work if only sessions use aux */
	g = oldfid->aux;
	if(g) {
		newfid->aux = oldfid->aux;
		gangrefinc(g, "fsclone");
	}

	return nil;
}

static void
fsopen(Req *r)
{
	ulong path;
	Fid *fid = r->fid;
	Qid *q = &fid->qid;
	Gang *mygang;

	if(TYPE(fid->qid) >= Qsubses) {
		respond(r, Eperm);
		return;
	}
	if (q->type&QTDIR){
		respond(r, nil);
		return;
	}
	
	if(TYPE(fid->qid) == Qctl) {
		mygang = fid->aux;
		
		if(mygang) {
			wlock(&glock);
			if(mygang->status >= GANG_CLOSING) {
				wunlock(&glock);
				respond(r, "gang already in the process of closing");
				return;
			} else {
				mygang->ctlref++; /* TODO: do we need to lock? */
			}
			wunlock(&glock);
		} else
			DPRINT(DERR, "*ERROR*: fsopen: ctl fid %d without aux\n", fid->fid);
	}

	if(TYPE(fid->qid) != Qclone) {
		/* FUTURE: probably need better ref count here */
		respond(r, nil);
		return;
	}

	DPRINT(DGAN, "\t newgang %p\n", r);
	/* insert new session at the end of the list & assign index*/
	mygang = newgang();
	if(mygang == nil) {
		respond(r, "out of resources");
		return;
	}
	fid->aux = mygang;
	
	path = CONVQIDP(mygang->index, Qctl);
	r->fid->qid.path = path;
	r->ofcall.qid.path = path;

	respond(r, nil);
}

static void
fsread(Req *r)
{
	Fid *fid = r->fid;
	Qid *q = &fid->qid;
	char buf[NAMELEN];
	char *statbuf;

	if (q->type&QTDIR) {
		dirread9p(r, dirgen, fid);
		respond(r, nil);
		return;
	}

	switch(TYPE(fid->qid)) {
		case Qctl:
			sprint(buf, "%lud\n", CONVP(fid->qid));
			readstr(r, buf);
			respond(r, nil);
			return;
		case Qgstat:
			statbuf=emalloc9p((NUMSIZE*11+1) + 1);
			fillstatbuf(&mystats, statbuf);
			readstr(r, statbuf);
			respond(r, nil);
			free(statbuf);
			return;
		default:
			respond(r, "not yet");
	}
}

/* mypath is the canonical path to my gproc */
/* path is the path to the target proc */
static char *
setupstdio(Session *s)
{
	Gang *g = s->g;
	char buf[255];
	char dest[255];
	int n;

	/* BUG: won't work for gang because gang pid is not an int */
	if(s->remote)
		snprint(buf, 255, "%s/g%d", s->path, s->pid);
	else
		snprint(buf, 255, "%s/%d", s->path, s->pid);
	snprint(dest, 255, "%s/g%d/%d", gangpath, g->index, s->index);
	n = bind(buf, dest, MREPL);
	if(n < 0)
		return smprint("couldn't bind %s %s: %r\n", buf, dest);

	if(s->remote)
		snprint(buf, 255, "%s/g%d/stdin", s->path, s->pid);
	else
		snprint(buf, 255, "%s/%d/stdin", s->path, s->pid);
	snprint(dest, 255, "%s/g%d/stdin", xgangpath, g->index);
	n = splicefrom(buf, dest); /* execfs initiates splicefrom gangfs */
	if(n < 0)
		return smprint("splicefrom: %r\n");

	if(s->remote)
		snprint(buf, 255, "%s/g%d/stdout", s->path, s->pid);
	else
		snprint(buf, 255, "%s/%d/stdout", s->path, s->pid);
	snprint(dest, 255, "%s/g%d/stdout", xgangpath, g->index);
	n = spliceto(buf, dest); /* execfs initiates spliceto gangfs stdout */
	if(n < 0)
		return smprint("spliceto: %r\n");

	if(s->remote)
		snprint(buf, 255, "%s/g%d/stderr", s->path, s->pid);
	else
		snprint(buf, 255, "%s/%d/stderr", s->path, s->pid);
	snprint(dest, 255, "%s/g%d/stderr", xgangpath, g->index);
	n = spliceto(buf, dest); /* execfs initiates spliceto gangfs stderr */
	if(n < 0)
		return smprint("spliceto: %r\n");

	return nil;
}

/* reserve a session using clone semantics */
/* NOTE: Should work with execfs or gangfs */
static void
cloneproc(void *arg)
{
	Session *sess = (Session *) arg;
	char buf[255];
	int retries = 0;
	int n;
	char *err;

	DPRINT(DEXE, "clone proc\n");
	
	if(sess->remote)
		snprint(buf, 255, "%s/gclone", sess->path);
	else
		snprint(buf, 255, "%s/clone", sess->path);
	while(1) {
		DPRINT(DEXE, "Attempting to open child exec: %s\n", buf);
		sess->fd = open(buf, ORDWR);
		if(sess->fd > 0)
			break;
		DPRINT(DERR, "*ERROR*: retry: %d opening execfs returned %d: %r -- retrying\n", 
				retries, sess->fd);
		if(retries++ > 10) {
			DPRINT(DERR, "*ERROR* giving up\n");
			sendp(sess->chan, smprint("execfs returning %r"));
			threadexits(Ebadpid);
		}
		/* retry on failure */
	}
	DPRINT(DEXE, "\t control channel on fd %d\n", sess->fd);

	n = pread(sess->fd, buf, 255, 0);
	if(n < 0) {
		sendp(sess->chan,
			smprint("couldn't read from session ctl %s/clone: %r\n", 
					sess->path));
		threadexits(Ebadctl);
	}
	if(sess->remote)
		sess->pid = atoi(buf+1); /* skip the g */
	else 
		sess->pid = atoi(buf); /* convert to local execs session number */
	
	/* if we are a remote session, setup backmount */
	if(sess->remote) {
		DPRINT(DEXE, "Attempting to remote checkmount %s\n", xgangpath);
		n = fprint(sess->fd, "checkmount %s", mysysname);
		if(n < 0) {
			DPRINT(DEXE, "setting up backmount failed: %r\n");
			chanprint(sess->chan, "setting up backmount failed: %r\n");
			goto error;
		}

		if(sess->g->imode == CMenum) {
			n = fprint(sess->fd, "enum");
			if(n < 0) {
				DPRINT(DEXE, "setting up enum failed: %r\n");
				chanprint(sess->chan, "setting up enum failed: %r\n");
				goto error;
			}
		}

		n = fprint(sess->fd, "res %d", sess->remote);
		if(n < 0) {
			DPRINT(DEXE, "remote reservation failed: %r\n");
			chanprint(sess->chan, "remote reservation failed: %r\n");
			goto error;
		}
	}

	/* finish by setting up stdio */
	if(err = setupstdio(sess)) {
		DPRINT(DEXE, "reporting failure from stdio setup session %d: %s\n", 
				sess->index, err);
		sendp(sess->chan, err);
	} else {
		DPRINT(DEXE, "reporting success from session %d\n", sess->index);
		sendp(sess->chan, nil);
	}

error:
	threadexits(nil);
}

static void
setupsess(Gang *g, Session *s, char *path, int r)
{
	if(path)
		s->path = smprint("/n/%s/proc", path);
	else
		s->path = smprint("/proc");
	s->r = nil;
	s->chan = chancreate(sizeof(char *), 0);
	s->g = g; /* back pointer */
	s->remote = r;
}

/* local reservation */
static char *
reslocal(Gang *g)
{
	int count;
	char *err = nil;

	DPRINT(DEXE, "reslocal %d\n", g->size);

	if(mystats.next == nil)
		mystats.njobs += g->size;

	g->sess = emalloc9p(sizeof(Session)*g->size);
	
	for(count = 0; count < g->size; count++) {
		DPRINT(DEXE, "\t reservation: count: %d\n", count);
		setupsess(g, &g->sess[count], nil, 0);
		proccreate(cloneproc, &g->sess[count], STACK);
	}

	for(count = 0; count < g->size; count++) {
		char *myerr;
		DPRINT(DEXE, "\t waiting on reservation: count: %d\n", count); 
		myerr = recvp(g->sess[count].chan);
		if(myerr != nil) 
			err = myerr;
			/* MAYBE: retry here? for reliability ? */
	}
	
	return err;
}

/* multinode reservation */
static char *
resgang(Gang *g)
{
	int *njobs; /* array of children with jobs per child */
	int remaining;
	Status *current;
	int scount = 0;
	int count;
	char *err = nil;
	int size = g->size;

	g->size=0;

	DPRINT(DEXE, "resgang %d\n", size);

	/* MAYBE: Lock? */
	/* TODO: implement other modes (block, time-share) */
	if(size > (mystats.nproc-mystats.njobs))
		return Enores;
	
	/* TODO: CRUX: determine how many subsessions we'll need */
	/* Lock down gang */
	njobs = emalloc9p(sizeof(int)*mystats.nchild);
	remaining = size;
	/* walk children tree, allocating cores and incrementing stats.njobs
		counting sessions and keeping children per subsession in an array? */
	for(current=mystats.next; current !=nil; current = current->next) {
		int avail = current->nproc - current->njobs;
		if(avail > remaining) {
			njobs[scount] = remaining;
			current->njobs += njobs[scount];
			break;
		} else {
			njobs[scount] = current->nproc - current->njobs;
			remaining -= njobs[scount];
		}
		current->njobs += njobs[scount];
		scount++;
	}
	
	/* allocate subsessions */
	g->sess = emalloc9p(sizeof(Session)*scount);
	g->size = scount;

	/* while more to schedule */
	current = mystats.next;
	for(count = 0; count < scount; count++) {
		checkmount(current->name);
		setupsess(g, &g->sess[count], current->name, njobs[count]);
		proccreate(cloneproc, &g->sess[count], STACK);
	}

	for(count = 0; count < scount; count++) {
		char *myerr;
		DPRINT(DEXE, "\t waiting on reservation: count: %d\n", count); 
		myerr = recvp(g->sess[count].chan);
		if(myerr != nil) {
			DPRINT(DERR, "*ERROR*: session %d broke\n", count);
			err = myerr;
			g->status = GANG_BROKE;
		}
			/* MAYBE: retry here? for reliability ? */
	}


	return err;
}

/* TODO: many of the error scenarios here probably need more cleanup */
static void
cmdres(Req *r, int num)
{
	Fid *f = r->fid;
	Gang *g = f->aux;

	char dest[255];
	char *mntargs;
	char *err;

	/* okay - by this point we should have gang embedded */
	if(g == nil) {
		respond(r, Enogang);
		return;
	} 

	if(g->size > 0) {
		respond(r, Ebusy);
		return;
	}

	/* setup aggregation I/O */
	snprint(dest, 255, "%s/g%d", gangpath, g->index);

	switch(g->imode) {
		case CMbcast:
			DPRINT(DGAN, "broadcast mode\n");
			if (mpipe(dest, "-b stdin") < 0) {
				DPRINT(DGAN, "setting up stdin pipe failed\n");
				respond(r, Estdin);
				return;
			}
			break;
		case CMenum:
			DPRINT(DGAN, "enumerated mode\n");
			mntargs = smprint("-e %d stdin", num);
			if (mpipe(dest, mntargs)< 0) {
				free(mntargs);
				respond(r, Estdin);
				return;
			}
			free(mntargs);
			break;
		default:
			respond(r, Ebadimode);
			return;
	};
	if (mpipe(dest, "stdout") < 0) {  
		respond(r, Estdout);
		return;
	}
	if (mpipe(dest, "stderr") < 0) { 
		respond(r, Estderr);
		return;
	}

	g->size = num;

	/* this assumes top down reservation model */
	if(mystats.nchild)
		err = resgang(g);
	else
		err = reslocal(g);

	if(err) {
		DPRINT(DERR, "*ERROR*: problem establishing gang reservation: %s\n", err);
		respond(r, err);
		/* TODO: we have to cleanup gang and sessions here */
		
		free(err);
	} else {
		DPRINT(DEXE, "\t gangs all here\n"); 
		g->status = GANG_RESV;
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
	}
}

/* this should probably be a generic function somewhere */
static char *
cleanupcb(char *buf, int count)
{
	int c;
	char *nbuf;
	/* check last few bytes for crap */
	for(c = count-1; c < count-3; c--) {
		if((buf[c] < ' ')||(c>'~'))
			buf[c] = 0;
	}
	
	nbuf = emalloc9p(count+1);
	memmove(nbuf, buf, count);
	nbuf[count] = 0;
	return nbuf;
}

static void
relaycmd(void *arg)
{
	Session *s = arg;
	int n;

	DPRINT(DEXE, "writing %d count of data to %d\n", s->r->ifcall.count, s->fd);
	n = pwrite(s->fd, s->r->ifcall.data, s->r->ifcall.count, 0);
	if(n < 0) {
		sendp(s->chan, smprint("relay write cmd failed: %r\n"));
	} else {
		sendp(s->chan, nil);
	}
}

static void
cmdbcast(Req *r, Gang *g)
{
	int count;
	char *resp;
	char *err = nil;

	DPRINT(DBCA, "broadcasting %s to gang %d (size: %d)\n", r->ifcall.data, g->index, g->size);

	/* broadcast to all subsessions */
	for(count = 0; count < g->size; count++) {
		/* slightly slimy */
		g->sess[count].r = r;
		proccreate(relaycmd, &g->sess[count], STACK);
	}
	
	/* wait for responses, checking for errors */
	for(count = 0; count < g->size; count++) {
		/* wait for response */
		resp = recvp(g->sess[count].chan);
		g->sess[count].r = nil;
		if(resp)
			err = resp;
	}
	
	if(err) {
		respond(r, err);
	} else {
		/* success */
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
	}
}

static void
fswrite(void *arg)
{
	Req *r = (Req *)arg;
	Fid *f = r->fid;
	Qid *q = &f->qid;
	Gang *g = f->aux;
	char e[ERRMAX];
	char *buf;
	Cmdbuf *cb;
	Cmdtab *cmd;
	int num;
	
	switch(TYPE(*q)) {
	default:
		snprint(e, sizeof e, "bug in gangfs path=%llux\n", q->path);
		respond(r, e);
		break;
	case Qgstat:
		buf = estrdup9p(r->ifcall.data);
		statuswrite(buf);	/* TODO: add error checking & reporting */
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
		return;
	case Qctl:
		if(r->ifcall.count >= 1024) {
			respond(r, "ctl message too long");
			return;
		}
		if(g->status > GANG_EXEC) {
			respond(r, Eclosing);
			return;
		}	
		
		buf = cleanupcb(r->ifcall.data, r->ifcall.count);
		cb = parsecmd(buf, strlen(buf));
		cmd = lookupcmd(cb, ctlcmdtab, nelem(ctlcmdtab));
		if(cmd == nil) {
			if((g == nil)||(g->size == 0)) {
				respondcmderror(r, cb, "%r");
			} else {
				DPRINT(DEXE, "Unknown command, broadcasting to children\n");
				cmdbcast(r, g);
			}
		} else {
			switch(cmd->index) {
			default:
				respondcmderror(r, cb, "unimplemented");
				break;
			case CMres:	/* reservation */	
				if(cb->nf < 2) {
					respondcmderror(r, cb, "res insufficient args %d", cb->nf);
					break;
				}
				num = atol(cb->f[1]);
				if(num < 0) {
					respondcmderror(r, cb, "bad arguments: %d", num);
				} else {
					cmdres(r, num);
				}
				break;
			case CMmount: /* check mount */
				DPRINT(DGAN, "check mount %s (%d)\n", cb->f[1], cb->nf);
				if(checkmount(cb->f[1]) < 0) {
					respond(r, Echeckmount);
				} else {
					r->ofcall.count = r->ifcall.count;
					respond(r, nil);
				}
				break;				
			case CMenum: /* enable enumeration mode */
				DPRINT(DGAN, "Setting enumerated mode\n");
				g->imode = CMenum;
				r->ofcall.count = r->ifcall.count;
				respond(r, nil);
				break;
			case CMbcast: /* enable broadcast mode */
				DPRINT(DGAN, "Setting broadcast mode\n");
				g->imode = CMbcast;
				r->ofcall.count = r->ifcall.count;
				respond(r, nil);
				break;		
			};
		}
		free(cb);
		free(buf);
		break;		
	};
}

static void
fsstat(Req* r)
{
	Fid *fid = r->fid;
	int n = TYPE(fid->qid) - 1;

	if(TYPE(fid->qid) >= Qsubses) {
		respond(r, Eperm);
		return;
	}

	if (dirgen(n, &r->d, fid) < 0)
		respond(r, Enotfound);
	else
		respond(r, nil);
}

static void
cleanupgang(void *arg)
{
	Gang *g = arg;
	char fname[255];

	DPRINT(DCLN, "cleaning up gang, entering umount\n");
	snprint(fname, 255, "%s/g%d/stdin", gangpath, g->index);
	flushmp(fname); /* flush pipe to be sure */
	unmount(0, fname);
	snprint(fname, 255, "%s/g%d/stdout", gangpath, g->index);
	unmount(0, fname);
	snprint(fname, 255, "%s/g%d/stderr", gangpath, g->index);
	unmount(0, fname);

	/* TODO: Clean up subsessions? */

	/* close it out */
	g->status=GANG_CLOSED;
}


static void
fsclunk(Fid *fid)
{
	Gang *g;

	DPRINT(DFID, "fsclunk: %d\n", fid->fid);
	if(TYPE(fid->qid) == Qctl) {
		DPRINT(DFID, "and its a ctl\n");
		g = fid->aux;
		if(g) {
			wlock(&glock);
			g->ctlref--;	
			DPRINT(DREF, "ctlref=%d\n", g->ctlref);
			if(g->ctlref == 0) {
				if(mystats.next == nil)
					mystats.njobs -= g->size;
				g->status=GANG_CLOSING;
				wunlock(&glock);
				proccreate(cleanupgang, (void *)g, STACK); 
			} else {
				wunlock(&glock);
			}
		} else
			DPRINT(DERR, "*ERROR*: fsclunk: ctl fid without aux\n");
	} else {
		if(fid->aux != nil) {
			g = fid->aux;
			releasegang(g);
		}
	}
}

/* handle certain ops in a separate thread in original namespace */
static void
iothread(void*)
{
	Req *r;

	threadsetname("gangfs-iothread");
	if(sendp(iochan, nil) < 0) {
		fprint(2, "iothread: problem sending sync message\n");
		threadexits("nosync");
	}

	for(;;) {
		r = recvp(iochan);
		if(r == nil)
			threadexits("interrupted");

		switch(r->ifcall.type){
		case Topen:
			fsopen(r);	
			break;
		case Tread:
			fsread(r);			
			break;
		case Twrite:
			proccreate(fswrite, (void *) r, STACK);
			break;
		case Tclunk:
			fsclunk(r->fid);
			sendp(clunkchan, r);
			free(r);
			break;
		default:
			fprint(2, "unrecognized io op %d\n", r->ifcall.type);
			break;
		}
	}
}

static void
ioproxy(Req *r)
{
	if(sendp(iochan, r) != 1) {
		fprint(2, "iochan hungup");
		threadexits("iochan hungup");
	}
}

static void
clunkproxy(Fid *f)
{
	/* really freaking clunky, but not sure what else to do */
	Req *r = emalloc9p(sizeof(Req));

	DPRINT(DFID, "clunkproxy: %p fidnum=%d aux=%p\n", f, f->fid, f->aux);

	r->ifcall.type = Tclunk;
	r->fid = f;
	if(sendp(iochan, r) != 1) {
		fprint(2, "iochan hungup");
		threadexits("iochan hungup");
	}
	DPRINT(DFID, "clunkproxy: waiting on response\n");
	recvp(clunkchan);
}


Srv fs=
{
	.attach=	fsattach,
	.walk1=		fswalk1,
	.clone=		fsclone,
	.open=		ioproxy,
	.write=		ioproxy,
	.read=		ioproxy,
	.stat=		fsstat,
	.destroyfid=clunkproxy,
	.end=		cleanup,
};

void
threadmain(int argc, char **argv)
{
	char *x;
	char *parent = nil;

	updateinterval = 5;	/* 5 second default update interval */
	mysysname = getenv("sysname");

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'v':
		x = ARGF();
		if(x)
			vflag = atoi(x);
		break;
	case 'p':	/* specify parent */
		parent = ARGF();
		break;
	case 'n':	/* name override */
		mysysname = estrdup9p(ARGF());
		break;
	case 'i':	/* update interval */
		x = ARGF();
		if(x)
			updateinterval = atoi(x);
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();

	if(argc)
		procpath = argv[0];
	else
		procpath = defaultpath;

	gangpath = procpath;
	xgangpath = procpath; /* until we get a statuswrite */
	
	DPRINT(DGAN, "Ganpath=%s procpath=%s\n", gangpath, procpath);

	/* initialize status */
	initstatus(&mystats);
	
	if(parent) {
		if(checkmount(parent) < 0) {
			DPRINT(DERR, "*ERROR*: Connecting to parent failed\n");
			threadexits("problem connecting to parent\n");
		}
		proccreate(updatestatus, (void *)parent, STACK);
	}

	/* spawn off a io thread */
	iochan = chancreate(sizeof(void *), 0);
	clunkchan = chancreate(sizeof(void *), 0);
	proccreate(iothread, nil, STACK);
	recvp(iochan);

	threadpostmountsrv(&fs, nil, procpath, MAFTER);

	threadexits(0);
}
