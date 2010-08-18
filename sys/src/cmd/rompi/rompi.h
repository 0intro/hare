#ifdef cnk
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#define nil NULL
#define OREAD 0
#define ORDWR 2
typedef unsigned char u8int;
typedef unsigned char uchar;
typedef unsigned long long u64int;
typedef long long vlong;
typedef unsigned long long uvlong;
char *smprint(const char *fmt, ...);
void fprint(int fd, const char *fmt, ...);
void print(const char *fmt, ...);
void *mallocz(size_t size, int zero);
static __inline__ u64int tbget(void)
{
     unsigned int tbl, tbu0, tbu1;

     do {
          __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
          __asm__ __volatile__ ("mftb %0" : "=r"(tbl));
          __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
     } while (tbu0 != tbu1);

     return (((unsigned long long)tbu0) << 32) | tbl;
}

#endif
#ifndef cnk
#include <u.h>
#include <libc.h>
#endif
#include "mpi.h"

