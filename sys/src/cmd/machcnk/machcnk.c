#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "elf.h"

enum {
	DumpCall = 16,		/* dump args and return (including errors) */
	DumpUregsBefore = 32,	/* dump uregs before system call */
	DumpUregsAfter = 64,	/* dump uregs after system call */
	PersyscallInfo = 128,	/* print per-system-call info for CNK-only system calls */
};

#define RND(size,up) (((size)+(up)-1) & ~((up)-1))
#define RNDM(size) ((size+0xfffff)&0xfff00000)

int
verbose(void){
	return 1;
}
void errexit(char *s)
{
	fprint(2, smprint("%s\n", s));
	exits("life sucks");
}
void ssize(int fd, u32int va, u32int size)
{
	char *cmd;
	cmd = smprint("va %#x %#x", va, size);
	print("%s\n", cmd);
	if (write(fd, cmd, strlen(cmd)) < 0)
		errexit("Setting size");
}

unsigned char *makeseg(char *name, u32int addr, u32int size, int markheap, u32int pv)
{
	char *segname = smprint("/n/cnk/%s", name);
	char *ctl = smprint("/n/cnk/%s/ctl", name);
	int ctlfd;
	unsigned char *ptr;

	remove(segname);

	if (create(segname, 0, DMDIR|0777)<0)
		errexit(segname);
print("SEGMENT CREATE OK!\n");
	if ((ctlfd = open(ctl, ORDWR)) < 0) errexit(ctl);	
	/* round size up to nearest MB *
	size += 0xfffff;
	size &= ~0xfffff;
	*/
	ssize(ctlfd, addr, size);
print("SSIZE DONE\n");
	if (markheap) {
		if (write(ctlfd, "heap", 4) < 0)
			errexit("Setting heap");
	}
print("DONE MARKHEAP\n");
	ptr = segattach(0, name, (void *)addr,size);
	if (ptr == (unsigned char *)-1)
		errexit(smprint("segattach(0, %s, %p, %d): %r", name, (void *)addr, size));
print("DONE SEGATTACH\n");
	memset(ptr, 0, size);
print("DONE MEMSET\n");
	return ptr;
}

/* aux vector structure. Can NOT be null.  */
typedef struct
{
  u32int a_type;              /* Entry type */
  union
    {
      u32int a_val;           /* Integer value */
      /* We use to have pointer elements added here.  We cannot do that,
         though, since it does not work when using 32-bit definitions
         on 64-bit platforms and vice versa.  */
    } a_un;
} Elf32_auxv_t;


/*
 *	All a.out header types.  The dummy entry allows canonical
 *	processing of the union as a sequence of longs
 */

typedef struct {
	union{
		Ehdr;			/* elf.h */
	} e;
	long dummy;			/* padding to ensure extra long */
} ExecHdr;

extern void *exechdr(void);
extern void *phdr(void);
/* we need an array of these. Cheese it out and just make 32 of them. 
 * they will start out empty (zero'd) and we'll just add what we need. 
 */
int auxc = 0;
Elf32_auxv_t aux[32];

void naux(int type, u32int val) 
{
	aux[auxc].a_type = type;
	aux[auxc].a_un.a_val = val;
	auxc++;
}

/*
 * Elf32 binaries.
 */
Phdr *
hdr(void)
{
	Ehdr *ep;
	Phdr *ph;
	int i;
	ep = exechdr();
	ph = phdr();
	fprint(2,"elf add %d entries at %p\n", ep->phnum, ph);
	for(i = 0; i < ep->phnum; i++)
		fprint(2,"%d: type %d va %p pa %p \n", i, ph[i].type, ph[i].vaddr, ph[i].paddr);
	/* GNU ELF is weird. GNUSTACK is the last phdr and it's confusing libmach. */
	naux(AT_PHNUM, ep->phnum-1);
	naux(AT_PHDR, (u32int)ph);

	return ph;
}

void
usage(void)
{
	errexit(smprint("usage: machcnk [-b] [breakpoint] [-p p==v base] [a]rgstosyscall [s]yscall [u]regsbefore [U]uregsafter [f]aultpower [F]aultpowerregs elf\n"));
}

main(int argc, char *argv[])
{
	char *ctlpath = "#P/cnk";
	void callcnk(void (*f)(int, char **), int, char **, char **);
	int fd, ctl;
	unsigned char  *textp, *datap, *bssp;
	void (*f)(int argc, char *argv[]);
	Fhdr fp;
	Map *map;
	int textseg, dataseg;
	int i;
	u32int breakpoint = 0;
	char *av[256];
	int cnk = 2;
	char cmd[2];
	u32int pv = 0; /* are we in a P==V world? */
	u32int bssbase = 512 * 1024 * 1024;
	u32int bsssize = 0x100000;

	ARGBEGIN{
	case 'b':	breakpoint = strtoul(EARGF(usage()), 0, 0); break;
	/* weird. No longer in the code? */
	case 'f':	cnk |= 4; break;
	case 'F':	cnk |= 8; break;
	case 's':	cnk |= DumpCall; break;
	case 'u':	cnk |= DumpUregsBefore; break;
	case 'U':	cnk |= DumpUregsAfter; break;
	case 'a':	cnk |= PersyscallInfo; break;
	case 'A':	cnk |= 0xfc; break;
	case 'p':	pv = strtoul(EARGF(usage()), 0, 0); break;
	case 'S': 	bsssize = strtoul(EARGF(usage()), 0, 0); break;
	default:	usage(); break;
	}ARGEND

	if (argc < 1)
		errexit("usage: elfcnk filename");

	if (pv) {
		print("We're supposed to be in P==V at %p\n", (void *)pv);
		*(unsigned char *)pv = 0;
		print("Store to pv worked\n");
	}

	fd = open(argv[0], OREAD);
	if (fd < 0)
		errexit(smprint("Can't open %s\n", argv[1]));

	if (crackhdr(fd, &fp) < 0)
		errexit("crackhdr failed");

	map = loadmap(nil, fd, &fp);

	if (! map)
		errexit("loadmap failed");

	textseg = findseg(map, "text");
	if (textseg < 0)
		errexit("no text segment");
	dataseg = findseg(map, "data");
	if (dataseg < 0)
		errexit("no data segment");

	naux(AT_DCACHEBSIZE, 0x20);
	naux(AT_PAGESZ, 0x1000);
	naux(AT_UID, 0x17be);
	naux(AT_EUID, 0x17be);
	naux(AT_GID, 0x64);
	naux(AT_EGID, 0x64);
	naux(AT_HWCAP, 0x4);
	hdr(); 


	fprint(2, "textseg is %d and dataseg is %d\n", textseg, dataseg);
	fprint(2, "base %#llx end %#llx off %#llx \n", map->seg[0].b, map->seg[0].e, map->seg[2].f);
	fprint(2, "base %#llx end %#llx off %#llx \n", map->seg[1].b, map->seg[1].e, map->seg[1].f);
	fprint(2, "txtaddr %#llx dataaddr %#llx entry %#llx txtsz %#lx datasz %#lx bsssz %#lx\n", 
		fp.txtaddr, fp.dataddr, fp.entry, fp.txtsz, fp.datsz, fp.bsssz);
	if (dirstat("/n/cnk") == nil)
	if (create("/n/cnk", 0, DMDIR|0777)<0)
		errexit("/n/cnk");

	/* let's make us some seggies */
	if (bind("#g", "/n/cnk", MREPL|MCREATE) < 0)
		errexit("bind segdriver");

	textp = makeseg("0", fp.txtaddr, RNDM(fp.txtsz), 0, 0);
	datap = makeseg("1", fp.dataddr, RNDM( fp.datsz + fp.bsssz), 0, 0);
	if (pv) {
		bssbase = pv;
		print("Memory at pv is %ld\n", *(unsigned long *)pv);
		memset((void *)bssbase, 0, bsssize);
		print("Zero memory BEFORE makeseg\n");
	}
	bssp = makeseg("3", bssbase, bsssize, 1, pv);
	print("bssp is %p\n", bssp);
	print("zero bss\n");
	memset((void *)bssp, 0, bsssize);
	print("BSS zero\n");

	/* now the big fun. Just copy it out */
	pread(fd, textp, fp.txtsz, fp.txtoff);
	pread(fd, datap, fp.datsz, fp.datoff);
	/* DEBUGGING
	hangpath = smprint("/proc/%d/ctl", getpid());

	fprint(2, "Open %s\n", hangpath);
	hang = open(hangpath, OWRITE);
	if (hang < 0){
		errexit(smprint("%s: %r", hangpath));
	}
	if (write(hang, "hang", 4) < 4){
		errexit(smprint("write cnk: %r"));
	}

	*/

	/* gnu is odd. they start out knowing args are on stack (sort of) */
	/* av is going to become the start of the stack. */
	/* this is where argc goes. */
	av[0] = (char *)argc;
	for(i = 0; i < argc; i++)
		av[i+1] = argv[i];
	i++;
	av[i++] = nil;
	av[i++] = "LANG=C";
	av[i++] = "MALLOC_MMAP_MAX_=0";
	av[i++] = nil;
	/* now just copy the aux array over av */
	memcpy(&av[i], aux, sizeof(aux));
	fprint(2, "env %p *env %p\n", &av[argc+2], av[argc+2]);
	/* set the breakpoint */
	if (breakpoint){
		*(u32int *)breakpoint = 0;
		fprint(2, "Breakpoint set at %#x\n", breakpoint);
	}
	fprint(2, "Open %s\n", ctlpath);
	ctl = open(ctlpath, OWRITE);
	if (ctl < 0){
		errexit(smprint("%s: %r", ctlpath));
	}
	sprint(cmd, "%1x", cnk);
	fprint(2, "write cnk cmd (%s)\n", cmd);	
	if (write(ctl, cmd, 2) < 1){
		errexit(smprint("write cnk: %r"));
	}

//	callcnk(f, argc, argv);
	print("Call CNK!\n");
	f = (void *)fp.entry;
	callcnk(f, argc, &av[1], av);
	return 0;

}
/* qc -wF machcnk.c; ql -o machcnk machcnk.q */
/* 8c -wF  machcnk.c; 8l -o machcnk machcnk.8 */

/* fcp q.out /mnt/term/tmp/machcnk 
qc -wF machcnk.c; ql -o machcnk machcnk.q
/usr/rminnich//bin/386/scp q.machcnk surveyor.alcf.anl.gov:rompi
*/
