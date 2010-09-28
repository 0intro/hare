/* 
	Replace the functionality of the runJob script
	
	xcpu3 -n <number of runs> <cmd>
*/
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>

void reader(void*);

void
usage(char *me)
{
	fprint(2, "%s: usage\n", me);
	exits("usage");
}
char *mnt;
char sess[64];

void
threadmain(int argc, char **argv)
{
	int n;
	int cfd, dfd;
	char fdfile[512];

	mnt = "/csrv/local";
	ARGBEGIN{
	}ARGEND
	if(argc != 0)
		usage(argv[0]);

	snprint(fdfile, 512, "%s/clone", mnt);
	cfd = open(fdfile, ORDWR);
	n = read(cfd, sess, 64);
	if(n < 0)
		sysfatal("couldn't read ctl");
	sess[n]=0;
	
	/* TODO:
		res n nodes
		execute date
	*/
	
	n = fprint(cfd, "res 64\n");
	if(n < 0)
		sysfatal("couldn't send res: %r");	
	n = fprint(cfd, "exec date\n");
	if(n < 0)
		sysfatal("couldn't send exec: %r");	
	
	snprint(fdfile, 512, "%s/%s/stdio", mnt, sess);
	dfd = open(fdfile, OREAD);
	for(;;) {
		n = read(dfd, fdfile, 512);
		if(n<=0) {
			break;
		}
		fdfile[n] = 0;
		print("%s", fdfile);
	}

	exits(0);
}
