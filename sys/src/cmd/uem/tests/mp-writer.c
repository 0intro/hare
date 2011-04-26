#include <u.h>
#include <libc.h>

int chatty = 0;
char *mppath;

static void
usage(void)
{
	fprint(2, "mp-writer [-D] [-e] [-n num-times] [-s testsize] [-i streamin] [-o streamout] <mpipe-data-path>\n");
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

/* pkthdr format:
	type: char {p, <, >}
	size: data size {for payload}
	which: which slot, 0 for don't care
	path: for streaming
*/
	
int
streamout(int fd, ulong which, char *path)
{
	int n; 
	char hdr[255];
	ulong tag = ~0;
	char pkttype='>';

	/* header byte is at offset ~0 */
	n = snprint(hdr, 31, "%c\n%lud\n%lud\n%s\n", pkttype, (ulong)0, which, path);
	n = pwrite(fd, hdr, n+1, tag);
	
	return n;
}

int
streamin(int fd, ulong which, char *path)
{
	int n; 
	char hdr[255];
	ulong tag = ~0;
	char pkttype='<';

	/* header byte is at offset ~0 */
	n = snprint(hdr, 31, "%c\n%lud\n%lud\n%s\n", pkttype, (ulong)0, which, path);
	n = pwrite(fd, hdr, n+1, tag);
	
	return n;
}

int
pipewrite(int fd, char *data, ulong size, ulong which)
{	
	int n; 
	char hdr[255];
	ulong tag = ~0;
	char pkttype='p';

	/* header byte is at offset ~0 */
	n = snprint(hdr, 31, "%c\n%lud\n%lud\n\n", pkttype, size, which);
	n = pwrite(fd, hdr, n+1, tag);
	if(n <= 0)
		return n;

	return write(fd, data, size);
}

void
main(int argc, char *argv[])
{
	int times=1;
	int size=1024;
	int which=1;
	int count;
	int n;
	int fd;
	char *d = nil;
	char *sin = nil;
	char *sout = nil;

	ARGBEGIN{
	case 'D':
		chatty++;
		break;
	case 'n':
		times = atoi(ARGF());
		break;
	case 'e':		/* add an enumerator to front of buffer */
		which = atoi(ARGF());
		break;
	case 's':
		size = atoi(ARGF());
		break;
	case 'i':
		sin = ARGF();
		break;
	case 'o':
		sout = ARGF();
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

	if(sin) {
		streamin(fd, which, sin);
		goto out;
	}
	if(sout) {
		streamout(fd, which, sout);
		goto out;
	}
	
	d = prepdata(size);

	for(count = 0; count < times; count++) {
		if(chatty)
			fprint(2, "writing %d time\n", count);
		n = pipewrite(fd, d, size, which);
		if(n != size) {
			fprint(2, "pipe write failed: %d: %r\n", n);
			exits("pipe failed");
		}	
	}
out:
	if(chatty)
		fprint(2, "closing and exiting\n");

	free(d);
	close(fd);
	exits("done");
}
