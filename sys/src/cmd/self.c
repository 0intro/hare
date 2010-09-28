
/* 
8c self.c &&
8l -o self self.8 &&
cp self $home/bin/386

9c self.c &&
9l -o self self.o &&
cp self $HOME/bin
*/
#include <u.h>
#include <libc.h>

int logfd;
char *prog;

void
usage(char *me)
{
	fprint(2, "%s: usage\n", me);
}

void
main(int argc, char **argv)
{
	int i, fd;
	char buf[8192];
	
	prog = argv[0];
	logfd = open("/tmp/xcpu.log", OWRITE);
	ARGBEGIN{
	default:
		usage(argv[0]);
	}ARGEND;
	if(argc <= 0)
		usage("self");
	fd = open("/srv/ionode0", ORDWR);
	if(fd < 0)
		sysfatal("no /srv/csrv");
	if(mount(fd, -1, "/n/csrv", MREPL, "") < 0)
		sysfatal("no xcpu namespace");
	for(i = 0; i < argc; i++){
		fd = open(argv[i], OWRITE);
		if(fd < 0)
			sysfatal("couldn't open file: %s", argv[i]);
		fprint(logfd, "%ld %d %s argv[i] %s i %d\n", time(0), getpid(),  prog, argv[i], i);
		fprint(fd, "%d\n", i);
		close(fd);
	}
	while(i = read(0, buf, 8192) > 0)
		fprint(logfd, "%ld %d %s %*s\n", time(0), getpid(),  prog, i, argv[i]); // just get rid of stuff to read
	exits(0);
}
