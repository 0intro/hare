/*
 * lat_syscall.c - time simple system calls
 *
 * Copyright (c) 1996 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 */

/*
 * This code has been modified to run under Plan 9; as specified in the
 * copyright information above, please do not publish results obtained
 * with this software.
 *
 * John Floren, 2008
 */

char	*id = "$Id$\n";

#include "bench.h"
#define	FNAME "/lib/namespace"

void
do_write(int fd)
{
	char	c;

	if (write(fd, &c, 1) != 1) {
		perror("/dev/null");
		return;
	}
}

void
do_read(int fd)
{
	char	c;

	if (read(fd, &c, 1) != 1) {
		perror("/dev/zero");
		return;
	}
}

void
do_stat(char *s)
{
	if (dirstat(s) == nil) {
		perror(s);
		return;
	}
}

void
do_fstat(int fd)
{
	if (dirfstat(fd) == nil) {
		perror("dirfstat");
		return;
	}
}

void
do_openclose(char *s)
{
	int	fd;

	fd = open(s, 0);
	if (fd == -1) {
		perror(s);
		return;
	}
	close(fd);
}

int
main(int ac, char **av)
{
	int	fd;
	char	*file;

	if (ac < 2) goto usage;
	file = av[2] ? av[2] : FNAME;

	if (!strcmp("null", av[1])) {
		BENCH(sysr1((ulong*)0), 0);
		micro("Simple syscall (sysr1)", get_n());
	} else if (!strcmp("write", av[1])) {
		fd = open("/dev/null", OWRITE);
		BENCH(do_write(fd), 0);;
		micro("Simple write", get_n());
		close(fd);
	} else if (!strcmp("read", av[1])) {
		fd = open("/dev/zero", OREAD);
		if (fd == -1) {
			fprintf(stderr, "Read from /dev/zero: -1");
			return(1);
		}
		BENCH(do_read(fd), 0);
		micro("Simple read", get_n());
		close(fd);
	} else if (!strcmp("stat", av[1])) {
		BENCH(do_stat(file), 0);
		micro("Simple stat", get_n());
	} else if (!strcmp("fstat", av[1])) {
		fd = open(file, OREAD);
		BENCH(do_fstat(fd), 0);
		micro("Simple fstat", get_n());
	} else if (!strcmp("open", av[1])) {
		BENCH(do_openclose(file), OREAD);
		micro("Simple open/close", get_n());
	} else {
usage:		print("Usage: %s null|read|write|stat|open\n", av[0]);
	}
	return(0);
}
