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
		* subsession creation, destruction and accounting
		* reservation & binding of subsessions
		* aggregated ctl via multipipes
		* aggregated io via multipipes
		* aggregated status via multipipes
		* aggregated wait via multipipe barrier (todo in multipipe)
		* garbage collection on close
*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <mp.h>
#include <libsec.h>
#include <stdio.h>

char 	defaultpath[] =	"/proc";
char *procpath;
char *srvctl;
Channel *iochan;
Channel *bindchan;

char Enotdir[] = "Not a directory";
char Enotfound[] = "File not found";
char Eperm[] = "Permission denied";

enum
{
	STACK = (8 * 1024),
	NAMELEN = 255,
};

/* 
  qid.path format:
	[CONV(16)][TYPE(16)] where low 00ff bytes are type
*/
#define TYPE(x) 	 ((ulong) (x).path & 0xff)
#define CONVP(x) 	 ((((ulong) (x).path >> 16)&0xffff) -1)
#define CONV(x) 	 ((((ulong) (x)->path >> 16)&0xffff) -1)
#define CONVQIDP(c, y) (((c+1) << 16)|(y))

typedef struct Session Session;
typedef struct Gang Gang;
struct Session
{
	Gang *g;		/* gang? necessary? */
	int fd;		/* fd to ctl file */
};

struct Gang
{
	int index;			/* gang id */
	int size;		/* number of subsessions */
	Session *sess;	/* array of subsessions */

	Channel *chan;	/* channel for sync/error reporting */

	int ctlmp;		/* handle to ctl multipipe */

	int refcount;		/* reference count */
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
	Qstdin,
	Qstdout,
	Qstderr,
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
	"stdin",	{Qstdin},				0222,
	"stdout",	{Qstdout},				0444,
	"stderr",	{Qstderr},				0444,
};

static void
usage(void)
{
	fprint(2, "gangfs [-D] [mtpt]\n");
	exits("usage");
}

static int
mpipe(char *path, char *name)
{
	int fd, ret;
	fd = open("/srv/mpipe", ORDWR);
	if(fd<0) {
		fprint(2, "couldn't open /srv/mpipe: %r\n");
		return -1;
	}
	
	ret = mount(fd, -1, path, MBEFORE, name);
	close(fd);
	return ret;
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
	mygang->refcount++;
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
			if(current->index == index) {
				current->refcount++;
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
			if(count == which) {
				current->refcount++;
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
/* TODO: Remove if we don't need it
	int count;
	
	for(count = 0; count < g->size; count++)
		free(g->sess[count].path;
*/
	free(g->sess);
}

static void
releasegang(Gang *g)
{
	wlock(&glock);
	g->refcount--;
	if(g->refcount == 0) {	/* clean up */
		Gang *current;
		if(glist == nil) {
			goto out;
		}
		for(current=glist; current->next != g; current = current->next) {
			if(current == nil) {
				goto out;
			}
		}
		current->next = g->next;
		releasesessions(g);
		chanfree(g->chan);
		free(g);
	}
out:
	wunlock(&glock);
}

static void
cleanup(Srv *)
{
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
			d->name = smprint("g%lud", CONV(q)); /* TODO: this isn't going to line up? */
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
		g->refcount++;
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
	void *resp;

	if(TYPE(fid->qid) >= Qsubses) {
		respond(r, Eperm);
		return;
	}
	if (q->type&QTDIR){
		respond(r, nil);
		return;
	}

	if(TYPE(fid->qid) != Qclone) {
		/* FUTURE: probably need better ref count here */
		respond(r, nil);
		return;
	}

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

	/* bind multipipes into place */	
	if(sendp(bindchan, mygang) != 1) {
		fprint(2, "bindchan hungup");
		threadexits("bindchan hungup");
	}

	if(recv(mygang->chan, &resp) < 0) {
		respond(r, "unknown problem on binder thread");
	} else {
		if(resp == nil) {
			respond(r, nil);
		} else {
			respond(r, resp);
			free(resp);
		}
	}
}

static void
fsread(Req *r)
{
	Fid *fid = r->fid;
	Qid *q = &fid->qid;

	if (q->type&QTDIR) {
		dirread9p(r, dirgen, fid);
		respond(r, nil);
		return;
	}

	if(TYPE(fid->qid) == Qctl) {
		char buf[NAMELEN];

		sprint(buf, "%lud\n", CONVP(fid->qid));
		readstr(r, buf);
		respond(r, nil);
		return;
	}

	respond(r, "not yet");
}

enum
{
	CMres,
};

Cmdtab ctlcmdtab[]={
	CMres, "res", 0,
};

static void
cmdres(Req *r, int num, int argc, char **argv)
{
	int c;
	Fid *f = r->fid;
	Gang *g = f->aux;
	void *resp = nil;

	USED(argv);

	/* okay - by this point we should have gang embedded */
	if(g == nil) {
		respond(r, "ctl file has no gang");
		return;
	} 

	if(g->size > 0) {
		respond(r, "gang already has subsessions reserved");
		return;
	}

	g->size = num;

	if(sendp(bindchan, g) != 1) {
		fprint(2, "bindchan hungup");
		threadexits("bindchan hungup");
	}

	if(recv(g->chan, &resp) < 0) {
		/* channel failure.... */
		respond(r, "unknown problem on binder thread");
		return;
	}

	if(resp == nil) {
		r->ofcall.count = r->ifcall.count;
		respond(r, nil);
	} else {
		respond(r, resp);
		free(resp);
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
fswrite(Req *r)
{
	Fid *f = r->fid;
	Qid *q = &f->qid;
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

	case Qctl:
		if(r->ifcall.count >= 1024) {
			respond(r, "ctl message too long");
			return;
		}
		
		buf = cleanupcb(r->ifcall.data, r->ifcall.count);
		cb = parsecmd(buf, strlen(buf));
		cmd = lookupcmd(cb, ctlcmdtab, nelem(ctlcmdtab));
		if(cmd == nil) {
			respondcmderror(r, cb, "%r");
		} else {
			switch(cmd->index) {
			default:
				respondcmderror(r, cb, "unimplemented");
				break;
			case CMres:	/* reservation */	
				if(cb->nf < 2) {
					respondcmderror(r, cb, "insufficient args %d", cb->nf);
					break;
				}
				num = atol(cb->f[1]);
				if(num < 0) {
					respondcmderror(r, cb, "bad arguments: %d", num);
				} else {
					cmdres(r, num, cb->nf-2, &cb->f[2]);
				}
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
fsclunk(Fid *f)
{
	USED(f);
	/*  
	Gang *g;

	if((0)&&(f->aux != nil)) {
		g = f->aux;
		releasegang(g);
	}
	*/
}

/* handle certain ops in a separate thread in original namespace */
static void
iothread(void*)
{
	Req *r;

	threadsetname("gangfs-iothread");
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
			fswrite(r);
			break;
		default:
			fprint(2, "unrecognized io op %d\n", r->ifcall.type);
			break;
		}
	}
}

/* find the execfs to allocate next session from */
/* right now we just return local execfs */
static char *
findexecfs(void)
{
	return "/proc";
}

/* handle exec binding in a separate thread in original namespace*/
static void
bindthread(void *)
{
	Gang *g;
	int n;
	int count;
	char buf[255];
	char dest[255];
	char *path;

	threadsetname("gangfs-bindthread");
	for(;;) {
		g = recvp(bindchan);
		if(g == nil)
			threadexits("interrupted");

		if(g->size == 0) { /* bind multipipes into place */
			snprint(buf, 255, "/proc/g%d", g->index);
			if(mpipe(buf, "ctl") < 0) {
				sendp(g->chan, smprint("couldn't bind ctl multipipe\n"));
				continue;
			} else {
				snprint(buf, 255, "/proc/g%d/ctl", g->index);
				g->ctlmp = open(buf, OWRITE);
			}
			send(g->chan, nil);
			continue;
		}

		/* FUTURE: do we want to refork for every new request? */
		g->sess = emalloc9p(sizeof(Session)*g->size);
		for(count = 0; count < g->size; count++) {
			path = findexecfs();
			snprint(buf, 255, "%s/clone", path);
			g->sess->fd = open(buf, ORDWR);
			if(g->sess->fd < 0) {
				sendp(g->chan, 
				  smprint("couldn't open session ctl %s/clone: %r\n", path));
				goto error;
			}
			n = read(g->sess->fd, buf, 255);
			if(n < 0) {
				sendp(g->chan,
				  smprint("couldn't read from session ctl %s/clone: %r\n", 
						path));
				goto error;
			}
			n = atoi(buf); /* convert to local execs session number */
			snprint(buf, 255, "%s/%d", path, n);
			/* of course we''l need another buf for our local fs ? */
			snprint(dest, 255, "/proc/g%d/%d", g->index, count);
			n = bind(buf, dest, MREPL);
			if(n < 0) {
				sendp(g->chan,
				   smprint("couldn't bind %s %s: %r\n", buf, dest));
				goto error;
			}
		}
		send(g->chan, nil);
error:
		;
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

Srv fs=
{
	.attach=	fsattach,
	.walk1=	fswalk1,
	.clone=	fsclone,
	.open=	ioproxy,
	.write=	ioproxy,
	.read=	ioproxy,
	.stat	=	fsstat,
	.destroyfid =	fsclunk,
	.end=	cleanup,
};

void
threadmain(int argc, char **argv)
{
	ARGBEGIN{
	case 'D':
		chatty9p++;
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

	/* spawn off a io thread */
	iochan = chancreate(sizeof(void *), 0);
	proccreate(iothread, nil, STACK);

	threadpostmountsrv(&fs, nil, procpath, MAFTER);

	/* spawn off a bind thread */
	bindchan = chancreate(sizeof(void *), 0);
	proccreate(bindthread, nil, STACK);
	threadexits(0);
}
