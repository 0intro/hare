#include <u.h>
#include <libc.h>
#include <mpipe.h>

/*
	so I get a list 
	that is right. 
	that gives me a multipipe. 
	so how do you do it in plan . 
	so what are we doing?
	so we have a multipipe. 
	so now how do you connect it to the process? 
	you can fork. 
	you have a thread that does this. 
	I have a worker to dispatch? 
	
	rfork
	
	Bugs:
	so how do you deal with leftovers?
	should sep handle them? 
	should I return that?	
	that is the right semantics. 
	dealt with. 
	
	the splicer process reads from stdin. 
	
	
*/

Mpipe*
mpipe(int (sep)(Mpipe*, char*, int, int*), int **fds, int n)
{
	int i;
	Mpipe *p;
	
	p = malloc(sizeof(Mpipe));
	p->sep = sep;
	p->npipe = n;
	p->fds = fds;
	for(i = 0; i < p->npipe; i++)
		pipe(p->fds[i]);
	return p;	
}

/*
// imaginary for later

	int i;
	char **argv;
	
	pipes = emalloc((p->npipe+3)*sizeof(char*));
	
	switch(rfork()){
	default:
		break;
	case 0:
		argv[0] = splitter;
		argv[1] = exec
		
		for(i = 0; i < p->npipes; i++)
			smprint
		exec(splitter, argv);
		break;
	case -1:
		
		sysfatal("fork failed: %r");	
	}

	free(pipes);


int
mpipe(int **fd, int x)
{
	char buf[8192];
	int cfd, n;
	snprint(buf, sizeof buf,  "/proc/%d/fd/ctl", pid);
	cfd = open(buf, ORDWR);
	fprint(cfd, "multipipe %d", x);
	n = read(cfd, buf, sizeof buf);
}






*/