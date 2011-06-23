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
	char *t;
	int fd;

	logfile = smprint("datestamp-%d.log", getpid());

	ARGBEGIN{
	case 'u':	uflg = 1; break;
	case 'L':	logdir = ARGF(); break;
	case 'F':	logfile = ARGF(); break;
	default:	fprint(2, "usage: date [-u] [-L out_dir] [-F out_file]\n"); exits("usage");
	}ARGEND

	 if(argc!=0){
		fprint(2,"*ERROR*: %s\n", Etomany);
		exits(Etomany);
	 }

	now = time(0);

	if(uflg)
		t = asctime(gmtime(now));
	else
		t = ctime(now);

	ofile = smprint("%s/%s", logdir, logfile);
	fd = create(ofile, OWRITE, 0664);
	if(fd<0){
		char *err = smprint("could not open %s", ofile);
		fprint(2,"*ERROR*: %s\n", err);
		exits(err);
	}

	fprint(fd, "%s", t);
	print("%s", t);

	exits(0);
}
