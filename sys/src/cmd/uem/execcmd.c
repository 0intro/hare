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

void
main(int argc, char **argv)
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
	
	int pid = getpid();

	switch(argc){
		case 3:
			vflag = atoi(argv[2]);
			if(vflag) {
				logfile = smprint("execcmd-%d.log", getpid());
				debugfd = create(logfile, OWRITE, 0666);
				assert(debugfd > 2);
			} 
		case 2:
			srvctl = argv[1];
			break;
		default:
			/* break */
			assert(0);
	}

	DPRINT(DEXC, "Inside execution wrapper: vflag=%d debugfd=%d\n", vflag, debugfd);

	/* open our files */
	ctlfd = open(srvctl, ORDWR|OCEXEC);
	assert(ctlfd>= 0);
	remove(srvctl);

	DPRINT(DEXC, "After srv ctl open\n");

	ret = read(ctlfd, ctlbuf, 255);
	assert(ret > 0);
	ctlbuf[ret] = 0;

	DPRINT(DEXC, "got ctlbuf: (%d) %s\n", ret, ctlbuf);

	/* need to open our bit */
	snprint(fname, 255, "/proc/%d/stdin", pid);
	stdinfd = open(fname, OREAD);
	assert(stdinfd > 0);
	snprint(fname, 255, "/proc/%d/stdout", pid);
	stdoutfd = open(fname, OWRITE);
	assert(stdoutfd > 0);
	snprint(fname, 255, "/proc/%d/stderr", pid);
	stderrfd = open(fname, OWRITE);
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
		cb = parsecmd(ctlbuf, ret);
		cmd = lookupcmd(cb, ctltab, nelem(ctltab));
		if(cmd == nil) {
			write(ctlfd, Ebadctl, strlen(Ebadctl));
			continue;
		}

		switch(cmd->index){
		case Cexec:
			if(cb->nf < 2) {
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
			
			/* go baby go */
			exec(cb->f[1], &cb->f[1]);

			/* something happened report it */
			ctlbuf = smprint("execcmd: exec: %r");
			write(ctlfd, ctlbuf, ret);
			free(ctlbuf);
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
