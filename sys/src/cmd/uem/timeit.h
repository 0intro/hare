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

vlong
timeit_ent_dt(Tentry *e1, Tentry *e2)
{
	if(e1==nil || e2==nil)
		return 0;
	if(e2->stamp > e1->stamp)
		return e2->stamp - e1->stamp;
	else
		return e1->stamp - e2->stamp;
}

Tentry *
timeit_index(int num)
{
	TimeIt *t=root;
	Tentry *e;
	int i;

	if(num < 0)
		goto err;

	for(i=0; num >= 0; num--, i--){
		if(i >= t->n){
			t = t->next;
			if(t == nil)
				goto err;
			i = 0;
			if(t->n == 0)
				goto err;
		}
		if(num==0)
			break;
	}

	e = &t->ent[i];

	return e;

err:
	DPRINT(DERR, "*ERROR*: timeit_index attemped to access non existant element\n");
	add_stamp(0, "*ERROR*: timeit_index attemped to access non existant element");
	return nil;
}

Tentry *
timeit_last(void)
{
	if(current==nil || current->n==0)
		goto err;

	return &current->ent[current->n -1];

err:
	DPRINT(DERR, "*ERROR*: timeit_last attemped to access non existant element\n");
	add_stamp(0, "*ERROR*: timeit_last attemped to access non existant element");
	return nil;
}

Tentry *
timeit_offset(Tentry *s, int num)
{
	TimeIt *t;
	Tentry *e;
	int b, p;
	int ds;
	int pid;

	if(s == nil)
		return nil;
	if(num == 0)
		return s;

	pid = s->pid;

	// find the element in the list
	for(b=0,t=root; t!=nil; b++){
		if(b >= t->n){
			t = t->next;
			b = 0;
			if(t==nil || t->n==0)
				goto err;
		}
		if(&t->ent[b] == s)
			break;
	}

	ds = -1;
	if(num<0)
		ds = 1;

	for(p=b-ds,num+=ds; num!=0; p-=ds){
		if(p >= t->n){
			t = t->next;
			if(t == nil)
				goto err;
			if(t->n == 0)
				goto err;
			p = 0;
		}
		if(p < 0){
			t = t->prev;
			if(t == nil)
				goto err;
			if(t->n == 0)
				goto err;
			p = t->n -1;
		}
		if(pid == t->ent[p].pid)
			num+=ds;
		if(num==0)
			break;
	}

	// termination sanity check
	if(p>=t->n || p<0){
		goto err;
	}

	e = &t->ent[p];

	return e;

err:
	DPRINT(DERR, "*ERROR*: timeit_offset attemped to access non existant element\n");
	add_stamp(0, "*ERROR*: timeit_offset attemped to access non existant element");
	return nil;
	
}

void
timeit_dt(int num, char *tag)
{
	Tentry *e1, *e2;

	e1 = timeit_last();
	e2 = timeit_offset(e1, num);
	if(e1==nil || e2==nil)
		goto err;

	add_stamp(timeit_ent_dt(e1,e2), tag);

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

void
timeit_end(void)
{
	stamp("timeit stopped");
}

