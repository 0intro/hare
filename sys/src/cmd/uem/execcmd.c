/*
	execcmd - proc file system execution wrapper wrapper

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

	Description:
		This provides a small wrapper the file system can use
		to boot strap the application while maintaining expected
		clone semantics.

	TODO:
		* fork namespace after mounting mpipes
		* support for ns configuration prior to exec

*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <debug.h>

static char defaultpath[] =	"/proc";
static char *procpath, *mmount;

char Estdios[] = "execcmd: couldn't open stdios";
char Ebadctl[] = "execcmd: bad ctl command";
char Ebadexec[] = "execcmd: bad exec command";

/* using parsecmd for later expansion */
enum
{
	Cexec,
};

Cmdtab ctltab[]={
	Cexec,	"exec", 0,
};	

enum {	/* DEBUG LEVELS (complimentary with execfs */
	DERR = 0,	/* error */
	DCUR = 1,	/* current - temporary trace */
	DFID = 2,	/* fid tracking */
	DEXC = 3,	/* debug execcmd */
	DARG = 10,	/* arguments */
};

/* FIXME: debugging... */
enum
{
	STACK = (8 * 1024),
};
char *extpath = 0;
static void
lsproc(void *arg)
{
	Channel *pidc = arg;
	// FIXME: procpath currently not used
	char *path = procpath;

	if (extpath)
		path = extpath;

//	Channel *pidc = chancreate(sizeof(ulong), 0);
//	char *path = arg;

	threadsetname("exec2fs-lsproc");
	DPRINT(DFID, "lsproc (%p) (%s) (%s) (%s) (%s) pid=(%d): %r\n",
	       pidc, "/bin/ls", "ls", "-l", path, getpid());
	procexecl(pidc, "/bin/ls", "ls", path, nil);

	sendul(pidc, 0);

	threadexits(nil);

}

static void
usage(void)
{
	fprint(2, "execcmd [-D] [-v debuglevel] [-s srvctl] [-L logdir]\n");
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	int stdinfd;
	int stdoutfd;
	int stderrfd;
	int ctlfd;
	int ret;
	char *ctlbuf = emalloc9p(255);
	char *fname = emalloc9p(255);
	Cmdbuf *cb;
	Cmdtab *cmd;
	char *srvctl = nil;
	char *logfile;
	char *logdir = nil;
	
	int pid = getpid();

	procpath = defaultpath;
	mmount = defaultpath;

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'v':
		vflag = atoi(ARGF());
		assert(vflag >= 0);
		break;
	case 's':
		srvctl = ARGF();
		break;
	case 'm':	/* mntpt override */
		procpath = ARGF();
		break;
	case 'M':	/* mntpt override */
		mmount = ARGF();
		break;
	case 'L':
		logdir = ARGF();
		break;
	default:
		/* break */
		usage();
		assert(0);
	}ARGEND

	assert(argc == 0);

	/* move the  log file generation outside the test directory */
	if(vflag > 0) {
		if(logdir)
			logfile = smprint("%s/execcmd-%d.log", logdir, getpid());
		else
			logfile = smprint("execcmd-%d.log", getpid());
		debugfd = create(logfile, OWRITE, 0666);
		assert(debugfd > 2);

		//sleep(100);
		DPRINT(DEXC, "Main: logfile=%s\n", logfile);
		free(logfile);
	} 

	DPRINT(DEXC, "Inside execution wrapper: logdir=%s vflag=%d debugfd=%d\n", logdir, vflag, debugfd);
	DPRINT(DEXC, "\tsrvctl=%s\n", srvctl);
	DPRINT(DEXC, "\tprocpath=%s\n", procpath);
	DPRINT(DEXC, "\tmmount=%s\n", mmount);

	/* open our files */
	ctlfd = open(srvctl, ORDWR|OCEXEC);
	assert(ctlfd>= 0);
	remove(srvctl);

	DPRINT(DEXC, "After srv ctl open\n");

	ret = read(ctlfd, ctlbuf, 255);
	assert(ret > 0);
	ctlbuf[ret] = 0;

	DPRINT(DEXC, "got ret=(%d) ctlbuf=(%s) pid=(%d)\n", ret, ctlbuf, pid);

	/* need to open our bit */
	snprint(fname, 255, "%s/%d/stdin", mmount, pid);
	stdinfd = open(fname, OREAD);
	DPRINT(DCUR, "stdin=(%s) stdinfd=(%d)\n", fname, stdinfd);
	assert(stdinfd > 0);

	snprint(fname, 255, "%s/%d/stdout", mmount, pid);
	stdoutfd = open(fname, OWRITE);
	DPRINT(DCUR, "stdout=(%s) stdoutfd=(%d)\n", fname, stdoutfd);
	assert(stdoutfd > 0);

	snprint(fname, 255, "%s/%d/stderr", mmount, pid);
	stderrfd = open(fname, OWRITE);
	DPRINT(DCUR, "stderr=(%s) stderrfd=(%d)\n", fname, stderrfd);
	assert(stderrfd > 0);

	DPRINT(DEXC, "after opening stdio pipes\n");

	if((stdinfd < 0)||(stdoutfd < 0)||(stderrfd < 0)) {
		DPRINT(DEXC, "some sort of issue opening stdio pipes\n");

		write(ctlfd, Estdios, strlen(Estdios));
		exits("stdios");
	}

	DPRINT(DEXC, "reporting partial success\n");

	ret = write(ctlfd, "1", 1);	/* report partial sucess */
	assert(ret > 0);


	DPRINT(DEXC, "waiting for cmds\n");
	while((ret = read(ctlfd, ctlbuf, 255)) > 0) {
		DPRINT(DEXC, "received cmd \"%s\"\n", ctlbuf);

		cb = parsecmd(ctlbuf, ret);
		cmd = lookupcmd(cb, ctltab, nelem(ctltab));
		if(cmd == nil) {
			DPRINT(DERR, "ERROR: bad ctl\n");
			write(ctlfd, Ebadctl, strlen(Ebadctl));
			continue;
		}

		switch(cmd->index){
		case Cexec:
			if(cb->nf < 2) {
				DPRINT(DERR, "ERROR: bad exec\n");
				write(ctlfd, Ebadexec, strlen(Ebadexec));
				continue;
			}
			DPRINT(DEXC, "exec, duping and going\n");

			dup(stdinfd, 0);
			dup(stdoutfd, 1);
			dup(stderrfd, 2);
			close(debugfd);
			debugfd = 2;
			free(fname);
			free(ctlbuf);

//exits(0);			
			/* go baby go */
			exec(cb->f[1], &cb->f[1]);

// FIXME: test this fail over...

			/* something happened report it */
			DPRINT(DERR, "ERROR:  something happened with the exec\n");
			ctlbuf = smprint("execcmd: exec: %r");
			write(ctlfd, ctlbuf, ret);

			/* make sure that we will not try to free fname twice... */
			fname = nil;
			goto error;
			break;
		/* room for more commands later */
		}		
	}
error:
	free(fname);
	free(ctlbuf);
	close(ctlfd);
	close(stdinfd);
	close(stdoutfd);
	close(stderrfd);
	exits("ctl abort");
}
