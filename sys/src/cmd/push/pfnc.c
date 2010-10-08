#include "rc.h"
#include "exec.h"
#include "io.h"
#include "fns.h"
struct{
	void (*f)(void);
	char *name;
}fname[] = {
	Xappend, "Xappend",
	Xasync, "Xasync",
	Xbang, "Xbang",
	Xclose, "Xclose",
	Xdup, "Xdup",
	Xeflag, "Xeflag",
	Xexit, "Xexit",
	Xfalse, "Xfalse",
	Xifnot, "Xifnot",
	Xjump, "Xjump",
	Xmark, "Xmark",
	Xpopm, "Xpopm",
	Xrdwr, "Xrdwr",
	Xread, "Xread",
	Xreturn, "Xreturn",
	Xtrue, "Xtrue",
	Xif, "Xif",
	Xwastrue, "Xwastrue",
	Xword, "Xword",
	Xwrite, "Xwrite",
	Xmatch, "Xmatch",
	Xcase, "Xcase",
	Xconc, "Xconc",
	Xassign, "Xassign",
	Xdol, "Xdol",
	Xcount, "Xcount",
	Xlocal, "Xlocal",
	Xunlocal, "Xunlocal",
	Xfn, "Xfn",
	Xdelfn, "Xdelfn",
	Xpipe, "Xpipe",
	Xfanin, "Xfanin",
	Xfanout, "Xfanout",
	Xpipewait, "Xpipewait",
	Xrdcmds, "Xrdcmds",
	(void (*)(void))Xerror, "Xerror",
	Xbackq, "Xbackq",
	Xpipefd, "Xpipefd",
	Xsubshell, "Xsubshell",
	Xdelhere, "Xdelhere",
	Xfor, "Xfor",
	Xglob, "Xglob",
	Xrdfn, "Xrdfn",
	Xsimple, "Xsimple",
	Xrdfn, "Xrdfn",
	Xqdol, "Xqdol",
0};

void
pfnc(io *fd, thread *t)
{
	char s[512];
	int n = 0;
	int i;
	void (*fn)(void) = t->code[t->pc].f;
	list *a;
	word *b;
	n += snprint(s, 512, "pid %d cycle %p %d ", getpid(), t->code, t->pc);
	
	//pfmt(fd, "pid %d cycle %p %d ", getpid(), t->code, t->pc);
	for(i = 0;fname[i].f;i++) if(fname[i].f==fn){
		n += snprint(s+n, 512-n, "%s ", fname[i].name);
		//pstr(fd, fname[i].name);
		break;
	}
	if(!fname[i].f)
		n += snprint(s+n, 512-n, "%p ", fn);
		//pfmt(fd, "%p", fn);
	for(a = t->argv;a;a = a->next){
		n += snprint(s+n, 512-n, "(");
		for(b = a->words; b; b = b->next){
			n += snprint(s+n, 512-n, "%s ", b->word);
		}
		n += snprint(s+n, 512-n, ") ");
	}
//	for(a = t->argv;a;a = a->next) pfmt(fd, " (%v)", a->words);
	n += snprint(s+n, 512-n, "\n");
	pstr(fd, s);
//	pchr(fd, '\n');
	flush(fd);
}
