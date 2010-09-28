#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
/* 
8c irf.c &&
8l -o irf irf.8 &&
irf
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
	while((n = ioread(io, fd, buf+off, 8192-off)) > 0){
		if(n <= 0)
			break;
		s = buf;
		for(;;){
			str = malloc(sizeof(Str));
			if(n <= 0){
				off = s - last;
				memmove(buf, last, off);
				goto loop;
			}
			str->buf = last = s;
			str->len = 0;
			while(*s++ != '\n' && --n >= 0)
				str->len++;
			sendp(out, str);
		}
	}
	send(quit, nil);
}


void
writer(void *v)
{
	Alt a[3];
	Str *s;
	
	a[0].op = CHANRCV;
	a[0].c = quit;
	a[0].v = nil;
	a[1].op = CHANRCV;
	a[1].c = out;
	a[1].v = &s;
	a[2].op = CHANEND;
	
	for(;;) switch(alt(a)){
	case 0:
		nread--;
		if(nread <= 0)
			goto done;
		break;
	case 1:
		Bwrite(bout, s->buf, s->len);
		free(s);
		break;
	}
done:
	Bflush(bout);
}


void
threadmain(int argc, char **argv)
{
	int i;
	int *fd;
	
	ARGBEGIN{
	
	}ARGEND;
	
	if(argc == 0)
		sysfatal("need input fds");
	
	fd = malloc(argc*sizeof(int));
	for(i = 0; i < argc; i++)
		fd[i] = atoi(argv[i]);
	bout = Bopen("/fd/1", OWRITE);
	
	out = chancreate(sizeof(char*), 0);
	quit = chancreate(sizeof(char*), 0);
	
	for(i = 0; i < argc; i++){
		nread++;
		threadcreate(reader, (void*)fd[i], 8192);
	}
	proccreate(writer, nil, 8192);
}
