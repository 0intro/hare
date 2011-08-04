/* this is a stupid little hack to date to make sure it is working for gnagfs on the BG
 */

#include <u.h>
#include <libc.h>

int uflg, nflg;

static char Etomany[] =	"to many arguments";

void
main(int argc, char *argv[])
{
	ulong now;
	char *logdir = ".";
	char *logfile = nil, *ofile;
	int fd=1;
	int n=0;

	ARGBEGIN{
	case 'u':	uflg = 1; break;
	case 'L':	logdir = ARGF(); break;
	case 'F':	logfile = ARGF(); break;
	case 'N':	n=1; break;
	// FIXME: need to turn on/of the logging
	default:	fprint(2, "usage: date [-u] [-L out_dir] [-F out_file]\n"); exits("usage");
	}ARGEND

	 if(argc!=0){
		fprint(2,"*ERROR*: %s\n", Etomany);
		exits(Etomany);
	 }

	if(logfile != nil) {
		ofile = smprint("%s/%s-%d.log", logdir, logfile, getpid());
		fd = create(ofile, OWRITE, 0664);
		if(fd<0){
			char *err = smprint("could not open %s", ofile);
			fprint(2,"*ERROR*: %s\n", err);
			exits(err);
		}
	}

	if(n){
		vlong t = nsec();

		if(logfile != nil)
			fprint(fd, "%lld\n", t);
		print("%lld\n", t);
	} else {
		char *t;

		now = time(0);

		if(uflg)
			t = asctime(gmtime(now));
		else
			t = ctime(now);

		if(logfile != nil)
			fprint(fd, "%s", t);
		print("%s", t);
	}

	exits(0);
}
