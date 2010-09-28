
/* 
8c fdecho.c &&
8l -o fdecho fdecho.8 &&
cp fdecho $home/bin/386 &&
8.out
*/
#include <u.h>
#include <libc.h>

int logfd;

void
main(int argc, char *argv[])
{
	int nflag;
	int i, len;
	char *buf, *p;

	logfd = open("/tmp/xcpu.log", OWRITE);
	nflag = 0;
	if(argc > 1 && strcmp(argv[1], "-n") == 0)
		nflag = 1;

	len = 1;
	for(i = 1+nflag; i < argc; i++)
		len += strlen(argv[i])+1;

	buf = malloc(len);
	if(buf == 0)
		exits("no memory");

	p = buf;
	for(i = 1+nflag; i < argc; i++){
		strcpy(p, argv[i]);
		p += strlen(p);
		if(i < argc-1)
			*p++ = ' ';
	}
		
	if(!nflag)
		*p++ = '\n';
	fprint(logfd, "%ld %d %s %*s\n", time(0), getpid(), argv[0], p-buf, buf);
	if(write(1, buf, p-buf) < 0){
		fprint(2, "echo: write error: %r\n");
		exits("write error");
	}

	exits((char *)0);
}
