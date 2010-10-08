#include <u.h>
#include <libc.h>
#include <mpipe.h>

int nhash;

int
main(int argc, char **argv)
{
	int i;
	int *fds;
	
	ARGBEGIN{
	
	}ARGEND;
	
	fds = malloc(argc*sizeof(char*));
	for(i = 0; i < argc; i++)
		fds[i] = atoi(argv[i]);
	nfds = argc;
	nl(fds, argc);
	// gompipe(p)
}

/* modified djb2 */
ulong
hash(char *s, int n; int *len)
{
	ulong hash = 5381;
	int c;
	*len = 0;

	for(;(c = *s++) != '\n'; n--, *len++){
		if(n == 0){
			*len = -1;
			return 0;
		}
		h = ((h << 5) + h) + c;
	}

	return h % nfds;
}

void
nl(int *fds, int n)
{
	int n, h, len;
	char *buf, *b;
	buf = emalloc(8192);
	n = 0;
start:
	while((n = read(0, buf+n, 8192)) > 0){
		b = buf;
		while(n){
			h = hash(buf, n, &len);
			if(len == -1){
				memmove(buf, b, n);
				goto start;
			}
			write(fds[h], buf, len);
			n -= len;
			buf+=len;
		}
	}
	free(buf);
}