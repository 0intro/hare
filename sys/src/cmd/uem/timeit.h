/*

  timeit - an in-memory timestamp utility library

  Copyright (C) 2011, IBM Corporation, 
 		John (EBo) David (ebo@users.sourceforge.net)

  Notes:
      optionally uses atexit to automatically dump timestamps
 */

typedef struct Tentry Tentry;
struct Tentry {
	vlong stamp;
	long pid;
	char *tag;
};

typedef struct TimeIt TimeIt;
struct TimeIt {
	int max_entries;
	int n;
	int last_dump;
	Tentry *ent;
	TimeIt *next;
	TimeIt *prev;
};

static TimeIt *root = nil;
static TimeIt *current = nil;
static max_entries = 1024;
static int timeit_fd = 1;

static TimeIt *timeit_new(int num);

int
timeit_setmax(int num)
{
	if(num >= 0)
		return -1;

	max_entries = num;

	return 0;
}

void
timeit_setfd(int fd)
{
	timeit_fd = fd;
}

void
timeit_setout(char *outfile)
{
	int fd = create(outfile, OWRITE, 0666);
	if (fd > 0)
		timeit_fd = fd;
	else
		DPRINT(DERR, "*ERROR*: timeit_setout could not open output file (%s)\n", outfile);
}

void
timeit_dump(void)
{
	int i;
	TimeIt *t = root;
	while(t != nil){
		for (i=t->last_dump+1; i<t->n; i++){
			fprint(timeit_fd, "%lud %llud %s\n", t->ent[i].pid, t->ent[i].stamp, t->ent[i].tag);
			t->last_dump = i;
		}
		t = t->next;
	}
}

static void
add_stamp(vlong s, char *tag)
{
	current->ent[current->n].stamp = s;
	current->ent[current->n].pid = getpid();
	current->ent[current->n].tag = tag;

	/* ensure that the next one is always valid */
	current->n++;
	if(current->n >= max_entries){
		current->next = timeit_new(max_entries);
		current->next->prev = current;
	}
}

void
timeit_dt(int num, char *tag)
{
	TimeIt *t = current;
	int p, i=t->n-1;
	vlong end;

	if(i < 0){
		t = t->prev;
		if(t == nil)
			goto err;
		i = t->n -1;
	}

	end = t->ent[i].stamp;

	for(p=i-1; num >= 0; num--, p--){
		if(p < 0){
			t = t->prev;
			if(t == nil)
				goto err;
			p = t->n -1;
		}
		if(num==0)
			break;
	}

	add_stamp(end-t->ent[p].stamp, tag);

	return;

err:
	DPRINT(DERR, "*ERROR*: timeit_dt attemped to access non existant element\n");
	add_stamp(0, "*ERROR*: timeit_dt attemped to access non existant element");
	return;
	
}

static void
timeit_atexit(void)
{
	timeit_dump();
}

static TimeIt *
timeit_new(int num)
{
	TimeIt *t = malloc(sizeof(TimeIt));
	if(t == nil) {
		fprint(2, "timeit_new: out of memory allocating %d\n", (int)sizeof(TimeIt));
		exits("timeit_new: mem");
	}
	t->max_entries = num;
	t->n = 0;
	t->last_dump = -1;
	t->ent = malloc(num*sizeof(Tentry));
	if(t->ent == nil) {
		fprint(2, "timeit_new: out of memory allocating %d\n", (int)num*sizeof(Tentry));
		exits("timeit_new: mem");
	}
	t->next = nil;
	t->prev = nil;

	return t;
}

void
stamp(char *tag)
{
	add_stamp(nsec(), tag);
}

void
timeit_init(void)
{
	root = timeit_new(max_entries);
	current = root;

	if(0==atexit(timeit_atexit)){
		DPRINT(DERR, "Internal Error: atexit died\n");
		exits("atexit failed");
	}
	stamp("timeit initalized");
}

