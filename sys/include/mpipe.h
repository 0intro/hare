#pragma	lib	"libmpipe.a"
#pragma	src	"/usr/npe/src/libmpipe"

typedef struct Mpipe Mpipe;
struct Mpipe 
{
	int (*sep)(Mpipe*, char*, int, int*);
	int infd;
	int npipe;
	int **fds;
};

Mpipe *mpipe(int (*sep)(Mpipe*, char*, int, int*), int **, int);
