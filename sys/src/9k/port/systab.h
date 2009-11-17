#include "/sys/src/libc/9syscall/sys.h"

typedef void Syscall(Ar0*, va_list);

Syscall sysr1;
Syscall sys_errstr;
Syscall sysbind;
Syscall syschdir;
Syscall sysclose;
Syscall sysdup;
Syscall sysalarm;
Syscall sysexec;
Syscall sysexits;
Syscall sys_fsession;
Syscall sysfauth;
Syscall sys_fstat;
Syscall syssegbrk;
Syscall sys_mount;
Syscall sysopen;
Syscall sys_read;
Syscall sysoseek;
Syscall syssleep;
Syscall sys_stat;
Syscall sysrfork;
Syscall sys_write;
Syscall syspipe;
Syscall syscreate;
Syscall sysfd2path;
Syscall sysbrk_;
Syscall sysremove;
Syscall sys_wstat;
Syscall sys_fwstat;
Syscall sysnotify;
Syscall sysnoted;
Syscall syssegattach;
Syscall syssegdetach;
Syscall syssegfree;
Syscall syssegflush;
Syscall sysrendezvous;
Syscall sysunmount;
Syscall sys_wait;
Syscall syssemacquire;
Syscall syssemrelease;
Syscall sysseek;
Syscall sysfversion;
Syscall syserrstr;
Syscall sysstat;
Syscall sysfstat;
Syscall syswstat;
Syscall sysfwstat;
Syscall sysmount;
Syscall sysawait;
Syscall syspread;
Syscall syspwrite;
struct {
	char*	n;
	Syscall*f;
	Ar0	r;
} systab[] = {
	[SYSR1]		{ "Sysr1", sysr1, { .i = -1 } },
	[_ERRSTR]	{ "_errstr", sys_errstr, { .i = -1 } },
	[BIND]		{ "Bind", sysbind, { .i = -1 } },
	[CHDIR]		{ "Chdir", syschdir, { .i = -1 } },
	[CLOSE]		{ "Close", sysclose, { .i = -1 } },
	[DUP]		{ "Dup", sysdup, { .i = -1 } },
	[ALARM]		{ "Alarm", sysalarm, { .i = -1 } },
	[EXEC]		{ "Exec", sysexec, { .i = -1 } },
	[EXITS]		{ "Exits", sysexits, { .i = -1 } },
	[_FSESSION]	{ "_fsession", sys_fsession, { .i = -1 } },
	[FAUTH]		{ "Fauth", sysfauth, { .i = -1 } },
	[_FSTAT]	{ "_fstat", sys_fstat, { .i = -1 } },
	[SEGBRK]	{ "Segbrk", syssegbrk, { .i = -1 } },
	[_MOUNT]	{ "_mount", sys_mount, { .i = -1 } },
	[OPEN]		{ "Open", sysopen, { .i = -1 } },
	[_READ]		{ "_read", sys_read, { .i = -1 } },
	[OSEEK]		{ "Oseek", sysoseek, { .i = -1 } },
	[SLEEP]		{ "Sleep", syssleep, { .i = -1 } },
	[_STAT]		{ "_stat", sys_stat, { .i = -1 } },
	[RFORK]		{ "Rfork", sysrfork, { .i = -1 } },
	[_WRITE]	{ "_write", sys_write, { .i = -1 } },
	[PIPE]		{ "Pipe", syspipe, { .i = -1 } },
	[CREATE]	{ "Create", syscreate, { .i = -1 } },
	[FD2PATH]	{ "Fd2path", sysfd2path, { .i = -1 } },
	[BRK_]		{ "Brk", sysbrk_, { .i = -1 } },
	[REMOVE]	{ "Remove", sysremove, { .i = -1 } },
	[_WSTAT]	{ "_wstat", sys_wstat, { .i = -1 } },
	[_FWSTAT]	{ "_fwstat", sys_fwstat, { .i = -1 } },
	[NOTIFY]	{ "Notify", sysnotify, { .i = -1 } },
	[NOTED]		{ "Noted", sysnoted, { .i = -1 } },
	[SEGATTACH]	{ "Segattach", syssegattach, { .i = -1 } },
	[SEGDETACH]	{ "Segdetach", syssegdetach, { .i = -1 } },
	[SEGFREE]	{ "Segfree", syssegfree, { .i = -1 } },
	[SEGFLUSH]	{ "Segflush", syssegflush, { .i = -1 } },
	[RENDEZVOUS]	{ "Rendez", sysrendezvous, { .i = -1 } },
	[UNMOUNT]	{ "Unmount", sysunmount, { .i = -1 } },
	[_WAIT]		{ "_wait", sys_wait, { .i = -1 } },
	[SEMACQUIRE]	{ "Semacquire", syssemacquire, { .i = -1 } },
	[SEMRELEASE]	{ "Semrelease", syssemrelease, { .i = -1 } },
	[SEEK]		{ "Seek", sysseek, { .i = -1 } },
	[FVERSION]	{ "Fversion", sysfversion, { .i = -1 } },
	[ERRSTR]	{ "Errstr", syserrstr, { .i = -1 } },
	[STAT]		{ "Stat", sysstat, { .i = -1 } },
	[FSTAT]		{ "Fstat", sysfstat, { .i = -1 } },
	[WSTAT]		{ "Wstat", syswstat, { .i = -1 } },
	[FWSTAT]	{ "Fwstat", sysfwstat, { .i = -1 } },
	[MOUNT]		{ "Mount", sysmount, { .i = -1 } },
	[AWAIT]		{ "Await", sysawait, { .i = -1 } },
	[PREAD]		{ "Pread", syspread, { .i = -1 } },
	[PWRITE]	{ "Pwrite", syspwrite, { .i = -1 } },
};

int nsyscall = nelem(systab);
