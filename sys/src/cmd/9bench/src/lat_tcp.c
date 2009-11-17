/*
 * tcp_xact.c - simple TCP transaction latency test
 *
 * Three programs in one -
 *	server usage:	tcp_xact -s
 *	client usage:	tcp_xact hostname
 *	shutdown:	tcp_xact -hostname
 *
 * Plan 9 note: Doesn't kill the server process, must do that yourself.
 *
 * Copyright (c) 1994 Larry McVoy.  Distributed under the FSF GPL with
 * additional restriction that results may published only if
 * (1) the benchmark is unmodified, and
 * (2) the version in the sccsid below is included in the report.
 * Support for this development by Sun Microsystems is gratefully acknowledged.
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

void	client_main(int ac, char **av);
void	doserver(int sock);
void	doclient(int sock);
void	server_main(int ac, char **av);
void	doserver(int sock);

int
main(int ac, char **av)
{
	if (ac != 2) {
		fprintf(stderr, "Usage: %s -s OR %s [-]serverhost\n",
		    av[0], av[0]);
		exits("usage");
	}
	if (!strcmp(av[1], "-s")) {
		if (fork() == 0) {
			server_main(ac, av);
		}
		exits(nil);
	} else {
		client_main(ac, av);
	}
	return(0);
}

void
client_main(int ac, char **av)
{
	int     sock;
	char	*server;
	char	buf[100];

	if (ac != 2) {
		fprintf(stderr, "usage: %s host\n", av[0]);
		exits("usage");
	}
	server = av[1][0] == '-' ? &av[1][1] : av[1];
	sock = dial(smprint("tcp!%s!6662", server), 0, 0, 0);

	/*
	 * Stop server code.
	 */
	if (av[1][0] == '-') {
		close(sock);
		exits(nil);
	}

	BENCH(doclient(sock), MEDIUM);
	sprint(buf, "TCP latency using %s", av[1]);
	micro(buf, get_n());
	exits(nil);
	/* NOTREACHED */
}

void
doclient(int sock)
{
	char    c = 'a';

	write(sock, &c, 1);
	if (read(sock, &c, 1) != 1)
		print("didn't read correct size...\n");
}

void
child()
{
	wait();
	//signal(SIGCHLD, child);
}

void
server_main(int ac, char **av)
{
	int     n = 0;
	int dfd, acfd, lcfd;
	char adir[40], ldir[40];
	char c[1];

	if (ac != 2) {
		fprintf(stderr, "usage: %s -s\n", av[0]);
		exits("usage");
	}
//	GO_AWAY;
//	signal(SIGCHLD, child);
//	sock = tcp_server(TCP_XACT, SOCKOPT_NONE);
	acfd = announce("tcp!*!6662", adir);

	if (acfd < 0)
		exits("couldn't announce");

	for (;;) {
		lcfd = listen(adir, ldir);
		if (lcfd < 0)
			exits("couldn't listen");
		switch(fork()) {
			case -1:
				perror("forking");
				close(lcfd);
				break;
			case 0:
				dfd = accept(lcfd, ldir);
				while((n = read(dfd, c, 1)) > 0)
					write(dfd, c, 1);
				exits(0);
			default:
				close(lcfd);
				break;
		}
	}
	/* NOTREACHED */
}

void
doserver(int sock)
{
	char    c;
	int	n = 0;

	while (read(sock, &c, 1) == 1) {
		write(sock, &c, 1);
		n++;
	}

	/*
	 * A connection with no data means shut down.
	 */
	if (n == 0) {
//		tcp_done(TCP_XACT);
//		kill(getppid(), SIGTERM);
		exits(nil);
	}
}
