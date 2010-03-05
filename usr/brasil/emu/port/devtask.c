#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"kernel.h"
enum
{
	Qtopdir,	/* top level directory */
	Qcmd,		/* "remote" dir */
	Qclonus,
	Qlocalfs,   /* local fs mount point */
	Qarch,	/* architecture description */
	Qtopns,	/* default ns for host */
	Qtopenv,	/* default env for host */
	Qtopstat,	/* Monitoring Information for host */
	Qconvdir,  /* resource dir */
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

	Debug=0	/* to help debug os.c */
};
     
#define TYPE(x) 	 ( (ulong) (x).path & 0xff)
#define CONV(x) 	 ( ( (ulong) (x).path >> 8)&0xff)
#define RJOBI(x) 	 ( ( (ulong) (x).path >> 16)&0xff)
#define QID(c, y) 	 ( ( (c)<<8) | (y))
#define RQID(c, x, y) 	 ( ( ( (c)<< 16) | ( (x) << 8)) | (y))

#define RCHANCOUNT 10
#define BASE "/task/remote/"

char ENoRemoteResources[] = "No remote resources available";
char ENOReservation[] = "No remote reservation done";
static int vflag = 0; /* for debugging messages: control prints */

long lastrrselected = 0;

struct RemoteResource 
{
	int x;
	int inuse;
	/* path to researved remote resource */
	Chan *remotechans[RCHANCOUNT];
	/* counter for accessing each file */
	struct RemoteResource *next;
};

typedef struct RemoteResource RemoteResource;

struct RemoteJob 
{
	int x;
	RWlock	l;	/* protects state changes */
	long rjobcount;
	int rcinuse[RCHANCOUNT];
	RemoteResource *first;
	RemoteResource *last;
};
typedef struct RemoteJob RemoteJob;

typedef struct Conv	Conv;
struct Conv
{
	int	x;
	int	inuse;
	int	fd[3];	/* stdin, stdout, and stderr */
	int	count[3];	/* number of readers on stdin/stdout/stderr */
	int	perm;
	char	*owner;
	char	*state;
	Cmdbuf	*cmd;
	char	*dir;
	QLock	l;	/* protects state changes */
	Queue	*waitq;
	void	*child;
	char	*error;	/* on start up */
	int		nice;
	short	killonclose;
	short	killed;
	Rendez	startr;
	RemoteJob *rjob; /* path to researved remote resource*/
};

static struct
{
	QLock	l;
	int		nc;
	int		maxconv;
	Conv	**conv;
} cmd;


static	Conv *cmdclone (char *);
char *fetchparentpath	 (char *filepath);
long lsdir (char *name, Dir **d);
char *fetchparentpathch	 (Chan *ch);

static long 
getrjobcount (RemoteJob *rjob) {

	long temp ;
	rlock (&rjob->l);
	temp = rjob->rjobcount ;
	runlock (&rjob->l);
	return temp;
}

static int
getfileopencount (RemoteJob *rjob, int filetype) {

	int temp ;
	rlock (&rjob->l);
	temp = rjob->rcinuse[filetype];
	runlock (&rjob->l);
	return temp;
}


/* Generate entry within resource directory */
static int
cmd3gen (Chan *c, int i, Dir *dp)
{
	Qid q;
	Conv *cv;

	cv = cmd.conv[CONV(c->qid)];
	switch (i){
	default:
		return -1;
	case Qdata:
		mkqid (&q, QID (CONV (c->qid), Qdata), 0, QTFILE);
		devdir (c, q, "stdio", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qstderr:
		mkqid (&q, QID (CONV (c->qid), Qstderr), 0, QTFILE);
		devdir (c, q, "stderr", 0, cv->owner, 0444, dp);
		return 1;
	case Qctl:
		mkqid (&q, QID (CONV (c->qid), Qctl), 0, QTFILE);
		devdir (c, q, "ctl", 0, cv->owner, cv->perm, dp);
		return 1;
	case Qstatus:
		mkqid (&q, QID (CONV (c->qid), Qstatus), 0, QTFILE);
		devdir (c, q, "status", 0, cv->owner, 0444, dp);
		return 1;
	case Qwait:
		mkqid (&q, QID (CONV (c->qid), Qwait), 0, QTFILE);
		devdir (c, q, "wait", 0, cv->owner, 0444, dp);
		return 1;
	case Qstdin:
		mkqid (&q, QID (CONV (c->qid), Qstdin), 0, QTFILE);
		devdir (c, q, "stdin", 0, cv->owner, 0222, dp);
		return 1;
	case Qstdout:
		mkqid (&q, QID (CONV (c->qid), Qstdout), 0, QTFILE);
		devdir (c, q, "stdout", 0, cv->owner, 0444, dp);
		return 1;
	case Qargs:
		mkqid (&q, QID (CONV (c->qid), Qstdin), 0, QTFILE);
		devdir (c, q, "args", 0, cv->owner, cv->perm, dp);
		return 1;	
	case Qenv:
		mkqid (&q, QID (CONV (c->qid), Qstdin), 0, QTFILE);
		devdir (c, q, "env", 0, cv->owner, cv->perm, dp);
		return 1;	
	case Qns:
		mkqid (&q, QID (CONV (c->qid), Qns), 0, QTFILE);
		devdir (c, q, "ns", 0, cv->owner, cv->perm, dp);
		return 1;						
	}
}

/* supposed to generate top level directory structure for task */
static int
cmdgen (Chan *c, char *name, Dirtab *d, int nd, int s, Dir *dp)
{
	Qid q;
	Conv* cv;
	Conv* myc;
	char tmpbuff[KNAMELEN*3];
	long tmpjc;

	char* buff;

	USED (name);
	USED (nd);
	USED (d);
	
	if (s == DEVDOTDOT){
		switch (TYPE (c->qid)){
		case Qtopdir:
		case Qcmd:
			mkqid (&q, QID (0, Qtopdir), 0, QTDIR);
			devdir (c, q, "#T", 0, eve, DMDIR|0555, dp);
			break;
		case Qconvdir:
			mkqid (&q, QID (0, Qcmd), 0, QTDIR);
			devdir (c, q, "remote", 0, eve, DMDIR|0555, dp);
			break;
		case QLconrdir :  /* for remote resource bindings */
			if (vflag) print ("cmdindex [%d], rjobi [%d]\n", 
				CONV (c->qid), RJOBI (c->qid));

			sprint (tmpbuff, "%d", CONV (c->qid));
			cv = cmd.conv[CONV (c->qid)];

			mkqid (&q, QID (CONV (c->qid), Qconvdir), 0, QTDIR);
			devdir (c, q, tmpbuff, 0, cv->owner, DMDIR|0555, dp);
			break;

		default:
			panic ("cmdgen %llux", c->qid.path);
		}
		return 1;
	}

	switch (TYPE (c->qid)) {
	case Qtopdir:
		if (s >= 1)
			return -1;
		mkqid (&q, QID (0, Qcmd), 0, QTDIR);
		devdir (c, q, "remote", 0, eve, DMDIR|0555, dp);
		return 1;
	case Qcmd:
		if (s < cmd.nc) {
			cv = cmd.conv[s];
			mkqid (&q, QID (s, Qconvdir), 0, QTDIR);
			sprint (up->genbuf, "%d", s);
			devdir (c, q, up->genbuf, 0, cv->owner, DMDIR|0555, dp);
			return 1;
		}
		s -= cmd.nc;
		if (s == 0){
			mkqid (&q, QID (0, Qclonus), 0, QTFILE);
			devdir (c, q, "clone", 0, eve, 0666, dp);
			return 1;
		}
		if (s == 1){
			mkqid (&q, QID (0, Qlocalfs), 0, QTDIR);
			devdir (c, q, "fs", 0, eve, DMDIR|0555, dp);
			return 1;
		}
		if (s == 2){
			mkqid (&q, QID (0, Qarch), 0, QTDIR);
			devdir (c, q, "arch", 0, eve, 0444, dp);
			return 1;
		}
		if (s == 3){
			mkqid (&q, QID (0, Qtopns), 0, QTDIR);
			devdir (c, q, "ns", 0, eve, 0666, dp);
			return 1;
		}
		if (s == 4){
			mkqid (&q, QID (0, Qtopenv), 0, QTDIR);
			devdir (c, q, "env", 0, eve, 0666, dp);
			return 1;
		}
		if (s == 5){
			mkqid (&q, QID (0, Qtopstat), 0, QTDIR);
			devdir (c, q, "status", 0, eve, 0444, dp);
			return 1;
		}				
		return -1;
	case Qclonus:
		if (s == 0){
			mkqid (&q, QID (0, Qclonus), 0, QTFILE);
			devdir (c, q, "clone", 0, eve, 0666, dp);
			return 1;
		}
		return -1;
	case Qlocalfs:
		if (s == 0){
			mkqid (&q, QID (0, Qlocalfs), 0, QTDIR);
			devdir (c, q, "fs", 0, eve, DMDIR|0555, dp);
			return 1;
		}
		return -1;		

	case Qconvdir:
		myc = cmd.conv[CONV (c->qid)];
		tmpjc = getrjobcount (myc->rjob);
		if (s < tmpjc) {
			mkqid (&q, RQID (s, CONV (c->qid), QLconrdir), 0, QTDIR);
			sprint (up->genbuf, "%d", s);
			devdir (c, q, up->genbuf, 0, eve, DMDIR|0555, dp);
			return 1;
		}
		s -= tmpjc;

		return cmd3gen (c, Qconvbase+s, dp);

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
		return cmd3gen (c, TYPE (c->qid), dp);

	}
	return -1;
}


static void
cmdinit (void)
{
	cmd.maxconv = 1000;
	cmd.conv = mallocz (sizeof (Conv*)* (cmd.maxconv+1), 1);
	/* cmd.conv is checked by cmdattach, below */
}



static Chan *
cmdattach (char *spec)
{
	Chan *c;

	if (cmd.conv == nil)
		error (Enomem);
	c = devattach ('T', spec);
	mkqid (&c->qid, QID (0, Qtopdir), 0, QTDIR);
	return c;
}

static Walkqid*
cmdwalk (Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk (c, nc, name, nname, 0, 0, cmdgen);
}

static int
cmdstat (Chan *c, uchar *db, int n)
{
	return devstat (c, db, n, 0, 0, cmdgen);
}

/* Opens file/dir 
	It checks if specified channel is allowed to open,
	if yes, it marks channel as open and returns it.
*/
static Chan *
cmdopen (Chan *c, int omode)
{
	int perm;
	Conv *cv;
	Chan *tmpchan;
	char *user;
	char buff[KNAMELEN*3];
	RemoteResource *tmprr;
	int tmpjc, filetype;
	int i;
	int tmpfileuse;
	char *fname;


	if (vflag) print ("trying to open [%s]\n", c->name->s);
	perm = 0;
	omode = openmode (omode);
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

	switch (TYPE (c->qid)) {
	default:
		break;
	case Qarch:
	case Qtopns:
	case Qtopenv:
	case Qtopstat:
	case Qstdin:
	case Qstdout:
	case Qenv:
	case Qargs:
	case Qns:
		error (Egreg);	/* TODO: IMPLEMENT */
		break;	
		/* Following directories are allowed only in read mode */
	case Qtopdir:
	case Qcmd:
	case Qconvdir:
	case QLconrdir:
		if (omode != OREAD)
			error (Eperm);
		break;
		
	case Qlocalfs:
		error (Eperm);
		break;
	case Qclonus:
		/* in case of clone, check if it is locked */
		qlock (&cmd.l);
		if (waserror ()){
			qunlock (&cmd.l);
			nexterror ();
		}
		/* opening clone will come here */
		cv = cmdclone (up->env->user);
		poperror ();
		qunlock (&cmd.l);
		if (cv == 0)
			error (Enodev);
		mkqid (&c->qid, QID (cv->x, Qctl), 0, QTFILE);
		break;
			
	case Qstatus:
		if (omode != OREAD)
			error (Eperm);
	case Qdata:
	case Qstderr:
	case Qctl:
	case Qwait:

		qlock (&cmd.l);
		cv = cmd.conv[CONV (c->qid)];
		qlock (&cv->l);
		if (waserror ()){
			qunlock (&cv->l);
			qunlock (&cmd.l);
			nexterror ();
		}
		user = up->env->user;
		if ( (perm & (cv->perm>>6)) != perm) {
			if (strcmp (user, cv->owner) != 0 ||
		 	  (perm & cv->perm) != perm)
				error (Eperm);
		}
		cv->inuse++;
		if (cv->inuse == 1) {
			cv->state = "Open";
			kstrdup (&cv->owner, user);
			cv->perm = 0660;
			cv->nice = 0;
		}
		poperror ();
		qunlock (&cv->l);
		qunlock (&cmd.l);


		tmpjc = getrjobcount (cv->rjob);

		/* making sure that remote resources are allocated */
		if (TYPE(c->qid) != Qctl) {
			if (tmpjc == 0) {
				if (vflag) print ("resources not reserved\n");
				error (ENOReservation);
			}
		}
		filetype = TYPE(c->qid) - Qdata;

		switch (TYPE (c->qid)){
		case Qdata:
			fname = "stdio";
			break;
		case Qstderr:
			fname = "stderr";
			break;
		case Qstatus:
			fname = "status";
			break;
		case Qwait:
			fname = NULL;
			if (cv->waitq == nil)
				cv->waitq = qopen (1024, Qmsg, nil, 0);
			break;
		default:
			fname = NULL;
		}
		
		if ( fname != NULL ) {

			tmpfileuse = getfileopencount (cv->rjob, filetype);
			if ( tmpfileuse < 1) {
				/* opening file for first time */
				/* open should be done on all resources */
				if (vflag) print ("opening file for first times[%d]\n", cv->rjob->rcinuse[filetype]);
				rlock (&cv->rjob->l);
				tmprr = cv->rjob->first ;
				runlock (&cv->rjob->l);
				for (i = 0 ; i < tmpjc; ++i) {
					sprint(buff,"%s/%ld/%d/%s",
							BASE,CONV(c->qid),i,fname);
					tmpchan = namec (buff,Aopen,omode,0);

					wlock (&cv->rjob->l);
					tmprr->remotechans[filetype] = tmpchan;
					tmprr = tmprr->next; /* this can be RO lock*/
					wunlock (&cv->rjob->l);
				} /* end for : */
				wlock (&cv->rjob->l);
				cv->rjob->rcinuse[filetype] = 1;
				wunlock (&cv->rjob->l);
			} else {
					if (vflag) print ("file is already open for [%d] times\n", tmpfileuse);
					wlock (&cv->rjob->l);
					cv->rjob->rcinuse[filetype] += 1;
					wunlock (&cv->rjob->l);
			}
		} /* end if : fname != NULL */

		break;
	} /* end switch */
	c->mode = omode;
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static void 
freeremoteresource (RemoteResource *rr) 
{
	int i;
	if (rr == NULL) return;

	for (i = 0; i < RCHANCOUNT ; ++i ) {
		if (rr->remotechans[i] != NULL) {
			cclose (rr->remotechans[i]);
			rr->remotechans[i] = NULL;
			/* unbind ?? 
			You can't unbind here as rjobcount is already 
			reset to zero.
			*/
		}
	}
}

static void 
freeremotejobs (RemoteJob *rjob) 
{
	RemoteResource *ppr, *tmp, *next;
	if (rjob == NULL) return;

	if (vflag) print ("releaseing remote resources [%d]", rjob->x);
	wlock (&rjob->l);
	ppr = rjob->first;
	rjob->first = NULL;
	rjob->last = NULL;
	rjob->rjobcount = 0;
	wunlock (&rjob->l);

	while (ppr != NULL) {
		tmp = ppr;
		freeremoteresource (ppr);
		ppr = ppr->next;
		free (tmp);
	}
}

static void
closeconv (Conv *c)
{
	if (vflag) print ("releasing [%d] remote resource\n", c->x);
	kstrdup (&c->owner, "cmd");
	kstrdup (&c->dir, rootdir);
	c->perm = 0666;
	c->state = "Closed";
	c->killonclose = 0;
	c->killed = 0;
	c->nice = 0;
	freeremotejobs (c->rjob);
	free (c->cmd);
	c->cmd = nil;
	if (c->waitq != nil){
		qfree (c->waitq);
		c->waitq = nil;
	}
	free (c->error);
	c->error = nil;
}


static void
cmdclose (Chan *c)
{
	Conv *cc;
	int r;
	RemoteResource *tmprr;
	int i;
	int filetype;
	int tmpjc;
	int tmpfilec;

	if ( (c->flag & COPEN) == 0)
		return;

	if (vflag) print ("trying to close file [%s]\n", c->name->s);
	switch (TYPE (c->qid)) {
	case Qctl:
	case Qdata:
	case Qstderr:
	case Qwait:

		filetype = TYPE(c->qid) - Qdata;
		cc = cmd.conv[CONV (c->qid)];
		tmpjc = getrjobcount (cc->rjob);

		tmpfilec = getfileopencount (cc->rjob, filetype);
		if ( tmpfilec == 1) {
			/* its opened only once, so close it */
			/* closing remote resources */
			if (vflag) print ("closing it as only one open in cmd[%d]\n", cc->x);
			wlock (&cc->rjob->l);
			cc->rjob->rcinuse[filetype] = 0;
			tmprr = cc->rjob->first; /* can be in read lock */
			wunlock (&cc->rjob->l);
			for (i = 0 ; i < tmpjc; ++i ) {
				if (tmprr->remotechans[filetype] != NULL ) {
					devtab[tmprr->remotechans[filetype]->type]->
					close(tmprr->remotechans[filetype]);	
					cclose (tmprr->remotechans[filetype]);
					/* I am not freeing remotechan explicitly */

					wlock (&cc->rjob->l);
					tmprr->remotechans[filetype] = NULL;
					tmprr = tmprr->next; /* can be in read lock */
					wunlock (&cc->rjob->l);
				}
			} /* end for : */
		} else {
			
			if (vflag) print ("not closing [%s],accessed by others [%d]\n", c->name->s, tmpfilec);
			wlock (&cc->rjob->l);
			cc->rjob->rcinuse[filetype] -= 1;
			wunlock (&cc->rjob->l);
		}

		r = --cc->inuse;
		if (cc->child != nil){
			if (!cc->killed)
			if (r == 0 || (cc->killonclose && TYPE (c->qid) == Qctl)){
				oscmdkill (cc->child);
				cc->killed = 1;
			}
		}else if (r == 0)
			closeconv (cc);

		qunlock (&cc->l);
		break;
	}
}

static long 
readfromall (Chan *ch, void *a, long n, vlong offset)
{
	void *c_a;
	vlong c_offset;
	long ret, c_ret, c_n, i;
	Conv *c;
	long tmpjc;
	int filetype; 
	RemoteResource *tmprr;

	USED (offset);
	c = cmd.conv[CONV (ch->qid)];

	tmpjc = getrjobcount (c->rjob);
	/* making sure that remote resources are allocated */
	if (tmpjc == 0) {
		if (vflag) print ("resources not reserved\n");
		error (ENOReservation);
	}

	if (vflag) print ("readfromall cname [%s]\n", ch->name->s);

	filetype = TYPE(ch->qid) - Qdata;
	/* read from multiple remote files */
	c_a = a;
	c_n = n;
	c_offset = offset;
	ret = 0;

	rlock (&c->rjob->l);
	tmprr = c->rjob->first ;
	runlock (&c->rjob->l);

	for (i = 0 ; i < tmpjc; ++i) {
		c_ret = devtab[tmprr->remotechans[filetype]->type]->
				read(tmprr->remotechans[filetype], c_a, c_n,c_offset);	
		if (vflag) print ("%dth read gave [%d] data\n", i, c_ret);
		ret = ret + c_ret;
		c_a = c_a + c_ret;
		c_n = c_n - c_ret;

		/* FIXME: should I lock following also in read mode ??  */
		tmprr = tmprr->next;
	}
	if (vflag) print ("readfromall [%d]\n", ret);
	return ret;
}

static long
cmdread (Chan *ch, void *a, long n, vlong offset)
{
	char *fname;
	Conv *c;
	char *p, *cmds;

	USED (offset);
	c = cmd.conv[CONV (ch->qid)];
	p = a;

	switch (TYPE (ch->qid)) {
	default:
		error (Eperm);

	case Qcmd:
	case Qtopdir:
	case Qconvdir:
	case QLconrdir:
		return devdirread (ch, a, n, 0, 0, cmdgen);

	case Qctl:
		sprint (up->genbuf, "%ld", CONV (ch->qid));
		return readstr (offset, p, n, up->genbuf);

	case Qstatus:
		fname = "status";
		return readfromall (ch, a, n, offset);

	case Qdata:
		fname = "stdio";
		return readfromall (ch, a, n, offset);

	case Qstderr:
		fname = "stderr";
		return readfromall (ch, a, n, offset);

	case Qwait:
		c = cmd.conv[CONV (ch->qid)];
		return qread (c->waitq, a, n);
	}
}

static int
cmdstarted (void *a)
{
	Conv *c;

	c = a;
	return c->child != nil || c->error != nil || strcmp (c->state, "Execute") != 0;
}

enum
{
	CMres,
	CMdir,
	CMexec,
	CMkill,
	CMnice,
	CMkillonclose
};

static
Cmdtab cmdtab[] = {
	CMres,	"res",	2,
	CMdir,	"dir",	2,
	CMexec,	"exec",	0,
	CMkill,	"kill",	1,
	CMnice,	"nice",	0,
	CMkillonclose, "killonclose", 0,
}; 


/* verifies if specified location is indeed remote resource
   or not. and returns path to the exact remote resource.  
   It is callers responsibility to free the memory of path returned */
static char *
validateremoteresource (char *location) 
{
	
	Dir *dr;
	Dir *tmpDr;
	long i, count;
	char *path;
	char localName[] = "local";

	if (waserror ()){
		if (vflag) print ("Cant verify Dir [%s] \n", location);
		nexterror ();
	}
	count = lsdir (location, &dr);


	if (vflag) print ("remote dir[%s] = %d entries\n", location, count);
	for (i = 0, tmpDr = dr; i < count; ++ i, ++tmpDr) {
		if (DMDIR & tmpDr->mode) {
			if (strcmp (tmpDr->name, localName) == 0) {
				break;	
			}
		}
	}
	
	path = (char *)malloc (strlen (location) + strlen (localName) + 2);
	snprint (path, (strlen (location) + strlen (localName) + 2),
				"%s/%s", location, localName);

	if (i == count) {
		free (path);
		poperror ();
		return NULL;
	}

	/* should I free dr here ?? */
	count = lsdir (path, &dr);
	poperror ();

	for (i = 0, tmpDr = dr; i < count; ++ i, ++tmpDr) {
		if (! (DMDIR & tmpDr->mode)) {
			if (strcmp (tmpDr->name, "clone") == 0) {
				return path;
			}
		}
	}
	free (path);
	return NULL;
}


/* find remote resource for usage */
static char *
findremoteresources () 
{

	char *path = "/remote";
	char location[KNAMELEN*3];
	char *selected = NULL; 
	char *tmp;
	long count, i, len;
	Dir *dr;
	Dir *tmpDr;
	char **allremotenodes;
	long validrcount;

	selected = NULL;
	if (waserror ()){
		if (vflag) print ("Cant open remote\n");
		nexterror ();
	}

	count = lsdir (path, &dr);
	poperror ();
	allremotenodes = (char **) malloc (sizeof (char *) * (count+1));
	validrcount = 0;

	selected = NULL;
	if (vflag) print ("remote dir has %d entries\n", count);
	for (i = 0, tmpDr = dr; i < count; ++ i, ++tmpDr) {
			snprint (location, sizeof (location), 
				"%s/%s", path, tmpDr->name);

		if (vflag) print ("checking for remote location[%s]\n",location);
		if (DMDIR & tmpDr->mode) {  
					tmp = validateremoteresource (location);
				if (tmp != NULL) {
					allremotenodes[validrcount] = tmp;
						++validrcount;
				if (vflag) print ("Resource [%s]\n", tmp);
			} 
		}
	} /* end for */

	if (validrcount == 0) {
		error (ENoRemoteResources);
	}
	/* select next resource in the list */
	lastrrselected = (lastrrselected + 1) % validrcount;
	selected = allremotenodes[lastrrselected]; 

	/* free all other paths */
	for (i = 0; i < validrcount; ++i) {
		if (i != lastrrselected) {
			free (allremotenodes[i]);
		}
	}
	free (allremotenodes);
	return selected;
}


static Chan *
allocatesingleremoteresource (char *localpath) 
{
	char data[10];
	Block *content;
	char location[KNAMELEN*3];
	volatile struct { Chan *cc; } rock;
	Chan *ctlchan;
	char *selectedrresource = NULL;

	if (waserror ()){
		if (vflag) print ("Remote resources not present at [/remote]");
		nexterror ();
	}
	selectedrresource = findremoteresources ();

	snprint (location, KNAMELEN*3, 
			"%s/%s", selectedrresource, "clone");
	rock.cc = namec (location, Aopen, OREAD, 0); 
	poperror ();

	if (waserror ()){
		if (vflag) print ("No remote resource\n");
		freeb (content);
		cclose (rock.cc);
		nexterror ();
	}
	content = devbread (rock.cc, 10, 0);
	if (BLEN (content) == 0) {
		error ("clone did not gave resources\n");
	}

	snprint (location, KNAMELEN*3, 
			"%s/%s",selectedrresource, content->rp);
	if (vflag) print ("clone returned %d data [%s]\n", 
			BLEN (content), content->rp);
	if (vflag) print ("Remote Resource Path[%s]\n", location);
	poperror ();

	if (waserror ()){
		if (vflag) print ("could not bind remote and local resource\n");
		freeb (content);
		cclose (rock.cc);
		nexterror ();
	}
	/* bind remote resource on local dir. */
	if (vflag) print ("binding [%s]->[%s]\n", location, localpath);
	kbind (location, localpath, MREPL);
	freeb (content);
	poperror ();

	snprint (location, KNAMELEN*3, 
			"%s/%s", localpath, "ctl" );
	/* opening actual resource ctl and passing it on */
	if (waserror ()){
		if (vflag) print ("Could not open resource ctl [%s]\n",location);
		cclose (rock.cc);
		/* I need to unbind the remote and local resource here */
		nexterror ();
	}

	ctlchan = namec (location, Aopen, ORDWR, 0); 
	poperror ();
	cclose (rock.cc);
	return (ctlchan);
}


int
addremoteresource (char *localpath,  RemoteJob *rjob) 
{
	RemoteResource *pp;
	char buff[KNAMELEN*3];
	char *remotePath;
	Chan *tmpchan;
	long tmpjc;
	int i;


	if (waserror ()){
		if (vflag)print ("Could not allocate needed remote resources\n");
		rjob->rjobcount -= 1; /* undo the effect */ 
		nexterror ();
	}
	
	tmpjc = getrjobcount (rjob);
	if (tmpjc == 0 ) {
		
		wlock (&rjob->l);
		for (i = 0; i < RCHANCOUNT ; ++i ) {
			rjob->rcinuse[i] = 0;
		}
		wunlock (&rjob->l);
	}

	pp = malloc (sizeof (RemoteResource));
	pp->next = NULL;
	for (i = 0; i < RCHANCOUNT ; ++i ) {
		pp->remotechans[i] = NULL;
	}
	pp->x = tmpjc;


	wlock (&rjob->l);
	rjob->rjobcount += 1;/* needed so that cmdgen will give local dir */
	wunlock (&rjob->l);

	sprint (buff, "%s/%d", localpath, pp->x);
	if (vflag) print ("allocating rresource for location [%s]\n",buff);
	tmpchan = allocatesingleremoteresource (buff);
	
	wlock (&rjob->l);
	pp->remotechans[Qctl-Qdata] = tmpchan; 
	rjob->rcinuse[Qctl-Qdata] = 1; 
	if (rjob->first == NULL) { /* if list is empty */
		rjob->first = pp;  /* add it at begining */
	} else {  /* if there are previous resources */
		rjob->last->next = pp; /* add it at end */
	}
	rjob->last = pp; /* mark this resource as last */
	wunlock (&rjob->l);

	poperror ();
	return 1;
} /* end function :  allocateRemoteResource */

static long 
sendtoall (Chan *ch, void *a, long n, vlong offset)
{
	int i, ret;
	Conv *c;
	long tmpjc ;
	RemoteResource *tmprr;
	int filetype;

	c = cmd.conv[CONV (ch->qid)];
	tmpjc = getrjobcount (c->rjob);

	/* making sure that remote resources are allocated */
	if (tmpjc == 0) {
		if (vflag) print ("resources not reserved\n");
		error (ENOReservation);
	}

	filetype = TYPE(ch->qid) - Qdata;

	/* data should be sent to all resources */
	rlock (&c->rjob->l);
	tmprr = c->rjob->first ;
	runlock (&c->rjob->l);

	for (i = 0 ; i < tmpjc; ++i) {
		ret = devtab[tmprr->remotechans[filetype]->type]->
				write (tmprr->remotechans[filetype], a, n, offset);	

		/* FIXME: should I lock following also in read mode ??  */
		tmprr = tmprr->next;
	}
	if (vflag) print ("write to [%d] res successful of size[%d]\n", tmpjc, n);

	return ret;
}




static long
cmdwrite (Chan *ch, void *a, long n, vlong offset)
{
	char *path; 
	char *fname;
	int i, r, ret;
	long resNo;
	Conv *c;
	Cmdbuf *cb;
	Cmdtab *ct;

	USED (offset);
	if (vflag) print ("write in file [%s]\n", ch->name->s); 

	switch (TYPE (ch->qid)) {
	default:
		error (Eperm);
	case Qctl:
		cb = parsecmd (a, n);
		if (waserror ()){
			free (cb);
			nexterror ();
		}
		ct = lookupcmd (cb, cmdtab, nelem (cmdtab));
		switch (ct->index){
		case CMres :
			resNo = atol (cb->f[1]);
			if (vflag) print ("reserving resources %s [%ld]\n", 
				cb->f[1], resNo); 

			c = cmd.conv[CONV (ch->qid)];
			path = fetchparentpathch (ch);
			for (i = 0; i < resNo; ++ i) {
				addremoteresource (path, c->rjob);
			}
			free (path);
			ret = n;
			if (vflag) print ("Reservation done\n" );
			break;

		case CMexec:
			if (vflag) print ("executing cammand\n" );
		case CMkillonclose:
		case CMkill:
		case CMdir:
			fname = "ctl";
			ret = sendtoall (ch, a, n, offset);
			if (vflag) print ("executing cammand - done\n" );
			break; 
		} /* end switch : ctl file commands */

		poperror ();
		free (cb);
		return ret;
		break;

	case Qdata:
		fname = "stdio";
		return sendtoall (ch, a, n, offset);
	} /* end switch : on filename */

	return n;
}

static int
cmdwstat (Chan *c, uchar *dp, int n)
{
	Dir *d;
	Conv *cv;

	switch (TYPE (c->qid)){
	default:
		error (Eperm);
	case Qctl:
	case Qdata:
	case Qstderr:
		d = malloc (sizeof (*d)+n);
		if (d == nil)
			error (Enomem);
		if (waserror ()){
			free (d);
			nexterror ();
		}
		n = convM2D (dp, n, d, (char*)&d[1]);
		if (n == 0)
			error (Eshortstat);
		cv = cmd.conv[CONV (c->qid)];
		if (!iseve () && strcmp (up->env->user, cv->owner) != 0)
			error (Eperm);
		if (!emptystr (d->uid))
			kstrdup (&cv->owner, d->uid);
		if (d->mode != ~0UL)
			cv->perm = d->mode & 0777;
		poperror ();
		free (d);
		break;
	}
	return n;
}

static Conv *
cmdclone (char *user)
{
	Conv *c, **pp, **ep;
	int i;

	if (waserror ()){
		nexterror ();
	}

	poperror ();
	/* - end change */

	c = nil;
	ep = &cmd.conv[cmd.maxconv];
	for (pp = cmd.conv; pp < ep; pp++) {
		c = *pp;
		if (c == nil) {
			c = malloc (sizeof (Conv));
			if (c == nil)
				error (Enomem);
			qlock (&c->l);
			c->inuse = 1;
			c->x = pp - cmd.conv;
			cmd.nc++;
			*pp = c;
			break;
		}
		if (canqlock (&c->l)){
			if (c->inuse == 0 && c->child == nil)
				break;
			qunlock (&c->l);
		}
	}

	if (pp >= ep) {
		return nil;
	}

	c->inuse = 1;
	kstrdup (&c->owner, user);
	kstrdup (&c->dir, rootdir);
	c->perm = 0660;
	c->state = "Closed";
	for (i=0; i<nelem (c->fd); i++)
		c->fd[i] = -1;

	
	/* allocating structure to store info about 
	remote job resources */
	c->rjob = malloc (sizeof (RemoteJob)); 
	wlock (&c->rjob->l);
	c->rjob->x = c->x;
	c->rjob->rjobcount = 0;
	c->rjob->first = NULL;
	c->rjob->last = NULL;
	wunlock (&c->rjob->l);
	qunlock (&c->l);
	return c;
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


/* Reads from specified channel.
	It works on both directory and file channels.
*/
long
readfromchan (Chan *rc, void *va, long n, vlong off)
{
	int dir;
	Lock *cl;

	if (waserror ()){
		if (vflag) print ("Error in readChan\n");
		nexterror ();
	}

	if (n < 0)
		error (Etoosmall);

	dir = rc->qid.type & QTDIR;
	if (dir && rc->umh) {
		n = unionread (rc, va, n);
	}
	else{
		n = devtab[rc->type]->read (rc, va, n, off);
	}

	poperror ();
	return n;
}

/* copied this structure from other file */
enum
{
	DIRSIZE = STATFIXLEN + 32 * 4,
	DIRREADLIM = 2048,	/* should handle the largest reasonable directory entry */
};

/* read from Chanel of type dir */
long readchandir (Chan *dirChan, Dir **d)
{
	uchar *buf;
	long ts;

	*d = nil;
	if (waserror ())
		return -1;
	buf = malloc (DIRREADLIM);
	if (buf == nil)
		error (Enomem);
	if (waserror ()){
		free (buf);
		nexterror ();
	}
	ts = readfromchan (dirChan, buf, DIRREADLIM, 0);
	if (ts > 0) {
		ts = dirpackage (buf, ts, d);
	}

	poperror ();
	free (buf);
	poperror ();
	return ts;
}


/* Returns the list of Dir entries for given directory name */
long
lsdir (char *name, Dir **d) 
{
	long count;
	volatile struct { Chan *cc; } rock;

	if (waserror ()){
		if (vflag) print ("Cant open [%s]\n", name);
		nexterror ();
	}

	rock.cc = namec (name , Aopen, OREAD, 0); 
	poperror ();

	if (waserror ()){
		if (vflag) print ("readchandir failed\n");
		cclose (rock.cc);
		nexterror ();
	}

	count = readchandir (rock.cc, d);
	poperror ();
	return count;
}


/* returns string containing path to parent,
	and path will end with '/'
   memory should be released by caller */
char *
fetchparentpathch (Chan * ch) 
{
	char * ptr;
	char buff[KNAMELEN*3];

	sprint (buff, "%s/%ld/", BASE, CONV(ch->qid));
	ptr = malloc ((sizeof(char)) * strlen(buff)+2);
	if (ptr == NULL) {
		return NULL;
	}
	strcpy (ptr, buff);
	if (vflag) print ("filepath for qid[%ld] is [%s][%s]\n", ch->qid.path,buff, ptr);
	return ptr ;
}

/* returns string containing path to parent,
	and path will end with '/'
   memory should be released by caller */
char *
fetchparentpath (char *filepath) 
{
	char *ptr;
	int i, len;

	ptr = strdup (filepath);
	if (ptr == NULL) {
		return NULL;
	}
	len = strlen (ptr);
	/* ignoring last / in case of directory entry */
	if (ptr[len-1] == '/') {
		ptr[len-1] = 0;
		--len;
	}
	for (i=len - 1; ptr[i] != '/' && i > 0; --i) {
		ptr[i] = 0;
	}
	if (i == 0) { /* not valid file */
		return NULL;
	}
	return ptr;
}

