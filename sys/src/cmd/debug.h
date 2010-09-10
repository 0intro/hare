/*
	debug.h - helpful debug routines

	Copyright (C) 2010, IBM Corporation, 
 		Eric Van Hensbergen (bergevan@us.ibm.com)

*/

static int vflag = 0;

static void
_dprint(ulong dlevel, char *fmt, ...)
{
	va_list args;
	char *p;
	int len;
	char s[255];
	char *newfmt;

	if(vflag<dlevel)
		return;

	p = strchr(fmt, '\n');

	newfmt = smprint("%ld %d %s\n", time(0), getpid(), fmt);
	if(p != nil){
		p = strchr(newfmt, '\n');
		*p = ' ';
	}
	va_start(args, fmt);
	len = vsnprint(s, sizeof s - 1, newfmt, args);
	va_end(args);
	if (newfmt != nil) free(newfmt);
	s[len]=0;	

	fprint(2, "%s", s);
}

#define DPRINT if(vflag)_dprint
