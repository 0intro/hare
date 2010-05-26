
/* 
8c list-res.c &&
8l -o list-res list-res.8 &&
cp list-res $home/bin/386 &&
8.out
*/
#include <u.h>
#include <libc.h>

char *MPOINT = "/n/anl/";
char *CMD = "qstat";

void
cat(int f, char *s)
{
	char buf[8192];
	long n;
	int logfd = 2;

	while((n=read(f, buf, (long)sizeof buf))>0){
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
main(int argc, char *argv[])
{
	int clonefd;
	char path[1024];
	char buf[1024];
	int n;
	int session_id;
	int iofd;
	
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

	snprint (buf, sizeof(buf), "exec %s", CMD );
	n = write (clonefd, buf, strlen(buf));

	
	/* read and display the content of stdio file */

	snprint (path, sizeof(path), "%s/cmd/%d/data", MPOINT, session_id );
	iofd = open(path, ORDWR);

	if(iofd < 0)
		sysfatal("can't open %s: %r", path);
	else{
		cat(iofd, path);
		close(iofd);
	}
	close(clonefd);
	exits(0);
}