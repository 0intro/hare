#include <u.h>
#include <libc.h>
#include <bio.h>

Biobuf *in;
/* 
8c nlf.c &&
8l -o nlf nlf.8 &&
nlf
*/
/* modified djb2 */

void
nl(Biobuf **out, int narg)
{
	int h, len, c, i;
	char *buf, *b, *s;

	buf = malloc(8192);
	i = 0;
	h = 5381;
	s = buf;
	while((c = Bgetc(in)) != Beof){
		if(i >= 8192)
			sysfatal("line too long");
		if(c == '\n'){
			// XXX: really need to add ioprocs to bio.
			Bwrite(out[h%narg], s, i);
			i = 0;
			h = 5381;
			continue;
		}
		s[i++] = c;
		h = ((h << 5) + h) + c;
	}
	for(i = 0; i < narg; i++){
		Bflush(out[i]);
	}
}

int
main(int argc, char **argv)
{
	int i;
	Biobuf **out;
	char fd[64];
	
	ARGBEGIN{
	
	}ARGEND;
	
	if(argc == 0)
		sysfatal("need fd arguments");
	
	out = malloc(argc*sizeof(Biobuf*));
	for(i = 0; i < argc; i++){
		snprint(fd, 64, "/fd/%s", argv[i]);
		if((out[i] = Bopen(fd, OWRITE)) == nil)
			sysfatal("bad fd argument");
	}
	in = Bopen("/fd/0", OREAD);
	nl(out, argc);
}

