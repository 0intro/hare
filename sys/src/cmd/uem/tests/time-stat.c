/*
  time-stat -- a trivial utility to calculate the dt stats from nsec(poke) info
*/

#include <u.h>
#include <stdio.h>
#include <libc.h>

#define MAXSTR 256
#define MAXTAG 32
#define MAXENT 32

int chatty = 0;
char *mppath;

typedef struct Ent Ent;
struct Ent {
	char tag[MAXSTR];
	vlong tstamp;
};

Ent ent[MAXENT];

static void
usage(void)
{
	fprint(2, "time-stat ...\n");
	exits("usage");
}

/* data format:
	char *   tag
	vlong    time in nsec
*/

void
print_dt(int fd, Ent *e1, Ent *e2)
{
	vlong dt = e1->tstamp - e2->tstamp;
	double fdt = ((double)dt) / ((double)1000000000LL);
	fprint(fd," %lld", dt);
	fprint(fd," %f", fdt);

	fprint(fd," (%s-%s)", e1->tag, e2->tag);
	fprint(fd,"\n");
}

void
main(int argc, char *argv[])
{
	int i, n;
	FILE *fin=nil;
	FILE *fout=nil;
	int fdout;
	char *arg;
	int list = 0;

	ARGBEGIN{
	case 'l':
		list = 1;
		break;
	case 'i':
		arg=ARGF();
		fin = fopen(arg, "r");
		if(fin==nil){
			fprint(2, "couldn't open input file %s\n", arg);
			exits("open input file");
		}
		break;
	case 'o':
		arg=ARGF();
		fout = fopen(arg, "w");
		if(fout==nil){
			fprint(2, "couldn't open output file %s\n", arg);
			exits("open output file");
		}
		break;
	default:
		usage();
	}ARGEND

	if(argc != 0)
		usage();

	if(fin==nil || fout==nil)
		usage();

	fdout = fileno(fout);

	/* read in the data */
	for (i=0; i<MAXENT; i++){
		char line[256];
		
		n = fscanf(fin, "%s%s", ent[i].tag, line);
		ent[i].tstamp = strtoll(line,nil,10);
		if(n<=0)
			break;
	}
	i--;

	if(i>=MAXENT){
		fprint(2, "*error*: ran out of space\n");
		exits("out of space");
	}

	/* process what we have */
	if(list)
		for (n=0; n<=i; n++){
			fprint(fdout,"%lld %s\n", ent[n].tstamp, ent[n].tag);
		}

	/* process what we have */
	for (n=1; n<=i; n++)
		print_dt(fdout, &ent[n], &ent[n-1]);

	/* do the subtotal */
	print_dt(fdout, &ent[i-1], &ent[0]);

	/* do the total */
	print_dt(fdout, &ent[i], &ent[0]);

	fclose(fin);
	fclose(fout);
	exits(0);
}
