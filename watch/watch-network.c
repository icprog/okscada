/* -*-c++-*-  vi: set ts=4 sw=4 :

  (C) Copyright 2006-2007, vitki.net. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

  $Date$
  $Revision$
  $Source$

  Socket interactions with the hub.

*/

#include "watch-priv.h"
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for OMAX,ONTOHS,... */
#include <optikus/conf-mem.h>	/* for oxnew,...,oxbcopy,... */
#include <string.h>				/* for memcpy,strncmp,... */

#include <unistd.h>			/* for close */
#include <stdlib.h>			/* for atoi */
#include <errno.h>			/* for errno,EPIPE */
#include <fcntl.h>			/* for F_GETFL,F_SETFL,O_NONBLOCK */
#include <stdio.h>			/* for perror */
#include <sys/socket.h>		/* for sockaddr */
#include <netinet/in.h>		/* for sockaddr_in */
#include <netinet/tcp.h>	/* for TCP_NODELAY */
#include <netdb.h>			/* for gethostbyname */

#if defined(HAVE_CONFIG_H)
#include <config.h>			/* for VXWORKS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS)
#include <sockLib.h>
#include <netinet/in.h>
#include <hostLib.h>
#include <netinet/tcp.h>
#include <ioLib.h>
#endif /* VXWORKS */

/* TCP/IP connection */

#if defined(HAVE_CONFIG_H)
#include <config.h>			/* for SOLARIS */
#endif /* HAVE_CONFIG_H */

#if defined(SOLARIS) && !defined(_SOCKLEN_T)
/* FIXME: must be handled by autoconf */
typedef int socklen_t;
#endif

#if defined(SOLARIS)
#define OWATCH_IO_FLAGS   0
#else
#define OWATCH_IO_FLAGS   MSG_NOSIGNAL	/* avoid SIGPIPE */
#endif

static int owatchCancelConnection(owop_t op, long data1, long data2);

static owop_t owatch_conn_op = OWOP_ERROR;

int     owatch_data_sock;
int     owatch_cntl_sock;

struct sockaddr_in owatch_hub_data_addr;
struct sockaddr_in owatch_hub_cntl_addr;
struct sockaddr_in owatch_watch_cntl_addr;

int     owatch_ping_cur_ms;
long    owatch_ping_wd;

#define OWATCH_CONN_NONE  0
#define OWATCH_CONN_OPEN  1
#define OWATCH_CONN_WAIT  2
#define OWATCH_CONN_EXIT  3

int     owatch_conn_state = OWATCH_CONN_NONE;

bool_t  owatch_is_polling;
int     owatch_poll_depth;

long    owatch_routine_ms;

long    owatch_alive_get_wd;
long    owatch_alive_put_wd;

#define OWATCH_TOLERABLE_POLL_DEPTH		1

/* packet sending */

#define OWATCH_Q_SIZE		2200	/* FIXME: use lists and avoid this limit */
#define OWATCH_POLL_SIZE	1000	/* ditto */

int     owatch_init_q_size = OWATCH_Q_SIZE;
int     owatch_init_poll_size = OWATCH_POLL_SIZE;

typedef struct
{
	int     len;
	int     off;
	int     type;
	char   *buf;
} OwatchQueuedPacket_t;

OwatchQueuedPacket_t owatch_reget = { -1, -1, -1, NULL };
OwatchQueuedPacket_t owatch_reput = { -1, -1, -1, NULL };

char    owatch_hdr_buf[16];
int     owatch_hdr_off = 0;

OwatchQueuedPacket_t *owatch_q_buf = NULL;

int     owatch_q_len;
int     owatch_q_size;
int     owatch_q_rd;
int     owatch_q_wr;

/* FIXME: remove workaround in Perl */

OwatchQueuedPacket_t *owatch_poll_buf = NULL;

int     owatch_poll_len;
int     owatch_poll_size;
int     owatch_poll_rd;
int     owatch_poll_wr;

int     owatch_can_immed = 1;	/* 0 = never send packets immediately
								 * 1 = packets less than 800 b
								 * 2 = send any packet immediately first
								 */

ushort_t owatch_watcher_id = 0;
ushort_t owatch_temp_id = 0;

/* secure packet sending */

typedef struct OwatchSecureSendRecord_st
{
	int     kind;
	int     type;
	int     len;
	void   *data;
	owop_t  op;
	struct OwatchSecureSendRecord_st *next;
	struct OwatchSecureSendRecord_st *prev;
} OwatchSecureSendRecord_t;

OwatchSecureSendRecord_t *owatch_first_sendrec = NULL;
OwatchSecureSendRecord_t *owatch_last_sendrec = NULL;


/*
 *  initiate connection with hub
 */
owop_t
owatchConnect(const char *hub_url)
{
	char    host[OWATCH_MAX_NAME];
	int     port;
	const char *s;
	int     n, rc;
	socklen_t addr_len;

	owatchDisconnect();
	owatch_conn_op = owatchAllocateOp(owatchCancelConnection,
									OWATCH_OPT_CONNECT, 0);
	if (owatch_conn_op == OWOP_ERROR)
		return OWOP_ERROR;

	/* prepare send queue */
	owatch_q_size = owatch_init_q_size;
	owatch_q_buf = oxnew(OwatchQueuedPacket_t, owatch_q_size);
	owatch_q_len = owatch_q_rd = owatch_q_wr = 0;

	/* prepare poll buffer */
	owatch_poll_size = owatch_init_poll_size;
	owatch_poll_buf = oxnew(OwatchQueuedPacket_t, owatch_poll_size);
	owatch_poll_len = owatch_poll_rd = owatch_poll_wr = 0;

	/* parse URL */
	if (!hub_url || !*hub_url)
		return OWOP_ERROR;
	s = hub_url;
	while (*s && *s == ' ')
		s++;
	if (!strncmp(s, "olang://", 8))
		s += 8;
	for (n = 0; s[n] && s[n] != ':'; n++);
	if (n >= sizeof(host) || n < 1)
		return OWOP_ERROR;
	memcpy(host, s, n);
	host[n] = 0;
	s += n;
	port = 0;
	if (*s == ':' && s[1])
		port = atoi(s + 1);
	if (port < 0 || port > 65534)
		return OWOP_ERROR;
	if (port == 0)
		port = OLANG_HUB_PORT;

	/* create hub address */
	oxvzero(&owatch_hub_data_addr);

#if defined(VXWORKS)
	/* FIXME: autoconf it ! */
	int     he = hostGetByName((char *) host);
	if (he == ERROR)
		he = 0;
	else {
		owatch_hub_data_addr.sin_addr.s_addr = he;
		owatch_hub_data_addr.sin_family = AF_INET;
	}
#else /* !VXWORKS */
	struct hostent *he = gethostbyname(host);
	if (he) {
		oxbcopy(he->h_addr, &owatch_hub_data_addr.sin_addr, he->h_length);
		owatch_hub_data_addr.sin_family = he->h_addrtype;
	}
#endif /* VXWORKS */

	if (!he) {
		printf("unknown host %s\n", host);
		goto FAIL;
	}

	owatchLog(8, "got host address for [%s]: %08x",
				host, (int) owatch_hub_data_addr.sin_addr.s_addr);
	owatch_hub_data_addr.sin_port = htons(port + 1);

	/* create UDP socket */
	owatch_data_sock = 0;
	owatch_ping_cur_ms = OLANG_PING_MIN_MS;
	owatch_cntl_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (owatch_cntl_sock == ERROR)
		goto FAIL;
	oxvzero(&owatch_watch_cntl_addr);
	owatch_watch_cntl_addr.sin_port = 0;
	owatch_watch_cntl_addr.sin_family = AF_INET;
	owatch_watch_cntl_addr.sin_addr.s_addr = INADDR_ANY;
	addr_len = sizeof(owatch_watch_cntl_addr);
	rc = bind(owatch_cntl_sock, (struct sockaddr *) &owatch_watch_cntl_addr,
			  addr_len);
	if (rc == ERROR)
		goto FAIL;
	rc = getsockname(owatch_cntl_sock, (struct sockaddr *) &owatch_watch_cntl_addr,
					 &addr_len);
	if (rc == ERROR)
		goto FAIL;
	owatchLog(7, "my udp port %hu", ntohs(owatch_watch_cntl_addr.sin_port));

#if defined(VXWORKS)
	/* FIXME: autoconf it ! */
	int     on = 1;
	rc = ioctl(owatch_cntl_sock, FIONBIO, (int) &on);
	if (rc == ERROR)
		perror("CNTL/FIONBIO");
#else /* !VXWORKS */
	rc = fcntl(owatch_cntl_sock, F_GETFL, 0);
	if (rc == ERROR)
		perror("CNTL/O_NONBLOCK/GET");
	else {
		rc = fcntl(owatch_cntl_sock, F_SETFL, rc | O_NONBLOCK);
		if (rc == ERROR)
			perror("CNTL/O_NONBLOCK/SET");
	}
#endif /* VXWORKS */

	/* initiate asynchronous connection */
	oxvcopy(&owatch_hub_data_addr, &owatch_hub_cntl_addr);
	owatch_hub_cntl_addr.sin_port = htons(port);
	owatch_conn_state = OWATCH_CONN_WAIT;
	owatch_watcher_id = owatch_temp_id = 0;
	/* wait for connection */
	return owatchInternalWaitOp(owatch_conn_op);
  FAIL:
	owatchDisconnect();
	return OWOP_ERROR;
}


/*
 *  close data connection with hub
 */
static void
owatchDisconnectData(void)
{
	int     i;
	OwatchQueuedPacket_t *pkt;

	/* flush caches etc... */
	owatchFlushInfoCache();
	owatchCancelAllTriggers();

	/* clear poll buffer */
	if (NULL != owatch_poll_buf) {
		for (i = 0; i < owatch_poll_size; i++) {
			pkt = &owatch_poll_buf[i];
			oxfree(pkt->buf);
			pkt->buf = NULL;
			pkt->len = pkt->off = pkt->type = 0;
		}
	}
	owatch_poll_rd = owatch_poll_wr = owatch_poll_len = 0;

	/* clear send queue */
	if (NULL != owatch_q_buf) {
		for (i = 0; i < owatch_q_size; i++) {
			pkt = &owatch_q_buf[i];
			oxfree(pkt->buf);
			pkt->buf = NULL;
			pkt->len = pkt->off = pkt->type = 0;
		}
	}
	owatch_q_rd = owatch_q_wr = owatch_q_len = 0;

	/* clear packet to be reget */
	owatch_hdr_off = 0;
	oxfree(owatch_reget.buf);
	owatch_reget.buf = NULL;
	owatch_reget.len = owatch_reget.off = owatch_reget.type = -1;

	/* clear packet to be reput */
	oxfree(owatch_reput.buf);
	owatch_reput.buf = NULL;
	owatch_reput.len = owatch_reput.len = owatch_reput.type = -1;

	/* close data socket */
	if (owatch_data_sock > 0)		/* FIXME: can be 0 ! */
		close(owatch_data_sock);	/* FIXME: consider shutdown */
	owatch_data_sock = 0;
	owatchSetSubjectState("*", "", '\0');

	/* clear ID */
	owatch_watcher_id = owatch_temp_id = 0;
}


/*
 *  connection aborter
 */
static int
owatchCancelConnection(owop_t op, long data1, long data2)
{
	owatchDisconnectData();
	owatch_conn_op = OWOP_ERROR;
	return 0;
}


/*
 *  close connection with hub (API)
 */
oret_t
owatchDisconnect()
{
	oret_t rc;

	/* cancel all pending operations */
	owatchCancelOp(OWOP_ALL);

	/* close sockets */
	owatchDisconnectData();
	if (owatch_cntl_sock > 0)
		close(owatch_cntl_sock);
	owatch_cntl_sock = 0;

	/* FIXME: is this a double freeing ? */

	/* free the poll buffer */
	oxfree(owatch_poll_buf);
	owatch_poll_buf = NULL;
	owatch_poll_len = owatch_poll_size = 0;
	owatch_poll_rd = owatch_poll_wr = 0;

	/* free the queue */
	oxfree(owatch_q_buf);
	owatch_q_buf = NULL;
	owatch_q_len = owatch_q_size = 0;
	owatch_q_rd = owatch_q_wr = 0;

	/* clear variables */
	oxvzero(&owatch_hub_data_addr);
	oxvzero(&owatch_hub_cntl_addr);
	owatch_ping_cur_ms = 0;
	owatch_ping_wd = owatch_alive_get_wd = owatch_alive_put_wd = 0;
	owatch_conn_state = OWATCH_CONN_NONE;
	owatch_is_polling = FALSE;
	owatch_poll_depth = 0;
	owatch_routine_ms = 0;

	/* return code */
	switch (owatch_conn_state) {
	case OWATCH_CONN_WAIT:
	case OWATCH_CONN_OPEN:
	case OWATCH_CONN_EXIT:
		rc = OK;
		break;
	default:
		rc = ERROR;
		break;
	}
	owatch_conn_state = OWATCH_CONN_NONE;
	return rc;
}


/*
 *  return array of socket descriptors
 */
oret_t
owatchGetFileHandles(int *file_handle_buf, int *buf_size)
{
	if (!file_handle_buf || !buf_size)
		return ERROR;
	if (*buf_size < 2) {
		*buf_size = 2;
		return ERROR;
	}
	file_handle_buf[0] = owatch_data_sock;
	file_handle_buf[1] = owatch_cntl_sock;
	*buf_size = 2;
	return OK;
}


/*
 * check sum
 */
static ushort_t
owatchCheckSum(char *buf, int nbytes)
{
#if OLANG_USE_CHECKSUMS
	ushort_t *wptr = (ushort_t *) buf;
	int     cksum_woff = nbytes >> 1;
	ushort_t cksum = 0;
	int     k;

	if ((nbytes & 1) != 0)
		buf[nbytes] = 0;
	for (k = 0; k < cksum_woff; k++)
		cksum += wptr[k];
	return cksum;
#else /* !OLANG_USE_CHECKSUMS */
	return 0;
#endif /* OLANG_USE_CHECKSUMS */
}


/*
 *  send data over network
 */
static oret_t
owatchPokeStream(int type, int len, void *dp)
{
	char   *buf;
	int     n, blen;
	ushort_t *wptr;
	ushort_t cksum;

	if (owatch_reput.len >= 0) {
		/* retry */
		blen = owatch_reput.len - owatch_reput.off;
		if (blen > 0)
			n = send(owatch_data_sock, owatch_reput.buf + owatch_reput.off,
					 blen, OWATCH_IO_FLAGS);
		else
			n = blen = 0;
		owatch_reput.off += n;
		if (n < blen)
			return ERROR;
		owatchLog(9, "reput TCP type=%04xh len=%d %08lxh:%hu OK",
					type, len, ONTOHL(owatch_hub_data_addr.sin_addr.s_addr),
					ONTOHS(owatch_hub_data_addr.sin_port));
		oxfree(owatch_reput.buf);
		owatch_reput.buf = NULL;
		owatch_reput.off = owatch_reput.len = owatch_reput.type = -1;
		owatch_alive_put_wd = osMsClock();
	}
	if (len < 0)
		return OK;
	if (len > 0 && NULL == dp)
		return ERROR;
	blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
	if ((blen & 1) != 0)
		blen++;

	if (NULL == (buf = oxnew(char, blen)))
		return ERROR;
	wptr = (ushort_t *) buf;
	wptr[0] = OHTONS(len);
	wptr[1] = OHTONS(type);
	wptr[2] = OHTONS(owatch_watcher_id);
	oxbcopy(dp, buf + OLANG_HEAD_LEN, len);
	cksum = owatchCheckSum(buf, len + OLANG_HEAD_LEN);
	*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
	if (blen > 0)
		n = send(owatch_data_sock, (void *) buf, blen, OWATCH_IO_FLAGS);
	else
		n = blen = 0;
	if (n == blen) {
		oxfree(buf);
		owatch_alive_put_wd = osMsClock();
		owatchLog(9, "sent TCP type=%04xh len=%d id=%04xh %08lxh:%hu OK",
					type, len, owatch_watcher_id,
					ONTOHL(owatch_hub_data_addr.sin_addr.s_addr),
					ONTOHS(owatch_hub_data_addr.sin_port));
		return OK;
	}

	/* send error */
	if (errno == EPIPE) {
		owatchLog(4, "broken stream - broken pipe len=%d got=%d", blen, n);
		owatch_conn_state = OWATCH_CONN_EXIT;
		oxfree(buf);
		return ERROR;
	}
	if (n > 0 && n < OLANG_HEAD_LEN) {
		owatchLog(4, "broken stream - pipe overflow len=%d got=%d", blen, n);
		owatch_conn_state = OWATCH_CONN_EXIT;
		oxfree(buf);
		return ERROR;
	}

	if (n <= 0) {
		oxfree(buf);
		return ERROR;
	}

	/* retry later */
	owatch_reput.buf = buf;
	owatch_reput.off = n;
	owatch_reput.len = blen;
	owatch_reput.type = type;

	return OK;
}


/*
 *  retry sending queued packets
 */
static void
owatchResendStream(void)
{
	OwatchQueuedPacket_t *pkt;

	for (;;) {
		if (owatch_reput.len >= 0) {
			if (owatchPokeStream(-1, -1, NULL) != OK)
				break;
		}
		if (0 == owatch_q_len)
			break;
		pkt = &owatch_q_buf[owatch_q_rd];
		if (owatchPokeStream(pkt->type, pkt->len, pkt->buf) != OK) {
			owatchLog(9, "unqueuing ERR TCP type=%04xh len=%d qrd=%d qlen=%d",
						pkt->type, pkt->len, owatch_q_rd, owatch_q_len);
			break;
		}
		if (++owatch_q_rd == owatch_q_size)
			owatch_q_rd = 0;
		owatch_q_len--;
		owatchLog(9, "unqueuing OK TCP type=%04xh len=%d qrd=%d qlen=%d",
					pkt->type, pkt->len, owatch_q_rd, owatch_q_len);
		pkt->type = pkt->len = pkt->off = 0;
		oxfree(pkt->buf);
		pkt->buf = NULL;
		if (owatch_q_len == 0 || owatch_reput.len >= 0)
			break;
	}
}


/*
 *  immediately or later send packet to hub
 */
oret_t
owatchSendPacket(int kind, int type, int len, void *data)
{
	OwatchQueuedPacket_t *pkt;
	ushort_t *wptr, cksum;
	char   *buf, small[16];
	int     blen, num;

	if (len < 0 || (len > 0 && NULL == data))
		return ERROR;

	switch (kind) {
	case OLANG_CONTROL:
	case OLANG_COMMAND:
		/* send a datagram */
		blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
		if (blen & 1)
			blen++;
		if (blen < sizeof(small))
			buf = small;
		else {
			if (NULL == (buf = oxnew(char, blen))) {
				return ERROR;
			}
		}
		wptr = (ushort_t *) buf;
		wptr[0] = OHTONS(len);
		wptr[1] = OHTONS(type);
		wptr[2] = OHTONS(owatch_watcher_id);
		oxbcopy(data, buf + OLANG_HEAD_LEN, len);
		cksum = owatchCheckSum(buf, len + OLANG_HEAD_LEN);
		*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
		num = sendto(owatch_cntl_sock, (void *) buf, blen, 0,
					 (struct sockaddr *) &owatch_hub_cntl_addr,
					 sizeof(owatch_hub_cntl_addr));
		if (buf != small)
			oxfree(buf);
		if (num == blen) {
			/*owatch_alive_put_wd = osMsClock(); */
			owatchLog(9, "send UDP type=%04xh len=%d id=%04xh %08lxh:%hu OK",
						type, len, owatch_watcher_id,
						ONTOHL(owatch_hub_cntl_addr.sin_addr.s_addr),
						ONTOHS(owatch_hub_cntl_addr.sin_port));
			return OK;
		}
		owatchLog(5, "cannot UDP type=%04xh len=%d id=%04xh %08lxh:%hu "
					"blen=%d num=%d errno=%d",
					type, len, owatch_watcher_id,
					ONTOHL(owatch_hub_cntl_addr.sin_addr.s_addr),
					ONTOHS(owatch_hub_cntl_addr.sin_port), blen, num, errno);
		return ERROR;
	case OLANG_DATA:
		break;
	default:
		return ERROR;
	}

	/* send to stream */
	if (0 == owatch_q_len && owatch_reput.len < 0
		&& owatch_can_immed
		&& (len < OLANG_SMALL_PACKET || owatch_can_immed > 1)) {
		if (owatchPokeStream(type, len, data) == OK) {
			owatch_alive_put_wd = osMsClock();
			return OK;
		}
	}

	if (owatch_q_len == owatch_q_size) {
		owatchLog(3,
				"packet type=%04xh len=%d queue overflow q_len=%d q_size=%d",
				type, len, owatch_q_len, owatch_q_size);
		return ERROR;
	}

	pkt = &owatch_q_buf[owatch_q_wr];
	if (NULL != pkt->buf) {
		owatchLog(2, "hanging pkt type=%04x off=%d len=%d qwr=%d qlen=%d",
					pkt->type, pkt->off, pkt->len, owatch_q_wr, owatch_q_len);
		oxfree(pkt->buf);
		pkt->buf = NULL;
	}
	if (len == 0) {
		pkt->buf = NULL;
	} else {
		if (NULL == (pkt->buf = oxnew(char, len))) {
			owatchLog(3, "cannot allocate %d bytes for TCP send", len);
			return ERROR;
		}
		oxbcopy(data, pkt->buf, len);
	}

	pkt->off = 0;
	pkt->len = len;
	pkt->type = type;

	if (++owatch_q_wr == owatch_q_size)
		owatch_q_wr = 0;
	++owatch_q_len;
	owatchLog(9, "queued TCP type=%04xh len=%d qwr=%d qlen=%d",
				type, len, owatch_q_wr, owatch_q_len);

	return OK;
}


/*
 *  receive stream from TCP socket
 */
static void
owatchReceiveStream(void)
{
	int     num, len;
	ushort_t cksum, cksum2, *hbuf, csbuf[8];
	bool_t  first_pass = TRUE;

	/* get packet from TCP */
	for (;;) {
		/* if there was partial packet */
		if (owatch_reget.len >= 0) {
			len = owatch_reget.len + OLANG_TAIL_LEN - owatch_reget.off;
			if (len & 1)
				len++;
			owatchLog(9, "trying to reget off=%d len=%d",
						owatch_reget.off, owatch_reget.len);
			num = recv(owatch_data_sock, owatch_reget.buf + owatch_reget.off,
					   len, OWATCH_IO_FLAGS);
			if (num <= 0) {
				owatchLog(4, "broken stream - null reget len=%d got=%d",
							len, num);
				owatch_conn_state = OWATCH_CONN_EXIT;
				break;
			}
			first_pass = FALSE;
			if (num != len) {
				owatch_reget.off += num;
				break;
			}
			cksum = *(ushort_t *) (owatch_reget.buf + len - OLANG_TAIL_LEN);
			cksum = ONTOHS(cksum);
		} else {
			/* check for new packet */
			owatchLog(9, "peek for header at stream");
			len = OLANG_HEAD_LEN - owatch_hdr_off;
			num = recv(owatch_data_sock,
					   (owatch_hdr_buf + owatch_hdr_off), len, OWATCH_IO_FLAGS);
			if (num > 0)
				owatch_hdr_off += num;
			owatchLog(9, "peek at stream len=%d num=%d", len, num);
			if (num != len) {
				if (first_pass) {
					owatchLog(4,
							"broken stream - null peek off=%d len=%d num=%d",
							owatch_hdr_off, len, num);
					if (num <= 0) {
						owatch_conn_state = OWATCH_CONN_EXIT;
						owatch_hdr_off = 0;
					}
				}
				break;
			}
			owatch_hdr_off = 0;
			hbuf = (ushort_t *) owatch_hdr_buf;
			first_pass = FALSE;
			owatch_reget.len = (ushort_t) ONTOHS(hbuf[0]);
			owatch_reget.off = 0;
			owatch_reget.type = (ushort_t) ONTOHS(hbuf[1]);
			owatch_temp_id = (ushort_t) ONTOHS(hbuf[2]);
			if (owatch_reget.len > OLANG_MAX_PACKET) {
				owatchLog(4, "broken stream - huge length %d", owatch_reget.len);
				owatch_conn_state = OWATCH_CONN_EXIT;
				break;
			}
			if (!owatch_reget.len) {
				/* get checksum */
				num = 0;
				owatch_reget.buf = NULL;
				num = recv(owatch_data_sock, (void *) csbuf, OLANG_TAIL_LEN,
						   MSG_PEEK | OWATCH_IO_FLAGS);
				if (num != OLANG_TAIL_LEN)
					break;
				num = recv(owatch_data_sock, (void *) csbuf, OLANG_TAIL_LEN,
							OWATCH_IO_FLAGS);
				cksum = csbuf[0];
				cksum = ONTOHS(cksum);
			} else {
				/* get data and checksum */
				len = owatch_reget.len + OLANG_TAIL_LEN - owatch_reget.off;
				if (len & 1)
					len++;

				if (NULL == (owatch_reget.buf = oxnew(char, len))) {
					/* not enough memory */
					owatchLog(3, "cannot allocate length %d", len);
					break;
				}
				num = recv(owatch_data_sock, owatch_reget.buf, len,
							OWATCH_IO_FLAGS);
				if (num <= 0) {
					owatch_conn_state = OWATCH_CONN_EXIT;
					owatchLog(4, "broken stream - length=%d got=%d", len, num);
					break;
				}
				if (num != len) {
					owatch_reget.off = len;
					break;
				}
				cksum = *(ushort_t *) (owatch_reget.buf + len - OLANG_TAIL_LEN);
				cksum = ONTOHS(cksum);
			}
		}
		/* if we are here then the packet is ready */
		cksum2 =
			owatchCheckSum(owatch_reget.buf, owatch_reget.len + OLANG_HEAD_LEN);
		if (cksum2 != cksum) {
			owatch_conn_state = OWATCH_CONN_EXIT;
			break;
		}
		owatch_alive_get_wd = osMsClock();
		owatchHubHandleIncoming(OLANG_DATA,
							   owatch_reget.type, owatch_reget.len,
							   owatch_reget.buf);
		oxfree(owatch_reget.buf);
		owatch_reget.buf = NULL;
		owatch_reget.type = owatch_reget.len = owatch_reget.off = -1;
		/* go on to the next packet */
	}
	if (owatch_conn_state == OWATCH_CONN_EXIT) {
		oxfree(owatch_reget.buf);
		owatch_reget.buf = NULL;
		owatch_reget.type = owatch_reget.len = owatch_reget.off = -1;
	}
}


/*
 *  send ping to check hub availability
 */
static void
owatchPingHub(void)
{
	int     interval = owatch_ping_cur_ms;
	long    now = osMsClock();
	long    delta = now - owatch_ping_wd;
	int     len;
	ushort_t data[2];
	oret_t rc;

	if (delta >= 0 && delta < interval && owatch_ping_wd)
		return;
	owatch_ping_wd = now;
	interval =
		interval * OLANG_PING_COEF2 / OLANG_PING_COEF1 + OLANG_PING_COEF0;
	if (interval > OLANG_PING_MAX_MS)
		interval = OLANG_PING_MAX_MS;
	owatch_ping_cur_ms = interval;
	len = 4;					/* 4 bytes of data inside */
	data[0] = OHTONS(OLANG_VERSION);
	data[1] = 0;				/* reserved */
	rc = owatchSendPacket(OLANG_CONTROL, OLANG_PING_REQ, sizeof(data), data);
	owatchLog(8, "send ping rc=%d interval=%d", rc, interval);
}


/*
 * receive a datagram
 */
static void
owatchReceiveDatagram(void)
{
	int     n, len, type, blen;
	ushort_t hdr[10], cksum;
	char   *buf;
	struct sockaddr_in hub_cntl_addr;
	socklen_t addr_len = sizeof(hub_cntl_addr);

	n = recvfrom(owatch_cntl_sock, (void *) hdr, OLANG_HEAD_LEN, MSG_PEEK,
				 (struct sockaddr *) &hub_cntl_addr, &addr_len);
	if (n != OLANG_HEAD_LEN) {
		/* header error */
		goto REMOVE;
	}
	len = (ushort_t) ONTOHS(hdr[0]);
	type = (ushort_t) ONTOHS(hdr[1]);
	owatch_temp_id = (ushort_t) ONTOHS(hdr[2]);
	if (len > OLANG_MAX_PACKET) {
		/* datagram too big */
		goto REMOVE;
	}
	blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
	if (blen & 1)
		blen++;
	if (NULL == (buf = oxnew(char, blen))) {
		/* not enough memory */
		goto REMOVE;
	}
	n = recvfrom(owatch_cntl_sock, buf, blen, 0, NULL, NULL);
	if (n != blen) {
		/* data receive error */
		goto END;
	}
	cksum = *(ushort_t *) (buf + blen - OLANG_TAIL_LEN);
	cksum = ONTOHS(cksum);
	if (cksum != owatchCheckSum(buf, blen - OLANG_TAIL_LEN)) {
		/* checksum fail */
		goto END;
	}
	owatch_alive_get_wd = osMsClock();
	owatchHubHandleIncoming(OLANG_CONTROL, type, len, buf + OLANG_HEAD_LEN);
  END:
	oxfree(buf);
	return;
  REMOVE:
	/* remove datagram from system buffer */
	recvfrom(owatch_cntl_sock, (char *) hdr, OLANG_HEAD_LEN, 0, NULL, NULL);
	return;
}


/*
 * handle ping reply
 */
oret_t
owatchHandlePingReply(int kind, int type, int len, char *data)
{
	int     rc, n, on;
	ushort_t *wptr = (ushort_t *) data;
	char    buf[OWATCH_MAX_PATH + 12];

	/* check packet validity */
	if (type != OLANG_PING_ACK || len != 4 || kind != OLANG_CONTROL
		|| wptr[0] != OHTONS(OLANG_VERSION)) {
		/* packet is wrong */
		return ERROR;
	}
	owatch_ping_cur_ms = OLANG_PING_MIN_MS;
	/* open TCP connection */
	if (owatch_conn_state == OWATCH_CONN_OPEN)
		return OK;
	owatch_data_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (owatch_data_sock == ERROR) {
		owatch_data_sock = 0;
		return ERROR;
	}
	if (connect(owatch_data_sock,
				(struct sockaddr *) &owatch_hub_data_addr,
				sizeof(owatch_hub_data_addr)) == ERROR) {
		goto FAIL;
	}

	/* set socket options */

#if defined(VXWORKS)
	/* FIXME: autoconf it ! */
	on = 1;
	rc = ioctl(owatch_data_sock, FIONBIO, (int) &on);
	if (rc == ERROR)
		perror("DATA/FIONBIO");
#else /* !VXWORKS */
	rc = fcntl(owatch_data_sock, F_GETFL, 0);
	if (rc == ERROR)
		perror("DATA/O_NONBLOCK/GET");
	else {
		rc = fcntl(owatch_data_sock, F_SETFL, rc | O_NONBLOCK);
		if (rc == ERROR)
			perror("DATA/O_NONBLOCK");
	}
#endif /* VXWORKS */

	on = 1;
	setsockopt(owatch_data_sock, IPPROTO_TCP, TCP_NODELAY, (void *) &on,
			   sizeof(on));

	/* send connection packet */
	*(ushort_t *) (buf + 0) = OHTONS(OLANG_VERSION);
	*(ushort_t *) (buf + 2) = owatch_watch_cntl_addr.sin_port;
	*(ulong_t *) (buf + 4) = OLANG_MAGIC;
	n = strlen(owatch_watcher_desc);
	buf[8] = (char) (uchar_t) n;
	strcpy(buf + 9, owatch_watcher_desc);
	owatch_conn_state = OWATCH_CONN_OPEN;
	rc = owatchSendPacket(OLANG_DATA, OLANG_CONN_REQ, 9 + n, buf);
	if (rc != OK)
		goto FAIL;
	owatch_alive_get_wd = owatch_alive_put_wd = osMsClock();
	return OK;
  FAIL:
	owatch_conn_state = OWATCH_CONN_WAIT;
	if (owatch_data_sock > 0)
		close(owatch_data_sock);
	owatch_data_sock = 0;
	return ERROR;
}


/*
 * check hub availability; send alive messages
 */
static void
owatchBackgroundTask(void)
{
	long    now = osMsClock();

	if (owatch_conn_state == OWATCH_CONN_OPEN) {
		/* check hub availability */
		if (now - owatch_alive_get_wd > OLANG_DEAD_MS) {
			owatchLog(4, "hub is dead");
			owatch_conn_state = OWATCH_CONN_EXIT;
		}
	}
	if (owatch_conn_state == OWATCH_CONN_OPEN) {
		if (now - owatch_alive_put_wd > OLANG_ALIVE_MS) {
			/* free up hanging packets */
			int     i, qty;

			for (qty = i = 0; i < owatch_q_size; i++) {
				OwatchQueuedPacket_t *pkt = &owatch_q_buf[i];

				if (pkt->len <= 0 && NULL != pkt->buf) {
					oxfree(pkt->buf);
					pkt->buf = NULL;
					qty++;
				}
			}
			if (qty > 0) {
				owatchLog(6, "freed up %d hanging packets", qty);
			}
			/* send alive messages */
			owatchLog(8, "send heartbeat");
			owatchSendPacket(OLANG_CONTROL, OLANG_ALIVE, 0, NULL);
			owatch_alive_put_wd = now;
		}
	}
}

static void
owatchIdleTask(void)
{
	owatchLazyDataHandlers();
	owatchHandleTriggerTimeouts();
	owatchMonSecureMore();
}


/*
 *  main poll.
 *  returns number of handled events or ERROR for failures.
 */
int
owatchInternalPoll(long timeout)
{
	fd_set  set[2];
	struct timeval to, *pto;
	int     n_events;
	int     nfds, num;
	long    cur, delta;
	OwatchQueuedPacket_t *pkt;

	if (owatch_is_polling) {
		owatchLog(3, "already polling");
		return ERROR;
	}

	owatch_is_polling = TRUE;
	++owatch_poll_depth;
	owatchLog(14, "owatch next poll depth=%d timeout=%ld",
				owatch_poll_depth, timeout);

	if (owatch_conn_state == OWATCH_CONN_NONE) {
		owatch_is_polling = FALSE;
		--owatch_poll_depth;
		owatchLog(2, "still did not try to connect");
		return ERROR;
	}

	/* unconditional periodic tasks */
	owatchIdleTask();

	/* activate conditional periodic tasks */
	delta = (cur = osMsClock()) - owatch_routine_ms;
	if (delta < 0 || delta > OLANG_STAT_MS
		|| delta > OLANG_ALIVE_MS || !owatch_routine_ms) {
		owatch_routine_ms = cur;
		owatchBackgroundTask();
	}

	/* calculate timeout */
	if (timeout < 0)
		pto = NULL;
	else {
		to.tv_sec = timeout / 1000;
		to.tv_usec = (timeout % 1000) * 1000;
		pto = &to;
	}

	if (owatch_conn_state == OWATCH_CONN_WAIT) {
		owatchPingHub();
	}

	/* prepare descriptors */
	FD_ZERO(set + 0);
	FD_ZERO(set + 1);
	nfds = 0;

	FD_SET(owatch_cntl_sock, set + 0);
	nfds = OMAX(nfds, owatch_cntl_sock);

	if (owatch_conn_state == OWATCH_CONN_OPEN) {
		FD_SET(owatch_data_sock, set + 0);
		nfds = OMAX(nfds, owatch_data_sock);
		if (owatch_q_len || owatch_reput.len >= 0) {
			FD_SET(owatch_data_sock, set + 1);
		}
	}

	/* poll descriptors */
	errno = 0;
	num = select(nfds + 1, set + 0, set + 1, NULL, pto);
	if (num < 0 && errno == EINTR) {
		owatchLog(4, "interrupted select");
		num = 0;
	}
	if (num < 0) {
		owatch_is_polling = FALSE;
		--owatch_poll_depth;
		perror("select");
		owatchLog(2, "bad select timeout=%ld nfds=%d num=%d errno=%d",
					timeout, nfds, num, errno);
		return ERROR;
	}

	n_events = 0;
	if (num > 0) {
		/* handle events */
		if (FD_ISSET(owatch_cntl_sock, set + 0)) {
			owatchLog(10, "got some datagram");
			owatchReceiveDatagram();
			n_events++;
		}

		if (FD_ISSET(owatch_data_sock, set + 0)
				&& owatch_conn_state != OWATCH_CONN_EXIT) {
			owatchLog(10, "got some data stream");
			owatchReceiveStream();
			n_events++;
		}

		if (FD_ISSET(owatch_data_sock, set + 1)
				&& owatch_conn_state != OWATCH_CONN_EXIT) {
			owatchLog(10, "resend some stream");
			owatchResendStream();
			n_events++;
		}
	}

	if (owatch_conn_state == OWATCH_CONN_EXIT) {
		owatchDisconnectData();
		owatch_conn_state = OWATCH_CONN_WAIT;
		owatchLog(4, "disconnected from hub");
		owatchPingHub();
	}

	owatch_is_polling = FALSE;

	/* now handle buffered packets */

	while (owatch_poll_len > 0
			&& owatch_poll_depth <= OWATCH_TOLERABLE_POLL_DEPTH) {
		pkt = &owatch_poll_buf[owatch_poll_rd];
		owatchLog(9, "dequeued poll DATA type=%04xh len=%d p_r=%d p_l=%d",
					pkt->type, pkt->len, owatch_poll_rd, owatch_poll_len);
		owatchHandleIncoming(OLANG_DATA, pkt->type, pkt->len, pkt->buf);
		pkt->type = pkt->len = pkt->off = 0;
		oxfree(pkt->buf);
		pkt->buf = NULL;
		if (++owatch_poll_rd == owatch_poll_size)
			owatch_poll_rd = 0;
		--owatch_poll_len;
	}

	--owatch_poll_depth;

	return n_events;
}


/*
 *  check whether connection is open
 */
bool_t
owatchIsConnected(void)
{
	return (owatch_conn_state == OWATCH_CONN_OPEN);
}


/*
 *  secure packet sending
 */

static void
owatchUnlinkSendRecord(OwatchSecureSendRecord_t * psr)
{
	if (owatch_first_sendrec == psr)
		owatch_first_sendrec = psr->next;
	if (owatch_last_sendrec == psr)
		owatch_last_sendrec = psr->prev;
	if (psr->next)
		psr->next->prev = psr->prev;
	if (psr->prev)
		psr->prev->next = psr->next;
}


int
owatchCancelSecureSend(owop_t op, long data1, long data2)
{
	OwatchSecureSendRecord_t *psr = (OwatchSecureSendRecord_t *) data2;

	if (data2 == OWOP_NULL || data2 == OWOP_SELF)
		return ERROR;
	if (!psr || op != psr->op)
		return ERROR;
	owatchUnlinkSendRecord(psr);
	oxfree(psr->data);
	psr->data = NULL;
	oxfree(psr);
	return OK;
}


owop_t
owatchSecureSend(int kind, int type, int len, void *data)
{
	oret_t rc;
	owop_t  op = OWOP_ERROR;
	OwatchSecureSendRecord_t *psr = NULL;

	if (owatch_conn_state == OWATCH_CONN_OPEN
		/* FIXME: we set CONN_OPEN too early, before the TCP ack & id comes. */
			&& owatch_watcher_id != 0) {
		rc = owatchSendPacket(OLANG_DATA, type, len, data);
		return (rc == OK ? OWOP_OK : OWOP_ERROR);
	}
	if (NULL == (psr = oxnew(OwatchSecureSendRecord_t, 1)))
		goto FAIL;
	psr->kind = kind;
	psr->type = type;
	psr->len = len;
	psr->data = NULL;
	psr->op = OWOP_NULL;
	psr->next = psr->prev = NULL;
	if (len != 0) {
		if (NULL == (psr->data = oxnew(char, len)))
			goto FAIL;
		memcpy(psr->data, data, len);
	}
	op = owatchAllocateOp(owatchCancelSecureSend, OWATCH_OPT_SEND, (long) psr->op);
	if (op == OWOP_ERROR)
		goto FAIL;
	psr->op = op;
	if (NULL == owatch_first_sendrec)
		owatch_first_sendrec = psr;
	if (NULL != (psr->prev = owatch_last_sendrec))
		owatch_last_sendrec->next = psr;
	owatch_last_sendrec = psr;
	return op;
  FAIL:
	if (NULL != psr) {
		oxfree(psr->data);
		oxfree(psr);
	}
	if (op != OWOP_ERROR)
		owatchDeallocateOp(op);
	return OWOP_ERROR;
}


oret_t
owatchHandleConnAck(int kind, int type, int len, char *buf)
{
	int     qty;
	oret_t rc;
	OwatchSecureSendRecord_t *psr;

	owatchLog(7, "got connection acknowledge with id %04xh", owatch_temp_id);
	owatch_watcher_id = owatch_temp_id;
	owatchFinalizeOp(owatch_conn_op, OWATCH_ERR_OK);
	owatch_conn_op = OWOP_OK;
	/* FIXME: we should limit number of packets and resend lazily */
	/* FIXME: we are inside poll loop. postpone resend until out of polling */
	qty = 0;
	while (owatch_first_sendrec) {
		psr = owatch_first_sendrec;
		owatchUnlinkSendRecord(psr);
		if (!owatchIsValidOp(psr->op)) {
			/* already cancelled */
			continue;
		}
		rc = owatchSendPacket(psr->kind, psr->type, psr->len, psr->data);
		owatchLog(8,
				"secure resend rc=%d kind=%d type=%04xh len=%d data=%08lxh",
				rc, kind, type, len, psr->data ? *(ulong_t *) psr->data : 0);
		if (rc == OK) {
			owatchFinalizeOp(psr->op, OWATCH_ERR_OK);
			qty++;
		} else {
			owatchFinalizeOp(psr->op, OWATCH_ERR_NETWORK);
		}
	}
	owatchLog(7, "%d secure resends OK", qty);
	owatchMonSecureStart();
	owatchMsgClassRegistration(TRUE);
	owatchSetSubjectState("*", "*", '\0');
	return OK;
}


/*
 * handle incoming packets
 */
oret_t
owatchHubHandleIncoming(int kind, int type, int len, char *buf)
{
	OwatchQueuedPacket_t *pkt;

	switch (type) {
	case OLANG_PING_ACK:
		owatchLog(8, "got ping acknowledge with id %04xh",
			  owatch_temp_id);
		owatchHandlePingReply(kind, type, len, buf);
		return OK;
	case OLANG_CONN_ACK:
		owatchHandleConnAck(kind, type, len, buf);
		return OK;
	case OLANG_CONN_NAK:
		owatchLog(7, "got connection refusal");
		owatchDisconnectData();
		owatchFinalizeOp(owatch_conn_op, OWATCH_ERR_REFUSED);
		owatch_conn_op = OWOP_ERROR;
		return OK;
	case OLANG_ALIVE:
		owatchLog(8, "got heartbeat");
		return OK;
	case OLANG_SUBJECTS:
		return owatchHandleIncoming(kind, type, len, buf);
	default:
		break;
	}

	if (!owatch_is_polling || kind != OLANG_DATA) {
		return owatchHandleIncoming(kind, type, len, buf);
	}

	/* buffer packet to handle as soon as polling is done */
	if (owatch_poll_len == owatch_poll_size) {
		owatchLog(3,
				"packet type=%04xh len=%d polling overflow p_len=%d p_size=%d",
				type, len, owatch_poll_len, owatch_poll_size);
		return ERROR;
	}

	pkt = &owatch_poll_buf[owatch_poll_wr];
	if (NULL != pkt->buf) {
		/* FIXME: this is an overkill */
		owatchLog(2, "hanging poll pkt type=%04x off=%d len=%d plen=%d",
					pkt->type, pkt->off, pkt->len, owatch_poll_len);
		oxfree(pkt->buf);
		pkt->buf = NULL;
	}

	pkt->off = 0;
	pkt->len = len;
	pkt->type = type;
	pkt->buf = NULL;

	if (len > 0) {
		if (NULL == (pkt->buf = oxnew(char, len))) {
			owatchLog(3, "cannot allocate %d bytes for poll", len);
			return ERROR;
		}
		oxbcopy(buf, pkt->buf, len);
	}

	owatchLog(9, "enqueued poll type=%04xh len=%d p_w=%d p_n=%d",
				type, len, owatch_poll_wr, owatch_poll_len);

	if (++owatch_poll_wr == owatch_poll_size)
		owatch_poll_wr = 0;
	++owatch_poll_len;

	return OK;
}

