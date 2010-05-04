
/* 
8c pxcpu.c &&
8l -o pxcpu pxcpu.8 &&
cp pxcpu $home/bin/386 &&
mount -nc /srv/csrv /n/csrv && xsdo 

9c pxcpu.c &&
9l -o pxcpu pxcpu.o &&
cp pxcpu $HOME/bin
*/
#include <u.h>
#include <libc.h>
#include <bio.h>

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
main(int argc, char **argv)
{
	Biobuf *in, *stdout;
	int n, i, j, nres;
	int cfd, *resfdctl, fd;
	char fdfile[512];
	char buf[8192], buf2[8192];
	char *s, *e, *line;
	char *a[4], **topo, *c[64];
	

	mnt = "/n/csrv/local";
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
	in = Bopen("/fd/0", OREAD);
	if(in == nil)
		sysfatal("couldn't open stdin");
	line = Brdstr(in, '\n', '\n');
	if(line == nil)
		sysfatal("eof");
	n = tokenize(line, a, 4);
	if(n != 2)
		sysfatal("two fields in a res");
	if(strcmp("res", a[0]) != 0)
		sysfatal("need to res in the beginning");
	fprint(cfd, "res %s", a[1]);

	nres = atoi(a[1]);
	if(nres <= 0)
		sysfatal("need >0 res");
	topo = malloc(nres*sizeof(char*));
	resfdctl = malloc(nres*sizeof(int));
	snprint(fdfile, 512, "%s/%s/topology", mnt, sess);
	fd = open(fdfile, OREAD);
	n = read(fd, buf2, 8192);
	if(n < 0)
		sysfatal("couldn't read topology");
	buf[n]=0;
	n = getfields(buf2, topo, nres, 1, "\t\r\n ");
	if(n != nres)
		sysfatal("topology != nres");
	for(i = 0; i < nres; i++){
		snprint(fdfile, 512, "%s/%s/%s/ctl", mnt, sess, topo[i]);
		print("fdfile %s\n", fdfile);
		resfdctl[i]=open(fdfile, ORDWR);
		if(resfdctl[i] < 0)
			sysfatal("couldn't open part of res");
	}
	while((line = Brdstr(in, '\n', '\n')) != nil){
		n = getfields(line, a, 4, 1, "\t\r\n ");
		if(strcmp("splice", a[0]) == 0){
			if(n != 3)
				sysfatal("splice has 2 args");
			i = atoi(a[2]);
			if(i < 0 || nres <= i)
				sysfatal("bad splice out res");
			j = atoi(a[1]);
			if(j < 0 || nres <= j)
				sysfatal("bad io endpoint");			
			fprint(resfdctl[i], "splice csrv/parent/parent/local/%s/%s/stdio", sess, topo[j]);
		}else if(strcmp("exec", a[0]) == 0){
			if(n < 3)
				sysfatal("exec needs >3 args");
			i = atoi(a[1]);
			if(i < 0 || nres <= i)
				sysfatal("bad splice out res");
			if(strcmp("filt", a[1]) == 0){
				n = tokenize(a[3], c, 64);
				s = buf;
				e = buf+8192;
				for(i = 0; i < n; i++){
					j = atoi(c[i]);
					if(j < 0 || nres <= j)
						sysfatal("bad splice out res");
					s = seprint(s, e, " %s/%s/%s/stdio", mnt, sess, topo[j]);
				}
				fprint(resfdctl[i], "exec %s %s", a[2], s);
			}else if(n == 4)
				fprint(resfdctl[i], "exec %s %s", a[2], a[3]);
			else
				fprint(resfdctl[i], "exec %s", a[2]);
		}else if(strcmp("end", a[0]) == 0){
			if(n != 2)
				sysfatal("end needs 1 arg");
			i = atoi(a[1]);
			if(i < 0 || nres <= i)
				sysfatal("bad io endpoint");		
			snprint(fdfile, 512, "%s/%s/%s/stdio", mnt, sess, topo[i]);
			print("end fdfile %s\n", fdfile);
			print("i %d topo[i] %s\n", i, topo[i]);
			print("reading from %s\n", fdfile);
			fd = open(fdfile, OREAD);
			if(fd < 0)
				sysfatal("couldn't open: %s %r", fdfile);
		}else
			sysfatal("invalid command string");
	}
	stdout = Bopen("/fd/1", OWRITE);
	if(stdout == nil)
		sysfatal("couldn't open stdout");
	print("reading\n");
	while((n = read(fd, buf, 8192)) > 0){
		print("read n %d %.*s\n", n, n, buf);
		write(1, buf, n);
		Bflush(stdout);
	}
	print("read n %d %.*s\n", n, n, buf);
	close(fd);
	Bterm(in);
	exits(0);
}

