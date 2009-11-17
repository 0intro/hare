void		_assert(char*);
void		accounttime(void);
void		addbootfile(char*, uchar*, ulong);
Timer*		addclock0link(void (*)(void), int);
int		addconsdev(Queue*, void (*fn)(char*,int), int, int);
int		addkbdq(Queue*, int);
int		addphysseg(Physseg*);
void		addwatchdog(Watchdog*);
int		adec(int*);
Block*		adjustblock(Block*, int);
int		ainc(int*);
void		alarmkproc(void*);
Block*		allocb(int);
int		anyhigher(void);
int		anyready(void);
Image*		attachimage(int, Chan*, uintptr, usize);
Page*		auxpage(void);
Block*		bl2mem(uchar*, Block*, int);
int		blocklen(Block*);
void		bootlinks(void);
void		cachedel(Image*, ulong);
void		cachepage(Page*, Image*);
void		callwithureg(void (*)(Ureg*));
char*		chanpath(Chan*);
int		canlock(Lock*);
int		canpage(Proc*);
int		canqlock(QLock*);
int		canrlock(RWlock*);
Chan*		cclone(Chan*);
void		cclose(Chan*);
void		ccloseq(Chan*);
void		chanfree(Chan*);
void		checkalarms(void);
void		checkb(Block*, char*);
void		closeegrp(Egrp*);
void		closefgrp(Fgrp*);
void		closepgrp(Pgrp*);
void		closergrp(Rgrp*);
void		cmderror(Cmdbuf*, char*);
int		cmount(Chan**, Chan*, int, char*);
void		confinit(void);
int		consactive(void);
void		(*consdebug)(void);
Block*		concatblock(Block*);
void		(*consputs)(char*, int);
Block*		copyblock(Block*, int);
void		copypage(Page*, Page*);
void		cunmount(Chan*, Chan*);
uintptr		dbgpc(Proc*);
int		decrypt(void*, void*, int);
void		delay(int);
void		delconsdevs(void);
Proc*		dequeueproc(Schedq*, Proc*);
Chan*		devattach(int, char*);
Block*		devbread(Chan*, long, vlong);
long		devbwrite(Chan*, Block*, vlong);
Chan*		devclone(Chan*);
int		devconfig(int, char *, DevConf *);
void		devcreate(Chan*, char*, int, int);
void		devdir(Chan*, Qid, char*, vlong, char*, long, Dir*);
long		devdirread(Chan*, char*, long, Dirtab*, int, Devgen*);
Devgen		devgen;
void		devinit(void);
Chan*		devopen(Chan*, int, Dirtab*, int, Devgen*);
void		devpermcheck(char*, int, int);
void		devpower(int);
void		devremove(Chan*);
void		devreset(void);
void		devshutdown(void);
long		devstat(Chan*, uchar*, long, Dirtab*, int, Devgen*);
Dev*		devtabget(int, int);
void		devtabinit(void);
void		devtabreset(void);
long		devtabread(Chan*, void*, long, vlong);
void		devtabshutdown(void);
Walkqid*	devwalk(Chan*, Chan*, char**, int, Dirtab*, int, Devgen*);
long		devwstat(Chan*, uchar*, long);
void		drawactive(int);
void		drawcmap(void);
void		dumpaproc(Proc*);
void		dumpregs(Ureg*);
void		dumpstack(void);
Fgrp*		dupfgrp(Fgrp*);
int		duppage(Page*);
void		dupswap(Page*);
void		edfinit(Proc*);
char*		edfadmit(Proc*);
int		edfready(Proc*);
void		edfrecord(Proc*);
void		edfrun(Proc*, int);
void		edfstop(Proc*);
void		edfyield(void);
int		emptystr(char*);
int		encrypt(void*, void*, int);
void		envcpy(Egrp*, Egrp*);
int		eqchanddq(Chan*, int, uint, Qid, int);
int		eqqid(Qid, Qid);
void		error(char*);
void		exhausted(char*);
void		exit(int);
uvlong		fastticks(uvlong*);
uvlong		fastticks2us(uvlong);
uvlong		fastticks2ns(uvlong);
int		fault(uintptr, int);
void		fdclose(int, int);
Chan*		fdtochan(int, int, int, int);
int		findmount(Chan**, Mhead**, int, uint, Qid);
int		fixfault(Segment*, uintptr, int, int);
void		forceclosefgrp(void);
void		free(void*);
void		freeb(Block*);
void		freeblist(Block*);
int		freebroken(void);
void		freepte(Segment*, Pte*);
void		getcolor(ulong, ulong*, ulong*, ulong*);
void		gotolabel(Label*);
char*		getconfenv(void);
int		haswaitq(void*);
long		hostdomainwrite(char*, long);
long		hostownerwrite(char*, long);
void		hzsched(void);
Block*		iallocb(int);
void		iallocsummary(void);
void		ilock(Lock*);
void		iunlock(Lock*);
void		initimage(void);
int		iprint(char*, ...);
void		isdir(Chan*);
int		iseve(void);
int		islo(void);
Segment*	isoverlap(Proc*, uintptr, usize);
int		ispages(void*);
int		isphysseg(char*);
void		ixsummary(void);
int		kbdcr2nl(Queue*, int);
int		kbdgetmap(int, int*, int*, Rune*);
int		kbdputc(Queue*, int);
void		kbdputmap(ushort, ushort, Rune);
void		kickpager(void);
void		killbig(char*);
void		kproc(char*, void(*)(void*), void*);
void		kprocchild(Proc*, void (*)(void*), void*);
void		(*kproftimer)(uintptr);
void		ksetenv(char*, char*, int);
void		kstrcpy(char*, char*, int);
void		kstrdup(char**, char*);
long		latin1(Rune*, int);
int		lock(Lock*);
void		logopen(Log*);
void		logclose(Log*);
char*		logctl(Log*, int, char**, Logflag*);
void		logn(Log*, int, void*, int);
long		logread(Log*, void*, ulong, long);
void		log(Log*, int, char*, ...);
Cmdtab*		lookupcmd(Cmdbuf*, Cmdtab*, int);
Page*		lookpage(Image*, ulong);
#define		MS2NS(n) (((vlong)(n))*1000000LL)
void		mallocinit(void);
void		mallocsummary(void);
Block*		mem2bl(uchar*, int);
void		(*mfcinit)(void);
void		(*mfcopen)(Chan*);
int		(*mfcread)(Chan*, uchar*, int, vlong);
void		(*mfcupdate)(Chan*, uchar*, int, vlong);
void		(*mfcwrite)(Chan*, uchar*, int, vlong);
void		mfreeseg(Segment*, uintptr, int);
void		microdelay(int);
uvlong		mk64fract(uvlong, uvlong);
void		mkqid(Qid*, vlong, ulong, int);
void		mmuflush(void);
void		mmuput(uintptr, uintptr, Page*);
void		mmurelease(Proc*);
void		mmuswitch(Proc*);
Chan*		mntauth(Chan*, char*);
usize		mntversion(Chan*, u32int, char*, usize);
void		mountfree(Mount*);
int		mregfmt(Fmt*);
ulong		ms2tk(ulong);
uvlong		ms2fastticks(ulong);
void		mul64fract(uvlong*, uvlong, uvlong);
void		muxclose(Mnt*);
Chan*		namec(char*, int, int, int);
void		nameerror(char*, char*);
Chan*		newchan(void);
int		newfd(Chan*);
Mhead*		newmhead(Chan*);
Mount*		newmount(Mhead*, Chan*, int, char*);
Page*		newpage(int, Segment **, uintptr);
Path*		newpath(char*);
Pgrp*		newpgrp(void);
Rgrp*		newrgrp(void);
Proc*		newproc(void);
void		nexterror(void);
int		nrand(int);
uvlong		ns2fastticks(uvlong);
int		okaddr(uintptr, long, int);
int		openmode(int);
Block*		packblock(Block*);
Block*		padblock(Block*, int);
void		pagechainhead(Page*);
void		pageinit(void);
ulong		pagenumber(Page*);
uvlong		pagereclaim(int);
void		pagersummary(void);
void		panic(char*, ...);
Cmdbuf*		parsecmd(char *a, int n);
void		pathclose(Path*);
ulong		perfticks(void);
void		pexit(char*, int);
void		pgrpcpy(Pgrp*, Pgrp*);
void		pgrpnote(ulong, char*, long, int);
int		psindex(int);
void		pio(Segment*, uintptr, ulong, Page**);
#define		poperror()		up->nerrlab--
int		postnote(Proc*, int, char*, int);
int		pprint(char*, ...);
int		preempted(void);
void		prflush(void);
void		printinit(void);
void		psinit(int);
ulong		procalarm(ulong);
void		procctl(Proc*);
void		procdump(void);
int		procfdprint(Chan*, int, int, char*, int);
void		procflushseg(Segment*);
void		procpriority(Proc*, int, int);
void		procrestore(Proc*);
void		procsave(Proc*);
Proc*		psincref(int);
void		psdecref(Proc*);
void		(*proctrace)(Proc*, int, vlong);
void		procwired(Proc*, int);
Pte*		ptealloc(void);
Pte*		ptecpy(Pte*);
int		pullblock(Block**, int);
Block*		pullupblock(Block*, int);
Block*		pullupqueue(Queue*, int);
void		putimage(Image*);
void		putmhead(Mhead*);
void		putpage(Page*);
void		putseg(Segment*);
void		putstrn(char*, int);
void		putswap(Page*);
int		pwait(Waitmsg*);
void		qaddlist(Queue*, Block*);
Block*		qbread(Queue*, int);
long		qbwrite(Queue*, Block*);
Queue*		qbypass(void (*)(void*, Block*), void*);
int		qcanread(Queue*);
void		qclose(Queue*);
int		qconsume(Queue*, void*, int);
Block*		qcopy(Queue*, int, ulong);
int		qdiscard(Queue*, int);
void		qflush(Queue*);
void		qfree(Queue*);
int		qfull(Queue*);
Block*		qget(Queue*);
void		qhangup(Queue*, char*);
int		qisclosed(Queue*);
int		qiwrite(Queue*, void*, int);
int		qlen(Queue*);
void		qlock(QLock*);
Queue*		qopen(int, int, void (*)(void*), void*);
int		qpass(Queue*, Block*);
int		qpassnolim(Queue*, Block*);
int		qproduce(Queue*, void*, int);
void		qputback(Queue*, Block*);
long		qread(Queue*, void*, int);
Block*		qremove(Queue*);
void		qreopen(Queue*);
void		qsetlimit(Queue*, int);
void		qunlock(QLock*);
int		qwindow(Queue*);
int		qwrite(Queue*, void*, int);
void		qnoblock(Queue*, int);
int		rand(void);
void		randominit(void);
ulong		randomread(void*, ulong);
void		rdb(void);
int		readnum(ulong, char*, ulong, ulong, int);
long		readstr(long, char*, long, char*);
void		ready(Proc*);
void		rebootcmd(int, char**);
void		reboot(void*, void*, long);
void		relocateseg(Segment*, uintptr);
void		renameuser(char*, char*);
void		resched(char*);
void		resrcwait(char*);
int		return0(void*);
void		rlock(RWlock*);
long		rtctime(void);
void		runlock(RWlock*);
Proc*		runproc(void);
void		sched(void);
void		scheddump(void);
void		schedinit(void);
long		seconds(void);
void		segclock(uintptr);
void		segpage(Segment*, Page*);
int		setcolor(ulong, ulong, ulong, ulong);
void		setkernur(Ureg*, Proc*);
int		setlabel(Label*);
void		setregisters(Ureg*, char*, char*, int);
void		setswapchan(Chan*);
char*		skipslash(char*);
void		sleep(Rendez*, int (*)(void*), void*);
void*		smalloc(ulong);
char*		srvname(Chan*);
int		swapcount(ulong);
int		swapfull(void);
void		swapinit(void);
void		sysrforkchild(Proc*, Proc*);
void		timeradd(Timer*);
void		timerdel(Timer*);
void		timersinit(void);
void		timerintr(Ureg*, vlong);
void		timerset(uvlong);
ulong		tk2ms(ulong);
#define		TK2MS(x) ((x)*(1000/HZ))
uvlong		tod2fastticks(vlong);
vlong		todget(vlong*);
void		todsetfreq(vlong);
void		todinit(void);
void		todset(vlong, vlong, int);
void		tsleep(Rendez*, int (*)(void*), void*, long);
Block*		trimblock(Block*, int, int);
Uart*		uartconsole(int, char*);
int		uartctl(Uart*, char*);
int		uartgetc(void);
void		uartkick(void*);
void		uartputc(int);
void		uartputs(char*, int);
void		uartrecv(Uart*, char);
int		uartstageoutput(Uart*);
void		unbreak(Proc*);
void		uncachepage(Page*);
long		unionread(Chan*, void*, long);
void		unlock(Lock*);
void		userinit(void);
uintptr		userpc(Ureg*);
long		userwrite(char*, long);
void*		validaddr(void*, long, int);
void		validname(char*, int);
char*		validnamedup(char*, int);
void		validstat(uchar*, usize);
void*		vmemchr(void*, int, int);
Proc*		wakeup(Rendez*);
int		walk(Chan**, char**, int, int, int*);
void		wlock(RWlock*);
void		wunlock(RWlock*);
void		yield(void);
Segment*	data2txt(Segment*);
Segment*	dupseg(Segment**, int, int);
Segment*	newseg(int, uintptr, usize);
Segment*	seg(Proc*, uintptr, int);
void		hnputv(void*, uvlong);
void		hnputl(void*, uint);
void		hnputs(void*, ushort);
uvlong		nhgetv(void*);
uint		nhgetl(void*);
ushort		nhgets(void*);
ulong		µs(void);

#pragma varargck	argpos	iprint		1
#pragma varargck	argpos	panic		1
