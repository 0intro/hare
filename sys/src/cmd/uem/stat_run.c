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
	char *command, *procpath="/n/io/proc";
	//char *tmpfile="/tmp/tmp1";
	//char *tmpfile="tests/.test/tmp1";
	char *tmpfile=nil;
	char *srvnm="io";
	char *log_file=nil;
	int i, num_tasks=1;
	char *x;
	int loop=0, use_cycles=0;
	char line[1024], gstdout[1024];

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
	case 'L':
		loop = 1;
		break;
	case 't':
		tmpfile = estrdup9p(ARGF());
		break;
	case 'N':
		num_tasks = atoi(ARGF());
		break;
	case 'C':
		use_cycles = 1;
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

	DPRINT(DARG, "Main: command=(%s)\n", command);
	DPRINT(DARG, "\ttmpfile=(%s)\n", (tmpfile!=nil)?tmpfile:"<nil>");
	DPRINT(DARG, "\tprocpath=(%s)\n", procpath);
	DPRINT(DARG, "\tnum_tasks=(%d)\n", num_tasks);

	mountgang(srvnm, "/n/io", "");

/*
uvlong ftime, stime;
uvlong ttime;
slice(ftime);
sleep(10000);
slice(stime);
ttime = (stime-ftime)/10;
fprint(2,"f/stime = %ulld\n", ttime);
*/

	timeit_init();
	timeit_settype(TIMIT_DEFAULT);
	timeit_setcycles(use_cycles);

	if(log_file)
		timeit_setout(log_file);

	if(loop)
		loop = num_tasks;
	if(num_tasks<=0){
		DPRINT(DERR, "*ERROR*: num_tasks must be greater than 0\n");
		exits("invalid num_tasks");
	}

	do{
		int gfdi, gfdo;
		int gnum, n;
		int tmpfd=1;

		snprint(line, 1024, "%s/gclone", procpath);
		if((gfdi=open(line,ORDWR)) < 0){
			DPRINT(DERR, "*ERROR*: could not open gclone file (%s)\n",  line);
			exits("could not open gclone");
		}
		
		if(loop)
			num_tasks = loop;
		
		print("tasks = %d\n", num_tasks);

		n = read(gfdi, line, 256);
		if(n<0){
			DPRINT(DERR, "*ERROR*: read failed\n");
			exits("could not read clone file");
		}
		line[n] = 0;
		gnum = atoi(line);
		DPRINT(DFID, "Main: gclone number = g%d\n", gnum);
		DPRINT(DFID, "\tnum_tests=%d\n", num_tasks);
		stamp("BEGIN");
		
		fprint(gfdi, "res %d", num_tasks);
		
		stamp("RES");
		
		snprint(gstdout, 1024, "%s/g%d/stdout", procpath, gnum);
		gfdo = open(gstdout,OREAD);
		if(gfdo < 0){
			DPRINT(DERR, "*ERROR*: could not open stdout file (%s)\n", gstdout);
			exits("could not open stdout");
		}
		
		stamp("finish opening stdout");
		
		
		fprint(gfdi, "exec %s", command);
		
		stamp("EXEC");
		
		if(tmpfile!=nil){
			sprint(line, "%s", tmpfile);
			print("opening tmpfile %s\n", line);
			if((tmpfd=open(line,OWRITE|OTRUNC)) < 0){
				if((tmpfd=create(line,OWRITE,0666)) < 0){
					DPRINT(DERR, "*ERROR*: could not open tmpfile (%s)\n", line);
					exits("could not open tmpfile");
				}
			}
		}
		
/*
	// FIXME: bind sais this is inconsistent
	if(tmpfile!=nil){
		int ret;
		//close(gfdo);
		ret = bind(tmpfile, gstdout, MBEFORE);
		if(ret < 0){
			DPRINT(DERR, "bind failed: %r\n");
			exits("bind failed");
		}
       	}
	dup(gfdo, 1);
*/

//		dup(tmpfd, 1);

//	int p[2];
//	pipe(p);
//	dup(p[1], 1);
//	close(p[0]);
//	close(p[1]);
		
		// FIXME: there is a better way to do this than here...
		DPRINT(DFID, "num_tasks=(%d) for output\n", num_tasks);
		for(i=0; i<num_tasks; i++){
			// read in a full buffer....
			n = read(gfdo, line, 1024);
			if(n<=0) break;
			
			n = write(tmpfd, line, n);
			if(n<=0) break;
		};
		
		stamp("OUT");

		// FIXME: close the ctl file timing...
		stamp("TERM");
		
		timeit_setfd(2);
		timeit_end();

		timeit_settype(TIMIT_FLOAT);

		timeit_tag_dt2_sec("BEGIN", "RES", "Time for stat_run RES");
		timeit_tag_dt2_sec("finish opening stdout", "EXEC", "Time for EXEC");
		timeit_tag_dt2_sec("EXEC", "OUT", "Time for OUT");
		timeit_tag_dt2_sec("OUT", "TERM", "Time for TERM");
		
		timeit_tag_dt2_sec("BEGIN", "OUT", "Total time");
		
		if(tmpfd>1)
			close(tmpfd);
		close(gfdi);
		close(gfdo);
		
		timeit_dump();
		fprint(2, "\n");

		loop /= 2;
		num_tasks = loop;
		
	} while (num_tasks>0);

	exits(0);
}
