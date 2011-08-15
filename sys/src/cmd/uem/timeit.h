/*

  timeit - an in-memory timestamp utility library

  Copyright (C) 2011, IBM Corporation, 
 		John (EBo) David (ebo@users.sourceforge.net)

  Notes:
      optionally uses atexit to automatically dump timestamps
 */

enum{
	TIMIT_DEFAULT=0,
	TIMIT_FLOAT,
	TIMIT_SILENT,
	TIMIT_END,
};

typedef struct Tentry Tentry;
struct Tentry {
	uvlong stamp;
	long pid;
	char *tag;
	int flag;
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
static int default_flag=0;

static int silent=0;

#define slice(X) X=nsec()
#define sec_cnv 1000000000LL
//#define slice(X) cycles(&X)
//#define sec_cnv 850000000LL // FIXME: this is an estimte

double
timeit_cnv_float(uvlong t)
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

void
timeit_dump(void)
{
	int i;
	// FIXME: no need for the extra var.  Just use root instead of t->
	TimeIt *t = root;
	double fdt;
	if(silent) return;
	if(root==nil) return;

	qlock(&timelck);
	while(t != nil){
		for (i=t->last_dump+1; i<t->n; i++){
			switch(t->ent[i].flag){
			case TIMIT_FLOAT:
				fdt = ((double)t->ent[i].stamp)/((double)sec_cnv);
				fprint(timeit_fd, "%lud %f %s\n",
				       t->ent[i].pid, fdt, t->ent[i].tag);
				break;
			default:
				fprint(timeit_fd, "%lud %llud %s\n",
				       t->ent[i].pid, t->ent[i].stamp, t->ent[i].tag);
				break;
			}
			t->last_dump = i;
		}
		t = t->next;
	}
	qunlock(&timelck);
}

static void
add_stamp(vlong s, char *tag, int pid)
{
	if(root==nil) return;
	qlock(&timelck);
	current->ent[current->n].stamp = s;
	current->ent[current->n].pid = pid;
	current->ent[current->n].tag = tag;
	current->ent[current->n].flag = default_flag;

	/* ensure that the next one is always valid */
	current->n++;
	if(current->n >= max_entries){
		current->next = timeit_new(max_entries);
		current->next->prev = current;
	}
	qunlock(&timelck);
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
	add_stamp(0, "*ERROR*: timeit_index attemped to access non existant element", getpid());
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
	add_stamp(0, "*ERROR*: timeit_find tag not found", getpid());
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
	add_stamp(0, "*ERROR*: timeit_find tag not found");
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
	add_stamp(0, "*ERROR*: timeit_last attemped to access non existant element", getpid());
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
	add_stamp(0, "*ERROR*: timeit_last_pid attemped to access non existant element", pid);
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
	add_stamp(0, "*ERROR*: timeit_last_pid attemped to access non existant element", getpid());
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
	add_stamp(0, "*ERROR*: timeit_offset attemped to access non existant element", getpid());
	return nil;
	
}

void
timeit_dt(int num, char *tag)
{
	Tentry *e1, *e2;

	if(root==nil) return;
fprint(2, "here1\n");
	e1 = timeit_last_pid();
	e2 = timeit_offset(e1, num);
	if(e1==nil || e2==nil)
		goto err;
fprint(2, "here2\n");
	add_stamp(timeit_ent_dt(e1,e2), tag, getpid());

fprint(2, "here3\n");
	return; 

err:
	DPRINT(DERR, "*ERROR*: timeit_dt attemped to access non existant element\n");
	add_stamp(0, "*ERROR*: timeit_dt attemped to access non existant element", getpid());
	return;
}

void
timeit_dt_last_float(char *tag)
{
//	if(root==nil) return;
//	timeit_settype(TIMIT_FLOAT);
//	timeit_dt(-1, tag);
//	timeit_settype(TIMIT_DEFAULT);


	int i;
	uvlong t;
	slice(t);

	if(root==nil) return;

	i = timeit_last_index();
	if(i<0)
		goto err;

	fprint(2,"\nDT[%d] = %ulld \n", i, t - current->ent[i].stamp);
	timeit_settype(TIMIT_FLOAT);
	add_stamp(t - current->ent[i].stamp, tag, getpid());
	timeit_settype(TIMIT_DEFAULT);

	return; 

err:
	DPRINT(DERR, "*ERROR*: timeit_dt attemped to access non existant element\n");
	add_stamp(0, "*ERROR*: timeit_dt attemped to access non existant element", getpid());
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
	uvlong ticks;
	slice(ticks);

	if(root==nil) return;
	add_stamp(ticks, tag, getpid());
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
timeit_tag_dt(char *tag1, char *tag2, char *new_tag)
{
	int pid = getpid();
	Tentry *t1 = timeit_find(tag1, pid);
	Tentry *t2 = timeit_find(tag2, pid);
	vlong dt;

	if(root==nil) return;
	if(t1==nil || t2==nil)
		goto err;

	if(t2->stamp > t1->stamp)
		dt = t2->stamp - t1->stamp;
	else
		dt = t1->stamp - t2->stamp;

	add_stamp(dt, new_tag, getpid());

	return;

err:
	DPRINT(DERR, "*ERROR*: timeit_tag_dt failed on tag1=(%s) tag2=(%s)\n",
	       (tag1==nil)?"<nil>":tag1,
	       (tag2==nil)?"<nil>":tag2);
	add_stamp(0, "*ERROR*: timeit_tag_dt failed", getpid());
}

void
timeit_tag_dt_float(char *tag1, char *new_tag)
{
	int pid = getpid();
	Tentry *t1 = timeit_find(tag1, pid);
	vlong dt;

	uvlong time;
	slice(time);

	if(root==nil) return;
	if(t1==nil)
		goto err;

	timeit_settype(TIMIT_FLOAT);

	dt = time - t1->stamp;

	add_stamp(dt, new_tag, getpid());

	timeit_settype(TIMIT_DEFAULT);

	return;

err:
	DPRINT(DERR, "*ERROR*: timeit_tag_dt failed on tag1=(%s)\n",
	       (tag1==nil)?"<nil>":tag1);
	add_stamp(0, "*ERROR*: timeit_tag_dt failed", getpid());
}
