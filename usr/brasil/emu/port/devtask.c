/*
 * devtask.c
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *		Pravin Shinde (shindepravin@gmail.com)
 *		Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Description: remote task file system
 *
 * Based in part on devcmd.c
 */

#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"kernel.h"

extern char *hosttype;
static int hostix = 0;
static int platix = 0;

enum {
	Qtopdir,				/* top level directory */
	Qcmd,					/* "remote" dir  */
	Qclonus,
	Qtopctl,
	Qlocalfs,				/* local fs mount point */
	Qlocalnet,				/* local net mount point */
	Qarch,					/* architecture description */
	Qtopns,					/* default ns for host */
	Qtopenv,				/* default env for host */
	Qtopstat,			/* Monitoring Information for host */
	Qconvdir,				/* resource dir */
	Qconvbase,
	Qdata = Qconvbase,
	Qstderr,
	Qctl,
	Qstatus,
	Qargs,
	Qenv,
	Qstdin,
	Qstdout,
	Qns,
	Qwait,
	QLconrdir,

	Debug = 0,					/* to help debug os.c */
	Nremchan = 10,
};

#define TYPE(x) 	 ( (ulong) (x).path & 0xff)
#define CONV(x) 	 ( ( (ulong) (x).path >> 8)&0xff)
#define RJOBI(x) 	 ( ( (ulong) (x).path >> 16)&0xff)
#define QID(c, y) 	 ( ( (c)<<8) | (y))
#define RQID(c, x, y) 	 ( ( ( (c)<< 16) | ( (x) << 8)) | (y))

char* base = "/csrv/local";
char* validdir = "local";
char* remmnt = "/csrv";
char* parent = "parent";

char ENoRemResrcs[] = "No remote resources available";
char ERemoteResFail[] = "remote reservation request failed";
char ENoResourceMatch[] = "No resources matching request";
char ENOReservation[] = "No remote reservation done";
char EResourcesReleased[] = "Resources already released";
char EResourcesINUse[] = "Resources already in use";
char EBadstatus[] = "Invalid status";
char ERemoteError[] = "Invalid status";

static int vflag = 0;		/* for debugging messages: control prints */

void
_dprint(ulong debuglevel, char *fmt, ...)
{
	va_list args;
	char *p;
	int len = 0;
	char s[255];
	char *newfmt;

	if(vflag<debuglevel)
		return;
	p = strchr(fmt, '\n');
//	newfmt = smprint("%ld %d %s :%q\n", time(0), getpid(), fmt, up->env->errstr);
	newfmt = smprint("%ld %d %s\n", time(0), getpid(), fmt);
	if(p != nil){
		p = strchr(newfmt, '\n');
		*p = ' ';
	}
	va_start(args, fmt);
	len += vsnprint(s + len, sizeof s - len, newfmt, args);
	va_end(args);
	if (newfmt != nil) free(newfmt);
//	newfmt = nil;
	
	print(s, len);
}
#define DPRINT if(vflag)_dprint

long lastrrselected = 0;

struct RemFile {
	int state;
	int lsessid;
	QLock l;				/* protects state changes */
	Chan *cfile;
	Queue *asyncq;
};
typedef struct RemFile RemFile;

struct RemResrc {
	int lsessid;
	int rsessid;
	int state;
	int inuse;
	int subsess;
	/* List of all files in remote resource directory */
	RemFile remotefiles[Nremchan];
	/* Next remote resource */
	struct RemResrc *next;
};
typedef struct RemResrc RemResrc;

struct RemJob {
	long rjobcount;				/* number of jobs in current session */
	int x;
	RWlock l;					/* protects state changes */
	int rcinuse[Nremchan];
	RWlock perfilelock[Nremchan];
	Queue *asyncq_list[Nremchan];
	RemResrc *first;
	RemResrc *last;
};
typedef struct RemJob RemJob;

enum {
	Nplatform = 7,
	Nos = 9,
};

/* list of OS and platforms supported */
char *oslist[Nos] = { 
	"Other", "Hp", "Inferno", "Irix", "Linux",
	"MacOSX", "Nt", "Plan9", "Solaris"
};

char *platformlist[Nplatform] = { 
	"Other", "386", "arm", "mips",
	"power", "s800", "sparc"
};

struct RemMount {
	char *path;
	Chan *status;
	long info[Nos][Nplatform];
};

typedef struct RemMount RemMount;

enum {
	CVmagic = 0xfeedbabe,

};

typedef struct Conv Conv;
struct Conv {
	int magic;
	int x;
	int inuse;
	int fd[3];				/* stdin, stdout, and stderr */
	int count[3];				/* number of readers on */
	/* stdin/stdout/stderr */
	int perm;
	char *owner;
	char *state;
	Cmdbuf *cmd;
	char *dir;
	QLock l;				/* protects state changes */
	Queue *waitq;
	void *child;
	char *error;				/* on start up */
	int nice;
	short killonclose;
	short killed;
	Rendez startr;
	RemJob *rjob;		/* path to researved remote resource */
	QLock inlock;		/* lock to block writes before exec */
	QLock outlock;		/* lock to block reads before exec */
};

void
dumpconv(Conv *c, char *s)
{
	if(c == nil){
		print("NIL COOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOONV\n");
		return;
	}
	if(c->magic != CVmagic){
		print("BAD MAGIC BOOOOOOOOOOOOOOOOOOOOOOOOPOOOIOOOOOOM\n");
		return;
	}
	if(c->cmd == nil){
		print("BAD CMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMD\n");
	}
	if(c->fd == nil){
		print("BAD FDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\n");
	}	
	print("%s: owner %s state %s error %s dir %s cmd->f[1] %s fd0 %d fd1 %d fd2 %d\n", s,
		c->owner, c->state, c->error, c->dir, c->cmd->f[1], c->fd[0], c->fd[1], c->fd[2]);

}

static int ccount = 0;

static struct {
	QLock l;
	int nc;
	int maxconv;
	Conv **conv;
} cmd;

struct for_splice {
	Chan *src;
	Chan *dst;
};

struct parallel_send_wrap {
	RemFile *rf;
	void *p_a;
	long p_n;
	vlong p_offset;
};

/* function prototypes */
static Conv *cmdclone(char *);
char *fetchparentpath(char *filepath);
long lsdir(char *name, Dir ** d);
char *fetchparentpathch(Chan * ch);
static void cmdproc(void *a);
static long lookup(char *word, char **wordlist, int len);
static void initinfo(long info[Nos][Nplatform]);
static long readstatus(char *a, long n, vlong offset);
static void spliceproc(void *);

/* function prototypes from other files */
long dirpackage(uchar * buf, long ts, Dir ** d);

static vlong 
timestamp (vlong stime)
{
	return osusectime() - stime;
}

static long
countrjob(RemJob * rjob)
{

	long temp;
	if (rjob == nil)
		return -2;
	rlock(&rjob->l);
	temp = rjob->rjobcount;
	runlock(&rjob->l);
	return temp;
}

static int
getfileopencount(RemJob * rjob, int filetype)
{

	int temp;
	if (rjob == nil)
		return -1;
	rlock(&rjob->l);
	temp = rjob->rcinuse[filetype];
	runlock(&rjob->l);
	return temp;
}

/* Generate entry within resource directory */
static int
cmd3gen(Chan * c, int i, Dir * dp)
{
	Qid q;
	Conv *cv;

	cv = cmd.conv[CONV(c->qid)];
	switch (i) {
	default:
		return -1;
	case Qdata:
		mkqid(&q, QID(CONV(c->qid), Qdata), 0, QTFILE);
		devdir(c, q, "stdio", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qstderr:
		mkqid(&q, QID(CONV(c->qid), Qstderr), 0, QTFILE);
		devdir(c, q, "stderr", 0, cv->owner, 0444, dp);
		return 1;
	case Qctl:
		mkqid(&q, QID(CONV(c->qid), Qctl), 0, QTFILE);
		devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qstatus:
		mkqid(&q, QID(CONV(c->qid), Qstatus), 0, QTFILE);
		devdir(c, q, "status", 0, cv->owner, 0444, dp);
		return 1;
	case Qwait:
		mkqid(&q, QID(CONV(c->qid), Qwait), 0, QTFILE);
		devdir(c, q, "wait", 0, cv->owner, 0444, dp);
		return 1;
	case Qstdin:
		mkqid(&q, QID(CONV(c->qid), Qstdin), 0, QTFILE);
		devdir(c, q, "stdin", 0, cv->owner, 0222, dp);
		return 1;
	case Qstdout:
		mkqid(&q, QID(CONV(c->qid), Qstdout), 0, QTFILE);
		devdir(c, q, "stdout", 0, cv->owner, 0444, dp);
		return 1;
	case Qargs:
		mkqid(&q, QID(CONV(c->qid), Qstdin), 0, QTFILE);
		devdir(c, q, "args", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qenv:
		mkqid(&q, QID(CONV(c->qid), Qstdin), 0, QTFILE);
		devdir(c, q, "env", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qns:
		mkqid(&q, QID(CONV(c->qid), Qns), 0, QTFILE);
		devdir(c, q, "ns", 0, cv->owner, cv->perm, dp);
		return 1;
	}
}

/* supposed to generate top level directory structure for task */
static int
cmdgen(Chan * c, char *name, Dirtab *d, int nd, int s, Dir * dp)
{
	Qid q;
	Conv *cv;
	Conv *myc;
	char buf[KNAMELEN * 3];
	long jc;

	USED(name);
	USED(d);
	USED(nd);

	if (s == DEVDOTDOT) {
		switch (TYPE(c->qid)) {
		case Qtopdir:
		case Qcmd:
			mkqid(&q, QID(0, Qtopdir), 0, QTDIR);
			devdir(c, q, "#T", 0, eve, DMDIR | 0555, dp);
			break;
		case Qconvdir:
			mkqid(&q, QID(0, Qcmd), 0, QTDIR);
			devdir(c, q, "local", 0, up->env->user, DMDIR | 0555, 
				dp);
			break;
		case QLconrdir:		/* for remote resource bindings */
			DPRINT(9,"cmdindex [%ld], rjobi [%ld]\n",
				CONV(c->qid), RJOBI(c->qid));
			snprint(buf, sizeof buf, "%ld", CONV(c->qid));
			cv = cmd.conv[CONV(c->qid)];
			mkqid(&q, QID(CONV(c->qid), Qconvdir), 0, QTDIR);
			devdir(c, q, buf, 0, cv->owner, DMDIR | 0555, dp);
			break;
		default:
			panic("cmdgen %llux", c->qid.path);
		}
		return 1;
	}
	switch (TYPE(c->qid)) {
	case Qtopdir:
		if (s >= 1)
			return -1;
		mkqid(&q, QID(0, Qcmd), 0, QTDIR);
		//devdir(c, q, "local", 0, eve, DMDIR | 0555, dp);
		devdir(c, q, "local", 0, up->env->user, DMDIR | 0555, dp);
		return 1;
	case Qcmd:
		if (s < cmd.nc) {
			cv = cmd.conv[s];
			mkqid(&q, QID(s, Qconvdir), 0, QTDIR);
			snprint(up->genbuf, sizeof up->genbuf, "%d", s);
			devdir(c, q, up->genbuf, 0, cv->owner, DMDIR | 0555, dp);
			return 1;
		}
		s -= cmd.nc;
		switch(s){
		case 0:
			mkqid(&q, QID(0, Qclonus), 0, QTFILE);
			devdir(c, q, "clone", 0, eve, 0666, dp);
			return 1;
		case 1:
			mkqid(&q, QID(0, Qtopctl), 0, QTFILE);
			//devdir(c, q, "ctl", 0, eve, 0777, dp);
			devdir(c, q, "ctl", 0, up->env->user, 0777, dp);
			return 1;
		case 2:
			mkqid(&q, QID(0, Qlocalfs), 0, QTDIR);
			devdir(c, q, "fs", 0, eve, DMDIR | 0555, dp);
			return 1;
		case 3:
			mkqid(&q, QID(0, Qlocalnet), 0, QTDIR);
			devdir(c, q, "net", 0, eve, DMDIR | 0555, dp);
			return 1;
		case 4:
			mkqid(&q, QID(0, Qarch), 0, QTFILE);
			devdir(c, q, "arch", 0, eve, 0666, dp);
			return 1;
		case 5:
			mkqid(&q, QID(0, Qtopns), 0, QTDIR);
			devdir(c, q, "ns", 0, eve, 0666, dp);
			return 1;
		case 6:
			mkqid(&q, QID(0, Qtopenv), 0, QTDIR);
			devdir(c, q, "env", 0, eve, 0666, dp);
			return 1;
		case 7:
			mkqid(&q, QID(0, Qtopstat), 0, QTFILE);
			devdir(c, q, "status", 0, eve, 0666, dp);
			return 1;
		}
		return -1;
	case Qclonus:
		if (s == 0) {
			mkqid(&q, QID(0, Qclonus), 0, QTFILE);
			devdir(c, q, "clone", 0, eve, 0666, dp);
			return 1;
		}
		return -1;
	case Qtopctl:
		if (s == 0) {
			mkqid(&q, QID(0, Qtopctl), 0, QTFILE);
			//devdir(c, q, "ctl", 0, eve, 0777, dp);
			devdir(c, q, "ctl", 0, up->env->user, 0777, dp);
			return 1;
		}
		return -1;
	case Qtopstat:
		if (s == 0) {
			mkqid(&q, QID(0, Qtopstat), 0, QTFILE);
			devdir(c, q, "status", 0, eve, 0666, dp);
			return 1;
		}
		return -1;
	case Qlocalfs:
		if (s == 0) {
			mkqid(&q, QID(0, Qlocalfs), 0, QTDIR);
			devdir(c, q, "fs", 0, eve, DMDIR | 0555, dp);
			return 1;
		}
		return -1;
	case Qlocalnet:
		if (s == 0) {
			mkqid(&q, QID(0, Qlocalnet), 0, QTDIR);
			devdir(c, q, "net", 0, eve, DMDIR | 0555, dp);
			return 1;
		}
		return -1;
	case Qarch:
		if (s == 0) {
			mkqid(&q, QID(0, Qarch), 0, QTFILE);
			devdir(c, q, "arch", 0, eve, 0666, dp);
			return 1;
		}
		return -1;
	case Qconvdir:
		myc = cmd.conv[CONV(c->qid)];
		//jc = countrjob(myc->rjob);
		if (myc->rjob == nil){
			jc = -2;
		} else {
			jc = myc->rjob->rjobcount;
		}
		if (s < jc) {
			mkqid(&q, RQID(s, CONV(c->qid), QLconrdir), 0, QTDIR);
			snprint(up->genbuf, sizeof up->genbuf, "%d", s);
			devdir(c, q, up->genbuf, 0, eve, DMDIR | 0555, dp);
			return 1;
		}
		if (jc > 0)
			s -= jc;
		return cmd3gen(c, Qconvbase + s, dp);
	case Qdata:
	case Qstderr:
	case Qctl:
	case Qstatus:
	case Qwait:
	case Qstdin:
	case Qstdout:
	case Qenv:
	case Qns:
	case Qargs:
		return cmd3gen(c, TYPE(c->qid), dp);

	}
	return -1;
}

static void
cmdinit(void)
{
	cmd.maxconv = 1000;
	cmd.conv = mallocz(sizeof(Conv *) * (cmd.maxconv + 1), 1);

	/*
	 * looking up OS and PLATFORM types so that it wont have to be done
	 * everytime
	 */
	hostix = lookup(hosttype, oslist, Nos);
	platix = lookup(platform, platformlist, Nplatform);
}

static Chan *
cmdattach(char *spec)
{
	Chan *c;

	if (cmd.conv == nil)
		error(Enomem);
	c = devattach('T', spec);
	mkqid(&c->qid, QID(0, Qtopdir), 0, QTDIR);
	return c;
}

static Walkqid *
cmdwalk(Chan * c, Chan * nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, 0, 0, cmdgen);
}

static int
cmdstat(Chan * c, uchar * db, int n)
{
	return devstat(c, db, n, 0, 0, cmdgen);
}

static void
procreadremote(void *a)
{
	RemFile *rf;
	Block *content = nil;

	rf = a;
	qlock(&rf->l);
	rf->state = 1;				/* thread running */

	if (waserror()) {
		DPRINT(9,"PRR:Some error occured..\n");
		freeb(content);
		rf->state = 3;			/* thread dead */
		qunlock(&rf->l);
		pexit("", 0);
		return;
	}
	while (1) {
		DPRINT(9,"PRR: reading from [%s]\n", rf->cfile->name->s);
		content = devbread(rf->cfile, 1024, 0);
		if (BLEN(content) == 0) {
			/*
			 * check the status of this child, if it is done,
			 * then exit else read again
			 */
			break;
		}
		/* end if */
		/* Write data to queue */
		qbwrite(rf->asyncq, content);
	}					/* end while : infinite */

	DPRINT(9,"PRR[%s]:No more data to read\n", rf->cfile->name->s);
	freeb(content);
	rf->state = 2;				/* thread dead */
	poperror();
	qunlock(&rf->l);
	pexit("", 0);
	return;
}				/* end function : procreadremote () */

static void
spliceproc(void *param)
{
	Chan *src;
	Chan *dst;
	char buf[513];
	char *a;
	long r, ret;
	struct for_splice *fs;

	fs = (struct for_splice *) param;

	src = fs->src;
	dst = fs->dst;

	DPRINT(9,"spliceproc starting [%s]->[%s]\n", src->name->s,
		dst->name->s);
	for(;;) {
		/* read data from source channel */
		DPRINT(9, "spliceproc: READING %s", src->name->s);
		ret = devtab[src->type]->read(src, buf, 512, 0);
		DPRINT(9, "spliceproc: READ  %s ret %d %.*s", src->name->s, ret, ret, buf);
		if (ret <= 0)
			break;
		DPRINT(9,"spliceproc: copying [%s]->[%s] %ld data %.*s\n",
			src->name->s, dst->name->s, ret, ret, buf);
		for (a = buf; ret > 0; a += r) {
			DPRINT(9, "spliceproc: WRITING %s ret %d %.*s", dst->name->s, ret, ret, a);
			r = devtab[dst->type]->write(dst, a, ret, 0);
			DPRINT(9, "spliceproc: WROTE  %s r %d", dst->name->s, r);
			ret = ret - r;
		}
	}
	DPRINT(9,"spliceproc: closing [%s]->[%s] \n", src->name->s,
		dst->name->s);
	DPRINT(9,"spliceproc: %s %s complete\n", src->name->s,
		dst->name->s);	
	cclose(src);
	cclose(dst);

} /* end function : spliceproc */


static void
remoteopen(Chan * c, int omode, char *fname, int filetype, int resno)
{
	Conv *cv;
	int tmpfileuse;
	RemResrc *tmprr;
	RemFile *rf;
	char buf[KNAMELEN * 3];
	Chan *tmpchan;
	int i;

	cv = cmd.conv[CONV(c->qid)];
	wlock(&cv->rjob->l);
	tmpfileuse = cv->rjob->rcinuse[filetype];
	if (tmpfileuse < 1 ) {
		cv->rjob->rcinuse[filetype] = 1;
		tmpfileuse = 1;
	} else  {
		cv->rjob->rcinuse[filetype]++;
		tmpfileuse++;
	}
	wunlock(&cv->rjob->l);
	
	if (tmpfileuse == 1) {
		/* opening file for first time */
		/* open should be done on all resources */
		DPRINT(9,"opening file for first times[%d]\n",
			cv->rjob->rcinuse[filetype]);
		rlock(&cv->rjob->l);
		tmprr = cv->rjob->first;
		runlock(&cv->rjob->l);
		for (i = 0; i < resno; ++i) {
			snprint(buf, sizeof buf, "%s/%ld/%d/%s", base, CONV(c->qid), i, 
				fname);
			tmpchan = namec(buf, Aopen, omode, 0);

			wlock(&cv->rjob->l);
			rf = &tmprr->remotefiles[filetype];
			rf->cfile = tmpchan;

			if ((TYPE(c->qid) == Qdata)) {

				if (omode == OREAD || omode == ORDWR) {
					/* start the reader thread */
					DPRINT(9,"\nthread starting");
					//snprint(buf, sizeof buf, "PRR[%s]", c->name->s);
					kproc(buf, procreadremote, rf, 0);
				}
			}
			tmprr = tmprr->next;	/* this can be RO lock */
			wunlock(&cv->rjob->l);
		} /* end for : */
	} else {
		DPRINT(9,"file is already open for [%d] times\n", tmpfileuse);
	}
} /* end function : remoteopen */

/*
 * Opens file/dir It checks if specified channel is allowed to open, if yes,
 * it marks channel as open and returns it.
 */
static Chan *
cmdopen(Chan * c, int omode)
{
	int perm;
	Conv *cv;
	char *user;
	int tmpjc, filetype;
	char *fname;
	vlong stime;

	stime = osusectime (); /* recording start time */
	DPRINT(9,"cmdopen: open %s\n", c->name->s);
	perm = 0;
	omode = openmode(omode);
	switch (omode) {
	case OREAD:
		perm = 4;
		break;
	case OWRITE:
		perm = 2;
		break;
	case ORDWR:
		perm = 6;
		break;
	}

	switch (TYPE(c->qid)) {
	default:
		break;

	case Qtopctl:
		DPRINT(9, "cmdopen: opening ctl\n");
		break;

	case Qarch:
	case Qtopstat:
		if (omode != OREAD)
			error(Eperm);
		break;

	case Qtopns:
	case Qtopenv:
	case Qenv:
	case Qargs:
	case Qns:
	case Qstdin:
	case Qstdout:
		error(Egreg);			/* TODO: IMPLEMENT */
		break;

		/* Following directories are allowed only in read mode */
	case Qtopdir:
	case Qcmd:
	case Qconvdir:
	case QLconrdir:
		if (omode != OREAD)
			error(Eperm);
		break;

	case Qlocalfs:
	case Qlocalnet:
		error(Eperm);
		break;
	case Qclonus:
		/* in case of clone, check if it is locked */
		//DPRINT(1, "cmdopen: locking cmd.l\n");
		qlock(&cmd.l);
		if (waserror()) {
			DPRINT(9,"cmdopen: bye bye clone\n");
			//DPRINT(1, "cmdopen: err unlocking cmd.l\n");

			qunlock(&cmd.l);
			nexterror();
		}
		/* opening clone will come here */
		cv = cmdclone(up->env->user);
		poperror();
		//DPRINT(1, "cmdopen: unlocking cmd.l\n");
		qunlock(&cmd.l);
		if (cv == 0)
			error(Enodev);
		mkqid(&c->qid, QID(cv->x, Qctl), 0, QTFILE);
		DPRINT(9,"cmdopen: clone open successful [%s]\n", c->name->s);
		break;

	case Qstatus:
	case Qstderr:
	case Qwait:
		if (omode != OREAD)
			error(Eperm);

	case Qdata:
	case Qctl:
		//DPRINT(1, "cmdopen: locking cmd.l\n");

		qlock(&cmd.l);
		cv = cmd.conv[CONV(c->qid)];
		qlock(&cv->l);
		if (waserror()) {
			DPRINT(9,"cmdopen: bye byte ctl\n");
			qunlock(&cv->l);
			//DPRINT(1, "cmdopen: err unlocking cmd.l\n");

			qunlock(&cmd.l);
			nexterror();
		}
		user = up->env->user;
		if ((perm & (cv->perm >> 6)) != perm) {
			if (strcmp(user, cv->owner) != 0 || 
				(perm & cv->perm) != perm)
				error(Eperm);
		}
		cv->inuse++;
		DPRINT(9, "cmdopen: c %p inused %d", cv->inuse);
		if (cv->inuse == 1) {
			cv->state = "Open";
			kstrdup(&cv->owner, user);
			cv->perm = 0660;
			cv->nice = 0;
		}
		poperror();
		qunlock(&cv->l);
		//DPRINT(1, "cmdopen:  unlocking cmd.l\n");

		qunlock(&cmd.l);

		tmpjc = countrjob(cv->rjob);

		/* making sure that remote resources are allocated */
		if (TYPE(c->qid) != Qctl && TYPE(c->qid) != Qstatus) {
			if (tmpjc < 0) {
				DPRINT(9,"resources not reserved\n");
				error(ENOReservation);
			}
		}
		filetype = TYPE(c->qid) - Qdata;

		switch (TYPE(c->qid)) {
		case Qdata:
			fname = "stdio";
			if (omode == OWRITE || omode == ORDWR) {
				cv->count[0]++;
			}
			if (omode == OREAD || omode == ORDWR) {
				cv->count[1]++;
			}
			break;

		case Qstderr:
			fname = "stderr";
			if (omode != OREAD)
				error(Eperm);
			cv->count[2]++;
			break;

		case Qstatus:
			fname = "status";
			break;

		case Qwait:
			fname = nil;
			if (cv->waitq == nil)
				cv->waitq = qopen(1024, Qmsg, nil, 0);
			break;
		default:
			fname = nil;
		}

		/* do remote open when there are sub-sessions */
		if (fname != nil && tmpjc > 0) {
			if (TYPE(c->qid) == Qdata) {
				/* special case of */
				if (omode == OWRITE || omode == ORDWR) {
					/* open stdin part */
					DPRINT(9,"actually opening stdin for  [%s]\n", c->name->s);
					remoteopen(c, omode, fname, (Qstdin - Qdata), tmpjc);
				}
				if (omode == OREAD || omode == ORDWR) {
					DPRINT(9,"actually opening stdout for  [%s]\n", c->name->s);
					remoteopen(c, omode, fname, (Qstdout - Qdata), tmpjc);
				}
			} else {
				remoteopen(c, omode, fname, filetype, tmpjc);
			}
		} /* end if : fname != nil */
		break;
	}
	c->mode = omode;
	c->flag |= COPEN;
	c->offset = 0;

	DPRINT(8,"cmdopen: open for [%s] complete in %uld\n", c->name->s, timestamp(stime));
	return c;
}

static void
freeremoteresource(RemResrc * rr)
{
	int i;
	char *location;
	if (rr == nil)
		return;

	for (i = 0; i < Nremchan; ++i) {
		if (rr->remotefiles[i].cfile != nil) {
			location = fetchparentpath(
				rr->remotefiles[i].cfile->name->s);
			cclose(rr->remotefiles[i].cfile);
			rr->remotefiles[i].cfile = nil;
			rr->remotefiles[i].asyncq = nil;

			/* unbind ??  */

			DPRINT(9,"umounting [%s]\n", location);
			if (location !=  nil ) free(location);
//			location = nil;
		}
	}
}

static void
freeremotejobs(RemJob * rjob)
{
	long i;
	RemResrc *ppr, *tmp;
	if (rjob == nil)
		return;

	DPRINT(9,"releaseing [%lud] remote resources [%d]\n", rjob->rjobcount,
		rjob->x);

	wlock(&rjob->l);
	/* may be a read lock here */
	ppr = rjob->first;
	while (ppr != nil) {
		tmp = ppr;
		freeremoteresource(ppr);
		ppr = ppr->next;
		if (tmp != nil ) free(tmp);
//		tmp = nil;
	}

	for (i = 0; i < Nremchan; ++i) {
		qfree(rjob->asyncq_list[i]);
	}
	rjob->first = nil;
	rjob->last = nil;
	rjob->rjobcount = 0;
	wunlock(&rjob->l);
}

static void
closeconv(Conv * c)
{
	DPRINT(9,"releasing [%d] remote resource\n", c->x);
	kstrdup(&c->owner, "cmd");
	kstrdup(&c->dir, rootdir);
	c->perm = 0666;
	c->state = "Closed";
	c->killonclose = 0;
	c->killed = 0;
	c->nice = 0;
	if (c->rjob != nil) {
		freeremotejobs(c->rjob);
		free(c->rjob);
		c->rjob = nil;
	}
	DPRINT(9,"closeconv: remote freed\n");
	if (c->cmd != nil) {
		DPRINT(9,"closeconv: freeing cmd\n");
		free(c->cmd);			/* FIXME: why this fails???? */
		DPRINT(9,"closeconv: freeing cmd done\n");
	}
	c->cmd = nil;
	if (c->waitq != nil) {
		qfree(c->waitq);
		c->waitq = nil;
	}
	DPRINT(9,"closeconv: free waitq crossed\n");
	if (c->error != nil) {
		DPRINT(9,"closeconv: freeing error\n");
		free(c->error);
	}
	DPRINT(9,"closeconv: free error crossed\n");
	c->error = nil;
	DPRINT(9,"Conv freed\n");
	ccount--;

}

static void
remoteclose(Chan * c, int filetype, int rjcount)
{
	Conv *cc;
	RemResrc *tmprr;
	int i;
	int tmpfilec;

	DPRINT(9,"remote closing [%s]\n", c->name->s);
	cc = cmd.conv[CONV(c->qid)];

	tmpfilec = getfileopencount(cc->rjob, filetype);

	/* close ctl files only when everything else is closed */
	if (TYPE(c->qid) == Qctl && cc->inuse > 0) {
		return;
	}
	if (tmpfilec == 1) {
		/* its opened only once, so close it */
		/* closing remote resources */
		DPRINT(9,"closing it as only one open in cmd[%d]\n", cc->x);
		wlock(&cc->rjob->l);
		cc->rjob->rcinuse[filetype] = 0;
		tmprr = cc->rjob->first;	/* can be in read lock */
		wunlock(&cc->rjob->l);
		for (i = 0; i < rjcount; ++i) {
			if (tmprr->remotefiles[filetype].cfile != nil) {
				qlock(&tmprr->remotefiles[filetype].l);
				cclose(tmprr->remotefiles[filetype].cfile);
				qunlock(&tmprr->remotefiles[filetype].l);
				/* I am not freeing remotechan explicitly */

				wlock(&cc->rjob->l);
				tmprr->remotefiles[filetype].cfile = nil;
				tmprr = tmprr->next;	/* can be in read lock */
				wunlock(&cc->rjob->l);
			}
		} /* end for : */
	} else {
		DPRINT(9,"not closing [%s],accessed by others [%d]\n", 
			c->name->s, tmpfilec);
		wlock(&cc->rjob->l);
		cc->rjob->rcinuse[filetype] -= 1;
		wunlock(&cc->rjob->l);
	}
}

static void
cmdfdclose(Conv * c, int fd)
{
	DPRINT(3, "cmdfdclose:  1");
	DPRINT(3, "cmdfdclose: fd %d c->count[fd] %d c %p", fd, c->count[fd], c);
	DPRINT(3, "cmdfdclose:  1");
	if (--c->count[fd] == 0 && c->fd[fd] != -1) {
		DPRINT(9,"cmdfdclose: %s c %p\n", c->cmd->f[1], c);
		close(c->fd[fd]);
		c->fd[fd] = -1;
	}
	DPRINT(3, "cmdfdclose:  1");
}

static void
cmdclose(Chan * c)
{
	Conv *cc;
	int r;
	int filetype;
	int jc;
	vlong stime;

	stime = osusectime (); /* recording start time */
	if ((c->flag & COPEN) == 0)
		return;

	DPRINT(9,"cmdclose: trying to close file [%s]\n", c->name->s);
	switch (TYPE(c->qid)) {
	case Qarch:
	case Qtopstat:
	case Qtopctl:
		break;
	case Qctl:
	case Qdata:
		DPRINT(7, "cmdclose: closing %s\n", c->name->s);
	case Qstderr:
	case Qwait:
	case Qstatus:
		cc = cmd.conv[CONV(c->qid)];
		jc = countrjob(cc->rjob);
		filetype = TYPE(c->qid) - Qdata;
		DPRINT(9,"rjobcount is [%d]\n", jc);
		if (jc < 0) {
			/*
			 * FIXME: you can only close status file you cant
			 * open other files if job count is -1 and if you
			 * can't open them, you cant close them.
			 */
			DPRINT(9,"returning as rjobcount is [%d]\n", jc);
		}
		qlock(&cc->l);

		/*
		 * Only for data file: FIXME: I am not sure if I should do
		 * this for remote data files
		 */
		if (TYPE(c->qid) == Qdata) {
			
			if (c->mode == OWRITE || c->mode == ORDWR) {
				DPRINT(1, "cmdclose: locking cc->inlock cc %p", cc);
				qlock(&cc->inlock);
				cmdfdclose(cc, 0);
				DPRINT(1, "cmdclose: unlocking cc->inlock cc %p", cc);
				qunlock(&cc->inlock);
			}
			if (c->mode == OREAD || c->mode == ORDWR) {
				DPRINT(1, "cmdclose: locking 1 cc->outlock");
				qlock(&cc->outlock);
				DPRINT(1, "cmdclose: locking 2 cc->outlock");
				cmdfdclose(cc, 1);
				DPRINT(1, "cmdclose: unlocking 3 cc->outlock");
				qunlock(&cc->outlock);
			}
		} else if (TYPE(c->qid) == Qstderr) {
			cmdfdclose(cc, 2);
		}
		cc->inuse--;
		r = cc->inuse;
		DPRINT(9,"cmdclose: %s inuse %d\n", c->name->s, r);
		if (jc > 0) {
			/* close for remote resources also */
			if (TYPE(c->qid) == Qdata) {
				if (c->mode == OWRITE || c->mode == ORDWR) {
					DPRINT(9,"cmdclose: closing %s stdin", c->name->s);
					remoteclose(c, (Qstdin - Qdata), jc);
				}
				if (c->mode == OREAD || c->mode == ORDWR) {
					DPRINT(9,"cmdclose: closing %s stdout", c->name->s);
					remoteclose(c, (Qstdout - Qdata), jc);
				}
			}else
				remoteclose(c, filetype, jc);
		}
		/* This code is only for CTL file */
		if (cc->child != nil) {
			/* its local execution with child */
			if (!cc->killed)
				if (r == 0 || 
					(cc->killonclose && TYPE(c->qid) == Qctl)) {
					oscmdkill(cc->child);
					DPRINT(9,"cmdclose: killed child %s\n", cc->cmd->f[1]);
					cc->killed = 1;
				}
		}
		if (r == 0) {
			/*
			 * r is count of all open files not just ctl So
			 * resources will be released only when all open
			 * files are closed
			 */
			closeconv(cc);
		}
		qunlock(&cc->l);
		break;
	} /* end switch : per file type */
	DPRINT(8,"close on [%s] complete in %uld\n", c->name->s, timestamp(stime));
}

static long
readfromallasync(Chan * ch, void *a, long n, vlong offset)
{
	long ret, i;
	Conv *c;
	long jc;
	long nc,nb;
	int error_state;
	int filetype;
	RemResrc *rr;
	RemFile *rf;

	USED(offset);
	c = cmd.conv[CONV(ch->qid)];
	jc = countrjob(c->rjob);
	/* making sure that remote resources are allocated */
	if (jc < 0) {
		DPRINT(9,"conv %d resources not reserved\n", CONV(ch->qid));
		error(ENOReservation);
	}
	DPRINT(9,"readfromallasync cname [%s]\n", ch->name->s);
	if(TYPE(ch->qid) == Qdata)
		filetype = Qstdout - Qdata;
	else
		filetype = TYPE(ch->qid) - Qdata;
	for(;;) {
		rlock(&c->rjob->l);
		rr = c->rjob->first;
		runlock(&c->rjob->l);
		nc = 0;
		nb = 0;
		error_state = 0;
		for (i = 0; i < jc; i++) {
			rf = &rr->remotefiles[filetype];
			switch ( rf->state ) {
				case 3: 
					++error_state;
					break;
				case 2:
				/* This thread is done */
					nc++;
					break;
				default: 
					nb++;
				} /* end switch */
			/*
			 * FIXME: should I lock following also in read mode
			 * ??
			 */
			rr = rr->next;
		}
		ret = qconsume(c->rjob->asyncq_list[filetype], a, n);
		if (ret >= 0)
			return ret;
		
		if (error_state > 0 ) {
			DPRINT(9,"----readfromallasync remote errors [%ld]\n", error_state);
			error (ERemoteError);
		}
			
		if (nc >= jc) {
			DPRINT(9,"----readfromallasync fileclose [%ld]\n", nc);
			/* as all nodes are close, return NOW */
			return 0;
		}
	} 
}

static long
readfromall(Chan * ch, void *a, long n, vlong offset)
{
	void *c_a;
	vlong c_offset;
	long ret, c_ret, c_n, i;
	Conv *c;
	long tmpjc;
	int filetype;
	RemResrc *tmprr;

	USED(offset);
	c = cmd.conv[CONV(ch->qid)];

	tmpjc = countrjob(c->rjob);
	/* making sure that remote resources are allocated */
	if (tmpjc < 0) {
		DPRINT(9,"resources not reserved\n");
		error(ENOReservation);
	}
	DPRINT(9,"readfromall cname [%s]\n", ch->name->s);

	if (TYPE(ch->qid) == Qdata) {
		filetype = Qstdout - Qdata;
	} else {
		filetype = TYPE(ch->qid) - Qdata;
	}

	/* read from multiple remote files */
	c_a = a;
	c_n = n;
	c_offset = offset;
	ret = 0;

	rlock(&c->rjob->l);
	tmprr = c->rjob->first;
	runlock(&c->rjob->l);

	for (i = 0; i < tmpjc; ++i) {
		c_ret =
			devtab[tmprr->remotefiles[filetype].cfile->type]->
			read(tmprr->remotefiles[filetype].cfile, c_a, c_n, 
			c_offset);
		DPRINT(9,"%ldth read gave [%ld] data\n", i, c_ret);
		ret = ret + c_ret;
		c_a = (uchar *) c_a + c_ret;
		c_n = c_n - c_ret;
		if (c_n <= 0) {
			break;
		}
		/* FIXME: should I lock following also in read mode ??  */
		tmprr = tmprr->next;
	}
	DPRINT(9,"readfromall [%ld]\n", ret);
	return ret;
}

static long
cmdread(Chan * ch, void *a, long n, vlong offset)
{
	Conv *c;
	char *p, *cmds;
	long jc;
	long ret;
	int fd;
	vlong stime;

	stime = osusectime (); /* recording start time */
	DPRINT(9,"cmdread: %s n %d\n", ch->name->s, n);
	USED(offset);
	p = a;

	switch (TYPE(ch->qid)) {
	default:
		error(Eperm);

	case Qarch:
		snprint(up->genbuf, sizeof up->genbuf,  "%s %s\n", oslist[hostix], platformlist[platix]);
		ret = readstr(offset, p, n, up->genbuf);
		break;

	case Qtopstat:
		ret = readstatus(a, n, offset);
		break;

	case Qcmd:
	case Qtopdir:
	case Qconvdir:
	case QLconrdir:
		ret = devdirread(ch, a, n, 0, 0, cmdgen);
		DPRINT(9,"### devdirread returned  %ld\n", ret);
		break;

	case Qtopctl:
		snprint(up->genbuf, sizeof up->genbuf, "debug %d\n", vflag);
		ret = readstr(offset, p, n, up->genbuf);
		break;

	case Qctl:
		snprint(up->genbuf, sizeof up->genbuf,  "%ld", CONV(ch->qid));
		ret = readstr(offset, p, n, up->genbuf);
		break;

	case Qstatus:
		c = cmd.conv[CONV(ch->qid)];
		DPRINT(9,"came here 1 %d\n", c->x);
		jc = countrjob(c->rjob);
		if (jc > 0) {
			ret = readfromall(ch, a, n, offset);
			break;
		}
		if (jc < 0) {
			snprint(up->genbuf, sizeof(up->genbuf), "cmd/unreserved\n");
			ret = readstr(offset, p, n, up->genbuf);
			break;
		}
		DPRINT(9,"came here 2 [%s]\n", c->state);
		/* getting status locally */
		cmds = "";
		if (c->cmd != nil) {
			DPRINT(9,"came here 31 [%q]\n", c->dir);
			cmds = c->cmd->f[1];
			DPRINT(9,"came here 32 [%q]\n", c->dir);
			DPRINT(9,"came here 33 [%q]\n", cmds);
		}
		DPRINT(9,"came here 4\n");
		snprint(up->genbuf, sizeof(up->genbuf), "cmd/%d %d %s %q %q\n",
			c->x, c->inuse, c->state, c->dir, cmds);
		DPRINT(9,"came here 5\n");
		ret = readstr(offset, p, n, up->genbuf);
		break;

	case Qdata:
	case Qstderr:
		c = cmd.conv[CONV(ch->qid)];
		if (c->rjob == nil) {
			DPRINT(9,"conv %ulx read on released conv", CONV(ch->qid));
			error(EResourcesReleased);
		}
		jc = countrjob(c->rjob);
		if (jc < 0)
			error(ENOReservation);

		if (jc > 0) {
			if (TYPE(ch->qid) == Qdata) {
				ret = readfromallasync(ch, a, n, offset);
			} else {
				ret = readfromall(ch, a, n, offset);
			}
			break;
		}
		/* getting data locally */
		fd = 1;
		if (TYPE(ch->qid) == Qstderr)
			fd = 2;
		DPRINT(7, "READ fd %d n %d\n", c->fd[fd], n);
		c = cmd.conv[CONV(ch->qid)];
		DPRINT(1, "cmdread: locking c->outlock\n");
		qlock(&c->outlock);
		if (c->fd[fd] == -1) {
			/* FIXME: check if command is failed */
			//DPRINT(1, "cmdread: err unlocking c->outlock\n");
			qunlock(&c->outlock);
			DPRINT(9, "cmdread: bad cmd exec\n");
			error ("Local execution failed");
		}
		DPRINT(1, "cmdread: about to read local data\n");
		osenter();
		n = read(c->fd[fd], a, n);
		DPRINT(7, "cmdread: READ FINISHED fd %d n %d\n", c->fd[fd], n);
		osleave();
		//DPRINT(1, "cmdread:  unlocking c->outlock\n");
		qunlock(&c->outlock);
		
		if (n < 0)
			oserror();
		ret = n;
		break;

	case Qwait:
		c = cmd.conv[CONV(ch->qid)];
		if (c->rjob == nil) {
			DPRINT(9,"resources released, as clone file closed\n");
			error(EResourcesReleased);
		}
		jc = countrjob(c->rjob);
		if (jc < 0)
			error(ENOReservation);
		if (jc > 0) {
			/*
			 * FIXME: decide how exactly you want to implement
			 * this
			 */
			/* return readfromall (ch, a, n, offset); */
		}
		/* Doing it locally */
		c = cmd.conv[CONV(ch->qid)];
		ret = qread(c->waitq, a, n);
		break;
	}
	DPRINT(8,"cmdread: read %s ret %ld ts %uld %.*s\n", ch->name->s, ret, timestamp(stime), ret, a);
	return ret;
}

static int
cmdstarted(void *a)
{
	Conv *c;

	c = a;
	return c->child != nil || c->error != nil
		|| strcmp(c->state, "Execute") != 0;
}

enum {
	CTLDebug,
};

static
Cmdtab ctlcmdtab[] = {
	CTLDebug, "debug", 2,
};

enum {
	CMres,
	CMdir,
	CMsplice,
	CMexec,
	CMkill,
	CMnice,
	CMkillonclose
};

static
Cmdtab cmdtab[] = {
	CMres, "res", 0,
		CMdir, "dir", 2,
		CMsplice, "splice", 2,
		CMexec, "exec", 0,
		CMkill, "kill", 1,
		CMnice, "nice", 0,
		CMkillonclose, "killonclose", 0,
};

/*
 * Adds information of client status file into aggregation.
 */
static void
analyseremotenode(RemMount * rmt)
{
	char buf[128];
	char *a[3];
	char *p;
	long offset = 0;
	long ret;
	int n = 0;
	long hn, an;

	initinfo(rmt->info);
	for(;;) {
		ret = devtab[rmt->status->type]->read(rmt->status, buf, 30, offset);
		DPRINT(9, "remnode: n %d\n", n, n);
		if (ret <= 0)
			return;
		p = strchr(buf, '\n');
		if(p == nil)
			return;
		*p = 0;
		n = tokenize(buf, a, 3);
		if(n != 3)
			error(EBadstatus);
		hn = lookup(a[1], oslist, Nos);
		if(hn == 0)
			error(EBadstatus);
		an = lookup(a[2], platformlist, Nplatform);
		if(an == 0)
			error(EBadstatus);
		rmt->info[hn][an] += atol(a[0]);
	}
}

/*
 * verifies if specified location is indeed remote resource or not. and
 * returns RemMount to the exact remote resource.  It is callers
 * responsibility to free the memory of path returned
 */
static RemMount *
validrr(char *mnt, char *os, char *arch)
{
	Dir *dirs;
	Dir *d, *e;
	int hn, pn;
	long i, n;
	char *local = validdir;	/* for validation */
	RemMount *ans;
	char buf[KNAMELEN * 3];

	if (waserror()) {
		DPRINT(9,"can't verify %s: %r\n", mnt);
		if (dirs != nil) free(dirs);
		dirs = nil;
		nexterror();
	}
	n = lsdir(mnt, &dirs);
	if(n < 0)
		error("invalid dir");
	DPRINT(9,"remote dir[%s]= %ld entries\n", mnt, n);
	e = dirs+n;
	for(d = dirs; d != e; d++) {
		DPRINT(9,"%ld [%s/%s]\n", d - dirs, mnt, d->name);
		if (DMDIR & d->mode) {
			DPRINT(9,"checking for dir[%s]\n", d->name);
			if (strcmp(d->name, local) == 0)
				break;
			if (strcmp(d->name, parent) == 0) {
				DPRINT(9,"%s is parent dir, ignoring\n", d->name);
					break;
			}
		}
	}
	if (dirs != nil) free(dirs);
	dirs = nil;

	ans = (RemMount *) malloc(sizeof(RemMount));
	n = strlen(mnt)+strlen(local)+2;
	ans->path = (char *) malloc(n);
	snprint(ans->path, n,"%s/%s", mnt, local);

	if (d == e) {
		DPRINT(9,"validrr: [%s] not found, returning\n", local);
		if (ans->path != nil) free(ans->path);
		ans->path = nil;
		if (ans != nil )free(ans);
//		ans = nil;
		poperror();
		return nil;
	}
	DPRINT(9,"validrr: now  searching for clone file\n");

	n = lsdir(ans->path, &dirs);
	poperror();

	for (i = 0, d = dirs; i < n; i++, d++) {
		if (d->mode&DMDIR  || strcmp(d->name, "clone") == 0)
			continue;
		snprint(buf, sizeof buf, "%s/%s", ans->path, "status");
		ans->status = namec(buf, Aopen, OREAD, 0);
		if (os == nil || os[0] == '*')
			goto Found;	
		hn = lookup(os, oslist, Nos);
		if (hn == 0)
			goto Error;		
		analyseremotenode(ans);
		if (arch == nil || arch[0] == '*')
			for (i = 1; i < Nplatform; i++)
				if (ans->info[hn][i] > 0)
					goto Found;
		pn = lookup(arch, platformlist, Nplatform);
		if (pn == 0)
			goto Error;	/* unknown arch */
		if (ans->info[hn][pn] > 0) /* needed OS and arch type are present */
			goto Found;
		goto Error;
	}
Found:
	if (dirs != nil) free(dirs);
	dirs = nil;
	return ans;
Error:
	if (ans->path != nil) free(ans->path);
	ans->path = nil;
	if (ans != nil ) free(ans);
//	ans = nil;
	if (dirs !=  nil) free(dirs);
	dirs = nil;
	
	return nil;
}

/* find remote resource for usage */
static RemMount **
findrr(int *validrc, char *os, char *arch)
{
	char *path = remmnt;
	char file[KNAMELEN * 3];
	RemMount *rm;
	int n;
	Dir *dirs;
	Dir *d, *e;
	RemMount **allremotenodes;
	long rc;

	if (waserror()) {
		DPRINT(9,"%s: %r\n", remmnt);
		*validrc = 0;
		if (dirs != nil)free(dirs);
		dirs = nil;
		return nil;
	}
	n = lsdir(path, &dirs);
	poperror();
	allremotenodes = (RemMount **) malloc(sizeof(RemMount *) * (n + 1));
	rc = 0;
	allremotenodes[rc] = nil;

	DPRINT(9,"findrr: remote entries: %ld\n", n);
	e = dirs + n;
	for (d = dirs; d != e; d++) {
		snprint(file, sizeof file, "%s/%s", path, d->name);
		DPRINT(9,"findrr: remloc %s isdir %d\n", file, d->mode&DMDIR);
		/* entry should be directory and should not be "local" */
		if(!(d->mode&DMDIR) || strcmp(d->name, validdir) == 0 || strcmp(d->name, parent) == 0)
			continue;
		rm = validrr(file, os, arch);
		if(rm == nil)
			DPRINT(9, "findrr: invalid %s %s %s\n", file, os, arch);
		if(rm == nil)
			continue;
		allremotenodes[rc++] = rm;
		DPRINT(9,"findrr: resource [%s]\n", rm->path);
	}
	allremotenodes[rc] = nil;
	*validrc = rc;
	if (dirs != nil) free(dirs);
	dirs = nil;
	return allremotenodes;
}

/*
 * allocates single remote resource, can be executed in kproc
 */
static void
alloc1rr(void *info)
{
	char data[128];
	char report[16];
	int len, ret;
	RemResrc *rresrc;
	RemFile *rfile;
	int nj;
	int filetype;

	filetype = Qctl - Qdata;
	rresrc = (RemResrc *) info;
	nj = rresrc->subsess;
	rfile = &rresrc->remotefiles[filetype];

	report[0] = rfile->lsessid;
	report[1] = 0;				/* indicates failure */
	report[2] = 0;				/* indicate end of report */

	if (waserror()) {
		DPRINT(9,"alloc1rr: error sending res: %r\n");
		/* report failure */
		qwrite(rfile->asyncq, report, 2);
		pexit("", 0);
	}
	/* send reservation request */
	snprint(data, sizeof(data), "res %d", nj);
	len = strlen(data);

	ret = devtab[rfile->cfile->type]->write(rfile->cfile, data, len, 0);
	if (ret != len) {
		DPRINT(9,"alloc1rr: res write size(%d) returned [%d]: %r\n",
			len, ret);
		/* report failure */
		qwrite(rfile->asyncq, report, 2);
		pexit("", 0);
	}
	poperror();

	rfile->state = 1;
	rresrc->inuse = 1;

	/* report success */
	report[1] = 1;				/* indicates success */
	qwrite(rfile->asyncq, report, 2);
	DPRINT(9,"alloc1rr: done with thread [%d]\n", rfile->lsessid);
	pexit("", 0);
	return;
} /* end function : alloc1rr */

/* allocates memory and initialize the RemResrc */
RemResrc *
allocrr(RemJob * rjob, int subsess, int rrn)
{
	RemResrc *pp;
	RemFile *f;
	int i;
	pp = malloc(sizeof(RemResrc));
	pp->next = nil;
	for (i = 0; i < Nremchan; ++i) {
		f = &pp->remotefiles[i];
		f->cfile = nil;
		f->asyncq = rjob->asyncq_list[i];
		f->state = 0;
		f->lsessid = rrn;
	}
	pp->lsessid = rrn;
	pp->rsessid = -1;
	pp->subsess = subsess;
	pp->state = 0;
	return pp;
}

/* does multiple remote reservation in parallel */
int
parallelres(char *localpath, RemMount ** rnodelist, int validrc,
Conv * c, int nres, int first, int others)
{
	RemJob *rjob;
	RemResrc *rr;
	RemResrc *prev;
	char *selected;
	long jc;
	char buf[128];
	char lsesspath[KNAMELEN * 3];
	char remsesspath[KNAMELEN * 3];
	char rctlpath[KNAMELEN * 3];
	int i;
	int flag;
	int filetype;
	int ret;
	Block *b;

	rjob = c->rjob;
	/* error checking */
	if (rjob == nil) {
		DPRINT(9,"resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	DPRINT(9,"parallelres: %s %d %d %d\n", localpath, nres, first, others);

	/* I think these errors should be checked much before */
	jc = countrjob(rjob);
	if (jc == -2) {
		DPRINT(9,"parallelres: resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	if (jc != -1) {
		DPRINT(9,"parallelres: already in use\n");
		error(EResourcesINUse);
	}
	/* handle all failures */
	if (waserror()) {
		DPRINT(9,"parallelres: could not allocate needed rmt resources\n");
		rjob->rjobcount = -1;
		/* TODO: I should also release the RemResrc memory here */
		wunlock(&rjob->l);
		qunlock(&c->l);
		nexterror();
	}
	/* now doing real work */

	/* completely lock this session */
	qlock(&c->l);
	wlock(&rjob->l);

	/* initiate rjob */
	/* create asynce_queue_list */
	for (i = 0; i < Nremchan; ++i) {
		rjob->rcinuse[i] = 0;
		rjob->asyncq_list[i] = qopen(1024, 0, nil, 0);
	}

	/* allocate memory and initialize RemResrc */
	rr = allocrr(rjob, first, 0);
	rjob->first = rr;
	prev = rr;
	for (i = 1; i < nres; ++i) {
		rr = allocrr(rjob, others, i);
		prev->next = rr;
		prev = rr;
	}
	rjob->last = rr;

	/* set the remote job count */
	rjob->rjobcount = nres;

	filetype = Qctl - Qdata;
	/* clear any old notifications from queue */
	qflush(rjob->asyncq_list[filetype]);
	rr = rjob->first;
	for (i = 0; i < nres; ++i) {

		/* find which remote resource to use */
		if (rnodelist == nil) {
			selected = base;
		} else {
			lastrrselected = (lastrrselected + 1) % validrc;
			selected = rnodelist[lastrrselected]->path;
		}

		snprint(lsesspath, sizeof lsesspath, "%s/%d", localpath,
			rr->lsessid);
		snprint(rctlpath, sizeof rctlpath, "%s/clone", selected);

		DPRINT(9,"parallelres: creating session id [%s]\n", lsesspath);

		/*
		 * quickfix as calls like namec and kbind are not working
		 * from threads
		 */
		if (waserror()) {
			/* FIXME : following warning msg is incorrect */
			DPRINT(9,"parallelres: namec failed\n");
			nexterror();
		}
		DPRINT(9,"parallelres: namec %s\n", rctlpath);
		rr->remotefiles[filetype].cfile = namec(rctlpath, 
			Aopen, ORDWR, 0);
		DPRINT(9,"parallelres: namec successful\n");
		poperror();

		if (waserror()) {
			DPRINT(9,"parallelres: could not bind remote and local resource\n");
			error(ENOReservation);
		}
		b = devbread(rr->remotefiles[filetype].cfile, 10, 0);

		if (BLEN(b) == 0) {
			error(ENOReservation);
		}
		DPRINT(9,"parallelres: clone returned %ld data [%s]\n",
			BLEN(b), (char *) b->rp);

		b->rp[BLEN(b)] = 0;
		rr->rsessid = atol((char *) b->rp);

		/* creating remote session paths */
		snprint(remsesspath, sizeof(remsesspath), "%s/%d", selected,
			rr->rsessid);

		/* bind remote resource on local dir. */
		DPRINT(9,"parallelres: binding r[%s]->l[%s]\n", remsesspath,
			lsesspath);
		kbind(remsesspath, lsesspath, MREPL);
		poperror();

		freeb(b);

		/* create kprocs for each job */
		snprint(buf, sizeof(buf), "ctlProc_%d__", rr->lsessid);
		//alloc1rr(pp);
		kproc(buf, alloc1rr, rr, 0);
		rr = rr->next;
		DPRINT(9,"parallelres: allocated one session [%d]\n", i);
	} /* end for : for each session resource */

	/* ensure that resources are properly allocated */
	flag = 0;
	for (i = 0; i < nres; ++i) {
		ret = qread(rjob->asyncq_list[filetype], buf, 2);
		if (ret != 2) {
			++flag;
			DPRINT(9,"parallelres: qread on ctl buf failed\n");
		} else {
			if (buf[1] != 1) {
				++flag;
				DPRINT(9,"parallelres: resource allocation for [%d] failed\n", buf[0]);
			} else {
				DPRINT(9,"parallelres: resource allocation for [%d] successful\n", buf[0]);
			}
		}
	} /* end for : for each resource */

	/* mark ctl file in use */
	rjob->rcinuse[filetype] = 1;

	/* completely unlock this session */
	wunlock(&rjob->l);
	qunlock(&c->l);

	poperror();

	/* report failure */
	if (flag > 0) {
		DPRINT(9,"parallelres: resource allocation failed for [%d] times\n", flag);
		/* FIXME: release half allocated resources */
		error(ERemoteResFail);
	}
	return nres;
}


/* frees the list of remote mounts */
void
freermounts(RemMount ** allremotenodes, int validrc)
{
	int i;

	for (i = 0; i < validrc; ++i) {
		if (allremotenodes[i]->path != nil) free(allremotenodes[i]->path);
		allremotenodes[i]->path = nil;
		/* close the status channel */
		cclose(allremotenodes[i]->status);
		if (allremotenodes[i] != nil) free(allremotenodes[i]);
		allremotenodes[i] = nil;
	}
	if (allremotenodes !=  nil) free(allremotenodes);
//	allremotenodes = nil;
}


/* reserve the needed number of resources */
static long
groupres(Chan * ch, int resNo, char *os, char *arch)
{
	char *lpath;
	Conv *c;
	int first, others;
	RemMount **allremotenodes;
	int validrc;
	int hn, pn;

	c = cmd.conv[CONV(ch->qid)];

	if (c->rjob == nil) {
		DPRINT(9,"resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	lpath = fetchparentpathch(ch);

	validrc = 0;
	allremotenodes = findrr(&validrc, os, arch);
	if (validrc == 0 || allremotenodes == nil) {
		/* no remote resources */
		DPRINT(9,"no remote resources,reserving [%d]locally\n", resNo);

		if ((os != nil) && (os[0] != '*')) {
			hn = lookup(os, oslist, Nos);
			if (hn == 0) {
				/* unknown os */
				DPRINT(9,"Wrong OS type\n");
				error(Eperm);
			}
			if (hn != hostix) {
				/* Wrong os requested */
				DPRINT(9,"Local OS differs requested OS\n");
				error(ENoResourceMatch);
			}
		}
		/* end if : OS specified */
		/* We have proper OS, lets check for architecture */
		if ((arch != nil) && (arch[0] != '*')) {
			pn = lookup(arch, platformlist, Nplatform);
			if (pn == 0) {
				/* unknown platform */
				DPRINT(9,"Wrong platform type\n");
				error(Eperm);
			}
			if (pn != platix) {
				/* Wrong platform requested */
				DPRINT(9,"Local platform differs requested one\n");
				error(ENoResourceMatch);
			}
		}
		/* end if : platform specified */
		/* now, doing local reservation */
		first = others = 0;
		parallelres(lpath, nil, 0, c, resNo, first, others);
	} else {
		if (validrc >= resNo) {
			/* more than enough remote resources */
			DPRINT(9,"lot of rr[%d], every1 get 1\n", validrc);
			first = others = 0;
			parallelres(lpath, allremotenodes, validrc, c, resNo, first,
				others);
		} else {
			DPRINT(9,"not enough rr[%d], distribute\n", validrc);
			/* not enough remote resources */
			first = (resNo / validrc) + (resNo % validrc);
			others = resNo / validrc;
			parallelres(lpath, allremotenodes, validrc, c, 
				validrc, first, others);
		}	/* end else : not enough remote resources */
	}
	freermounts(allremotenodes, validrc);
	return resNo;
}

static void
initinfo(long info[Nos][Nplatform])
{
	int i, j;
	for (i = 0; i < Nos; ++i) {
		for (j = 0; j < Nplatform; ++j) {
			info[i][j] = 0;
		}
	}
}

/*
 * Looks up the word in the given list, returns zero if not found Note :
 * words are searched only from postion 1 ahead position zero is reserved for
 * "NOT MATCHED"
 */
static long
lookup(char *word, char **wordlist, int len)
{
	int i;
	for (i = 1; i < len; ++i) {
		if (strcmp(word, wordlist[i]) == 0)
			return i;
	}
	return 0;
}

/*
 * Adds information of client status file into aggregation.
 */
static void
addremotenode(long info[Nos][Nplatform], Chan * status)
{

	char buf[100];
	char buf2[100];
	long offset;
	long count, ret;
	int i, len, j, k;
	long hn, an;

	offset = 0;
	while (1) {
		/* read one line from Chan */
		ret = devtab[status->type]->read(status, buf, 30, offset);
		if (ret <= 0)
			return;
		for (i = 0; buf[i] != '\0' && buf[i] != '\n'; ++i);
		if (buf[i] == '\0')
			return;
		if (buf[i] == '\n') {
			buf[i] = '\0';
		}
		len = i;

		/* parse it */

		/* find number of nodes */
		for (j = 0, k = 0; buf[j] != ' '; ++j, ++k) {
			buf2[k] = buf[j];
		}
		buf2[k] = '\0';
		count = atol(buf2);

		/* find host type */
		for (++j, k = 0; buf[j] != ' '; ++j, ++k) {
			buf2[k] = buf[j];
		}
		buf2[k] = '\0';
		hn = lookup(buf2, oslist, Nos);

		/* find arch type */
		for (++j, k = 0; buf[j] != '\0'; ++j, ++k) {
			buf2[k] = buf[j];
		}
		buf2[k] = '\0';
		an = lookup(buf2, platformlist, Nplatform);

		/* update statistics */
		info[hn][an] += count;

		/* prepare for next line */
		offset += len + 1;
	} /* end while : infinite */
} /* end function : addRemoteNode */


/* reads top level status file */
static long
readstatus(char *a, long n, vlong offset)
{
	long i, j;
	int validrc;
	RemMount **allremotenodes;
	long info[Nos][Nplatform];
	char buf[(Nos + 1) * 30];
	char buf2[30];

	validrc = 0;
	allremotenodes = findrr(&validrc, nil, nil);

	if (validrc == 0) {			/* Leaf node */
		/* free up the memory allocated by findrr call */
		freermounts(allremotenodes, validrc);
		snprint(buf2,  sizeof(buf2), "%d %s %s\n", 1, oslist[hostix], platformlist[platix]);
		return readstr(offset, a, n, buf2);
	}
	initinfo(info);

	/* Add local type to info */
	info[hostix][platix] = 1;

	/* Add remote client stats to info */
	for (i = 0; i < validrc; ++i) {
		addremotenode(info, allremotenodes[i]->status);
	} /* end for : */

	/* Free up the memory allocated by findrr call */
	freermounts(allremotenodes, validrc);

	/* Prepare Buffer to return */
	buf[0] = '\0';
	for (i = 0; i < Nos; ++i) {
		for (j = 0; j < Nplatform; ++j) {
			if (info[i][j] > 0) {
				snprint(buf2, sizeof(buf2), "%ld %s %s\n", info[i][j], oslist[i],
					platformlist[j]);
				strcat(buf, buf2);
			}
		}
	} /* end for : */
	return readstr(offset, a, n, buf);
} /* end function : readstatus */

/*
 * function : p_send2one : will send data to one node but this function can
 * be executed parallelly in thread
 */
static void
p_send2one(void *param)
{
	long ret;
	struct parallel_send_wrap *psw;
	RemFile *rf;
	void *a;
	long n;
	vlong offset;
	char report[10];

	DPRINT(9,"###p_send2one: started\n");
	psw = (struct parallel_send_wrap *) param;
	rf = psw->rf;
	a = psw->p_a;
	n = psw->p_n;
	offset = psw->p_offset;
	if (psw != nil) free(psw);
//	psw = nil;

	report[0] = rf->lsessid;
	report[1] = 0;				/* indicates failure */
	report[2] = 0;				/* indicate end of report */

	if (waserror()) {
		DPRINT(9,"###p_send2one[%d]: error in sending write\n",
			rf->lsessid);
		/* report failure */
		qwrite(rf->asyncq, report, 2);
		pexit("", 0);
	}
	DPRINT(9,"###p_send2one: started\n");
	/* FIXME : lock the file to be used */

	DPRINT(9,"###p_send2one[%d]: about to write\n", rf->lsessid);

	/* do actual write */
	ret = devtab[rf->cfile->type]->write(rf->cfile, a, n, offset);

	DPRINT(9,"###p_send2one[%d]: write done\n", rf->lsessid);

	if (ret != n) {
		/* problem: all bytes are not written, report failure */
		DPRINT(9,"###p_send2one[%d]:wrot %ld instd of %ld bytes\n",
			rf->lsessid, ret, n);
		/* report failure */
		qwrite(rf->asyncq, report, 2);
		pexit("", 0);
	}
	poperror();

	/* report success */
	report[1] = 1;				/* indicates success */
	qwrite(rf->asyncq, report, 2);
	DPRINT(9,"###p_send2one[%d]:  success wrote %ld bytes\n",
		rf->lsessid, ret);
	pexit("", 0);
	return;
} /* end function : p_send2one */


/*
 * p_send2all : sends write command to all nodes parallelly 
 * 
 */
static long
p_send2all(Chan * ch, void *a, long n, vlong offset)
{
	int i, ret;
	Conv *c;
	long tmpjc;
	RemResrc *rr;
	int filetype;
	char buf[100];
	int flag;
	struct parallel_send_wrap *psw;

	c = cmd.conv[CONV(ch->qid)];

	/*
	 * sanity checks : making sure that this session is eligible for
	 * parallel send to all
	 */
	if (c->rjob == nil) {
		DPRINT(9,"resources released\n");
		error(EResourcesReleased);
	}
	tmpjc = countrjob(c->rjob);

	DPRINT(9,"write to [%s] repeating [%ld] times\n", ch->name->s, tmpjc);
	/* making sure that remote resources are allocated */
	if (tmpjc <= 0) {
		DPRINT(9,"resources not reserved\n");
		error(ENOReservation);
	}
	if (TYPE(ch->qid) == Qdata) {
		filetype = Qstdin - Qdata;
	} else {
		filetype = TYPE(ch->qid) - Qdata;
	}

	/* data should be sent to all resources */
	wlock(&c->rjob->l);
	rr = c->rjob->first;
	qflush(c->rjob->asyncq_list[filetype]);
	wunlock(&c->rjob->l);

	for (i = 0; i < tmpjc; ++i) {

		/* create argument to pass */
		psw = (struct parallel_send_wrap *)
			malloc(sizeof(struct parallel_send_wrap));
		psw->rf = &rr->remotefiles[filetype];
		psw->p_a = a;
		psw->p_n = n;
		psw->p_offset = offset;

		/* create kproc to do the write */
		snprint(buf, sizeof(buf), "send2allProc_%d_%d__", c->x, rr->lsessid);
		//p_send2one(psw);
		kproc(buf, p_send2one, psw, 0);

		/* FIXME: should I lock following also in read mode ??  */
		rr = rr->next;
	}

	DPRINT(9,"looping done anayling result\n");
	/* ensure that resources are properly allocated */

	/* FIXME: lock the filetype for remote resources */
	flag = 0;
	for (i = 0; i < tmpjc; ++i) {
		ret = qread(c->rjob->asyncq_list[filetype], buf, 2);
		if (ret != 2) {
			++flag;
			DPRINT(9,"qread failed\n");
		} else {
			if (buf[1] != 1) {
				++flag;
				DPRINT(9,"failue reported by [%d] thread\n", buf[0]);
			} else {
				DPRINT(9,"success reported by [%d] thread\n", buf[0]);
			}
		}
	} /* end for : for each resource */
	DPRINT(9,"analysis done, declaring\n");

	/* report failure */
	if (flag > 0) {
		DPRINT(9,"write failed for [%d] nodes\n", flag);
		error(ERemoteResFail);
	}
	DPRINT(9,"write to [%ld] remote nodes successful of size[%ld]\n", tmpjc, n);

	return n;
} /* end function : p_send2all */

/* Prepares given Conv for local execution */
static void
preparelocalexecution(Conv * c, char *os, char *arch)
{
	int i;
	int hn, pn;

	if (c->rjob == nil) {
		DPRINT(9,"resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	DPRINT(9,"doing local reservation\n");
	DPRINT(9,"os=[%s] arch=[%s]\n", os, arch);

	if ((os != nil) && (os[0] != '*')) {
		hn = lookup(os, oslist, Nos);
		if (hn == 0) {
			/* unknown os */
			DPRINT(9,"Wrong OS type\n");
			error(Eperm);
		}
		if (hn != hostix) {
			/* Wrong os requested */
			DPRINT(9,"Local OS differs requested OS\n");
			error(ENoResourceMatch);
		}
	}
	/* end if : OS specified */
	/* We have proper OS, lets check for architecture */
	if ((arch != nil) && (arch[0] != '*')) {
		pn = lookup(arch, platformlist, Nplatform);
		if (pn == 0) {
			/* unknown platform */
			DPRINT(9,"Wrong platform type\n");
			error(Eperm);
		}
		if (pn != platix) {
			/* Wrong platform requested */
			DPRINT(9,"Local platform differs requested one\n");
			error(ENoResourceMatch);
		}
	}
	/* end if : platform specified */
	/* Now we have proper OS and architecture */
	qlock(&c->l);
	wlock(&c->rjob->l);
	c->state = "Closed";
	for (i = 0; i < nelem(c->fd); i++)
		c->fd[i] = -1;
	c->rjob->rjobcount = 0;
	wunlock(&c->rjob->l);
	qunlock(&c->l);
} /* end function : preparelocalexecution  */

/* executes given command locally */
static void
dolocalexecution(Conv * c, Cmdbuf * cb)
{
	int i;

	if (c->rjob == nil) {
		DPRINT(9,"resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	qlock(&c->l);
	if (waserror()) {
		DPRINT(9,"so much for local execution\n");
		qunlock(&c->l);
		nexterror();
	}
	if (c->child != nil || c->cmd != nil) {
		DPRINT(9,"I like using things twice c->child %p c->cmd %p\n",
			c->child, c->cmd);
		error(Einuse);
	}
	for (i = 0; i < nelem(c->fd); i++)
		if (c->fd[i] != -1) {
			DPRINT(9,"not the channel!\n");
			error(Einuse);
		}
	if (cb->nf < 1) {
		DPRINT(9,"whoops I'm too small\n");
		error(Etoosmall);
	}

	/* cmdproc held back until unlock below */
	kproc("cmdproc", cmdproc, c, 0);
	if (c->cmd != nil) {
		free(c->cmd);
		c->cmd = nil;
	}
	c->cmd = cb; /* don't free cb */
	/* c->cmd  will be freed in closeconv call */
	c->state = "Execute";
	poperror();
	qunlock(&c->l);
	while (waserror())
		DPRINT(9,"I hate waserror");
	Sleep(&c->startr, cmdstarted, c);
	poperror();
	if (c->error)
		error(c->error);
}

static long
cmdwrite(Chan * ch, void *a, long n, vlong offset)
{
	int ret;
	long jc;
	Conv *c;
	Cmdbuf *cb;
	Cmdtab *ct;
	long resNo;
	char *os;
	char *arch;
	char *parent_path;
	char buf[512];
	struct for_splice *fs;
	char *s;
	vlong stime;

	stime = osusectime (); /* recording start time */
	USED(offset);
	DPRINT(9,"cmdwrite: write %s\n", ch->name->s);
	ret = n;				/* for default return value */

	switch (TYPE(ch->qid)) {
	default:
		error(Eperm);

	case Qtopctl:
		DPRINT(9, "cmdwrite: topctl\n");
		cb = parsecmd(a, n);
		DPRINT(9, "cmdwrite: cmd arg[%d] done [%s]\n", cb->nf, a);
		ct = lookupcmd(cb, ctlcmdtab, nelem(ctlcmdtab));
		switch (ct->index) {
		default:
			error(Eperm);
		case CTLDebug:
			/* two statements so that atlease one will be shown */
			DPRINT(9,"cmdwrite: setting debug to %d\n", atoi(cb->f[1]));
			vflag = atoi(cb->f[1]);
			DPRINT(9,"cmdwrite: setting debug to %d\n", vflag);
			break;
		}
		break;
	case Qctl:
		c = cmd.conv[CONV(ch->qid)];
		jc = countrjob(c->rjob);
		if (c->rjob == nil) {
			DPRINT(9,"cmdwrite: resources released, as clone file closed\n");
			error(EResourcesReleased);
		}
		s = (char *) a;
		s[n] = 0;
		cb = parsecmd(s, n);
		if (waserror()) {
			DPRINT(9,"cmdwrite: screw you cmd buffer!: %r\n");
			//free(cb);
			//FIXME:        something bad happening
			/* send the actual error code back */
			error("CTL request failed");
		}
		DPRINT(8,"cmdwrite: cmd arg[%d] done [%s]\n", cb->nf, a);
		ct = lookupcmd(cb, cmdtab, nelem(cmdtab));
		switch (ct->index) {
		case CMres:
			DPRINT(9,"cmdwrite: reserving resources %s \n", cb->f[1]);
			resNo = atol(cb->f[1]);
			DPRINT(9,"cmdwrite: reserving resources %s [%ld]\n", cb->f[1], resNo);

			if (resNo < 0) {
				/* negative value for reservation is invalid */
				DPRINT(9,"cmdwrite: err: negative error value: %r\n");
				error(Eperm);
			}
			if (jc != -1) {
				DPRINT(9,"cmdwrite: resource already in use\n");
				error(Einuse);
			}
			os = nil;
			arch = nil;
			if (cb->nf > 2) {
				os = cb->f[2];
			}
			if (cb->nf > 3) {
				arch = cb->f[3];
			}
			if (resNo == 0) {
				/* local execution request */
				preparelocalexecution(c, os, arch);
				break;
			}
			groupres(ch, resNo, os, arch);
			DPRINT(9,"cmdwrite: reservation done\n");
			/*
			  * unlock this conversation's inlocks and
			  * outlocks.  Otherwise when you try to make
			  * another reservation using the same
			  * conversation you'll deadlock.
			  */


			break;

		case CMexec:
			DPRINT(9,"cmdwrite: nremote res [%ld]\n", jc);
			if (jc < -1) {
				DPRINT(9,"cmdwrite: res failed\n");
				error(ENOReservation);
			}
			/* request for execution of command */
			if (jc == -1) {
				DPRINT(9,"cmdwrite: no reservation done\n");
				/* local execution request */
				preparelocalexecution(c, nil, nil);
				jc = 0;
				/* error(ENOReservation); */
			}
			if (jc == 0) {
				/* local execution request */
				DPRINT(9,"cmdwrite: executing cmd locally\n");
				dolocalexecution(c, cb);
			} else {
				DPRINT(9,"cmdwrite: executing command remotely\n");
				ret = p_send2all(ch, a, n, offset);
				DPRINT(9,"cmdwrite: remote command done\n");
			}
			c = cmd.conv[CONV(ch->qid)];
			qunlock(&c->inlock);
			qunlock(&c->outlock);
/*			if (canqlock(&c->inlock)) {
				DPRINT(1, "cmdwrite: unlocking c->inlock");
				qunlock(&c->inlock);
			}
			if (canqlock(&c->outlock)) {
				DPRINT(1, "cmdwrite: unlocking c->outlock");
				qunlock(&c->outlock);
			}
*/
			break;

		case CMkillonclose:
			if (jc < 0) {
				DPRINT(9,"cmdwrite: no reservation done\n");
				error(ENOReservation);
				break;
			}
			if (jc == 0) {
				/* local execution request */
				DPRINT(9,"cmdwrite: locals are kill on close\n");
				c->killonclose = 1;
				break;
			}
			ret = p_send2all(ch, a, n, offset);
			DPRINT(9,"cmdwrite: local killonclose done\n");
			break;

		case CMkill:
			if (jc < 0) {
				DPRINT(9,"cmdwrite: no reservation done\n");
				error(ENOReservation);
				break;
			}
			if (jc == 0) {
				/* local execution request */
				DPRINT(9,"cmdwrite: executing kill locally\n");
				qlock(&c->l);
				if (waserror()) {
					DPRINT(9,"cmdwrite: we aren't good killers: %r\n");
					qunlock(&c->l);
					nexterror();
				}
				if (c->child == nil)
					error("not started");
				if (oscmdkill(c->child) < 0)
					oserror();
				poperror();
				qunlock(&c->l);
				break;
			}
			ret = p_send2all(ch, a, n, offset);
			DPRINT(9,"cmdwrite: kill done\n");
			break;

		case CMsplice:
			if (jc < 0) {
				DPRINT(9,"cmdwrite: no reservation done\n");
				error(ENOReservation);
				break;
			}
			if (waserror()) {
				DPRINT(9,"cmdwrite: splice failed: \n");
				nexterror();
			}
			/* find the path to stdio file */
			parent_path = fetchparentpath(ch->name->s);
			snprint(buf, sizeof buf, "%s/stdio", parent_path);
			fs = (struct for_splice *) malloc(sizeof(struct for_splice));
			DPRINT(8,"cmdwrite: splice request opening [%s]\n", buf);
			fs->dst = namec(buf, Aopen, OWRITE, 0);
			DPRINT(8,"cmdwrite: splice request opening [%s]\n", cb->f[1]);
			fs->src = namec(cb->f[1], Aopen, OREAD, 0);
			DPRINT(8,"cmdwrite: splice request open done, going for proc\n");
			kproc("spliceproc", spliceproc, fs, 0);
			poperror();
			break;

		case CMdir:
			if (jc < 0) {
				DPRINT(9,"No reservation done\n");
				error(ENOReservation);
				break;
			}
			if (jc == 0) {
				/* local execution request */
				DPRINT(9,"cmdwrite: executing dir locally\n");
				kstrdup(&c->dir, cb->f[1]);
				break;
			}
			ret = p_send2all(ch, a, n, offset);
			DPRINT(9,"cmdwrite: dir done\n");
			break;

		default:
			DPRINT(9,"cmdwrite: error : wrong command type\n");
			error(Eunknown);
			break;

		} /* end switch : ctl file commands */

		poperror();
		//free(cb);
		break; /* end of write in ctl file */

	case Qdata:
		/* find no. of remote jobs running */
		c = cmd.conv[CONV(ch->qid)];
		jc = countrjob(c->rjob);

		if (c->rjob == nil) {
			DPRINT(9,"cmdwrite: resources already released\n");
			error(EResourcesReleased);
		}
		if (jc < 0) {
			DPRINT(9,"cmdwrite: no reservation\n");
			error(ENOReservation);
			break;
		}
		if (jc != 0) {
			DPRINT(7, "EVH: Remote Qdatawrite: [%d]\n", n);
			ret = p_send2all(ch, a, n, offset);
			break;
		}
		/* local data file write request */
		DPRINT(7, "EVH: Local Qdatawrite: [%d]\n", n );
		//DPRINT(1, "cmdwrite: lock c->inlock c %p", c);	
		qlock(&c->inlock);
		if (c->fd[0] == -1) {
			/* FIXME: check if command is failed */
			//DPRINT(1, "cmdwrite: err unlock c->inlock c %p", c);
			qunlock(&c->inlock);
			DPRINT(9, "cmd execution not proper\n");
			error ("Local execution failed");
		}
		
		osenter();
		ret = write(c->fd[0], a, n);
		osleave();
		//DPRINT(1, "cmdwrite: unlock c->inlock c %p", c);	
		qunlock(&c->inlock);
		
		DPRINT(8,"cmdwrite: WRITEFD %d ret %d n %d\n", c->fd[0], ret, n, n);
		break;
	}

	DPRINT(8,"cmdwrite: %s wrote %ld in %uld\n", ch->name->s, ret, timestamp(stime));
	return ret;
}

static int
cmdwstat(Chan * c, uchar * dp, int n)
{
	Dir *d;
	Conv *cv;

	switch (TYPE(c->qid)) {
	default:
		error(Eperm);
	case Qctl:
	case Qdata:
	case Qstderr:
		d = malloc(sizeof *d + n);
		if (d == nil)
			error(Enomem);
		if (waserror()) {
			DPRINT(9,"cmdwstat: error has errors!\n");
			if (d != nil) free(d);
//			d = nil;
			
			nexterror();
		}
		n = convM2D(dp, n, d, (char *) &d[1]);
		if (n == 0)
			error(Eshortstat);
		cv = cmd.conv[CONV(c->qid)];
		if (!iseve() && strcmp(up->env->user, cv->owner) != 0)
			error(Eperm);
		if (!emptystr(d->uid))
			kstrdup(&cv->owner, d->uid);
		if (d->mode != ~0UL)
			cv->perm = d->mode & 0777;
		poperror();
		if (d != nil) free(d);
//		d = nil;
		break;
	}
	return n;
}

static Conv *
cmdclone(char *user)
{
	Conv *c, **pp, **ep;
	int i;

	/* - end change */
	c = nil;
	ep = &cmd.conv[cmd.maxconv];
	for (pp = cmd.conv; pp < ep; pp++) {
		c = *pp;
		if (c == nil) {
			c = malloc(sizeof(Conv));
			if (c == nil)
				error(Enomem);
			qlock(&c->l);
			c->magic = CVmagic;
			c->inuse = 1;
			c->x = pp - cmd.conv;
			cmd.nc++;
			*pp = c;
			break;
		}
		if (canqlock(&c->l)) {
			if (c->inuse == 0 && c->child == nil)
				break;
			qunlock(&c->l);
		}
	}

	if (pp >= ep) {
		return nil;
	}
	/*
	 * FIXME: may be few of the fillowing things should be moved to "res
	 * 0 > ctl"
	 */
	c->inuse = 1;
	c->cmd = nil;
	c->child = nil;
	kstrdup(&c->owner, user);
	kstrdup(&c->dir, rootdir);
	c->perm = 0660;
	c->state = "Unreserved";
	for (i = 0; i < nelem(c->fd); i++)
		c->fd[i] = -1;

	/*
	 * allocating structure to store info about remote job resources
	 */
	c->rjob = malloc(sizeof(RemJob));
	wlock(&c->rjob->l);
	c->rjob->x = c->x;
	c->rjob->rjobcount = -1;
	c->rjob->first = nil;
	c->rjob->last = nil;
	ccount++;
	wunlock(&c->rjob->l);
	//DPRINT(1, "cmdclone: lock c->inlock c %p\n", c);
	canqlock(&c->inlock);
	qunlock(&c->inlock);
	qlock(&c->inlock);	/* lock writes to stdio until exec */
	//DPRINT(1, "cmdclone: lock c->outlock\n");
	canqlock(&c->outlock);
	qunlock(&c->outlock);
	qlock(&c->outlock);	/* lock reads from stdio until exec */
	qunlock(&c->l);
	DPRINT(9,"cmdclone: success\n");
	return c;
}

static void
cmdproc(void *a)
{
	Conv *c;
	int n;
	char status[ERRMAX];
	void *t;
	char root[] = "/";

	c = a;
	qlock(&c->l);
	DPRINT(9,"cmdproc: f[0]=%q f[1]=%q\n", c->cmd->f[0], c->cmd->f[1]);
	if (waserror()) {
		DPRINT(9,"cmdproc: failed: %q\n", up->env->errstr);
		kstrdup(&c->error, up->env->errstr);
		//c->state = "Done";
		c->state = "Fail";
		qunlock(&c->l);
		Wakeup(&c->startr);
		pexit("cmdproc", 0);
	}
	t = oscmd(c->cmd->f + 1, c->nice, root, c->fd);
	if (t == nil)
		oserror();
	c->child = t;				/* to allow oscmdkill */
	poperror();
	qunlock(&c->l);
	Wakeup(&c->startr);
	DPRINT(9,"started\n");
	while (waserror()) {
		DPRINT(9,"killin all the city of New Orleans, I'll have gone 5000 miles for the day is done\n");
		oscmdkill(t);
	}
	osenter();
	n = oscmdwait(t, status, sizeof status);
	osleave();
	if (n < 0) {
		oserrstr(up->genbuf, sizeof up->genbuf);
		n = snprint(status, sizeof status, "0 0 0 0 %q", up->genbuf);
	}
	qlock(&c->l);
	c->child = nil;
	oscmdfree(t);
	status[n] = 0;
	DPRINT(9,"done %d %d %d: %q\n", c->fd[0], c->fd[1], c->fd[2], status);
	if (c->inuse > 0) {
		c->state = "Done";
		if (c->waitq != nil)
			qproduce(c->waitq, status, n);
	} else
		closeconv(c);
	qunlock(&c->l);
	pexit("", 0);
}

Dev taskdevtab = {
	'T',
		"task",

		cmdinit,
		cmdattach,
		cmdwalk,
		cmdstat,
		cmdopen,
		devcreate,
		cmdclose,
		cmdread,
		devbread,
		cmdwrite,
		devbwrite,
		devremove,
		cmdwstat
};

/*************** UTILITY FUNCTIONS *************/
long
myunionread(Chan * c, void *va, long n)
{
	int i;
	long nr;
	long a_nr;
	void *a_va;
	long a_n;
	Mhead *m;
	Mount *mount;

	qlock(&c->umqlock);
	m = c->umh;
	rlock(&m->lock);
	mount = m->mount;

	DPRINT(9,"inside myunionread for [%s]\n", c->name->s);
	/* bring mount in sync with c->uri and c->umc */
	for (i = 0; mount != nil && i < c->uri; i++)
		mount = mount->next;

	nr = 0;
	a_va = va;
	a_n = n;
	a_nr = 0;
	while (mount != nil) {
		DPRINT(9,"myunionread looping  [%s]\n", c->name->s);
		/* Error causes component of union to be skipped */
		if (mount->to && !waserror()) {
			if (c->umc == nil) {
				c->umc = cclone(mount->to);
				c->umc = devtab[c->umc->type]->open(c->umc, OREAD);
			}
			DPRINT(9,"myuninrd reading [%s]\n", c->umc->name->s);
			nr = devtab[c->umc->type]->read(c->umc, a_va, a_n, c->umc->offset);
			if (nr < 0)
				nr = 0;			/* dev.c can return -1 */
			c->umc->offset += nr;
			poperror();
		}						/* end if */
		if (nr > 0) {
			DPRINT(9,"myunionread got something out[%s]\n",
				c->umc->name->s);
			/*
			 * break; *//* commenting so that it will read from
			 * all chans
			 */
			a_va = (char *) a_va + nr;
			a_n = a_n - nr;
			a_nr = a_nr + nr;
			if (a_n <= 0) {
				DPRINT(9,"myunionread Buffer full, breking");
				break;
			}
		}
		/* end if : nr > 0 */
		/* Advance to next element */
		c->uri++;
		if (c->umc) {
			cclose(c->umc);
			c->umc = nil;
		}
		mount = mount->next;
	}							/* end while */
	runlock(&m->lock);
	qunlock(&c->umqlock);
	return a_nr;
} /* end function : myunionread */


/*
 * Reads from specified channel. It works on both directory and file
 * channels.
 */
long
readfromchan(Chan * rc, void *va, long n, vlong off)
{
	int dir;

	if (waserror()) {
		DPRINT(9,"Error in readChan\n");
		nexterror();
	}
	if (n < 0)
		error(Etoosmall);

	dir = rc->qid.type & QTDIR;
	if (dir && rc->umh) {
		DPRINT(9,"going for unionread\n");
		n = myunionread(rc, va, n);
		DPRINT(9,"done from unionread\n");
	} else {
		DPRINT(9,"not going for unionread\n");
		n = devtab[rc->type]->read(rc, va, n, off);
	}

	poperror();
	return n;
}

/* copied this structure from other file */
enum {
	DIRSIZE = STATFIXLEN + 128 * 4,
	DIRREADLIM = 8048,
};

/* read from Chanel of type dir */
long
readchandir(Chan * dirChan, Dir ** d)
{
	uchar *buf;
	long ts, t2;

	*d = nil;
	if (waserror())
		return -1;
	buf = malloc(DIRREADLIM);
	if (buf == nil)
		error(Enomem);
	if (waserror()) {
		if (buf != nil) free(buf);
		buf = nil;
		nexterror();
	}

	ts = readfromchan(dirChan, buf, DIRREADLIM, 0);
	if (ts > 0) {
		t2 = dirpackage(buf, ts, d);
		ts = t2;
	}

	poperror();
	if (buf != nil) free(buf);
//	buf = nil;
	poperror();
	return ts;
}

/* Returns the list of Dir entries for given directory name */
long
lsdir(char *name, Dir ** d)
{
	int n;
	volatile struct {
		Chan *cc;
	} 
	rock;

	if (waserror()) {
		DPRINT(9,"can't open %s: %r\n", name);
		nexterror();
	}
	rock.cc = namec(name, Aopen, OREAD, 0);
	poperror();

	if (waserror()) {
		DPRINT(9,"readchandir failed\n");
		cclose(rock.cc);
		nexterror();
	}

	n = readchandir(rock.cc, d);
	DPRINT(9,"done readchandir\n");
	cclose(rock.cc);
	DPRINT(9,"freed cc\n");

	poperror();
	return n;
}

/*
 * returns string containing path to parent, and path will end with '/'
 * memory should be released by caller
 */
char *
fetchparentpathch(Chan * ch)
{
	char *ptr;
	char buf[KNAMELEN * 3];

	snprint(buf, sizeof buf, "%s/%ld/", base, CONV(ch->qid));
	ptr = malloc(sizeof buf + 2);
	if (ptr == nil) {
		return nil;
	}
	strncpy(ptr, buf, sizeof buf + 2);
	DPRINT(9,"filepath for qid[%llux] is [%s][%s]\n", ch->qid.path, buf,
		ptr);
	return ptr;
}

/*
 * returns string containing path to parent, and path will end with '/'
 * memory should be released by caller
 */
char *
fetchparentpath(char *filepath)
{
	char *ptr;
	int i, len;

	ptr = strdup(filepath);
	if (ptr == nil) {
		return nil;
	}
	len = strlen(ptr);

	/* ignoring last / in case of directory entry */
	if (ptr[len - 1] == '/') {
		ptr[len - 1] = 0;
		--len;
	}

	for (i = len - 1; ptr[i] != '/' && i > 0; --i) {
		ptr[i] = 0;
	}

	if (i == 0) {				/* not valid file */
		return nil;
	}

	return ptr;
}
