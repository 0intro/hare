/*
	execcmd - proc file system execution wrapper wrapper

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

	Description:
		This provides a small wrapper the file system can use
		to boot strap the application while maintaining expected
		clone semantics.

	TODO:
		* open our own ctl file so we can pass through commands
			until we get going.
		* man page
		* support for ns configuration prior to exec

*/

#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

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

void
main(int argc, char **argv)
{
	int stdinfd, stdoutfd, stderrfd, ctlfd;
	int ret;
	char ctlbuf[255];
	Cmdbuf *cb;
	Cmdtab *cmd;
	char *srvctl;

	assert(argc == 2);
	srvctl = argv[1];

	/* open our files */
	ctlfd = open(srvctl, ORDWR|OCEXEC);
	assert(ctlfd>= 0);
	remove(srvctl);

	stdinfd = open("/n/stdin/data1", OREAD);
	stdoutfd = open("/n/stdout/data", OWRITE);
	stderrfd = open("/n/stderr/data", OWRITE);

	if((stdinfd < 0)||(stdoutfd < 0)||(stderrfd < 0)) {
		write(ctlfd, Estdios, strlen(Estdios));
		exits("stdios");
	}

	ret = write(ctlfd, "1", 1);	/* report partial sucess */
	if(ret < 0)
		exits("ctlfdack");

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

			dup(stdinfd, 0);
			dup(stdoutfd, 1);
			dup(stderrfd, 2);

			/* go baby go */
			exec(cb->f[1], &cb->f[1]);

			/* something happened report it */
			ret = snprint(ctlbuf, 255, "execcmd: exec: %r");
			write(ctlfd, ctlbuf, ret);
			goto error;
			break;
		/* room for more commands later */
		}		
	}

error:
	close(ctlfd);
	close(stdinfd);
	close(stdoutfd);
	close(stderrfd);
	exits("ctl abort");
}
