#include "rc.h"
#include "io.h"
#include "fns.h"
char onl='\n';		/* change to semicolon for bourne-proofing */
#define	c0	t->child[0]
#define	c1	t->child[1]
#define	c2	t->child[2]

extern void pdeglob(io *f, char *s);
tree *t0, *t1, *t2;



tree*
gangstr(io *f, tree *t)
{
	if(t==0)
		return t;
	switch(t->type){
	case FANIN:
		gangstr(f, c1);
		pfmt(f, "| %t'", c0);
		t0 = c2;
		break;
	case FANOUT:
		t1 = c1;
		pfmt(f, "push -c '%t", c2);
		t2 = c0;
		break;		
	}
	return t;
}

tree*
mkgang(io *f, tree *t)
{
	int n, fd;
	char buf0[64];
	tree *root, *tt0, *tt1;
	io *es = openstr(); // exec string

	if(t == 0)
		return t;
	root = t;

	if(t->type != FANIN)
		sysfatal("must make gang on a fanin");
//	tree2dot(f, t);
	fd = open("/proc/gclone", ORDWR);
	if(fd < 0)
		sysfatal("mkgang open: %r");
	n = read(fd, buf0, 64);
	if(n < 0)
		sysfatal("mkgang read: %r");
	buf0[n-1]=0;
	/* when I make this do it how I expect? change mung3mp to do the proper formatting*/
//	addredir(t, 1, smprint("/proc/g%s/stdin", buf0));
//	t = c0;
//	if(t == 0)
	/* so how do I do this? I need to push it? */
	/* how do I redirect the incoming command? */
		
	fprint(fd, "enum");
	fprint(fd, "res %d", t->fd0);
	fprint(2, "enum\n");
	fprint(2, "res %d\n", t->fd0);	
//	c1 = nil;
	gangstr(es, t);
	c0 = nil;
	c2 = nil;
	tt0= tree1(SIMPLE, tree2(ARGLIST, t2->child[0], token(smprint("/proc/g%s/stdin", buf0), WORD)));
	tt0->rtype = WRITE;
	tt1 = tree2(REDIR, token(smprint("/proc/g%s/stdout", buf0), WORD), t0);
	tt1->rtype = READ;
	c0 = tree2(';', tree2(PIPE, t1, tt0), tt1);
	fprint(fd, "exec %s", (char*)es->strp);
	fprint(2, "exec %s\n", (char*)es->strp);
	
	fprint(2, "done mking gang\n");
	tree2dot(f, root);
	
	return root;
}

// push -gt -c 'echo test |< cat >| cat' >[2]/n/local/tmp/tree.dot
