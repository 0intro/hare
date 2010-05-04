#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
/* 
8c xirf.c &&
8l -o xirf xirf.8 &&
cp xirf $home/bin/386


9c xirf.c &&
9l -o xirf xirf.o &&
cp xirf $HOME/bin
*/

Biobuf *bout;
Channel *out;
Channel *quit;
int nread = 0;

typedef struct Str Str;
struct Str {
	char *buf;
	int len;
};
int logfd;
char *prog;

void
reader(void *v)
{
	Ioproc *io;
	char *buf, *s, *last;
	int off;
	int fd;
	int n;
	Str *str;
	
	fd = (int)v;
	buf = malloc(8192);
	io = ioproc();
	off = 0;
loop:
	while((n = ioread(io, fd, buf+off, 8192-off)) >= 0){
		fprint(logfd, "%ld %d %s ioread fd %d n %d off %d buf+off %*s\n", time(0), getpid(), prog, fd, n, off, n+off, buf);
		n += off;
		off = 0;
		s = last = buf;
		for(;;){
			str = malloc(sizeof(Str));
			if(n <= 0){
				fprint(logfd, "%ld %d %s memmove(%p, %p, %d)\n", time(0), getpid(), prog, buf, last, off);
				memmove(buf, last, off);
				goto loop;
			}
			str->buf = last = s;
			str->len = off;
			while(*s++ != '\n' && n-- >= 0){
				fprint(logfd, "*s '%c' ", *(s-1));
				if(*(s-1)==0)
					*(s-1)=' ';
				str->len++;
			}
				if(s==0)
					*s=' ';			
			fprint(logfd, "*s '%c'\n", *s);
			if(n <= 0 && *s != '\n')
				off = s - last;
			sendp(out, str);
		}
	}
	fprint(logfd, "%ld %d %s final ioread fd %d n %d\n", time(0), getpid(), prog, fd, n);
	closeioproc(io);
	send(quit, nil);
}




void
threadmain(int argc, char **argv)
{
	int i;
	int *fd, nfd;
	char fdfile[512];
	char *mnt;
	Alt a[3];
	Str *s;

	prog = argv[0];
	logfd = open("/tmp/xcpu.log", OWRITE);
	if(logfd < 0)
		sysfatal("couldn't open log");
	
	mnt = "/n/csrv/parent/local";
	ARGBEGIN{
	
	}ARGEND;
	
	if(argc == 0)
		sysfatal("need input fds");
	nfd = open("/srv/csrv", ORDWR);
	if(nfd < 0)
		sysfatal("no /srv/csrv");
	if(mount(nfd, -1, "/n/csrv", MREPL, "") < 0)
		sysfatal("no xcpu namespace");	
	fd = malloc(argc*sizeof(int));
	for(i = 0; i < argc; i++){
		fprint(logfd, "%ld %d %s opening %s\n",time(0), getpid(), prog,  argv[i]);
		fd[i] = open(argv[i], OREAD);
		if(fd[i] < 0){
			fprint(logfd, "%ld %d %s couldn't open arg %s %d\n",time(0), getpid(),  prog, fdfile, i);
			sysfatal("couldn't open arg %s %d", fdfile, i);
		}
	}
	bout = Bopen("/fd/1", OWRITE);
	
	out = chancreate(sizeof(char*), 0);
	quit = chancreate(sizeof(char*), 0);
	
	for(i = 2; i < argc; i++){
		nread++;
		fprint(logfd, "%ld %d %s creating reader fd %d\n",time(0), getpid(), prog, fd[i]);
		threadcreate(reader, (void*)fd[i], 8192);
	}
	a[0].op = CHANRCV;
	a[0].c = quit;
	a[0].v = nil;
	a[1].op = CHANRCV;
	a[1].c = out;
	a[1].v = &s;
	a[2].op = CHANEND;
	
	for(;;) switch(alt(a)){
	case 0:
		fprint(logfd, "%ld %d %s dead reader\n", time(0), getpid(), prog);
		nread--;
		if(nread <= 0)
			goto done;
		break;
	case 1:
		s->buf[s->len]=0;
		fprint(logfd, "%ld %d %s writer: %s\n", time(0), getpid(), prog, s->buf);
		Bwrite(bout, s->buf, s->len);
		free(s);
		break;
	}
done:
	Bflush(bout);
	threadexitsall(nil);
}
