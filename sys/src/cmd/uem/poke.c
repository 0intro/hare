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
	char *logfile, *ofile;
	int fd;
	int n=0;

	logfile = smprint("datestamp-%d.log", getpid());

	ARGBEGIN{
	case 'u':	uflg = 1; break;
	case 'L':	logdir = ARGF(); break;
	case 'F':	logfile = ARGF(); break;
	case 'N':	n=1; break;
	default:	fprint(2, "usage: date [-u] [-L out_dir] [-F out_file]\n"); exits("usage");
	}ARGEND

	 if(argc!=0){
		fprint(2,"*ERROR*: %s\n", Etomany);
		exits(Etomany);
	 }

	ofile = smprint("%s/%s", logdir, logfile);
	fd = create(ofile, OWRITE, 0664);
	if(fd<0){
		char *err = smprint("could not open %s", ofile);
		fprint(2,"*ERROR*: %s\n", err);
		exits(err);
	}

	if(n){
		vlong t = nsec();

		fprint(fd, "%lld\n", t);
		print("%lld\n", t);
	} else {
		char *t;

		now = time(0);

		if(uflg)
			t = asctime(gmtime(now));
		else
			t = ctime(now);

		fprint(fd, "%s", t);
		print("%s", t);
	}

	exits(0);
}
