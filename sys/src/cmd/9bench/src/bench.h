/*
 * $Id$
 */
#ifndef _BENCH_H
#define _BENCH_H

#include <u.h>
#include <libc.h>

//#define _DEBUG

typedef u64int uint64;
typedef usize size_t;
typedef ulong u_long;
typedef long ssize_t;
typedef int pid_t;

struct timeval {
	long tv_sec;
	long tv_usec;
};

typedef struct stop_result {
	double time;
	uvlong cycles;
} stop_result;

#define NULL 0
#define stderr 2
#define fprintf fprint
/* We don't have bzero or bcopy, but memset/memcpy work similarly */
#define bzero(area, num) memset(area, 0, num)
#define bcopy(s1, s2, n) memcpy(s1, s2, n)
#define valloc(nbytes) mallocalign(nbytes, 0, 0, 0);

#define PORTMAP

void TRACE(char* format, ...);

#define	NO_PORTMAPPER	/* needs to be up here, lib_*.h look at it */
#include	"stats.h"
#include	"timing.h"
#include	"lib_tcp.h"
#include	"lib_udp.h"
#include	"lib_unix.h"


#ifdef	DEBUG
#	define		debug(x) fprintf x
#else
#	define		debug(x)
#endif
#ifdef	NO_PORTMAPPER
#define TCP_SELECT	-31233
#define	TCP_XACT	-31234
#define	TCP_CONTROL	-31235
#define	TCP_DATA	-31236
#define	TCP_CONNECT	-31237
#define	UDP_XACT	-31238
#define	UDP_DATA	-31239
#else
#define	TCP_SELECT	(u_long)404038	/* XXX - unregistered */
#define	TCP_XACT	(u_long)404039	/* XXX - unregistered */
#define	TCP_CONTROL	(u_long)404040	/* XXX - unregistered */
#define	TCP_DATA	(u_long)404041	/* XXX - unregistered */
#define	TCP_CONNECT	(u_long)404042	/* XXX - unregistered */
#define	UDP_XACT 	(u_long)404032	/* XXX - unregistered */
#define	UDP_DATA 	(u_long)404033	/* XXX - unregistered */
#define	VERS		(u_long)1
#endif

#define	UNIX_CONTROL	"/tmp/lmbench.ctl"
#define	UNIX_DATA	"/tmp/lmbench.data"
#define	UNIX_LAT	"/tmp/lmbench.lat"

/*
 * socket send/recv buffer optimizations
 */
#define	SOCKOPT_READ	0x0001
#define	SOCKOPT_WRITE	0x0002
#define	SOCKOPT_RDWR	0x0003
#define	SOCKOPT_PID	0x0004
#define	SOCKOPT_REUSE	0x0008
#define	SOCKOPT_NONE	0

#ifndef SOCKBUF
#define	SOCKBUF		(1024*1024)
#endif

#ifndef	XFERSIZE
#define	XFERSIZE	(64*1024)	/* all bandwidth I/O should use this */
#endif

#if defined(SYS5) || defined(WIN32)
#define	bzero(b, len)	memset(b, 0, len)
#define	bcopy(s, d, l)	memcpy(d, s, l)
#define	rindex(s, c)	strrchr(s, c)
#endif
#define	gettime		usecs_spent
#define	streq		!strcmp
#define	ulong		unsigned long

#ifdef USE_RAND
#define srand48		srand
#define drand48()	((double)rand() / (double)RAND_MAX)
#endif
#ifdef USE_RANDOM
#define srand48		srand
#define drand48()	((double)rand() / (double)RAND_MAX)
#endif

#ifdef WIN32
#include <process.h>
#define getpid _getpid
int	gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#define	SMALLEST_LINE	32		/* smallest cache line size */
#define	TIME_OPEN2CLOSE

#define	GO_AWAY	signal(SIGALRM, exit); alarm(60 * 60);
#define	REAL_SHORT	   50000
#define	SHORT	 	 1000000
#define	MEDIUM	 	 2000000
#define	LONGER		 7500000	/* for networking data transfers */
#define	ENOUGH		REAL_SHORT

#define	TRIES		11

typedef struct {
	int	N;
	uint64	u[TRIES];
	uint64	n[TRIES];
} result_t;



void    insertinit(result_t *r);
void    insertsort(uint64, uint64, result_t *);
void	save_median();
void	save_minimum();
void	save_results(result_t *r);
void	get_results(result_t *r);

#define	BENCHO(loop_body, overhead_body, enough) { 			\
	int 		__i, __N;					\
	double 		__oh;						\
	result_t 	__overhead, __r;				\
	insertinit(&__overhead); insertinit(&__r);			\
	__N = (enough == 0 || get_enough(enough) <= 100000) ? TRIES : 1;\
	if (enough < LONGER) {loop_body;} /* warm the cache */		\
	for (__i = 0; __i < __N; ++__i) {				\
		BENCH1(overhead_body, enough);				\
		if (gettime() > 0) 					\
			insertsort(gettime(), get_n(), &__overhead);	\
		BENCH1(loop_body, enough);				\
		if (gettime() > 0) 					\
			insertsort(gettime(), get_n(), &__r);		\
	}								\
	for (__i = 0; __i < __r.N; ++__i) {				\
		__oh = __overhead.u[__i] / (double)__overhead.n[__i];	\
		if (__r.u[__i] > (uint64)((double)__r.n[__i] * __oh)) {	\
			__r.u[__i] -= (uint64)((double)__r.n[__i] * __oh); \
		} else {							\
			__r.u[__i] = 0;					\
		}		\
	}								\
	save_results(&__r);						\
}

#define	BENCH(loop_body, enough) { 					\
	long		__i, __N;					\
	result_t 	__r;						\
	insertinit(&__r);						\
	__N = (enough == 0 || get_enough(enough) <= 100000) ? TRIES : 1;\
	if (enough < LONGER) {loop_body;} /* warm the cache */		\
	for (__i = 0; __i < __N; ++__i) {				\
		BENCH1(loop_body, enough);				\
		if (gettime() > 0) 					\
			insertsort(gettime(), get_n(), &__r);		\
	}								\
	save_results(&__r);						\
}

#define	BENCH1(loop_body, enough) { 					\
	double		__usecs;					\
	uvlong __cycles;			\
	BENCH_INNER(loop_body, enough);  				\
	__usecs = gettime();						\
	__cycles = getcycles();				\
	__usecs -= t_overhead() + get_n() * l_overhead();			\
	settime(__usecs >= 0. ? (uint64)__usecs : 0);	setcycles(__cycles);		\
}
	
#define	BENCH_INNER(loop_body, enough) { 				\
	static u_long	__iterations = 1;				\
	int		__enough = get_enough(enough);			\
	u_long		__n;						\
	stop_result		__result;					\
									\
	TRACE("BENCH_INNER(%s, %s): enter: __iterations=%lud\n", #loop_body, #enough, (unsigned long)__iterations); \
	while(__result.time < 0.95 * __enough) {				\
		TRACE("BENCH_INNER(%s, %s): start: __iterations=%lud\n", #loop_body, #enough, (unsigned long)__iterations); \
		start(0);						\
		for (__n = __iterations; __n > 0; __n--) {		\
			loop_body;					\
		}							\
		stop(0,0,&__result);					\
		TRACE("BENCH_INNER(%s, %s): stop: __result=%llud, __result / __enough = %llud\n", #loop_body, #enough, __result.time, __result.time / (double)__enough); \
		if (__result.time < 0.99 * __enough 				\
		    || __result.time > 1.2 * __enough) {			\
			if (__result.time > 150.) {				\
				double	tmp = __iterations / __result.time;	\
				tmp *= 1.1 * __enough;			\
				__iterations = (u_long)(tmp + 1);	\
			} else {					\
				if (__iterations > (u_long)1<<27) {	\
					__result.time = 0.;			\
					__result.cycles = 0;	\
					break;				\
				}					\
				__iterations <<= 3;			\
			}						\
		}							\
	} /* while */							\
	save_n((uint64)__iterations); settime((uint64)(__result.time));	setcycles(__result.cycles);\
}


/*
 * Generated from msg.x which is included here:

	program XACT_PROG {
	    version XACT_VERS {
		char
		RPC_XACT(char) = 1;
    	} = 1;
	} = 3970;

 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#define XACT_PROG ((u_long)404040)
#define XACT_VERS ((u_long)1)
#define RPC_XACT ((u_long)1)
#define RPC_EXIT ((u_long)2)
extern char *rpc_xact_1();
extern char *client_rpc_xact_1();

#endif /* _BENCH_H */
