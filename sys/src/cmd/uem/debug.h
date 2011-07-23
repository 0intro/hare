/*
	debug.h - helpful debug routines

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

*/

static ulong vflag = 0;
static int debugfd = 2;

#define DERR 0x00000	/* error -- always print the error */
#define DCUR 0x00001	/* current - temporary trace */
#define DPIP 0x00002	/* interactions with multipipes */
#define DGAN 0x00004	/* gang tracking */
#define DEXE 0x00008	/* execfs interactions */
#define DBCA 0x00010	/* broadcast operations */
#define DCLN 0x00020	/* cleanup tracking */
#define DFID 0x00040	/* fid tracking */
#define DREF 0x00080	/* ref count tracking */
#define DARG 0x00100	/* arguments */
#define DSTS 0x00200	/* status */
#define DNET 0x00400	/* remote access */
#define DRDR 0x00800	/* reader */
#define DOPS 0x01000	/* operation tracking */
#define DWRT 0x02000	/* writer */
#define DHDR 0x04000	/* packet headers */
#define DSPF 0x08000	/* splice from */
#define DSPT 0x10000	/* splice to */
#define DEXC 0x20000	/* debug execcmd */

static void
DPRINT(ulong dcmd, char *fmt, ...)
{
	va_list args;
	char *p;
	int len;
	char s[255];
	char *newfmt;

	if(dcmd!=DERR && !(vflag&dcmd))
		return;

	p = strchr(fmt, '\n');

	newfmt = smprint("\t[%8.8llud][%8.8d](0x%lx)\t %s\n", nsec()>>16, getpid(), vflag&dcmd, fmt);
	if(p != nil){
		p = strchr(newfmt, '\n');
		*p = ' ';
	}
	va_start(args, fmt);
	len = vsnprint(s, sizeof s - 1, newfmt, args);
	va_end(args);
	if (newfmt != nil) free(newfmt);
	s[len]=0;	

	fprint(debugfd, "%s", s);
}

