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
		n = fscanf(fin, "%s%ld", ent[i].tag, &(ent[i].tstamp));
		if(n<=0)
			break;
	}

	if(i>=MAXENT){
		fprint(2, "*error*: ran out of space\n");
		exits("out of space");
	}

	/* process what we have */
	for (n=0; n<i; n++){
		fprint(fdout,"%lld %s\n", ent[n].tstamp, ent[n].tag);
	}

	/* process what we have */
	vlong dt;
	for (n=1; n<i; n++){
		dt = ent[n].tstamp - ent[n-1].tstamp;
		fprint(fdout,"%lld (%s-%s)\n", dt, ent[n].tag, ent[n-1].tag);
	}
	/* do the total */
	dt = ent[i-1].tstamp - ent[0].tstamp;
	fprint(fdout,"%lld total=(%s-%s)\n", dt, ent[i-1].tag, ent[0].tag);
	
	fclose(fin);
	fclose(fout);
	exits(0);
}
