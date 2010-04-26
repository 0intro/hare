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
static int IHN = 0;
static int IAN = 0;

#ifdef ndef
/* I hate myself for this */
void *
mymalloc(size_t x)
{
	void *z;

	z = malloc(x);
	print("         mymalloc  0x%8.8lux 0x%8.8lux\n", z, getcallerpc(&x));
	return z;
}

void *
mymallocz(ulong x, int y)
{
	void *z;

	z = mallocz(x, y);
	print("         mymallocz 0x%8.8lux 0x%8.8lux\n", z, getcallerpc(&x));
	return z;
}

void
myfree(void *x)
{
	print("         myfree    0x%8.8lux 0x%8.8lux\n", x, getcallerpc(&x));
	free(x);
}

#define malloc(x) mymalloc(x)
#define mallocz(x,y) mymallocz(x,y)
#define free(x) myfree(x)
#endif

enum {
	Qtopdir,					/* top level directory */
	Qcmd,						/* "remote" dir */
	Qclonus,
	Qtopctl,
	Qlocalfs,					/* local fs mount point */
	Qlocalnet,					/* local net mount point */
	Qarch,						/* architecture description */
	Qtopns,						/* default ns for host */
	Qtopenv,					/* default env for host */
	Qtopstat,					/* Monitoring Information for host */
	Qconvdir,					/* resource dir */
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

	Debug = 0					/* to help debug os.c */
};

#define TYPE(x) 	 ( (ulong) (x).path & 0xff)
#define CONV(x) 	 ( ( (ulong) (x).path >> 8)&0xff)
#define RJOBI(x) 	 ( ( (ulong) (x).path >> 16)&0xff)
#define QID(c, y) 	 ( ( (c)<<8) | (y))
#define RQID(c, x, y) 	 ( ( ( (c)<< 16) | ( (x) << 8)) | (y))

#define RCHANCOUNT 10
#define BASE "/csrv/local/"
#define VALIDATEDIR "local"
#define REMOTEMOUNTPOINT "/csrv"
#define PARENTNAME "parent"

char ENoRemoteResources[] = "No remote resources available";
char ERemoteResFail[] = "remote reservation request failed";
char ENoResourceMatch[] = "No resources matching request";
char ENOReservation[] = "No remote reservation done";
char EResourcesReleased[] = "Resources already released";
char EResourcesINUse[] = "Resources already in use";

static int vflag = 0;		/* for debugging messages: control prints */
				/* right now everything set to 9 */
void
_dprint(ulong debuglevel, char *fmt, ...)
{
	va_list args;
	int len = 0;
	char s[255];

	if(debuglevel<vflag)
		return;

	va_start(args, fmt);
	len += vsnprint(s + len, sizeof s - len, fmt, args);
	va_end(args);

	print(s, len);
}
#define DPRINT if(vflag)_dprint

long lastrrselected = 0;

struct RemoteFile {
	int state;
	int local_session_id;
	Chan *cfile;
	Queue *async_queue;
};
typedef struct RemoteFile RemoteFile;

struct RemoteResource {
	int local_session_id;
	int remote_session_id;
	int state;
	int inuse;
	int sub_sessions;
	/* List of all files in remote resource directory */
	RemoteFile remotefiles[RCHANCOUNT];
	/* Next remote resource */
	struct RemoteResource *next;
};
typedef struct RemoteResource RemoteResource;

struct RemoteJob {
	long rjobcount;				/* number of jobs in current session */
	int x;
	RWlock l;					/* protects state changes */
	int rcinuse[RCHANCOUNT];
	RWlock perfilelock[RCHANCOUNT];
	Queue *async_queue_list[RCHANCOUNT];
	RemoteResource *first;
	RemoteResource *last;
};
typedef struct RemoteJob RemoteJob;

#define PLATFORMCOUNT 7
#define OSCOUNT 9

/* list of OS and platforms supported */
char *oslist[OSCOUNT] = { "Other", "Hp", "Inferno", "Irix", "Linux",
	"MacOSX", "Nt", "Plan9", "Solaris"
};

char *platformlist[PLATFORMCOUNT] = { "Other", "386", "arm", "mips",
	"power", "s800", "sparc"
};

struct remoteMount {
	char *path;
	Chan *status;
	long info[OSCOUNT][PLATFORMCOUNT];
};

typedef struct remoteMount remoteMount;

typedef struct Conv Conv;
struct Conv {
	int x;
	int inuse;
	int fd[3];					/* stdin, stdout, and stderr */
	int count[3];				/* number of readers on
								 * stdin/stdout/stderr */
	int perm;
	char *owner;
	char *state;
	Cmdbuf *cmd;
	char *dir;
	QLock l;					/* protects state changes */
	Queue *waitq;
	void *child;
	char *error;				/* on start up */
	int nice;
	short killonclose;
	short killed;
	Rendez startr;
	RemoteJob *rjob;			/* path to researved remote resource */
};

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
	RemoteFile *rf;
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
static void initinfo(long info[OSCOUNT][PLATFORMCOUNT]);
static long readstatus(char *a, long n, vlong offset);
static void proc_splice(void *);

/* function prototypes from other files */
long dirpackage(uchar * buff, long ts, Dir ** d);

static long
getrjobcount(RemoteJob * rjob)
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
getfileopencount(RemoteJob * rjob, int filetype)
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
cmdgen(Chan * c, char *name, Dirtab * d, int nd, int s, Dir * dp)
{
	Qid q;
	Conv *cv;
	Conv *myc;
	char tmpbuff[KNAMELEN * 3];
	long tmpjc;

	USED(name);
	USED(nd);
	USED(d);

	if (s == DEVDOTDOT) {
		switch (TYPE(c->qid)) {
		case Qtopdir:
		case Qcmd:
			mkqid(&q, QID(0, Qtopdir), 0, QTDIR);
			devdir(c, q, "#T", 0, eve, DMDIR | 0555, dp);
			break;
		case Qconvdir:
			mkqid(&q, QID(0, Qcmd), 0, QTDIR);
			//devdir(c, q, "local", 0, eve, DMDIR | 0555, dp);
			devdir(c, q, "local", 0, up->env->user, DMDIR | 0555, dp);
			break;
		case QLconrdir:		/* for remote resource bindings */
			DPRINT(9,"cmdindex [%ld], rjobi [%ld]\n",
					  CONV(c->qid), RJOBI(c->qid));

			sprint(tmpbuff, "%ld", CONV(c->qid));
			cv = cmd.conv[CONV(c->qid)];

			mkqid(&q, QID(CONV(c->qid), Qconvdir), 0, QTDIR);
			devdir(c, q, tmpbuff, 0, cv->owner, DMDIR | 0555, dp);
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
			sprint(up->genbuf, "%d", s);
			devdir(c, q, up->genbuf, 0, cv->owner, DMDIR | 0555, dp);
			return 1;
		}
		s -= cmd.nc;
		if (s == 0) {
			mkqid(&q, QID(0, Qclonus), 0, QTFILE);
			devdir(c, q, "clone", 0, eve, 0666, dp);
			return 1;
		}
		if (s == 1) {
			mkqid(&q, QID(0, Qtopctl), 0, QTFILE);
			//devdir(c, q, "ctl", 0, eve, 0777, dp);
			devdir(c, q, "ctl", 0, up->env->user, 0777, dp);
			return 1;
		}
		if (s == 2) {
			mkqid(&q, QID(0, Qlocalfs), 0, QTDIR);
			devdir(c, q, "fs", 0, eve, DMDIR | 0555, dp);
			return 1;
		}
		if (s == 3) {
			mkqid(&q, QID(0, Qlocalnet), 0, QTDIR);
			devdir(c, q, "net", 0, eve, DMDIR | 0555, dp);
			return 1;
		}
		if (s == 4) {
			mkqid(&q, QID(0, Qarch), 0, QTFILE);
			devdir(c, q, "arch", 0, eve, 0666, dp);
			return 1;
		}
		if (s == 5) {
			mkqid(&q, QID(0, Qtopns), 0, QTDIR);
			devdir(c, q, "ns", 0, eve, 0666, dp);
			return 1;
		}
		if (s == 6) {
			mkqid(&q, QID(0, Qtopenv), 0, QTDIR);
			devdir(c, q, "env", 0, eve, 0666, dp);
			return 1;
		}
		if (s == 7) {
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
		//tmpjc = getrjobcount(myc->rjob);
		tmpjc = myc->rjob->rjobcount;
		if (s < tmpjc) {
			mkqid(&q, RQID(s, CONV(c->qid), QLconrdir), 0, QTDIR);
			sprint(up->genbuf, "%d", s);
			devdir(c, q, up->genbuf, 0, eve, DMDIR | 0555, dp);
			return 1;
		}
		if (tmpjc > 0)
			s -= tmpjc;

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
	IHN = lookup(hosttype, oslist, OSCOUNT);
	IAN = lookup(platform, platformlist, PLATFORMCOUNT);
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
	RemoteFile *rf;
	Block *content = nil;

	rf = a;
	rf->state = 1;				/* thread running */

	if (waserror()) {
		DPRINT(9,"PRR:No more data to read\n");
		freeb(content);
		rf->state = 2;			/* thread dead */
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
		qbwrite(rf->async_queue, content);
	}							/* end while : infinite */

	DPRINT(9,"PRR[%s]:No more data to read\n", rf->cfile->name->s);
	freeb(content);
	rf->state = 2;				/* thread dead */
	poperror();
	pexit("", 0);
	return;
}								/* end function : procreadremote () */

static void
proc_splice(void *param)
{
	Chan *src;
	Chan *dst;
	char buff[513];
	char *a;
	long r, ret;
	struct for_splice *fs;

	fs = (struct for_splice *) param;

	src = fs->src;
	dst = fs->dst;

	DPRINT(9,"proc_splice starting [%s]->[%s]\n", src->name->s,
			  dst->name->s);
	while (1) {
		/* read data from source channel */
		ret = devtab[src->type]->read(src, buff, 512, 0);
		if (ret <= 0) {
			break;
		}
		DPRINT(9,"proc_splice copying [%s]->[%s] %ld data \n",
				  src->name->s, dst->name->s, ret);
		a = buff;
		while (ret > 0) {
			r = devtab[dst->type]->write(dst, a, ret, 0);
			ret = ret - r;
			a = a + r;
		}

	}							/* end while */
	DPRINT(9,"proc_splice closing [%s]->[%s] \n", src->name->s,
			  dst->name->s);

	cclose(src);
	cclose(dst);
	DPRINT(9,"proc_splice complete \n");
}								/* end function : proc_splice */


static void
remoteopen(Chan * c, int omode, char *fname, int filetype, int resno)
{
	Conv *cv;
	int tmpfileuse;
	RemoteResource *tmprr;
	RemoteFile *rf;
	char buff[KNAMELEN * 3];
	Chan *tmpchan;
	int i;

	cv = cmd.conv[CONV(c->qid)];

	tmpfileuse = getfileopencount(cv->rjob, filetype);

	if (tmpfileuse < 1) {
		/* opening file for first time */
		/* open should be done on all resources */
		DPRINT(9,"opening file for first times[%d]\n",
				  cv->rjob->rcinuse[filetype]);
		rlock(&cv->rjob->l);
		tmprr = cv->rjob->first;
		runlock(&cv->rjob->l);
		for (i = 0; i < resno; ++i) {
			sprint(buff, "%s/%ld/%d/%s", BASE, CONV(c->qid), i, fname);
			tmpchan = namec(buff, Aopen, omode, 0);

			wlock(&cv->rjob->l);
			rf = &tmprr->remotefiles[filetype];
			rf->cfile = tmpchan;

			if ((TYPE(c->qid) == Qdata)) {

				if (omode == OREAD || omode == ORDWR) {
					/* start the reader thread */
					DPRINT(9,"\nthread starting");
					//sprint(buff, "PRR[%s]", c->name->s);
					kproc(buff, procreadremote, rf, 0);
				}
			}
			tmprr = tmprr->next;	/* this can be RO lock */
			wunlock(&cv->rjob->l);
		}						/* end for : */
		wlock(&cv->rjob->l);
		cv->rjob->rcinuse[filetype] = 1;
		wunlock(&cv->rjob->l);
	} else {
		DPRINT(9,"file is already open for [%d] times\n", tmpfileuse);
		wlock(&cv->rjob->l);
		cv->rjob->rcinuse[filetype] += 1;
		wunlock(&cv->rjob->l);
	}
}								/* end function : remoteopen */

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

	DPRINT(9,"trying to open [%s]\n", c->name->s);
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
		print("opening ctl file\n");
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
		qlock(&cmd.l);
		if (waserror()) {
			DPRINT(9,"bye bye clone\n");
			qunlock(&cmd.l);
			nexterror();
		}
		/* opening clone will come here */
		cv = cmdclone(up->env->user);
		poperror();
		qunlock(&cmd.l);
		if (cv == 0)
			error(Enodev);
		mkqid(&c->qid, QID(cv->x, Qctl), 0, QTFILE);
		DPRINT(9,"clone open successful [%s]\n", c->name->s);
		break;

	case Qstatus:
	case Qstderr:
	case Qwait:
		if (omode != OREAD)
			error(Eperm);

	case Qdata:
	case Qctl:

		qlock(&cmd.l);
		cv = cmd.conv[CONV(c->qid)];
		qlock(&cv->l);
		if (waserror()) {
			DPRINT(9,"bye byte ctl\n");
			qunlock(&cv->l);
			qunlock(&cmd.l);
			nexterror();
		}
		user = up->env->user;
		if ((perm & (cv->perm >> 6)) != perm) {
			if (strcmp(user, cv->owner) != 0 || (perm & cv->perm) != perm)
				error(Eperm);
		}
		cv->inuse++;
		if (cv->inuse == 1) {
			cv->state = "Open";
			kstrdup(&cv->owner, user);
			cv->perm = 0660;
			cv->nice = 0;
		}
		poperror();
		qunlock(&cv->l);
		qunlock(&cmd.l);

		tmpjc = getrjobcount(cv->rjob);

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
					remoteopen(c, omode, fname, (Qstdin - Qdata), tmpjc);
				}
				if (omode == OREAD || omode == ORDWR) {
					remoteopen(c, omode, fname, (Qstdout - Qdata), tmpjc);
				}
			} else {
				remoteopen(c, omode, fname, filetype, tmpjc);
			}
		}						/* end if : fname != nil */
		break;
	}							/* end switch */
	c->mode = omode;
	c->flag |= COPEN;
	c->offset = 0;

	DPRINT(9,"open for [%s] complete\n", c->name->s);
	return c;
}								/* end function : cmdopen */

static void
freeremoteresource(RemoteResource * rr)
{
	int i;
	char *location;
	if (rr == nil)
		return;

	for (i = 0; i < RCHANCOUNT; ++i) {
		if (rr->remotefiles[i].cfile != nil) {
			location = fetchparentpath(rr->remotefiles[i].cfile->name->s);
			cclose(rr->remotefiles[i].cfile);
			rr->remotefiles[i].cfile = nil;
			rr->remotefiles[i].async_queue = nil;

			/* unbind ??  */

			DPRINT(9,"umounting [%s]\n", location);
			free(location);
		}
	}
}

static void
freeremotejobs(RemoteJob * rjob)
{
	long i;
	RemoteResource *ppr, *tmp;
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
		free(tmp);
	}

	for (i = 0; i < RCHANCOUNT; ++i) {
		qfree(rjob->async_queue_list[i]);
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
	DPRINT(9,"remote resources released\n");
	if (c->cmd != nil) {
		DPRINT(9,"freeing cmd\n");
		free(c->cmd);			/* FIXME: why this fails???? */
		DPRINT(9,"freeing cmd done\n");
	}
	c->cmd = nil;
	if (c->waitq != nil) {
		qfree(c->waitq);
		c->waitq = nil;
	}
	DPRINT(9,"free waitq crossed\n");
	if (c->error != nil) {
		DPRINT(9,"freeing error\n");
		free(c->error);
	}
	DPRINT(9,"free error crossed\n");
	c->error = nil;
	DPRINT(9,"Conv freed\n");
	--ccount;

}

static void
remoteclose(Chan * c, int filetype, int rjcount)
{
	Conv *cc;
	RemoteResource *tmprr;
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
				cclose(tmprr->remotefiles[filetype].cfile);
				/* I am not freeing remotechan explicitly */

				wlock(&cc->rjob->l);
				tmprr->remotefiles[filetype].cfile = nil;
				tmprr = tmprr->next;	/* can be in read lock */
				wunlock(&cc->rjob->l);
			}
		}						/* end for : */
	} else {
		DPRINT(9,"not closing [%s],accessed by others [%d]\n", c->name->s,
				  tmpfilec);
		wlock(&cc->rjob->l);
		cc->rjob->rcinuse[filetype] -= 1;
		wunlock(&cc->rjob->l);
	}
}

static void
cmdfdclose(Conv * c, int fd)
{
	if (--c->count[fd] == 0 && c->fd[fd] != -1) {
		DPRINT(9,"calling close in cmdfdclose\n");
		close(c->fd[fd]);
		c->fd[fd] = -1;
	}
}

static void
cmdclose(Chan * c)
{
	Conv *cc;
	int r;
	int filetype;
	int tmpjc;

	if ((c->flag & COPEN) == 0)
		return;

	DPRINT(9,"trying to close \n");
	DPRINT(9,"trying to close file [%s]\n", c->name->s);
	switch (TYPE(c->qid)) {

	case Qarch:
	case Qtopstat:
	case Qtopctl:
		break;

	case Qctl:
	case Qdata:
	case Qstderr:
	case Qwait:
	case Qstatus:
		cc = cmd.conv[CONV(c->qid)];
		tmpjc = getrjobcount(cc->rjob);
		filetype = TYPE(c->qid) - Qdata;

		DPRINT(9,"rjobcount is [%d]\n", tmpjc);
		if (tmpjc < 0) {
			/*
			 * FIXME: you can only close status file you cant
			 * open other files if job count is -1 and if you
			 * can't open them, you cant close them.
			 */
			DPRINT(9,"returning as rjobcount is [%d]\n", tmpjc);
		}
		qlock(&cc->l);

		/*
		 * Only for data file: FIXME: I am not sure if I should do
		 * this for remote data files
		 */
		if (TYPE(c->qid) == Qdata) {
			if (c->mode == OWRITE || c->mode == ORDWR)
				cmdfdclose(cc, 0);
			if (c->mode == OREAD || c->mode == ORDWR)
				cmdfdclose(cc, 1);
		} else if (TYPE(c->qid) == Qstderr) {
			cmdfdclose(cc, 2);
		}
		r = --cc->inuse;
		DPRINT(9,"r value is [%d] for  [%s]\n", r, c->name->s);

		if (tmpjc > 0) {
			/* close for remote resources also */

			if (TYPE(c->qid) == Qdata) {
				if (c->mode == OWRITE || c->mode == ORDWR) {
					remoteclose(c, (Qstdin - Qdata), tmpjc);
				}
				if (c->mode == OREAD || c->mode == ORDWR) {
					remoteclose(c, (Qstdout - Qdata), tmpjc);
				}
			} else {
				remoteclose(c, filetype, tmpjc);
			}
		}
		/* This code is only for CTL file */
		if (cc->child != nil) {
			/* its local execution with child */
			if (!(cc->killed))
				if (r == 0 || (cc->killonclose && TYPE(c->qid) == Qctl)) {
					oscmdkill(cc->child);

					DPRINT(9,"killing clild\n");
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
	}							/* end switch : per file type */
	DPRINT(9,"close complete\n");
}

static long
readfromallasync(Chan * ch, void *a, long n, vlong offset)
{
	long ret, i;
	Conv *c;
	long tmpjc;
	long ccount, bcount;
	int filetype;
	RemoteResource *tmprr;
	RemoteFile *rf;

	USED(offset);
	c = cmd.conv[CONV(ch->qid)];

	tmpjc = getrjobcount(c->rjob);
	/* making sure that remote resources are allocated */
	if (tmpjc < 0) {
		DPRINT(9,"resources not reserved\n");
		error(ENOReservation);
	}
	DPRINT(9,"readfromallasync cname [%s]\n", ch->name->s);

	if (TYPE(ch->qid) == Qdata) {
		filetype = Qstdout - Qdata;
	} else {
		filetype = TYPE(ch->qid) - Qdata;
	}

	while (1) {
		rlock(&c->rjob->l);
		tmprr = c->rjob->first;
		runlock(&c->rjob->l);

		ccount = 0;
		bcount = 0;
		for (i = 0; i < tmpjc; ++i) {
			rf = &tmprr->remotefiles[filetype];
			if (rf->state == 2) {
				/* This thread is done */
				++ccount;
			} else {
				++bcount;
			}
			/*
			 * FIXME: should I lock following also in read mode
			 * ??
			 */
			tmprr = tmprr->next;
		}

		ret = qconsume(c->rjob->async_queue_list[filetype], a, n);
		if (ret >= 0) {
			return ret;
		}
		if (ccount >= tmpjc) {
			DPRINT(9,"----readfromallasync fileclose [%ld]\n", ccount);
			/* as all nodes are close, return NOW */
			return 0;
		}
	}							/* end while : infinite */
}								/* end function : readfromallasync */

static long
readfromall(Chan * ch, void *a, long n, vlong offset)
{
	void *c_a;
	vlong c_offset;
	long ret, c_ret, c_n, i;
	Conv *c;
	long tmpjc;
	int filetype;
	RemoteResource *tmprr;

	USED(offset);
	c = cmd.conv[CONV(ch->qid)];

	tmpjc = getrjobcount(c->rjob);
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
	long tmpjc;
	long ret;
	int fd;

	DPRINT(9,"reading from [%s]\n", ch->name->s);
	USED(offset);
	p = a;

	switch (TYPE(ch->qid)) {
	default:
		error(Eperm);

	case Qarch:
		sprint(up->genbuf, "%s %s\n", oslist[IHN], platformlist[IAN]);
		return readstr(offset, p, n, up->genbuf);

	case Qtopstat:
		return readstatus(a, n, offset);

	case Qcmd:
	case Qtopdir:
	case Qconvdir:
	case QLconrdir:
		ret = devdirread(ch, a, n, 0, 0, cmdgen);
		DPRINT(9,"### devdirread returned  %ld\n", ret);
		return ret;

	case Qtopctl:
		sprint(up->genbuf, "debug %d\n", vflag);
		return readstr(offset, p, n, up->genbuf);

	case Qctl:
		sprint(up->genbuf, "%ld", CONV(ch->qid));
		return readstr(offset, p, n, up->genbuf);

	case Qstatus:
		c = cmd.conv[CONV(ch->qid)];
		DPRINT(9,"came here 1 %d\n", c->x);
		tmpjc = getrjobcount(c->rjob);
		if (tmpjc > 0) {
			return readfromall(ch, a, n, offset);
		}
		if (tmpjc < 0) {
			snprint(up->genbuf, sizeof(up->genbuf), "cmd/unreserved\n");
			return readstr(offset, p, n, up->genbuf);
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
		return readstr(offset, p, n, up->genbuf);

	case Qdata:
	case Qstderr:
		c = cmd.conv[CONV(ch->qid)];
		if (c->rjob == nil) {
			DPRINT(9,"resources released, as clone file closed\n");
			error(EResourcesReleased);
		}
		tmpjc = getrjobcount(c->rjob);
		if (tmpjc < 0)
			error(ENOReservation);

		if (tmpjc > 0) {
			if (TYPE(ch->qid) == Qdata) {
				return readfromallasync(ch, a, n, offset);
			} else {
				return readfromall(ch, a, n, offset);
			}
		}
		/* getting data locally */
		DPRINT(9,"reading data locally\n");
		fd = 1;
		if (TYPE(ch->qid) == Qstderr)
			fd = 2;
		c = cmd.conv[CONV(ch->qid)];
		qlock(&c->l);
		if (c->fd[fd] == -1) {
			qunlock(&c->l);
			DPRINT(9,"file descriptor is closed\n");
			return 0;
		}
		qunlock(&c->l);
		osenter();
		n = read(c->fd[fd], a, n);
		osleave();
		if (n < 0)
			oserror();
		DPRINT(9,"reading %ld locally done\n", n);
		return n;

	case Qwait:
		c = cmd.conv[CONV(ch->qid)];
		if (c->rjob == nil) {
			DPRINT(9,"resources released, as clone file closed\n");
			error(EResourcesReleased);
		}
		tmpjc = getrjobcount(c->rjob);
		if (tmpjc < 0)
			error(ENOReservation);

		if (tmpjc > 0) {
			/*
			 * FIXME: decide how exactly you want to implement
			 * this
			 */
			/* return readfromall (ch, a, n, offset); */
		}
		/* Doing it locally */
		c = cmd.conv[CONV(ch->qid)];
		return qread(c->waitq, a, n);
	}							/* end switch : file-type */
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
analyseremotenode(remoteMount * rmt)
{
	char buff[100];
	char buff2[100];
	long offset;
	long ret;
	int i, len, j, k;
	long hn, an;

	initinfo(rmt->info);

	offset = 0;
	while (1) {
		/* read one line from Chan */
		ret =
			devtab[rmt->status->type]->read(rmt->status, buff, 30, offset);
		if (ret <= 0)
			return;
		for (i = 0; buff[i] != '\0' && buff[i] != '\n'; ++i);
		if (buff[i] == '\0')
			return;
		if (buff[i] == '\n') {
			buff[i] = '\0';
		}
		len = i;

		/* parse it */

		/* find host type */
		for (j = 0, k = 0; buff[j] != ' '; ++j, ++k) {
			buff2[k] = buff[j];
		}
		buff2[k] = '\0';
		hn = lookup(buff2, oslist, OSCOUNT);

		/* find arch type */
		for (++j, k = 0; buff[j] != ' '; ++j, ++k) {
			buff2[k] = buff[j];
		}
		buff2[k] = '\0';
		an = lookup(buff2, platformlist, PLATFORMCOUNT);

		/* find number of nodes */
		for (++j, k = 0; buff[j] != '\0'; ++j, ++k) {
			buff2[k] = buff[j];
		}
		buff2[k] = '\0';

		/* update statistics */
		rmt->info[hn][an] += atol(buff2);

		/* prepare for next line */
		offset += len + 1;

	}							/* end while : infinite */
}								/* end function : analyseremotenode */



/*
 * verifies if specified location is indeed remote resource or not. and
 * returns remoteMount to the exact remote resource.  It is callers
 * responsibility to free the memory of path returned
 */
static remoteMount *
validaterr(char *location, char *os, char *arch)
{
	Dir *dr;
	Dir *tmpDr;
	int hn, pn;
	long i, count;
	char localName[] = VALIDATEDIR;	/* for validation */
	char parentName[] = PARENTNAME;	/* for ignoring parent */
	remoteMount *ans;
	char buff[KNAMELEN * 3];

	if (waserror()) {
		DPRINT(9,"Cant verify Dir [%s] \n", location);
		free(dr);
		nexterror();
	}
	count = lsdir(location, &dr);

	DPRINT(9,"remote dir[%s]= %ld entries\n", location, count);
	for (i = 0, tmpDr = dr; i < count; ++i, ++tmpDr) {
		DPRINT(9,"%ld [%s/%s]\n", i, location, tmpDr->name);
		if (DMDIR & tmpDr->mode) {
			DPRINT(9,"checking for dir[%s]\n", tmpDr->name);
			if (strcmp(tmpDr->name, localName) == 0) {
				break;
			}
			if (strcmp(tmpDr->name, parentName) == 0) {
				DPRINT(9,"this is parent dir, ignoring\n");
				break;
			}
		}
	}
	free(dr);

	ans = (remoteMount *) malloc(sizeof(remoteMount));
	ans->path = (char *) malloc(strlen(location) + strlen(localName) + 2);
	snprint(ans->path, (strlen(location) + strlen(localName) + 2),
			"%s/%s", location, localName);

	if (i == count) {
		DPRINT(9,"validaterr: [%s] not found, returning\n", localName);
		free(ans->path);
		free(ans);
		poperror();
		return nil;
	}
	DPRINT(9,"validaterr: now  searching for clone file\n");

	count = lsdir(ans->path, &dr);
	poperror();

	for (i = 0, tmpDr = dr; i < count; ++i, ++tmpDr) {

		if ((!(DMDIR & tmpDr->mode)) &&
			(strcmp(tmpDr->name, "clone") == 0)) {
			/* not directory and name is "clone" */

			/* open channel to status file */
			sprint(buff, "%s/%s", ans->path, "status");
			ans->status = namec(buff, Aopen, OREAD, 0);

			/* verify if requested platform is available or not */

			/* give priority to OS */
			if (os == nil) {
				free(dr);
				return ans;		/* wildcard match */
			}
			if (os[0] == '*') {
				free(dr);
				return ans;		/* wildcard match */
			}
			hn = lookup(os, oslist, OSCOUNT);
			if (hn == 0)
				break;			/* unknown os */

			analyseremotenode(ans);
			if (arch == nil || arch[0] == '*') {
				/* wild card for architecture */
				for (i = 1; i < PLATFORMCOUNT; ++i) {
					if (ans->info[hn][i] > 0) {
						/*
						 * needed OS is present for
						 * some architecture
						 */
						free(dr);
						return ans;
					}
				}
			}					/* end if : wildcard for architecture */
			pn = lookup(arch, platformlist, PLATFORMCOUNT);
			if (pn == 0)
				break;			/* unknown arch */

			if (ans->info[hn][pn] > 0) {
				/* needed OS and arch type are present */
				free(dr);
				return ans;
			}
			/* needed OS and arch type not present */
			break;

		}						/* end if : file is clone */
	}							/* end for : each entry in directory */

	/* cleanup */
	free(ans->path);
	free(ans);
	free(dr);
	return nil;
}

/* find remote resource for usage */
static remoteMount **
findrr(int *validrc, char *os, char *arch)
{
	char *path = REMOTEMOUNTPOINT;
	char location[KNAMELEN * 3];
	remoteMount *tmp;
	long count, i;
	Dir *dr;
	Dir *tmpDr;
	remoteMount **allremotenodes;
	long tmprc;

	if (waserror()) {
		DPRINT(9,"Cant open %s\n", REMOTEMOUNTPOINT);
		*validrc = 0;
		free(dr);
		return nil;
	}
	count = lsdir(path, &dr);
	poperror();
	allremotenodes =
		(remoteMount **) malloc(sizeof(remoteMount *) * (count + 1));
	tmprc = 0;
	allremotenodes[tmprc] = nil;

	DPRINT(9,"remote dir has %ld entries\n", count);
	for (i = 0, tmpDr = dr; i < count; ++i, ++tmpDr) {
		snprint(location, sizeof(location), "%s/%s", path, tmpDr->name);

		DPRINT(9,"checking for remote location[%s]\n", location);
		/* entry should be directory and should not be "local" */
		if ((DMDIR & tmpDr->mode)
			&& (strcmp(tmpDr->name, VALIDATEDIR) != 0)
			&& (strcmp(tmpDr->name, PARENTNAME) != 0)) {
			tmp = validaterr(location, os, arch);
			if (tmp != nil) {
				allremotenodes[tmprc] = tmp;
				++tmprc;
				DPRINT(9,"Resource [%s]\n", tmp->path);
			}
		}
	}							/* end for */
	allremotenodes[tmprc] = nil;
	*validrc = tmprc;
	free(dr);
	return allremotenodes;
}

/*
 * allocates single remote resource, can be executed in kproc
 */
static void
allocatesinglerr(void *info)
{
	char data[100];
	char report[10];
	int len, ret;
	RemoteResource *r_resource;
	RemoteFile *r_file;
	int jcount;
	int filetype;

	filetype = Qctl - Qdata;
	r_resource = (RemoteResource *) info;
	jcount = r_resource->sub_sessions;
	r_file = &r_resource->remotefiles[filetype];

	report[0] = r_file->local_session_id;
	report[1] = 0;				/* indicates failure */
	report[2] = 0;				/* indicate end of report */

	if (waserror()) {
		DPRINT(9,"#### error in sending reservation request to remote\n");
		/* report failure */
		qwrite(r_file->async_queue, report, 2);
		pexit("", 0);
	}
	/* send reservation request */
	snprint(data, sizeof(data), "res %d", jcount);
	len = strlen(data);

	ret = devtab[r_file->cfile->type]->write(r_file->cfile, data, len, 0);
	if (ret != len) {
		DPRINT(9,"#### ERR : res write size(%d) returned [%d]\n",
				  len, ret);
		/* report failure */
		qwrite(r_file->async_queue, report, 2);
		pexit("", 0);
	}
	poperror();

	r_file->state = 1;
	r_resource->inuse = 1;

	/* report success */
	report[1] = 1;				/* indicates success */
	qwrite(r_file->async_queue, report, 2);
	DPRINT(9,"#### done with thread [%d]\n", r_file->local_session_id);
	pexit("", 0);
	return;
}								/* end function : allocatesinglerr */

/* allocates memory and initialize the RemoteResource */
RemoteResource *
allocate_remote_resource(RemoteJob * rjob, int sub_sessions, int rrnumber)
{
	RemoteResource *pp;
	int i;
	pp = malloc(sizeof(RemoteResource));
	pp->next = nil;
	for (i = 0; i < RCHANCOUNT; ++i) {
		pp->remotefiles[i].cfile = nil;
		pp->remotefiles[i].async_queue = rjob->async_queue_list[i];
		pp->remotefiles[i].state = 0;
		pp->remotefiles[i].local_session_id = rrnumber;
	}
	pp->local_session_id = rrnumber;
	pp->remote_session_id = -1;
	pp->sub_sessions = sub_sessions;
	pp->state = 0;
	return pp;
}

/* does multiple remote reservation in parallel */
int
parallelres(char *localpath, remoteMount ** remote_node_list, int validrc,
			Conv * c, int resno, int first, int others)
{
	RemoteJob *rjob;
	RemoteResource *rr;
	RemoteResource *prev;
	char *selected;
	long tmpjc;
	char buf[100];
	char local_session_path[KNAMELEN * 3];
	char remote_session_path[KNAMELEN * 3];
	char remote_ctl_path[KNAMELEN * 3];
	int i;
	int flag;
	int filetype;
	int ret;
	Block *content;

	rjob = c->rjob;
	/* error checking */
	if (rjob == nil) {
		DPRINT(9,"resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	DPRINT(9,"parallelres(%s, %d, %d, %d)\n",
			  localpath, resno, first, others);

	/* I think these errors should be checked much before */
	tmpjc = getrjobcount(rjob);
	if (tmpjc == -2) {
		DPRINT(9,"resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	if (tmpjc != -1) {
		DPRINT(9,"already in use\n");
		error(EResourcesINUse);
	}
	/* handle all failures */
	if (waserror()) {
		DPRINT(9,"Could not allocate needed rmt resources\n");

		rjob->rjobcount = -1;
		/* TODO: I should also release the RemoteResource memory here */
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
	for (i = 0; i < RCHANCOUNT; ++i) {
		rjob->rcinuse[i] = 0;
		rjob->async_queue_list[i] = qopen(1024, 0, nil, 0);
	}

	/* allocate memory and initialize RemoteResource */
	rr = allocate_remote_resource(rjob, first, 0);
	rjob->first = rr;
	prev = rr;
	for (i = 1; i < resno; ++i) {
		rr = allocate_remote_resource(rjob, others, i);
		prev->next = rr;
		prev = rr;
	}
	rjob->last = rr;

	/* set the remote job count */
	rjob->rjobcount = resno;

	filetype = Qctl - Qdata;
	/* clear any old notifications from queue */
	qflush(rjob->async_queue_list[filetype]);
	rr = rjob->first;
	for (i = 0; i < resno; ++i) {

		/* find which remote resource to use */
		if (remote_node_list == nil) {
			selected = BASE;
		} else {
			lastrrselected = (lastrrselected + 1) % validrc;
			selected = remote_node_list[lastrrselected]->path;
		}

		sprint(local_session_path, "%s/%d", localpath,
			   rr->local_session_id);
		sprint(remote_ctl_path, "%s/%s", selected, "clone");

		DPRINT(9,"creating session id [%s]\n", local_session_path);

		/*
		 * quickfix as calls like namec and kbind are not working
		 * from threads
		 */
		if (waserror()) {
			/* FIXME : following warning msg is incorrect */
			DPRINT(9,"namec failed\n");
			nexterror();
		}
		DPRINT(9,"before namec on [%s]\n", remote_ctl_path);
		rr->remotefiles[filetype].cfile = namec(remote_ctl_path, Aopen, ORDWR, 0);
		DPRINT(9,"namec successful\n");
		poperror();

		if (waserror()) {
			DPRINT(9,"could not bind remote and local resource\n");
			error(ENOReservation);
		}
		content = devbread(rr->remotefiles[filetype].cfile, 10, 0);

		if (BLEN(content) == 0) {
			error(ENOReservation);
		}
		DPRINT(9,"clone returned %ld data [%s]\n",
				  BLEN(content), (char *) content->rp);

		content->rp[BLEN(content)] = 0;
		rr->remote_session_id = atol((char *) content->rp);

		/* creating remote session paths */
		sprint(remote_session_path, "%s/%d", selected,
			   rr->remote_session_id);

		/* bind remote resource on local dir. */
		DPRINT(9,"binding r[%s]->l[%s]\n", remote_session_path,
				  local_session_path);
		kbind(remote_session_path, local_session_path, MREPL);
		poperror();

		freeb(content);

		/* create kprocs for each job */
		sprint(buf, "ctlProc_%d__", rr->local_session_id);
		//allocatesinglerr(pp);
		kproc(buf, allocatesinglerr, rr, 0);
		rr = rr->next;
		DPRINT(9,"allocated one session [%d]\n", i);
	} /* end for : for each session resource */

	/* ensure that resources are properly allocated */
	flag = 0;
	for (i = 0; i < resno; ++i) {
		ret = qread(rjob->async_queue_list[filetype], buf, 2);
		if (ret != 2) {
			++flag;
			DPRINT(9,"qread on ctl buff failed\n");
		} else {
			if (buf[1] != 1) {
				++flag;
				DPRINT(9,"resource allocation for [%d] failed\n", buf[0]);
			} else {
				DPRINT(9,"resource allocation for [%d] successful\n",
						  buf[0]);
			}
		}
	}				/* end for : for each resource */


	/* mark ctl file in use */
	rjob->rcinuse[filetype] = 1;

	/* completely unlock this session */
	wunlock(&rjob->l);
	qunlock(&c->l);

	poperror();

	/* report failure */
	if (flag > 0) {
		DPRINT(9,"resource allocation failed for [%d] times\n", flag);
		/* FIXME: release half allocated resources */
		error(ERemoteResFail);
	}
	return resno;
}					/* end function : parellelres */


/* frees the list of remote mounts */
void
freermounts(remoteMount ** allremotenodes, int validrc)
{
	int i;

	for (i = 0; i < validrc; ++i) {
		free(allremotenodes[i]->path);
		/* close the status channel */
		cclose(allremotenodes[i]->status);
		free(allremotenodes[i]);
	}
	free(allremotenodes);
}


/* reserve the needed number of resources */
static long
groupres(Chan * ch, int resNo, char *os, char *arch)
{
	char *lpath;
	Conv *c;
	int first, others;
	remoteMount **allremotenodes;
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
			hn = lookup(os, oslist, OSCOUNT);
			if (hn == 0) {
				/* unknown os */
				DPRINT(9,"Wrong OS type\n");
				error(Eperm);
			}
			if (hn != IHN) {
				/* Wrong os requested */
				DPRINT(9,"Local OS differs requested OS\n");
				error(ENoResourceMatch);
			}
		}
		/* end if : OS specified */
		/* We have proper OS, lets check for architecture */
		if ((arch != nil) && (arch[0] != '*')) {
			pn = lookup(arch, platformlist, PLATFORMCOUNT);
			if (pn == 0) {
				/* unknown platform */
				DPRINT(9,"Wrong platform type\n");
				error(Eperm);
			}
			if (pn != IAN) {
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
initinfo(long info[OSCOUNT][PLATFORMCOUNT])
{
	int i, j;
	for (i = 0; i < OSCOUNT; ++i) {
		for (j = 0; j < PLATFORMCOUNT; ++j) {
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
addremotenode(long info[OSCOUNT][PLATFORMCOUNT], Chan * status)
{

	char buff[100];
	char buff2[100];
	long offset;
	long count, ret;
	int i, len, j, k;
	long hn, an;

	offset = 0;
	while (1) {
		/* read one line from Chan */
		ret = devtab[status->type]->read(status, buff, 30, offset);
		if (ret <= 0)
			return;
		for (i = 0; buff[i] != '\0' && buff[i] != '\n'; ++i);
		if (buff[i] == '\0')
			return;
		if (buff[i] == '\n') {
			buff[i] = '\0';
		}
		len = i;

		/* parse it */

		/* find number of nodes */
		for (j = 0, k = 0; buff[j] != ' '; ++j, ++k) {
			buff2[k] = buff[j];
		}
		buff2[k] = '\0';
		count = atol(buff2);

		/* find host type */
		for (++j, k = 0; buff[j] != ' '; ++j, ++k) {
			buff2[k] = buff[j];
		}
		buff2[k] = '\0';
		hn = lookup(buff2, oslist, OSCOUNT);

		/* find arch type */
		for (++j, k = 0; buff[j] != '\0'; ++j, ++k) {
			buff2[k] = buff[j];
		}
		buff2[k] = '\0';
		an = lookup(buff2, platformlist, PLATFORMCOUNT);

		/* update statistics */
		info[hn][an] += count;

		/* prepare for next line */
		offset += len + 1;

	}							/* end while : infinite */
}								/* end function : addRemoteNode */


/* reads top level status file */
static long
readstatus(char *a, long n, vlong offset)
{
	long i, j;
	int validrc;
	remoteMount **allremotenodes;
	long info[OSCOUNT][PLATFORMCOUNT];
	char buff[(OSCOUNT + 1) * 30];
	char buff2[30];

	validrc = 0;
	allremotenodes = findrr(&validrc, nil, nil);

	if (validrc == 0) {			/* Leaf node */
		/* free up the memory allocated by findrr call */
		freermounts(allremotenodes, validrc);
		sprint(buff2, "%d %s %s\n", 1, oslist[IHN], platformlist[IAN]);
		return readstr(offset, a, n, buff2);
	}
	initinfo(info);

	/* Add local type to info */
	info[IHN][IAN] = 1;

	/* Add remote client stats to info */
	for (i = 0; i < validrc; ++i) {
		addremotenode(info, allremotenodes[i]->status);
	}							/* end for : */

	/* Free up the memory allocated by findrr call */
	freermounts(allremotenodes, validrc);

	/* Prepare Buffer to return */
	buff[0] = '\0';
	for (i = 0; i < OSCOUNT; ++i) {
		for (j = 0; j < PLATFORMCOUNT; ++j) {
			if (info[i][j] > 0) {
				sprint(buff2, "%ld %s %s\n", info[i][j], oslist[i],
					   platformlist[j]);
				strcat(buff, buff2);
			}
		}
	}							/* end for : */
	return readstr(offset, a, n, buff);
}								/* end function : readstatus */

/*
 * function : p_send2one : will send data to one node but this function can
 * be executed parallelly in thread
 */
static void
p_send2one(void *param)
{
	long ret;
	struct parallel_send_wrap *psw;
	RemoteFile *rf;
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
	free(psw);

	report[0] = rf->local_session_id;
	report[1] = 0;				/* indicates failure */
	report[2] = 0;				/* indicate end of report */

	if (waserror()) {
		DPRINT(9,"###p_send2one[%d]: error in sending write\n",
				  rf->local_session_id);
		/* report failure */
		qwrite(rf->async_queue, report, 2);
		pexit("", 0);
	}
	DPRINT(9,"###p_send2one: started\n");
	/* FIXME : lock the file to be used */

	DPRINT(9,"###p_send2one[%d]: about to write\n", rf->local_session_id);

	/* do actual write */
	ret = devtab[rf->cfile->type]->write(rf->cfile, a, n, offset);

	DPRINT(9,"###p_send2one[%d]: write done\n", rf->local_session_id);

	if (ret != n) {
		/* problem: all bytes are not written, report failure */
		DPRINT(9,"###p_send2one[%d]:wrot %ld instd of %ld bytes\n",
				  rf->local_session_id, ret, n);
		/* report failure */
		qwrite(rf->async_queue, report, 2);
		pexit("", 0);
	}
	poperror();

	/* report success */
	report[1] = 1;				/* indicates success */
	qwrite(rf->async_queue, report, 2);
	DPRINT(9,"###p_send2one[%d]:  success wrot %ld bytes\n",
			  rf->local_session_id, ret);
	pexit("", 0);
	return;
}								/* end function : p_send2one */


/*
 * p_send2all : sends write command to all nodes parallelly FIXME: works only
 * for ctl file
 */
static long
p_send2all(Chan * ch, void *a, long n, vlong offset)
{
	int i, ret;
	Conv *c;
	long tmpjc;
	RemoteResource *rr;
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
		DPRINT(9,"resources released, as clone file closed\n");
		error(EResourcesReleased);
	}
	tmpjc = getrjobcount(c->rjob);

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
	qflush(c->rjob->async_queue_list[filetype]);
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
		sprint(buf, "send2allProc_%d_%d__", c->x, rr->local_session_id);
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
		ret = qread(c->rjob->async_queue_list[filetype], buf, 2);
		if (ret != 2) {
			++flag;
			DPRINT(9,"qread on ctl buff failed\n");
		} else {
			if (buf[1] != 1) {
				++flag;
				DPRINT(9,"resource allocation for [%d] failed\n", buf[0]);
			} else {
				DPRINT(9,"resource allocation for [%d] successful\n",
						  buf[0]);
			}
		}
	}							/* end for : for each resource */
	DPRINT(9,"analysis done, declaring\n");

	/* report failure */
	if (flag > 0) {
		DPRINT(9,"write failed for [%d] nodes\n", flag);
		error(ERemoteResFail);
	}
	DPRINT(9,"write to [%ld] res successful of size[%ld]\n", tmpjc, n);

	return n;
}								/* end function : p_send2all */

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
		hn = lookup(os, oslist, OSCOUNT);
		if (hn == 0) {
			/* unknown os */
			DPRINT(9,"Wrong OS type\n");
			error(Eperm);
		}
		if (hn != IHN) {
			/* Wrong os requested */
			DPRINT(9,"Local OS differs requested OS\n");
			error(ENoResourceMatch);
		}
	}
	/* end if : OS specified */
	/* We have proper OS, lets check for architecture */
	if ((arch != nil) && (arch[0] != '*')) {
		pn = lookup(arch, platformlist, PLATFORMCOUNT);
		if (pn == 0) {
			/* unknown platform */
			DPRINT(9,"Wrong platform type\n");
			error(Eperm);
		}
		if (pn != IAN) {
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
}								/* end function : preparelocalexecution  */

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
	kproc("cmdproc", cmdproc, c, 0);	/* cmdproc held back until
										 * unlock below */
	if (c->cmd != nil) {
		free(c->cmd);
		c->cmd = nil;
	}
	c->cmd = cb;				/* don't free cb */
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
	int r, ret;
	long tmpjc;
	Conv *c;
	Cmdbuf *cb;
	Cmdtab *ct;
	long resNo;
	char *os;
	char *arch;
	char *parent_path;
	char *buff;
	struct for_splice *fs;
	char *s;


	USED(offset);
	DPRINT(9,"write in file [%s]\n", ch->name->s);
	ret = n;				/* for default return value */

	switch (TYPE(ch->qid)) {
	default:
		error(Eperm);

	case Qtopctl:
		print("came here\n");
		cb = parsecmd(a, n);
		print("cmd arg[%d] done [%s]\n", cb->nf, a);
		ct = lookupcmd(cb, ctlcmdtab, nelem(ctlcmdtab));
		switch (ct->index) {
		default:
			error(Eperm);

		case CTLDebug:
			/* two statements so that atlease one will be shown */
			DPRINT(9,"setting debug to %d\n", atoi(cb->f[1]));
			vflag = atoi(cb->f[1]);
			DPRINT(9,"setting debug to %d\n", vflag);
			break;
		}						/* end switch : Qtopctl commands  */
		break;

	case Qctl:

		/* find no. of remote jobs running */
		c = cmd.conv[CONV(ch->qid)];
		tmpjc = getrjobcount(c->rjob);

		if (c->rjob == nil) {
			DPRINT(9,"resources released, as clone file closed\n");
			error(EResourcesReleased);
		}
		s = (char *) a;
		s[n] = 0;
		cb = parsecmd(s, n);
		if (waserror()) {
			DPRINT(9,"screw you cmd buffer!\n");
			//free(cb);
			//FIXME:        something bad happening
			/* send the actual error code back */
			error("Execution failed");
		}
		DPRINT(9,"cmd arg[%d] done [%s]\n", cb->nf, a);
		ct = lookupcmd(cb, cmdtab, nelem(cmdtab));
		switch (ct->index) {
		case CMres:
			DPRINT(9,"reserving resources %s \n", cb->f[1]);
			resNo = atol(cb->f[1]);
			DPRINT(9,"reserving resources %s [%ld]\n", cb->f[1], resNo);

			if (resNo < 0) {
				/* negative value for reservation is invalid */
				DPRINT(9,"Error: negative error value\n");
				error(Eperm);
			}
			if (tmpjc != -1) {
				DPRINT(9,"resource already in use\n");
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
			DPRINT(9,"Reservation done\n");
			break;

		case CMexec:
			DPRINT(9,"no of remote res are [%ld]\n", tmpjc);
			if (tmpjc < -1) {
				DPRINT(9,"previous res command failed\n");
				error(ENOReservation);
			}
			/* request for execution of command */
			if (tmpjc == -1) {
				DPRINT(9,"No reservation done\n");
				/* local execution request */
				preparelocalexecution(c, nil, nil);
				tmpjc = 0;
				/* error(ENOReservation); */
			}
			if (tmpjc == 0) {
				/* local execution request */
				DPRINT(9,"executing cmd locally\n");
				dolocalexecution(c, cb);
				poperror();
				return ret;
			}
			DPRINT(9,"executing cammand remotely\n");
			ret = p_send2all(ch, a, n, offset);
			DPRINT(9,"executing cammand - done\n");
			break;

		case CMkillonclose:
			if (tmpjc < 0) {
				DPRINT(9,"No reservation done\n");
				error(ENOReservation);
				break;
			}
			if (tmpjc == 0) {
				/* local execution request */
				DPRINT(9,"\n");
				c->killonclose = 1;
				break;
			}
			ret = p_send2all(ch, a, n, offset);
			DPRINT(9,"executing cammand - done\n");
			break;

		case CMkill:
			if (tmpjc < 0) {
				DPRINT(9,"No reservation done\n");
				error(ENOReservation);
				break;
			}
			if (tmpjc == 0) {
				/* local execution request */
				DPRINT(9,"executing kill locally\n");
				qlock(&c->l);
				if (waserror()) {
					DPRINT(9,"we aren't good killers\n");
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
			DPRINT(9,"executing cammand - done\n");
			break;

		case CMsplice:
			if (tmpjc < 0) {
				DPRINT(9,"No reservation done\n");
				error(ENOReservation);
				break;
			}
			if (waserror()) {
				DPRINT(9,"splice failed\n");
				nexterror();
			}
			/* find the path to stdio file */
			parent_path = fetchparentpath(ch->name->s);
			buff = malloc(strlen(parent_path) + 8);
			sprint(buff, "%s/stdio", parent_path);
			fs = (struct for_splice *) malloc(sizeof(struct for_splice));
			fs->dst = namec(buff, Aopen, OWRITE, 0);
			fs->src = namec(cb->f[1], Aopen, OREAD, 0);
			kproc("proc_splice", proc_splice, fs, 0);
			poperror();

			break;

		case CMdir:
			if (tmpjc < 0) {
				DPRINT(9,"No reservation done\n");
				error(ENOReservation);
				break;
			}
			if (tmpjc == 0) {
				/* local execution request */
				DPRINT(9,"executing CMdir locally\n");
				kstrdup(&c->dir, cb->f[1]);
				break;
			}
			ret = p_send2all(ch, a, n, offset);
			DPRINT(9,"executing cammand - done\n");
			break;

		default:
			DPRINT(9,"Error : wrong command type\n");
			error(Eunknown);
			break;

		}						/* end switch : ctl file commands */

		poperror();
		//free(cb);
		return ret;
		break;

	case Qdata:
		/* find no. of remote jobs running */
		c = cmd.conv[CONV(ch->qid)];
		tmpjc = getrjobcount(c->rjob);

		if (c->rjob == nil) {
			DPRINT(9,"resources released, as clone file closed\n");
			error(EResourcesReleased);
		}
		if (tmpjc < 0) {
			DPRINT(9,"No reservation done\n");
			error(ENOReservation);
			break;
		}
		if (tmpjc == 0) {
			/* local data file write request */
			DPRINT(9,"sending data locally\n");

			qlock(&c->l);
			if (c->fd[0] == -1) {
				qunlock(&c->l);
				error(Ehungup);
			}
			qunlock(&c->l);
			osenter();
			r = write(c->fd[0], a, n);
			osleave();
			if (r == 0)
				error(Ehungup);
			if (r < 0) {
				/*
				 * XXX perhaps should kill writer "write on
				 * closed pipe" here, 2nd time around?
				 */
				oserror();
			}
			return r;
		}
		/* end if : local file write request */
		/* sending to remote resources */
		return p_send2all(ch, a, n, offset);
	}							/* end switch : on filename */

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
		d = malloc(sizeof(*d) + n);
		if (d == nil)
			error(Enomem);
		if (waserror()) {
			DPRINT(9,"error has errors!\n");
			free(d);
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
		free(d);
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
	c->rjob = malloc(sizeof(RemoteJob));
	wlock(&c->rjob->l);
	c->rjob->x = c->x;
	c->rjob->rjobcount = -1;
	c->rjob->first = nil;
	c->rjob->last = nil;
	++ccount;
	wunlock(&c->rjob->l);
	qunlock(&c->l);
	DPRINT(9,"cmdclone successful \n");
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
	DPRINT(9,"f[0]=%q f[1]=%q\n", c->cmd->f[0], c->cmd->f[1]);
	if (waserror()) {
		DPRINT(9,"failed: %q\n", up->env->errstr);
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
	n = oscmdwait(t, status, sizeof(status));
	osleave();
	if (n < 0) {
		oserrstr(up->genbuf, sizeof(up->genbuf));
		n = snprint(status, sizeof(status), "0 0 0 0 %q", up->genbuf);
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
			nr = devtab[c->umc->type]->read(c->umc, a_va, a_n,
											c->umc->offset);
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
}								/* end function : myunionread */


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
		free(buf);
		nexterror();
	}

	ts = readfromchan(dirChan, buf, DIRREADLIM, 0);
	if (ts > 0) {
		t2 = dirpackage(buf, ts, d);
		ts = t2;
	}

	poperror();
	free(buf);
	poperror();
	return ts;
}


/* Returns the list of Dir entries for given directory name */
long
lsdir(char *name, Dir ** d)
{
	long count;
	volatile struct {
		Chan *cc;
	} rock;

	if (waserror()) {
		DPRINT(9,"Cant open [%s]\n", name);
		nexterror();
	}
	rock.cc = namec(name, Aopen, OREAD, 0);
	poperror();

	if (waserror()) {
		DPRINT(9,"readchandir failed\n");
		cclose(rock.cc);
		nexterror();
	}

	count = readchandir(rock.cc, d);
	DPRINT(9,"done readchandir\n");
	cclose(rock.cc);
	DPRINT(9,"freed cc\n");

	poperror();
	return count;
}


/*
 * returns string containing path to parent, and path will end with '/'
 * memory should be released by caller
 */
char *
fetchparentpathch(Chan * ch)
{
	char *ptr;
	char buff[KNAMELEN * 3];

	sprint(buff, "%s/%ld/", BASE, CONV(ch->qid));
	ptr = malloc((sizeof(char)) * strlen(buff) + 2);
	if (ptr == nil) {
		return nil;
	}
	strcpy(ptr, buff);
	DPRINT(9,"filepath for qid[%llux] is [%s][%s]\n", ch->qid.path, buff,
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
