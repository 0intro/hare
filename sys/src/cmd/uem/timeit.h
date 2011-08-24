/*

  timeit - an in-memory timestamp utility library

  Copyright (C) 2011, IBM Corporation, 
 		John (EBo) David (ebo@users.sourceforge.net)

  Notes:
      optionally uses atexit to automatically dump timestamps
 */

enum{
	TIMIT_DEFAULT=0x0,
	TIMIT_FLOAT=0x1,
	TIMIT_DT=0x2,
	TIMIT_SILENT=0x4,
	TIMIT_END,
	TIMIT_USED=0x8,
};

typedef struct Tentry Tentry;
struct Tentry {
	uvlong stamp;
	double dt;
	long pid;
	char *tag;
	int flag;
};

typedef struct TimeIt TimeIt;
struct TimeIt {
	int max_entries;
	int n;
	long last_pid;
	Tentry *ent;
	TimeIt *next;
	TimeIt *prev;
};

static TimeIt *root = nil;
static TimeIt *current = nil;
static max_entries = 1024;
static int timeit_fd = 1;
static int default_flag=0;
static int silent=0;

static int use_cycles=0;
static uvlong sec_cnv=1000000000LL;


static void
tslice(uvlong *t)
{
	if(use_cycles){
		sec_cnv = 850000000LL;
		cycles(t);
	}else{
		sec_cnv = 1000000000LL;
		*t = nsec();
	}
}

void
timeit_setcycles(int flag)
{
	use_cycles=flag;
}

double
timeit_cnv_sec(uvlong t)
{
	double dt = ((double) t);
	double ct = ((double) sec_cnv);

	return dt/ct;
}

static TimeIt *timeit_new(int num);

void
timeit_setsilent(int flag)
{
	silent=flag;	
}

int
timeit_settype(int flag)
{
	if(flag>=TIMIT_DEFAULT && flag<TIMIT_END)
		default_flag=flag;

	return default_flag;
}

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
	if(root==nil) return;
	if(outfile==nil) return;

	int fd = create(outfile, OWRITE, 0666);
	if (fd > 0)
		timeit_fd = fd;
	else
		DPRINT(DERR, "*ERROR*: timeit_setout could not open output file (%s)\n", outfile);
}

QLock timelck;

int
timit_ent_cmp(void *v1, void *v2)
{
	Tentry *e1 = (Tentry*) v1;
	Tentry *e2 = (Tentry*) v2;

//if((e1->pid-e2->pid) < 0)
//	print("(swap) %d %d\n", e1->pid,e2->pid);

		return (int)e1->pid-e2->pid;
//	else
//		return (int)e1->stamp-e2->stamp;
}

void
timeit_dump(void)
{
	int i;
	TimeIt *t;
	if(silent) return;
	if(root==nil) return;

	qlock(&timelck);
	for(t=root; t!=nil; t=t->next){
		for (i=0; i<t->n; i++){
			if(t->ent[i].flag & TIMIT_USED)
				continue;
			if(t->ent[i].pid != t->last_pid){
				fprint(timeit_fd, "\n");
				t->last_pid = t->ent[i].pid;
			}
			if(t->ent[i].flag & (TIMIT_FLOAT|TIMIT_DT)){
				fprint(timeit_fd, "%p %lud %f %s\n",
				       root, t->ent[i].pid, t->ent[i].dt, t->ent[i].tag);
			}else{
				fprint(timeit_fd, "%p %lud %llud %s\n",
				       root, t->ent[i].pid, t->ent[i].stamp, t->ent[i].tag);
			}
			t->ent[i].flag |= TIMIT_USED;
		}
	}
	qunlock(&timelck);
}

void
timeit_dump_ordered(void)
{
	TimeIt *t;
	if(silent) return;
	if(root==nil) return;

	qlock(&timelck);
	for(t=root; t!=nil; t=t->next)
		qsort(t->ent, t->n -1, sizeof(Tentry), timit_ent_cmp);

	qunlock(&timelck);

	timeit_dump();	
}

static Tentry *
add_stamp(vlong s, char *tag, int pid)
{
	Tentry *e;

	if(root==nil || current==nil) return nil;
	qlock(&timelck);
	e = &current->ent[current->n];
	e->stamp = s;
	e->dt = 0.0;
	e->pid = pid;
	e->tag = tag;
	e->flag = default_flag;

	/* ensure that the next one is always valid */
	current->n++;
	if(current->n >= max_entries){
		current->next = timeit_new(max_entries);
		current->next->prev = current;
	}
	qunlock(&timelck);

	return e;
}

Tentry *
stamp(char *tag)
{
	uvlong ticks;
	tslice(&ticks);

	if(root==nil) return nil;
	return add_stamp(ticks, tag, getpid());
}

vlong
timeit_ent_dt(Tentry *e1, Tentry *e2)
{
	if(root==nil) return 0;
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

	if(root==nil) return nil;
	if(num < 0)
		goto err;

	qlock(&timelck);
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
	qunlock(&timelck);

	return e;

err:
	qunlock(&timelck);
	DPRINT(DERR, "*ERROR*: timeit_index attemped to access non existant element\n");
	stamp("*ERROR*: timeit_index attemped to access non existant element");
	return nil;
}

Tentry *
timeit_find(char *tag, int pid)
{
	TimeIt *t=root;
	int i;

	if(root==nil) return nil;
	if(tag==nil)
		goto err;

	qlock(&timelck);
	for(i=t->n-1; i>=0; i--)
		if(t->ent[i].pid == pid)
			if(!strcmp(t->ent[i].tag, tag)){
				qunlock(&timelck);
				return  &t->ent[i];
			}
	qunlock(&timelck);

err:
	DPRINT(DERR, "*ERROR*: timeit_find tag=(%s) not found\n", 
	       (tag==nil)?"<nil>":tag);
	stamp("*ERROR*: timeit_find tag not found");
	return nil;
}

/*
Tentry *
timeit_find(char *tag)
{
	TimeIt *t=root;
	int i;

	if(tag==nil)
		goto err;

	for(i=t->n-1; i>=0; i--)
		if(!strcmp(t->ent[i].tag, tag))
			return  &t->ent[i];

err:
	DPRINT(DERR, "*ERROR*: timeit_find tag=(%s) not found\n", 
	       (tag==nil)?"<nil>":tag);
	stamp("*ERROR*: timeit_find tag not found");
	return nil;
}
*/

Tentry *
timeit_last(void)
{
	if(root==nil) return nil;
	if(current==nil || current->n==0)
		goto err;

	return &current->ent[current->n -1];

err:
	DPRINT(DERR, "*ERROR*: timeit_last attemped to access non existant element\n");
	stamp("*ERROR*: timeit_last attemped to access non existant element");
	return nil;
}

int
timeit_last_index(void)
{
	int i, pid=getpid();

	if(root==nil) return -1;
	if(current==nil || current->n==0)
		goto err;

	for(i=current->n-1; i>=0; i--)
		if(current->ent[i].pid == pid)
			return i;

err:
	DPRINT(DERR, "*ERROR*: timeit_last_pid attemped to access non existant element\n");
	stamp("*ERROR*: timeit_last_pid attemped to access non existant element");
	return -1;
}

Tentry *
timeit_last_pid(void)
{
	if(root==nil) return nil;
	int i=timeit_last_index();
	if(current==nil || current->n==0 || i<0)
		goto err;

	return &current->ent[i];

err:
	DPRINT(DERR, "*ERROR*: timeit_last_pid attemped to access non existant element\n");
	stamp("*ERROR*: timeit_last_pid attemped to access non existant element");
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

	if(root==nil) return nil;
	if(s == nil)
		return nil;
	if(num == 0)
		return s;

	pid = s->pid;

	// find the element in the list
	qlock(&timelck);
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
	qunlock(&timelck);

	return e;

err:
	qunlock(&timelck);
	DPRINT(DERR, "*ERROR*: timeit_offset attemped to access non existant element\n");
	stamp("*ERROR*: timeit_offset attemped to access non existant element");
	return nil;
	
}

void
timeit_dt(int num, char *tag)
{
	Tentry *e1, *e2;

	if(root==nil) return;

	e1 = timeit_last_pid();
	e2 = timeit_offset(e1, num);
	if(e1==nil || e2==nil)
		goto err;

	Tentry *e = stamp(tag);

	e->dt = (double) timeit_ent_dt(e1,e2);
	e->flag = TIMIT_DT;

	return; 

err:
	DPRINT(DERR, "*ERROR*: timeit_dt attemped to access non existant element\n");
	stamp("*ERROR*: timeit_dt attemped to access non existant element");
	return;
}

void
timeit_dt_last_sec(char *tag)
{
	int i;

	if(root==nil) return;

	i = timeit_last_index();
	if(i<0)
		goto err;

	Tentry *e = stamp(tag);

	e->dt = timeit_cnv_sec(e->stamp - current->ent[i].stamp);
	e->flag = TIMIT_FLOAT | TIMIT_DT;


	return; 

err:
	DPRINT(DERR, "*ERROR*: timeit_dt attemped to access non existant element\n");
	stamp("*ERROR*: timeit_dt attemped to access non existant element");
	return;
}



static void
timeit_atexit(void)
{
	timeit_dump_ordered();
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
	t->last_pid = -1;

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
timeit_init(void)
{
	// FIXME: clean up old 
	root = timeit_new(max_entries);
	current = root;
	default_flag=TIMIT_DEFAULT;

	if(0==atexit(timeit_atexit)){
		DPRINT(DERR, "Internal Error: atexit died\n");
		exits("atexit failed");
	}
	stamp("timeit initalized");
}

void
timeit_end(void)
{
	if(root==nil) return;
	stamp("End Timer");
}

void
timeit_tag_dt2(char *tag1, char *tag2, char *new_tag)
{
	int pid = getpid();
	Tentry *t1 = timeit_find(tag1, pid);
	Tentry *t2 = timeit_find(tag2, pid);

	if(root==nil) return;
	if(t1==nil || t2==nil)
		goto err;

	Tentry *e = stamp(new_tag);

	if(t2->stamp > t1->stamp)
		e->dt = timeit_cnv_sec(t2->stamp - t1->stamp);
	else
		e->dt = timeit_cnv_sec(t1->stamp - t2->stamp);

	e->flag = TIMIT_DT;


	return;

err:
	DPRINT(DERR, "*ERROR*: timeit_tag_dt failed on tag1=(%s) tag2=(%s)\n",
	       (tag1==nil)?"<nil>":tag1,
	       (tag2==nil)?"<nil>":tag2);
	stamp("*ERROR*: timeit_tag_dt failed");
}

void
timeit_tag_dt2_sec(char *tag1, char *tag2, char *new_tag)
{
	int pid = getpid();
	Tentry *t1 = timeit_find(tag1, pid);
	Tentry *t2 = timeit_find(tag2, pid);

	if(root==nil) return;
	if(t1==nil || t2==nil)
		goto err;

	Tentry *e = stamp(new_tag);

	if(t2->stamp > t1->stamp)
		e->dt = timeit_cnv_sec(t2->stamp - t1->stamp);
	else
		e->dt = timeit_cnv_sec(t1->stamp - t2->stamp);

	e->flag = TIMIT_FLOAT | TIMIT_DT;


	return;

err:
	DPRINT(DERR, "*ERROR*: timeit_tag_dt failed on tag1=(%s) tag2=(%s)\n",
	       (tag1==nil)?"<nil>":tag1,
	       (tag2==nil)?"<nil>":tag2);
	stamp("*ERROR*: timeit_tag_dt failed");
}

void
timeit_tag_dt_sec(char *tag1, char *new_tag)
{
	int pid = getpid();
	Tentry *t1 = timeit_find(tag1, pid);

	if(root==nil) return;
	if(t1==nil)
		goto err;

	Tentry *e = stamp(new_tag);

	e->dt = timeit_cnv_sec(e->stamp - t1->stamp);
	e->flag = TIMIT_FLOAT | TIMIT_DT;

	return;

err:
	DPRINT(DERR, "*ERROR*: timeit_tag_dt failed on tag1=(%s)\n",
	       (tag1==nil)?"<nil>":tag1);
	stamp("*ERROR*: timeit_tag_dt failed");
}
