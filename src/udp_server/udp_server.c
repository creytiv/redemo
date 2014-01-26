/**
 * @file udp_server.c  UDP Server Demo
 *
 * The demo can for instance be tested with the netcat tool:
 *  $ nc -u 127.0.0.1 3456
 *
 * Copyright (C) 2011 Creytiv.com
 */

#include <string.h>
#include <re.h>


/* UDP Socket */
static struct udp_sock *us;


/* called upon reception of  SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
	re_printf("terminating on signal %d...\n", sig);

	/* stop libre main loop */
	re_cancel();
}


/* Asynchronus UDP callback, called when UDP packets are received */
static void recv_handler(const struct sa *src, struct mbuf *mb, void *arg)
{
	/* callback argument not used */
	(void)arg;

	re_printf("got %zu bytes of UDP data from %J\n",
		  mbuf_get_left(mb), src);

	/* received data is echoed back */
	(void)udp_send(us, src, mb);
}


int main(void)
{
	struct sa laddr;
	int err; /* errno return values */

	/* enable coredumps to aid debugging */
	(void)sys_coredump_set(true);

	/* initialize libre state */
	err = libre_init();
	if (err) {
		re_fprintf(stderr, "re init failed: %s\n", strerror(err));
		goto out;
	}

	(void)sa_set_str(&laddr, "0.0.0.0", 3456);

	/* Create listening UDP socket, IP address 0.0.0.0, UDP port 3456 */
	err = udp_listen(&us, &laddr, recv_handler, NULL);
	if (err) {
		re_fprintf(stderr, "udp listen error: %s\n", strerror(err));
		goto out;
	}

	re_printf("listening on UDP socket: %J\n", &laddr);

	/* main loop */
	err = re_main(signal_handler);

 out:
	/* free UDP socket */
	us = mem_deref(us);

	/* free library state */
	libre_close();

	/* check for memory leaks */
	tmr_debug();
	mem_debug();

	return err;
}
