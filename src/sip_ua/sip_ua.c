/**
 * @file sip_ua.c SIP User Agent Demo
 *
 *
 *  To test inbound calls, invoke without arguments. sip_ua will
 *  register and wait for an inbound call:
 *
 *  $ ./sip_ua
 *
 *
 *  To test otbound calls, invoke with a SIP URI argument.
 *  sip_will will invite provided URI:
 *
 *  $ ./sip_ua sip:echo@creytiv.com
 *
 *
 * Copyright (C) 2011 Creytiv.com
 */

#include <string.h>
#include <re.h>


static struct sipsess_sock *sess_sock;  /* SIP session socket */
static struct sdp_session *sdp;         /* SDP session        */
static struct sdp_media *sdp_media;     /* SDP media          */
static struct sipsess *sess;            /* SIP session        */
static struct sipreg *reg;              /* SIP registration   */
static struct sip *sip;                 /* SIP stack          */
static struct rtp_sock *rtp;            /* RTP socket         */

const char *registrar  = "sip:creytiv.com";
const char *uri  = "sip:demo@creytiv.com";
const char *name = "demo";


/* terminate */
static void terminate(void)
{
	/* terminate session */
	sess = mem_deref(sess);

	/* terminate registration */
	reg = mem_deref(reg);

	/* wait for pending transactions to finish */
	sip_close(sip, false);
}


/* called for every received RTP packet */
static void rtp_handler(const struct sa *src, const struct rtp_header *hdr,
			struct mbuf *mb, void *arg)
{
	(void)hdr;
	(void)arg;

	re_printf("rtp: recv %zu bytes from %J\n", mbuf_get_left(mb), src);
}


/* called for every received RTCP packet */
static void rtcp_handler(const struct sa *src, struct rtcp_msg *msg, void *arg)
{
	(void)arg;

	re_printf("rtcp: recv %s from %J\n", rtcp_type_name(msg->hdr.pt), src);
}


/* called when challenged for credentials */
static int auth_handler(char **user, char **pass, const char *realm, void *arg)
{
	int err = 0;
	(void)realm;
	(void)arg;

	err |= str_dup(user, "demo");
	err |= str_dup(pass, "secret");

	return err;
}


/* print SDP status */
static void update_media(void)
{
	const struct sdp_format *fmt;

	re_printf("SDP peer address: %J\n", sdp_media_raddr(sdp_media));

	fmt = sdp_media_rformat(sdp_media, NULL);
	if (!fmt) {
		re_printf("no common media format found\n");
		return;
	}

	re_printf("SDP media format: %s/%u/%u (payload type: %u)\n",
		  fmt->name, fmt->srate, fmt->ch, fmt->pt);
}


/*
 * called when an SDP offer is received (got offer: true) or
 * when an offer is to be sent (got_offer: false)
 */
static int offer_handler(struct mbuf **mbp, const struct sip_msg *msg,
			 void *arg)
{
	const bool got_offer = mbuf_get_left(msg->mb);
	int err;
	(void)arg;

	if (got_offer) {

		err = sdp_decode(sdp, msg->mb, true);
		if (err) {
			re_fprintf(stderr, "unable to decode SDP offer: %s\n",
				   strerror(err));
			return err;
		}

		re_printf("SDP offer received\n");
		update_media();
	}
	else {
		re_printf("sending SDP offer\n");
	}

	return sdp_encode(mbp, sdp, !got_offer);
}


/* called when an SDP answer is received */
static int answer_handler(const struct sip_msg *msg, void *arg)
{
	int err;
	(void)arg;

	re_printf("SDP answer received\n");

	err = sdp_decode(sdp, msg->mb, false);
	if (err) {
		re_fprintf(stderr, "unable to decode SDP answer: %s\n",
			   strerror(err));
		return err;
	}

	update_media();

	return 0;
}


/* called when SIP progress (like 180 Ringing) responses are received */
static void progress_handler(const struct sip_msg *msg, void *arg)
{
	(void)arg;

	re_printf("session progress: %u %r\n", msg->scode, &msg->reason);
}


/* called when the session is established */
static void establish_handler(const struct sip_msg *msg, void *arg)
{
	(void)msg;
	(void)arg;

	re_printf("session established\n");
}


/* called when the session fails to connect or is terminated from peer */
static void close_handler(int err, const struct sip_msg *msg, void *arg)
{
	(void)arg;

	if (err)
		re_printf("session closed: %s\n", strerror(err));
	else
		re_printf("session closed: %u %r\n", msg->scode, &msg->reason);

	terminate();
}


/* called upon incoming calls */
static void connect_handler(const struct sip_msg *msg, void *arg)
{
	struct mbuf *mb;
	bool got_offer;
	int err;
	(void)arg;

	if (sess) {
		/* Already in a call */
		(void)sip_treply(NULL, sip, msg, 486, "Busy Here");
		return;
	}

	got_offer = (mbuf_get_left(msg->mb) > 0);

	/* Decode SDP offer if incoming INVITE contains SDP */
	if (got_offer) {

		err = sdp_decode(sdp, msg->mb, true);
		if (err) {
			re_fprintf(stderr, "unable to decode SDP offer: %s\n",
				   strerror(err));
			goto out;
		}

		update_media();
	}

	/* Encode SDP */
	err = sdp_encode(&mb, sdp, !got_offer);
	if (err) {
		re_fprintf(stderr, "unable to encode SDP: %s\n",
			   strerror(err));
		goto out;
	}

	/* Answer incoming call */
	err = sipsess_accept(&sess, sess_sock, msg, 200, "OK",
			     name, "application/sdp", mb,
			     auth_handler, NULL, false,
			     offer_handler, answer_handler,
			     establish_handler, NULL, NULL,
			     close_handler, NULL, NULL);
	mem_deref(mb); /* free SDP buffer */
	if (err) {
		re_fprintf(stderr, "session accept error: %s\n",
			   strerror(err));
		goto out;
	}

 out:
	if (err) {
		(void)sip_treply(NULL, sip, msg, 500, strerror(err));
	}
	else {
		re_printf("accepting incoming call from <%r>\n",
			  &msg->from.auri);
	}
}


/* called when register responses are received */
static void register_handler(int err, const struct sip_msg *msg, void *arg)
{
	(void)arg;

	if (err)
		re_printf("register error: %s\n", strerror(err));
	else
		re_printf("register reply: %u %r\n", msg->scode, &msg->reason);
}


/* called when all sip transactions are completed */
static void exit_handler(void *arg)
{
	(void)arg;

	/* stop libre main loop */
	re_cancel();
}


/* called upon reception of  SIGINT, SIGALRM or SIGTERM */
static void signal_handler(int sig)
{
	re_printf("terminating on signal %d...\n", sig);

	terminate();
}


int main(int argc, char *argv[])
{
	struct sa nsv[16];
	struct dnsc *dnsc = NULL;
	struct sa laddr;
	uint32_t nsc;
	int err; /* errno return values */

	/* enable coredumps to aid debugging */
	(void)sys_coredump_set(true);

	/* initialize libre state */
	err = libre_init();
	if (err) {
		re_fprintf(stderr, "re init failed: %s\n", strerror(err));
		goto out;
	}

	nsc = ARRAY_SIZE(nsv);

	/* fetch list of DNS server IP addresses */
	err = dns_srv_get(NULL, 0, nsv, &nsc);
	if (err) {
		re_fprintf(stderr, "unable to get dns servers: %s\n",
			   strerror(err));
		goto out;
	}

	/* create DNS client */
	err = dnsc_alloc(&dnsc, NULL, nsv, nsc);
	if (err) {
		re_fprintf(stderr, "unable to create dns client: %s\n",
			   strerror(err));
		goto out;
	}

	/* create SIP stack instance */
	err = sip_alloc(&sip, dnsc, 32, 32, 32,
			"ua demo v" VERSION " (" ARCH "/" OS ")",
			exit_handler, NULL);
	if (err) {
		re_fprintf(stderr, "sip error: %s\n", strerror(err));
		goto out;
	}

	/* fetch local IP address */
	err = net_default_source_addr_get(AF_INET, &laddr);
	if (err) {
		re_fprintf(stderr, "local address error: %s\n", strerror(err));
		goto out;
	}

	/* listen on random port */
	sa_set_port(&laddr, 0);

	/* add supported SIP transports */
	err |= sip_transp_add(sip, SIP_TRANSP_UDP, &laddr);
	err |= sip_transp_add(sip, SIP_TRANSP_TCP, &laddr);
	if (err) {
		re_fprintf(stderr, "transport error: %s\n", strerror(err));
		goto out;
	}

	/* create SIP session socket */
	err = sipsess_listen(&sess_sock, sip, 32, connect_handler, NULL);
	if (err) {
		re_fprintf(stderr, "session listen error: %s\n",
			   strerror(err));
		goto out;
	}

	/* create the RTP/RTCP socket */
	err = rtp_listen(&rtp, IPPROTO_UDP, &laddr, 10000, 30000, true,
			 rtp_handler, rtcp_handler, NULL);
	if (err) {
		re_fprintf(stderr, "rtp listen error: %m\n", err);
		goto out;
	}

	re_printf("local RTP port is %u\n", sa_port(rtp_local(rtp)));

	/* create SDP session */
	err = sdp_session_alloc(&sdp, &laddr);
	if (err) {
		re_fprintf(stderr, "sdp session error: %s\n", strerror(err));
		goto out;
	}

	/* add audio sdp media, using port from RTP socket */
	err = sdp_media_add(&sdp_media, sdp, "audio",
			    sa_port(rtp_local(rtp)), "RTP/AVP");
	if (err) {
		re_fprintf(stderr, "sdp media error: %s\n", strerror(err));
		goto out;
	}

	/* add G.711 sdp media format */
	err = sdp_format_add(NULL, sdp_media, false, "0", "PCMU", 8000, 1,
			     NULL, NULL, NULL, false, NULL);
	if (err) {
		re_fprintf(stderr, "sdp format error: %s\n", strerror(err));
		goto out;
	}

	/* invite provided URI */
	if (argc > 1) {

		struct mbuf *mb;

		/* create SDP offer */
		err = sdp_encode(&mb, sdp, true);
		if (err) {
			re_fprintf(stderr, "sdp encode error: %s\n",
				   strerror(err));
			goto out;
		}

		err = sipsess_connect(&sess, sess_sock, argv[1], name,
				      uri, name,
				      NULL, 0, "application/sdp", mb,
				      auth_handler, NULL, false,
				      offer_handler, answer_handler,
				      progress_handler, establish_handler,
				      NULL, NULL, close_handler, NULL, NULL);
		mem_deref(mb); /* free SDP buffer */
		if (err) {
			re_fprintf(stderr, "session connect error: %s\n",
				   strerror(err));
			goto out;
		}

		re_printf("inviting <%s>...\n", argv[1]);
	}
	else {

		err = sipreg_register(&reg, sip, registrar, uri, uri, 60, name,
				      NULL, 0, 0, auth_handler, NULL, false,
				      register_handler, NULL, NULL, NULL);
		if (err) {
			re_fprintf(stderr, "register error: %s\n",
				   strerror(err));
			goto out;
		}

		re_printf("registering <%s>...\n", uri);
	}

	/* main loop */
	err = re_main(signal_handler);

 out:
	/* clean up/free all state */
	mem_deref(sdp); /* will also free sdp_media */
	mem_deref(rtp);
	mem_deref(sess_sock);
	mem_deref(sip);
	mem_deref(dnsc);

	/* free librar state */
	libre_close();

	/* check for memory leaks */
	tmr_debug();
	mem_debug();

	return err;
}
