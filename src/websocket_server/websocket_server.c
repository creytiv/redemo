/**
 * @file websocket_server.c  Websocket Server Demo
 *
 * The demo can for instance be tested by pasting this into
 * a web browser javascript console:
 *	var ws = new WebSocket('ws://localhost:3456');
 *	ws.addEventListener('open', () => ws.send('hello'));
 *	ws.addEventListener('close', (e) => console.log('close', e));
 *	ws.addEventListener('message', (e) => console.log('msg', e));
 *
 * Copyright (C) 2018 Creytiv.com
 */

#include <re.h>


struct websocket {
	struct le le;				/* list entry */
	struct websock_conn *conn;	/* Websocket connection */
};

static struct websock *server;

/* Linked list of active Websocket connections */
static struct list connl;

/* called upon reception of  SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
	re_printf("terminating on signal %d...\n", sig);

	/* destroy active Websocket connections */
	list_flush(&connl);

	websock_shutdown(server);
}


static void websock_shutdown_handler(void *arg)
{
	(void)arg;
	re_printf("websocket server shutting down\n");

	/* stop libre main loop */
	re_cancel();
}


/* destructor called when reference count on conn reach zero */
static void destructor(void *arg)
{
	struct websocket *ws = arg;

	/* remove this connection entry from the list */
	list_unlink(&ws->le);

	/* clean up local Websocket connection state */
	mem_deref(ws->conn);
}


/* called when a message is received from the other side */
static void srv_websock_recv_handler(const struct websock_hdr *hdr,
					 struct mbuf *mb, void *arg)
{
	struct websocket *ws = arg;
	int err;

	err = websock_send(ws->conn, hdr->opcode, "%b",
		mbuf_buf(mb), mbuf_get_left(mb));
	if (err)
		re_fprintf(stderr, "ws send error: %m\n", err);
}


/* called when the websocket is closed by the other side */
static void srv_websock_close_handler(int err, void *arg)
{
	struct websocket *ws = arg;

	re_printf("ws close %m\n", err);
	mem_deref(ws);
}


/* HTTP request handler */
static void http_req_handler(struct http_conn *conn,
				 const struct http_msg *msg, void *arg)
{
	struct websocket *ws;
	int err;

	/* allocate connection state */
	ws = mem_zalloc(sizeof(*ws), destructor);
	if (!ws) {
		err = ENOMEM;
		re_fprintf(stderr, "http req handler alloc error: %m\n", err);
		goto out;
	}

	err = websock_accept(&ws->conn, server, conn, msg,
				 0, srv_websock_recv_handler,
				 srv_websock_close_handler, ws);
	if (err) {
		re_fprintf(stderr, "websocket accept error: %m\n", err);
		goto out;
	}

	/* append connection to connection list */
	list_append(&connl, &ws->le, ws);
 out:
	if (err) {
		http_reply(conn, 500, "Server Error", NULL);
		/* destroy state */
		mem_deref(ws);
	}
}


int main(void)
{
	struct sa laddr;
	struct http_sock *httpsock = NULL;
	int err; /* errno return values */

	/* enable coredumps to aid debugging */
	(void)sys_coredump_set(true);

	/* initialize libre state */
	err = libre_init();
	if (err) {
		re_fprintf(stderr, "re init failed: %m\n", err);
		goto out;
	}

	(void)sa_set_str(&laddr, "0.0.0.0", 3456);

	/* Create listening HTTP server, IP address 0.0.0.0, TCP port 3456 */
	err = http_listen(&httpsock, &laddr, http_req_handler, NULL);
	if (err) {
		re_fprintf(stderr, "http listen error: %m\n", err);
		goto out;
	}

	err = websock_alloc(&server, websock_shutdown_handler, NULL);
	if (err) {
		re_fprintf(stderr, "websocket alloc error: %m\n", err);
		goto out;
	}

	re_printf("listening on WS socket: %J\n", &laddr);

	/* main loop */
	err = re_main(signal_handler);

 out:
	/* free HTTP socket */
	httpsock = mem_deref(httpsock);

	/* free Websocket server */
	server = mem_deref(server);

	/* free library state */
	libre_close();

	/* check for memory leaks */
	tmr_debug();
	mem_debug();

	return err;
}
