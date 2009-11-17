/*
 * bw_tcp.c - simple TCP bandwidth test
 *
 * Three programs in one -
 *	server usage:	bw_tcp -s
 *	client usage:	bw_tcp hostname [msgsize]
 *	shutdown:	bw_tcp -hostname
 *
 * Copyright (c) 1994 Larry McVoy.  
 * Copyright (c) 2002 Carl Staelin.  Distributed under the FSF GPL with
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

int	server_main(int ac, char **av);
int	client_main(int ac, char **av);
void	source(int lfd, char *ldir);

void
transfer(uint64 msgsize, int server, char *buf)
{
	int	c;

	while ((c = read(server, buf, msgsize)) > 0) {
		msgsize -= c;
	}
	if (c < 0) {
		perror("bw_tcp: transfer: read failed");
		exits("read");
	}
}

/* ARGSUSED */
int
client_main(int ac, char **av)
{
	int	server;
	uint64	msgsize = XFERSIZE;
	uint64	usecs;
	char	t[512];
	char*	buf;
	char*	usage = "usage: %s -remotehost OR %s remotehost [msgsize]\n";

	if (ac != 2 && ac != 3) {
		(void)fprint(stderr, usage, av[0], av[0]);
		exits(0);
	}
	if (ac == 3) {
		msgsize = bytes(av[2]);
	}
	/*
	 * Disabler message to other side.
	 */
	if (av[1][0] == '-') {
//		server = tcp_connect(&av[1][1], TCP_DATA, SOCKOPT_REUSE);
		server = dial(netmkaddr(&av[1][1], "tcp", "6662"), 0, 0, 0);
		write(server, "0", 1);
		exits(0);
	}

	buf = valloc(msgsize);
	touch(buf, msgsize);
	if (!buf) {
		perror("valloc");
		exits("valloc");
	}

//	server = tcp_connect(av[1], TCP_DATA, SOCKOPT_READ|SOCKOPT_REUSE);
	server = dial(netmkaddr(av[1], "tcp", "6662"), 0, 0, 0);

	if (server < 0) {
		perror("bw_tcp: could not open socket to server");
		exits("couldn't open socket");
	}

	(void)sprint(t, "%llud", msgsize);
	if (write(server, t, strlen(t) + 1) != strlen(t) + 1) {
		perror("control write");
		exits("couldn't write");
	}

	/*
	 * Send data over socket for at least 7 seconds.
	 * This minimizes the effect of connect & opening TCP windows.
	 */

	BENCH1(transfer(msgsize, server, buf), LONGER);

	BENCH(transfer(msgsize, server, buf), 0);
out:	(void)fprint(stderr, "TCP bandwidth using %s: ", av[1]);
	mb(msgsize * get_n());
	close(server);
	exits(0);
	/*NOTREACHED*/
}

void
child()
{
	wait();
//	signal(SIGCHLD, child);
}

/* ARGSUSED */
int
server_main(int ac, char **av)
{
	int	data, newdata;
	int dfd, acfd, lcfd;
	char adir[40], ldir[40];

//	GO_AWAY;

//	signal(SIGCHLD, child);
//	data = tcp_server(TCP_DATA, SOCKOPT_READ|SOCKOPT_WRITE|SOCKOPT_REUSE);
	acfd = announce("tcp!*!6662", adir);

	if (acfd < 0) {
		print("Couldn't announce\n");
		exits("couldn't announce");
	}

	for ( ;; ) {
		lcfd = listen(adir, ldir);
		if (lcfd < 0)
			exits("couldn't listen");
		//newdata = tcp_accept(data, SOCKOPT_WRITE|SOCKOPT_READ);
		switch (fork()) {
		    case -1:
				perror("fork");
				close(lcfd);
				break;
		    case 0:
				source(lcfd, ldir);
			exits(0);
		    default:
				close(newdata);
				break;
		}
	}
}

/*
 * Read the number of bytes to be transfered.
 * Write that many bytes on the data socket.
 */
void
source(int lfd, char *ldir)
{
	int	n;
	char	t[512];
	char*	buf;
	uint64	msgsize;
	int fd;

	fd = accept(lfd, ldir);

	bzero((void*)t, 512);
	if (read(fd, t, 511) <= 0) {
		perror("control nbytes");
		exits("read");
	}
//	sscanf(t, "%llu", &msgsize);
	msgsize = (uint64)strtoull(t, 0, 10);

	buf = valloc(msgsize);
	touch(buf, msgsize);
	if (!buf) {
		perror("valloc");
		exits("valloc");
	}

	/*
	 * A hack to allow turning off the absorb daemon.
	 */
     	if (msgsize == 0) {
//		tcp_done(TCP_DATA);
//		kill(getppid(), SIGTERM);
		exits(0);
	}
	
//	fprint(stderr, "server: msgsize=%llud, t=%s\n", msgsize, t);
	
	/* XXX */
	while ((n = write(fd, buf, msgsize)) > 0) {
#ifdef	TOUCH
		touch(buf, msgsize);
#endif
//		print("wrote\n");
		;
	}
	free(buf);
}


int
main(int ac, char **av)
{
	char*	usage = "Usage: %s -s OR %s -serverhost OR %s serverhost [msgsize]\n";
	if (ac < 2 || 3 < ac) {
		fprintf(stderr, usage, av[0], av[0], av[0]);
		exits("usage");
	}
	if (ac == 2 && !strcmp(av[1], "-s")) {
		if (fork() == 0) {
			server_main(ac, av);
		}
		exits(0);
	} else {
		client_main(ac, av);
	}
	return(0);
}
