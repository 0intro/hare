
/* 
8c fdcat.c &&
8l -o fdcat fdcat.8 &&
cp fdcat $home/bin/386 &&
8.out
*/
#include <u.h>
#include <libc.h>

int logfd;


void
cat(int f, char *s)
{
	char buf[8192];
	long n;

	while((n=read(f, buf, (long)sizeof buf))>0){
		fprint(logfd, "%ld %d %s %*s\n", time(0), getpid(), argv0, n, buf);
		if(write(1, buf, n)!=n){
			fprint(logfd, "%ld %d %s write error copying %s: %r", time(0), getpid(), argv0, s);
			sysfatal("write error copying %s: %r", s);
		}
	}
	if(n < 0){
		fprint(logfd, "%ld %d %s error reading %s: %r", time(0), getpid(), argv0, s);
		sysfatal("error reading %s: %r", s);
	}
}

void
main(int argc, char *argv[])
{
	int f, i;

	logfd = open("/tmp/xcpu.log", OWRITE);
	argv0 = "cat";
	if(argc == 1)
		cat(0, "<stdin>");
	else for(i=1; i<argc; i++){
		f = open(argv[i], OREAD);
		if(f < 0)
			sysfatal("can't open %s: %r", argv[i]);
		else{
			cat(f, argv[i]);
			close(f);
		}
	}
	exits(0);
}