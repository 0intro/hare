/*
 * udp_xact.c - simple UDP transaction latency test
 *
 * Three programs in one -
 *	server usage:	udp_xact -s
 *	client usage:	udp_xact hostname
 *	shutdown:	udp_xact -hostname
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
void	server_main(int ac, char **av);

void
timeout()
{
	fprint(stderr, "Recv timed out\n");
	exits("timeout");
}

void
doit(int sock)
{
	char c[4] = {'f', 'o', 'o', '\0'};
//	int n = 0;

//	print("in doit\n");

	if (write(sock, c, sizeof(c)) != sizeof(c)) {
		perror("send failed");
		exits("send");
	}
//	print("client wrote %s\n", c);
	if (read(sock, c, sizeof(c)) != sizeof(c)) {
		perror("receive failed");
		exits("receive");
	}
//	print("client read %s\n", c);

/*
	int net = htonl(seq);
	int ret;

	if (send(sock, &net, sizeof(net), 0) != sizeof(net)) {
		perror("lat_udp client: send failed");
		exits("send");
	}
	if (recv(sock, &ret, sizeof(ret), 0) != sizeof(ret)) {
		perror("lat_udp client: recv failed");
		exits("recv");
	}
*/
}

int
main(int ac, char **av)
{
	if (sizeof(int) != 4) {
		fprintf(stderr, "lat_udp: Wrong sequence size\n");
		return(1);
	}
	if (ac != 2 && ac != 3) {
		fprintf(stderr, "Usage: %s -s OR %s [-]serverhost [proto]\n",
		    av[0], av[0]);
		return(1);
	}
	if (!strcmp(av[1], "-s")) {
		if (fork() == 0) {
			server_main(ac, av);
		}
		return(0);
	} else {
		client_main(ac, av);
	}
	return(0);
}

void
client_main(int ac, char **av)
{
	int     sock;
	int     seq = -1;
	char   *server;
	char	buf[256];

	if (ac != 2) {
		fprintf(stderr, "Usage: %s hostname\n", av[0]);
		exits("usage");
	}

	server = av[1][0] == '-' ? &av[1][1] : av[1];
//	sock = udp_connect(server, UDP_XACT, SOCKOPT_NONE);
	sock = dial(netmkaddr(server, "udp", "6663"), 0, 0, 0);

	BENCH(doit(sock), MEDIUM);
	sprint(buf, "UDP latency using %s", server);
	micro(buf, get_n());
	exits(0);
}

/* ARGSUSED */
void
server_main(int ac, char **av)
{
//	int     net, sock, sent, namelen, seq = 0;
	int n = 0;
	int fd, acfd;
	char adir[40], ldir[40];
	char data[50];
	char c[128];
	struct sockaddr_in it;

//	GO_AWAY;

//	sock = udp_server(UDP_XACT, SOCKOPT_NONE);

	acfd = announce("udp!*!6663", adir);

	if (acfd < 0)
		exits("couldn't announce");

	fprint(acfd, "headers");
	
	sprint(data, "%s/data", adir);
	fd = open(data, ORDWR);

	/*
	for (;;) {
		if (!(n = read(fd, c, sizeof(c)) > 0)) {
			perror("server couldn't read");
			exits("read");
		}
		print("server read %s\n", c);
		if (write(fd, c, sizeof(c)) != sizeof(c)) {
			perror("server couldn't write");
			exits("write");
		}
		print("server wrote %s\n", c);
	}
	*/

	while ((n = read(fd, c, sizeof c)) > 0)
		write(fd, c, n);

/*
	while (1) {
		namelen = sizeof(it);
		if (recvfrom(sock, (void*)&sent, sizeof(sent), 0, 
		    (struct sockaddr*)&it, &namelen) < 0) {
			fprintf(stderr, "lat_udp server: recvfrom: got wrong size\n");
			exits("bad size");
		}
		sent = ntohl(sent);
		if (sent < 0) {
			udp_done(UDP_XACT);
			exits(0);
		}
		if (sent != ++seq) {
			seq = sent;
		}
		net = htonl(seq);
		if (sendto(sock, (void*)&net, sizeof(net), 0, 
		    (struct sockaddr*)&it, sizeof(it)) < 0) {
			perror("lat_udp sendto");
			exits("lat_udp sendto");
		}
	}
*/
}
