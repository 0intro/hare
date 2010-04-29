
/* 
8c self.c &&
8l -o self self.8 &&
cp self $home/bin/386 &&
self
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
	int i, n;
	int fd;
	char fdfile[512];
	char *s;
	int *fds;
	char *mnt;
	
	prog = argv[0];
	logfd = open("/tmp/xcpu.log", OWRITE);
	mnt = "/n/csrv/local";
	ARGBEGIN{
	default:
		usage(argv[0]);
	}ARGEND;
	if(argc <= 0)
		usage("self");
	fd = open("/srv/csrv", ORDWR);
	if(fd < 0)
		sysfatal("no /srv/csrv");
	if(mount(fd, -1, "/n/csrv", MREPL, "") < 0)
		sysfatal("no xcpu namespace");
	fds = malloc(sizeof(int)*argc);
	for(i = 2; i < argc; i++){
		snprint(fdfile, 512, "%s/%s/%s/stdio", mnt, argv[0], argv[i]);
		fd = open(fdfile, OWRITE);
		if(fd < 0){
			s = smprint("couldn't open file: %s", fdfile);
			sysfatal(s);
		}
		fprint(logfd, "%ld %d %s fdfile %s i %d\n", time(0), getpid(),  prog, fdfile, i);
		fprint(fd, "%d\n", i);
		close(fd);
	}
	while(i = read(0, fdfile, 512) > 0)
		fprint(logfd, "%ld %d %s %*s\n", time(0), getpid(),  prog, i, fdfile); // just get rid of stuff to read
	exits(0);
}
