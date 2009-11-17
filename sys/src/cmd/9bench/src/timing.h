/*
 * $Id$
 */
#ifndef _TIMING_H
#define _TIMING_H

char	*p64(uint64 big);
char	*p64sz(uint64 big);
double	Delta(void);
double	Now(void);
void	adjust(int usec);
void	bandwidth(uint64 bytes, uint64 times, int verbose);
size_t	bytes(char *s);
void	context(uint64 xfers);
int		cyclesoverhead(void);
uint64	delta(void);
int	get_enough(int);
uint64	get_n(void);
uvlong	getcycles(void);
void	kb(uint64 bytes);
double	l_overhead(void);
char	last(char *s);
void	latency(uint64 xfers, uint64 size);
void	mb(uint64 bytes);
void	micro(char *s, u64int n);
void	micromb(uint64 mb, uint64 n);
void	milli(char *s, uint64 n);
void	morefds(void);
void	nano(char *s, u64int n);
uint64	now(void);
void	ptime(uint64 n);
void	rusage(void);
void	save_n(uint64);
void	setcycles(uvlong c);
void	settime(uint64 usecs);
void	start(struct timeval *tv);
void	stop(struct timeval *begin, struct timeval *end, stop_result *res);
uint64	t_overhead(void);
double	timespent(void);
void	timing(int out);
uint64	tvdelta(struct timeval *, struct timeval *);
void	tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0);
void	print_results(int details);
void	use_int(int result);
void	use_pointer(void *result);
uint64	usecs_spent(void);
void	touch(char *buf, size_t size);

#if defined(hpux) || defined(__hpux) || defined(WIN32)
int	getpagesize();
#endif

#endif /* _TIMING_H */
