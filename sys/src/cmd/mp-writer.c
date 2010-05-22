#include <u.h>
#include <libc.h>

int chatty = 0;
char *mppath;

static void
usage(void)
{
	fprint(2, "mp-writer [-D] [-e] [-n num-times] [-s testsize] <mpipe-data-path>\n");
	exits("usage");
}

char *
prepdata(int size)
{
	char *buf = malloc(size);
	int count;

	for(count=0; count < size; count++)
		buf[count] = '0'+(count % 10);

	buf[size-1] = 0;
	return buf;
}

int
pipewrite(int fd, char *data, int size)
{	
	int n; 
	/* write size as a string, then write data */
	/* leaving a space should help make things more readable, and shouldn't hurt anything */
	n = fprint(fd, "%d        ", size);
	if(n <= 0)
		return n;

	return write(fd, data, size);
}

void
main(int argc, char *argv[])
{
	int times=5;
	int size=1024;
	int e=0;
	int count;
	char *d;
	int n;
	int fd;

	ARGBEGIN{
	case 'D':
		chatty++;
		break;
	case 'n':
		times = atoi(ARGF());
		break;
	case 'e':		/* add an enumerator to front of buffer */
		e++;
		break;
	case 's':
		size = atoi(ARGF());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	if(argc)
		mppath = argv[0];

	if(chatty)
		fprint(2, "initializing\n");

	fd = open(mppath, OWRITE);
	if(fd < 0) {
		fprint(2, "couldn't open file (%s): %r\n", mppath);
		exits("open pipe");
	}

	d = prepdata(size);

	for(count = 0; count < times; count++) {
		if(chatty)
			fprint(2, "writing %d time\n", count);
		if(e)
			snprint(d, size, "[time %8.8d] ", count);
		n = pipewrite(fd, d, size);
		if(n != size) {
			fprint(2, "pipe write failed: %d: %r\n", n);
			exits("pipe failed");
		}	
	}

	if(chatty)
		fprint(2, "closing and exiting\n");

	free(d);
	close(fd);
	exits("done");
}
