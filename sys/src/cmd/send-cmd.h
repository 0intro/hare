#ifndef __send_cmd__
#define __send_cmd__ 1

#include <u.h>
#include <libc.h>

char *MPOINT = "/n/anl/";
char *CMD = "rc";
char *HOME_DIR = "/home/ericvh";

void
cat  (int f, char *s, char *msg)
{
	char buf[8192];
	long n;
	int logfd = 2;
	int first = 1;

	while((n=read(f, buf, (long)sizeof buf))>0){
		
		/* showing first message */
		if (n>0) {
			if (first) {
				if (msg) {
					print ("%s\n", msg);
				}
				first = 0;
			}
		}
		if(write(1, buf, n)!=n){
			fprint(logfd, "%ld %d %s outputing error %s: %r\n", time(0), getpid(), argv0, s);
			sysfatal("write error reading %s: %r", s );
		}
	}
	if(n < 0){
		fprint(logfd, "%ld %d  %s error reading: %s %r", time(0), getpid(), argv0, s);
		sysfatal("error reading %s: %r", s );
	}
}


void
execute_cmd (char *cmd)
{
	int clonefd;
	char path[1024];
	char buf[1024];
	int n;
	int session_id;
	int iofd;
	int i;
	
	snprint (path, sizeof(path), "%s/cmd/clone", MPOINT );
	clonefd = open(path, ORDWR);


	n = read(clonefd, buf, (long)sizeof buf);
	if ( n <= 0) {
		sysfatal ("can't read from clone file :%r");
	}

	buf[n] = 0;
	session_id = atoi (buf);
	if ( session_id < 0) {
		sysfatal ("getting invalid session id %d [%s]: %r", session_id, buf);
	}

	/* changing dir to working dir */
	snprint (buf, sizeof(buf), "dir %s", HOME_DIR );
	n = write (clonefd, buf, strlen(buf));

	if (n < 0 ) {
		sysfatal ("write failed : %r");
	}
	
	snprint (buf, sizeof(buf), "exec %s", cmd );
	n = write (clonefd, buf, strlen(buf));
	if (n < 0 ) {
		sysfatal ("write failed : %r");
	}

	/* read and display the content of stdio file */

	snprint (path, sizeof(path), "%s/cmd/%d/data", MPOINT, session_id );
	iofd = open(path, ORDWR);

	if(iofd < 0)
		sysfatal("can't open %s: %r", path);
	else{
		cat (iofd, path, nil );
		close (iofd);
	}

	/* read and display the content of stderr file */

	snprint (path, sizeof(path), "%s/cmd/%d/stderr", MPOINT, session_id );
	iofd = open(path, OREAD);

	if(iofd < 0)
		sysfatal("can't open %s: %r", path);
	else{
		cat (iofd, path, "STDERR content" );
		close (iofd);
	}

	close(clonefd);
}

#endif /* __send_cmd__ */