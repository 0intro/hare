/*
  stat_run - a simple front end to gangfs to help with timing tests

  prerun "rc scale 1" to start things up

	Copyright (C) 2010, IBM Corporation, 
 		John (EBo) David (jldavid@us.ibm.com)
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
#include "timeit.h"

static void
usage(void)
{
	fprint(2, "stat_run [-D] [-v debug_level] [-N num_tasks] [-m mtpt] [-t tmpfile] [-l log_file] command\n");

	exits("usage");
}

static int
mountgang(char *srv, char *path, char *name)
{
	int fd, ret;
	char *srvpt = smprint("/srv/%s", srv);
	fd = open(srvpt, ORDWR);
	if(fd<0) {
		DPRINT(DERR, "*ERROR*: couldn't open %s: %r\n", srvpt);
		free(srvpt);
		return -1;
	}
	free(srvpt);

	DPRINT(DFID, "mountgang: mounting name=(%s) on path=(%s) pid=(%d) fd=%d\n",
	       name, path, getpid(), fd);
	ret = mount(fd, -1, path, MBEFORE|MCREATE, name);
	if(ret<0)
		DPRINT(DERR, "\t*ERROR*: mpipe mount failed: %r\n");

	close(fd);
	return ret;
}

void
main(int argc, char **argv)
{
	char *command, *tmpfile="/tmp/tmp1", *procpath="/n/io/proc";
	char *srvnm="io";
	char *log_file=nil;
	int i, num_tasks=1;
	char *x;
	int p[2];
	char line[1024];

	ARGBEGIN{
	case 'D':
		chatty9p++;
		break;
	case 'v':
		x = ARGF();
		if(x)
			vflag = atol(x);
		break;
	case 'm':	/* mntpt override */
		procpath = ARGF();
		break;
	case 'l':
		log_file = ARGF();
		break;
	case 't':
		tmpfile = ARGF();
		break;
	case 'N':
		num_tasks = atoi(ARGF());
		break;
	default:
		usage();
	}ARGEND

	if(argc <= 0)
		usage();

	command = estrdup9p(argv[0]);
	for(i=1; i<argc; i++){
		command = erealloc9p(command, strlen(command)+strlen(argv[i])+2);
		strcat(command, " ");
		strcat(command, argv[i]);
	}

	DPRINT(DCUR, "Main: command=(%s)\n", command);
	DPRINT(DCUR, "\ttmpfile=(%s)\n", tmpfile);
	DPRINT(DCUR, "\tprocpath=(%s)\n", procpath);
	DPRINT(DCUR, "\tnum_tasks=(%d)\n", num_tasks);

	mountgang(srvnm, "/n/io", "");

	pipe(p);

	timeit_init();
	if(log_file)
		timeit_setout(log_file);

	int gfdi, gfdo=-1;
	snprint(line, 1024, "%s/gclone", procpath);
	if((gfdi=open(line,ORDWR)) < 0){
		DPRINT(DERR, "*ERROR*: could not open gclone file (%s)\n",  line);
		exits("could not open gclone");

	}

	stamp("BEGIN");

	fprint(gfdi, "res %d", num_tasks);

	stamp("RES");

	int gnum;
	int n = read(gfdi, line, 256);
	if(n<0){
		DPRINT(DERR, "*ERROR*: read failed\n");
		exits("could not read clone file");
	}
	gnum = atoi(line);
	//scanf(gfdi, "", &gnum);
	print("gnum = %d\n", gnum);

	// FIXME: find a better way to find the gang ID.  R/W gfdi returns 0
	int gpid;
	for(gpid=0; gpid<100; gpid++){
		snprint(line, 1024, "%s/g%d/stdout", procpath, gpid);
		gfdo = open(line,OREAD);
		if(gfdo > 0)
			break;
	}
	if(gfdo < 0){
		DPRINT(DERR, "*ERROR*: could not open stdout file (%s)\n", line);
		exits("could not open stdout");
	}
	stamp("finish opening stdout");

	fprint(gfdi, "exec %s", command);
	
	stamp("EXEC");

	smprint(line, 1024, "%s", tmpfile);
	int tmpfd;
	if((tmpfd=open(line,OWRITE)) < 0){
		DPRINT(DERR, "*ERROR*: could not open tmpfile (%s)\n",  line);
		exits("could not open tmpfile");

	}

//		dup(p[1], 1);
//		close(p[0]);
//		close(p[1]);

	// FIXME: there is a better way to do this than here.
//	do{
		n = read(gfdo, line, 1024);
		fprint(2,"READ: %d (%s)\n", n, line);
		//n = write(tmpfd, line, n);
		n = write(2, line, n);
		fprint(2,"WRITE: %d\n", n);
//	} while(n>0);

	stamp("OUT");

	stamp("TERM");

	timeit_end();

	close(tmpfd);
}

