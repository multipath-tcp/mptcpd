#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP	262
#endif

static volatile sig_atomic_t g_alarm;

static void on_alarm(int sig)
{
	(void)sig;
	g_alarm = 1;
}

static int mptcp_socket(void)
{
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);

	if (fd < 0)
		perror("socket");

	return fd;
}

/* run_session: keep the connection alive for @wait seconds.
 * server drains incoming data; client keeps sending so the path manager
 * has data to piggyback ADD_ADDR on and drive MP_JOIN. */
static int run_session(int fd, int wait, int is_server)
{
	char buf[4096];

	if (wait <= 0)
		return 0;

	signal(SIGALRM, on_alarm);
	alarm(wait);
	memset(buf, 'x', sizeof(buf));

	if (is_server) {
		while (!g_alarm)
			if (read(fd, buf, sizeof(buf)) < 0
			    && errno != EINTR) {
				perror("read");
				return -1;
			}
	} else {
		while (!g_alarm) {
			if (write(fd, buf, sizeof(buf)) < 0
			    && errno != EINTR) {
				perror("write");
				return -1;
			}
			usleep(1000);	/* ~4 MB/s, keep connection very active */
		}
	}
	return 0;
}

static int do_listen(const char *addr, int port)
{
	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port   = htons(port)
	};
	int fd = mptcp_socket(), cfd;

	if (fd < 0)
		return -1;

	if (!addr)
		addr = "0.0.0.0";

	if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1 ||
	    bind(fd, (void *)&sa, sizeof(sa)) != 0 ||
	    listen(fd, 1) != 0) {
		perror("listen");
		close(fd);
		return -1;
	}

	cfd = accept(fd, NULL, NULL);
	if (cfd < 0) {
		perror("accept");
		close(fd);
		return -1;
	}

	/* Keep the listen socket open so that additional MP_JOIN subflows
	 * belonging to the same MPTCP connection can still be accepted. */
	return cfd;
}

static int do_connect(const char *addr, int port)
{
	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port   = htons(port)
	};
	int fd = mptcp_socket();

	if (fd < 0)
		return -1;

	if (!addr)
		addr = "127.0.0.1";

	if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1 ||
	    connect(fd, (void *)&sa, sizeof(sa)) != 0) {
		fprintf(stderr, "connect error addr: %s\n", addr);
		close(fd);
		return -1;
	}

	return fd;
}

int main(int argc, char *argv[])
{
	int listen = 0, port = 0, wait = 30, opt;
	const char *addr = NULL;
	int fd, rc;

	while ((opt = getopt(argc, argv, "lp:w:h:")) != -1) {
		switch (opt) {
		case 'l': listen = 1; break;
		case 'p': port = atoi(optarg); break;
		case 'w': wait = atoi(optarg); break;
		case 'h': addr = optarg; break;
		default:
			  fprintf(stderr,
				  "usage: %s -l -p PORT [-h ADDR] [-w SECS]\n"
				  "	  %s -p PORT -h ADDR [-w SECS]\n",
				  argv[0], argv[0]);
			  return 1;
		}
	}

	fd = listen ? do_listen(addr, port)
		    : do_connect(addr, port);
	rc = run_session(fd, wait, listen);
	close(fd);

	return rc;
}
