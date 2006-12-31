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

  Implementation of the protocol with hub.

*/

#include "subj-priv.h"
#include <optikus/conf-net.h>	/* for OHTONS,... */

#include <unistd.h>			/* for close */
#include <stdio.h>			/* for perror */
#include <errno.h>			/* for errno,EPIPE */
#include <string.h>			/* for strlen,... */
#include <fcntl.h>			/* for F_GETFL,F_SETFL,O_NONBLOCK */
#include <sys/socket.h>		/* for recv,... */
#include <netinet/in.h>		/* for sockaddr_in */
#include <netinet/tcp.h>	/* for TCP_NODELAY */
#include <arpa/inet.h>		/* for inet_ntoa */

#if defined(HAVE_CONFIG_H)
#include <config.h>			/* for VXWORKS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS)
#include <sockLib.h>
#include <selectLib.h>
#include <netinet/in.h>
#include <hostLib.h>
#include <netinet/tcp.h>
#include <ioLib.h>
#include <inetLib.h>
#endif /* VXWORKS */

char    osubj_hub_desc[140] = { 0 };

/* TCP/IP connection */

#if defined(HAVE_CONFIG_H)
#include <config.h>			/* for VXWORKS,SOLARIS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS) || defined(SOLARIS)
/* FIXME: autoconf it ! */
#if !defined(_SOCKLEN_T)
typedef int socklen_t;			/* not defined in VxWorks/Solaris6 */
#endif /* !SOCKLEN_T */
#endif /* VXWORKS || SOLARIS */

#if defined(VXWORKS) || defined(SOLARIS)
/* FIXME: autoconf it ! */
#define OSUBJ_IO_FLAGS   0
#else
#define OSUBJ_IO_FLAGS   MSG_NOSIGNAL	/* avoid SIGPIPE */
#endif /* VXWORKS || SOLARIS */

bool_t  osubj_allow_reconnect = TRUE;

int     osubj_serv_sock = 0;
int     osubj_data_sock = 0;
int     osubj_cmnd_sock = 0;
int     osubj_cntl_sock = 0;

struct sockaddr_in osubj_hub_cntl_addr;
struct sockaddr_in osubj_tmp_cntl_addr;

#define OSUBJ_CONN_NONE  0
#define OSUBJ_CONN_OPEN  1
#define OSUBJ_CONN_EXIT  2

int     osubj_conn_state = OSUBJ_CONN_NONE;

int     osubj_polling_mask = 0;

long    osubj_routine_ms = 0;

long    osubj_alive_get_wd = 0;
long    osubj_alive_put_wd = 0;

ushort_t osubj_subj_id = 0;
ushort_t osubj_temp_id = 0;

/* packet sending */

#define OSUBJ_Q_SIZE     100

int     osubj_init_q_size = OSUBJ_Q_SIZE;

int     osubj_resend_limit = 0;
long    osubj_resend_suspend_ms = 0;

typedef struct
{
	int     len;
	int     off;
	int     type;
	char   *buf;
} OsubjQueuedPacket_t;

char    osubj_hdr_buf[16];
int     osubj_hdr_off = 0;

OsubjQueuedPacket_t osubj_reget = { -1, -1, -1, NULL };
OsubjQueuedPacket_t osubj_reput = { -1, -1, -1, NULL };

OsubjQueuedPacket_t *osubj_q_buf = NULL;

int     osubj_q_len = 0;
int     osubj_q_size = 0;
int     osubj_q_rd = 0;
int     osubj_q_wr = 0;

int     osubj_can_immed = 1;	/* 0 = never send packets immediately
								 * 1 = packets less than 800 bytes
								 * 2 = always try immediate send
								 */

/* special answers */
#define OSUBJ_DATA_MON_TTL        50
#define OSUBJ_MAX_DATA_MON_REPLY  100

long   *osubj_data_mon_buf = 0;
long    osubj_data_mon_start = 0;



bool_t
osubjIsConnected()
{
	return (osubj_conn_state == OSUBJ_CONN_OPEN);
}


/*
 *  initialize server sockets
 */
oret_t
osubjInitNetwork(int serv_port)
{
	int     rc;
	struct sockaddr_in serv_addr, cntl_addr, cmnd_addr;
	int     on = 1;

	/* cleanup */
	if (serv_port == 0)
		serv_port = OLANG_SUBJ_PORT;
	osubjExitNetwork();

	/* prepare reply buffers */
	osubj_data_mon_buf = osubjNew(long, OSUBJ_MAX_DATA_MON_REPLY * 2 + 8, 2);

	osubj_data_mon_buf[0] = 0;
	osubj_data_mon_start = 0;

	/* prepare send queue */
	osubj_q_size = osubj_init_q_size;
	osubj_q_buf = osubjNew(OsubjQueuedPacket_t, osubj_q_size, 2);
	osubj_q_len = osubj_q_rd = osubj_q_wr = 0;

	/* create UDP control socket */
	osubj_cntl_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (osubj_cntl_sock == ERROR) {
		perror("cannot open control socket");
		goto FAIL;
	}
	rc = setsockopt(osubj_cntl_sock, SOL_SOCKET, SO_REUSEADDR,
					(char *) &on, sizeof(on));
	if (rc == ERROR)
		perror("CNTL/SO_REUSEADDR");
	osubjRecZero(&cntl_addr);
	cntl_addr.sin_port = htons(serv_port);
	cntl_addr.sin_family = AF_INET;
	cntl_addr.sin_addr.s_addr = INADDR_ANY;
	rc = bind(osubj_cntl_sock, (struct sockaddr *) &cntl_addr,
			  sizeof(cntl_addr));
	if (rc == ERROR) {
		perror("cannot bind control socket");
		goto FAIL;
	}

#if defined(VXWORKS)
	/* FIXME: autoconf it ! */
	rc = ioctl(osubj_cntl_sock, FIONBIO, (int) &on);
	if (rc == ERROR)
		perror("CNTL/FIONBIO");
#else /* !VXWORKS */
	rc = fcntl(osubj_cntl_sock, F_GETFL, 0);
	if (rc == ERROR)
		perror("CNTL/O_NONBLOCK/GET");
	else {
		rc = fcntl(osubj_cntl_sock, F_SETFL, rc | O_NONBLOCK);
		if (rc == ERROR)
			perror("CNTL/O_NONBLOCK/SET");
	}
#endif /* VXWORKS */

	/* create TCP accepting socket */
	osubj_data_sock = 0;
	osubj_serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (osubj_serv_sock == ERROR) {
		perror("cannot open server socket");
		goto FAIL;
	}
	rc = setsockopt(osubj_serv_sock, SOL_SOCKET, SO_REUSEADDR,
					(char *) &on, sizeof(on));
	if (rc == ERROR)
		perror("SERV/SO_REUSEADDR");
	osubjRecZero(&serv_addr);
	serv_addr.sin_port = htons(serv_port + 1);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	rc = bind(osubj_serv_sock, (struct sockaddr *) &serv_addr,
			  sizeof(serv_addr));
	if (rc == ERROR) {
		perror("cannot bind server socket");
		goto FAIL;
	}
	rc = listen(osubj_serv_sock, 1);
	if (rc == ERROR) {
		perror("cannot listen on server socket");
		goto FAIL;
	}

#if defined(VXWORKS)
	/* FIXME: autoconf it ! */
	rc = ioctl(osubj_serv_sock, FIONBIO, (int) &on);
	if (rc == ERROR)
		perror("SERV/FIONBIO");
#else /* !VXWORKS */
	rc = fcntl(osubj_serv_sock, F_GETFL, 0);
	if (rc == ERROR)
		perror("SERV/O_NONBLOCK/GET");
	else {
		rc = fcntl(osubj_serv_sock, F_SETFL, rc | O_NONBLOCK);
		if (rc == ERROR)
			perror("SERV/O_NONBLOCK/SET");
	}
#endif /* VXWORKS */

	/* create UDP command socket */
	osubj_cmnd_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (osubj_cmnd_sock == ERROR) {
		perror("cannot open control socket");
		goto FAIL;
	}
	rc = setsockopt(osubj_cmnd_sock, SOL_SOCKET, SO_REUSEADDR,
					(char *) &on, sizeof(on));
	if (rc == ERROR)
		perror("CNTL/SO_REUSEADDR");
	osubjRecZero(&cmnd_addr);
	cmnd_addr.sin_port = htons(serv_port + 2);
	cmnd_addr.sin_family = AF_INET;
	cmnd_addr.sin_addr.s_addr = INADDR_ANY;
	rc = bind(osubj_cmnd_sock, (struct sockaddr *) &cmnd_addr,
			  sizeof(cmnd_addr));
	if (rc == ERROR) {
		perror("cannot bind control socket");
		goto FAIL;
	}

#if defined(VXWORKS)
	/* FIXME: autoconf it ! */
	rc = ioctl(osubj_cmnd_sock, FIONBIO, (int) &on);
	if (rc == ERROR)
		perror("CNTL/FIONBIO");
#else /* !VXWORKS */
	rc = fcntl(osubj_cmnd_sock, F_GETFL, 0);
	if (rc == ERROR)
		perror("CNTL/O_NONBLOCK/GET");
	else {
		rc = fcntl(osubj_cmnd_sock, F_SETFL, rc | O_NONBLOCK);
		if (rc == ERROR)
			perror("CNTL/O_NONBLOCK/SET");
	}
#endif /* VXWORKS */

	/* setup variables */
	osubj_subj_id = osubj_temp_id = 0;
	osubjLogAt(9,
		"network init ports=%d,%d,%d serv_sock=%d cntl_sock=%d cmnd_sock=%d",
		serv_port + 1, serv_port, serv_port + 2, osubj_serv_sock,
		osubj_cntl_sock, osubj_cmnd_sock);
	return OK;

  FAIL:
	osubjExitNetwork();
	return ERROR;
}


/*
 *  close data connection with hub
 */
static void
osubjDisconnectData(void)
{
	int     i;
	OsubjQueuedPacket_t *pkt;

	/* clear monitoring */
	osubjClearMonitoring();
	if (osubj_data_mon_buf)
		osubj_data_mon_buf[0] = 0;
	osubj_data_mon_start = 0;
	/* clear send queue */
	if (osubj_q_buf) {
		for (i = 0; i < osubj_q_size; i++) {
			pkt = &osubj_q_buf[i];
			osubjFree(pkt->buf, 3);
			pkt->buf = NULL;
			pkt->len = pkt->off = pkt->type = 0;
		}
	}
	osubj_resend_suspend_ms = 0;
	osubj_q_rd = osubj_q_wr = osubj_q_len = 0;
	/* clear packet to be reget */
	osubj_hdr_off = 0;
	osubjFree(osubj_reget.buf, 4);
	osubj_reget.buf = NULL;
	osubj_reget.len = osubj_reget.off = osubj_reget.type = -1;
	/* clear packet to be reput */
	osubjFree(osubj_reput.buf, 5);
	osubj_reput.buf = NULL;
	osubj_reput.len = osubj_reput.len = osubj_reput.type = -1;
	/* close data socket */
	if (osubj_data_sock > 0)
		close(osubj_data_sock);
	osubj_data_sock = 0;
}


/*
 *  close network connection with hub
 */
oret_t
osubjExitNetwork(void)
{
	osubjDisconnectData();
	/* close sockets */
	if (osubj_cntl_sock > 0)
		close(osubj_cntl_sock);
	osubj_cntl_sock = 0;
	if (osubj_serv_sock > 0)
		close(osubj_serv_sock);
	osubj_cntl_sock = 0;
	if (osubj_cmnd_sock > 0)
		close(osubj_cmnd_sock);
	osubj_cmnd_sock = 0;
	/* free the queue */
	osubjFree(osubj_q_buf, 2);
	osubj_q_buf = NULL;
	osubj_q_len = osubj_q_size = 0;
	osubj_q_rd = osubj_q_wr = 0;
	/* free reply buffers */
	osubjFree(osubj_data_mon_buf, 2);
	osubj_data_mon_buf = NULL;
	osubj_data_mon_start = 0;
	/* clear variables */
	osubj_conn_state = OSUBJ_CONN_NONE;
	osubj_polling_mask = 0;
	osubj_routine_ms = 0;
	osubj_subj_id = osubj_temp_id = 0;
	return OK;
}


/*
 *  return array of socket descriptors
 */
oret_t
osubjGetFileHandles(int *file_handle_buf, int *buf_size)
{
	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (!file_handle_buf || !buf_size)
		return ERROR;
	if (*buf_size < 2) {
		*buf_size = 2;
		return ERROR;
	}
	file_handle_buf[0] = osubj_data_sock;
	file_handle_buf[1] = osubj_cntl_sock;
	file_handle_buf[2] = osubj_cmnd_sock;
	*buf_size = 3;
	return OK;
}


/*
 * check sum
 */
static ushort_t
osubjCheckSum(char *buf, int nbytes)
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
osubjPokeStream(int type, int len, void *dp)
{
	char   *buf;
	int     n, blen;
	ushort_t *wptr;
	ushort_t cksum;

	if (osubj_reput.len >= 0) {
		/* retry */
		blen = osubj_reput.len - osubj_reput.off;
		if (blen > 0)
			n = send(osubj_data_sock, osubj_reput.buf + osubj_reput.off,
					 blen, OSUBJ_IO_FLAGS);
		else
			n = blen = 0;
		osubj_reput.off += n;
		if (n < blen) {
			osubjLogAt(10, "poke ERR some type=%04xh len=%d blen=%d n=%d",
						osubj_reput.type, osubj_reput.len, blen, n);
			return ERROR;
		}
		osubjLogAt(9, "reput TCP type=%04xh len=%d OK",
					osubj_reput.type, osubj_reput.len);
		osubjFree(osubj_reput.buf, 5);
		osubj_reput.buf = NULL;
		osubj_reput.off = osubj_reput.len = osubj_reput.type = -1;
		osubj_alive_put_wd = osubjClock();
	}
	if (len < 0)
		return OK;
	if (len > 0 && NULL == dp) {
		osubjLogAt(8, "poke ERR !dp type=%04xh len=%d",
					osubj_reput.type, osubj_reput.len);
		return ERROR;
	}
	blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
	if ((blen & 1) != 0)
		blen++;

	if (NULL == (buf = osubjNew(char, blen, 5))) {
		osubjLogAt(3, "poke ERR nomem type=%04xh len=%d",
					osubj_reput.type, osubj_reput.len);
		return ERROR;
	}
	wptr = (ushort_t *) buf;
	wptr[0] = OHTONS(len);
	wptr[1] = OHTONS(type);
	wptr[2] = OHTONS(osubj_subj_id);
	if (len > 0)
		osubjCopy(dp, buf + OLANG_HEAD_LEN, len);
	cksum = osubjCheckSum(buf, len + OLANG_HEAD_LEN);
	*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
	if (blen > 0)
		n = send(osubj_data_sock, (void *) buf, blen, OSUBJ_IO_FLAGS);
	else
		n = blen = 0;
	osubj_alive_put_wd = osubjClock();
	if (n == blen) {
		osubjFree(buf, 5);
		osubjLogAt(9, "sent TCP type=%04xh len=%d id=%04xh %08lxh:%hu OK",
					type, len, osubj_subj_id,
					ONTOHL(osubj_hub_cntl_addr.sin_addr.s_addr),
					ONTOHS(osubj_hub_cntl_addr.sin_port));
		osubj_alive_put_wd = osubjClock();
		return OK;
	}

	/* send error */
	if (errno == EPIPE) {
		osubjLogAt(4, "broken stream - broken pipe len=%d got=%d", blen, n);
		osubj_conn_state = OSUBJ_CONN_EXIT;
		osubjFree(buf, 5);
		return ERROR;
	}
	if (n > 0 && n < OLANG_HEAD_LEN) {
		/* FIXME: have to handle, it is very possible under high loads ! */
		osubjLogAt(4, "broken stream - pipe overflow len=%d got=%d", blen, n);
		osubj_conn_state = OSUBJ_CONN_EXIT;
		osubjFree(buf, 5);
		return ERROR;
	}
	if (n <= 0) {
		osubjFree(buf, 5);
		osubjLogAt(9, "poke ERR negative type=%04xh len=%d n=%d errno=%d[%s]",
					type, len, n, errno, strerror(errno));
		return ERROR;
	}

	/* retry later */
	osubj_reput.buf = buf;
	osubj_reput.off = n;
	osubj_reput.len = blen;
	osubj_reput.type = type;

	return OK;
}


/*
 *  retry sending queued packets
 */
static void
osubjResendStream(void)
{
	OsubjQueuedPacket_t *pkt;
	int     total = 0;

	for (;;) {
		if (osubj_reput.len >= 0) {
			if (osubjPokeStream(-1, -1, NULL) != OK)
				break;
		}
		if (!osubj_q_len)
			break;
		pkt = &osubj_q_buf[osubj_q_rd];
		if (osubjPokeStream(pkt->type, pkt->len, pkt->buf) != OK) {
			/* FIXME: memory leak around here */
			osubjLogAt(8, "unqueuing ERR TCP type=%04xh len=%d qrd=%d qlen=%d",
						pkt->type, pkt->len, osubj_q_rd, osubj_q_len);
			break;
		}
		total += pkt->len;
		if (++osubj_q_rd == osubj_q_size)
			osubj_q_rd = 0;
		osubj_q_len--;
		osubjLogAt(9, "unqueuing OK TCP type=%04xh len=%d qrd=%d qlen=%d",
					pkt->type, pkt->len, osubj_q_rd, osubj_q_len);
		pkt->type = pkt->len = pkt->off = 0;
		osubjFree(pkt->buf, 3);
		pkt->buf = NULL;
		if (osubj_q_len == 0 || osubj_reput.len >= 0)
			break;
		if (osubj_resend_limit && total > osubj_resend_limit) {
			osubj_resend_suspend_ms = osubjClock() + 10;
			osubjLogAt(8,
				"unqueuing freezed - resend limit %d reached, unfreeze at %lu",
				osubj_resend_limit, osubj_resend_suspend_ms);
			break;
		}
	}
}


/*
 *  immediately or later send packet to hub
 */
oret_t
osubjSendPacket(int id, int kind, int type, int len, void *data)
{
	OsubjQueuedPacket_t *pkt;
	ushort_t *wptr, cksum;
	char   *buf, small[16];
	int     blen, num, sock;
	struct sockaddr_in *cntl_addr_ptr;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (len < 0 || (len > 0 && !data))
		return ERROR;
	switch (kind) {
	case OLANG_CONTROL:
		sock = osubj_cntl_sock;
		break;
	case OLANG_COMMAND:
		sock = osubj_cmnd_sock;
		break;
	case OLANG_DATA:
		sock = 0;
		break;
	default:
		return ERROR;
	}
	if (sock) {
		/* send a datagram */
		switch (id) {
		case OK:
			id = osubj_subj_id;
			cntl_addr_ptr = &osubj_hub_cntl_addr;
			break;
		case ERROR:
			id = osubj_temp_id;
			cntl_addr_ptr = &osubj_tmp_cntl_addr;
			break;
		default:
			return ERROR;
		}
		blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
		if (blen & 1)
			blen++;
		if (blen < sizeof(small))
			buf = small;
		else {
			buf = osubjNew(char, blen, 6);

			if (!buf)
				return ERROR;
		}
		wptr = (ushort_t *) buf;
		wptr[0] = OHTONS(len);
		wptr[1] = OHTONS(type);
		wptr[2] = OHTONS(id);
		if (len > 0)
			osubjCopy(data, buf + OLANG_HEAD_LEN, len);
		cksum = osubjCheckSum(buf, len + OLANG_HEAD_LEN);
		*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
		num = sendto(sock, (void *) buf, blen, 0,
					 (struct sockaddr *) cntl_addr_ptr,
					 sizeof(struct sockaddr_in));
		if (buf != small)
			osubjFree(buf, 6);
		if (num == blen) {
			osubjLogAt(9, "send UDP type=%04xh len=%d id=%04xh %08lxh:%hu OK",
						type, len, id,
						ONTOHL(osubj_hub_cntl_addr.sin_addr.s_addr),
						ONTOHS(osubj_hub_cntl_addr.sin_port));
			osubj_alive_put_wd = osubjClock();
			return OK;
		}
		osubjLogAt(5, "cannot UDP type=%04xh len=%d id=%04xh %08lxh:%hu "
					"blen=%d num=%d errno=%d",
					type, len, id,
					ONTOHL(osubj_hub_cntl_addr.sin_addr.s_addr),
					ONTOHS(osubj_hub_cntl_addr.sin_port), blen, num, errno);
		return ERROR;
	}

	/* send to stream */
	if (osubj_conn_state != OSUBJ_CONN_OPEN) {
		osubjLogAt(1, "cannot send stream: connection not open");
		return ERROR;
	}

	if (!osubj_q_len && osubj_reput.len < 0
		&& osubj_can_immed
		&& (len < OLANG_SMALL_PACKET || osubj_can_immed > 1)) {
		if (osubjPokeStream(type, len, data) == OK) {
			/* FIXME: memory leak around here */
			return OK;
		}
	}

	if (osubj_q_len == osubj_q_size) {
		osubjLogAt(3,
				"packet type=%04xh len=%d queue overflow q_len=%d q_size=%d",
				type, len, osubj_q_len, osubj_q_size);
		return ERROR;
	}

	pkt = &osubj_q_buf[osubj_q_wr];
	if (pkt->buf) {
		osubjLogAt(6, "hanging pkt type=%04x off=%d len=%d qwr=%d qlen=%d",
					pkt->type, pkt->off, pkt->len, osubj_q_wr, osubj_q_len);
		osubjFree(pkt->buf, 3);
		pkt->buf = NULL;
	}
	if (len == 0)
		pkt->buf = NULL;
	else {
		if (NULL == (pkt->buf = osubjNew(char, len, 3))) {
			osubjLogAt(3, "cannot allocate %d bytes for TCP send", len);
			return ERROR;
		}
		osubjCopy(data, pkt->buf, len);
	}

	pkt->off = 0;
	pkt->len = len;
	pkt->type = type;

	if (++osubj_q_wr == osubj_q_size)
		osubj_q_wr = 0;
	++osubj_q_len;
	osubjLogAt(9, "queued TCP type=%04xh len=%d qwr=%d qlen=%d",
				type, len, osubj_q_wr, osubj_q_len);

	return OK;
}


/*
 *  receive stream from TCP socket
 */
static void
osubjReceiveStream(void)
{
	int     num, len;
	ushort_t cksum, csbuf[8], *hbuf;
	bool_t  first_pass = TRUE;

	/* get packet from TCP */
	for (;;) {
		/* if there was partial packet */
		if (osubj_reget.len >= 0) {
			len = osubj_reget.len + OLANG_TAIL_LEN - osubj_reget.off;
			if (len & 1)
				len++;
			osubjLogAt(9, "trying to reget off=%d len=%d",
						osubj_reget.off, osubj_reget.len);
			num = recv(osubj_data_sock, osubj_reget.buf + osubj_reget.off, len,
					   OSUBJ_IO_FLAGS);
			if (num <= 0) {
				osubjLogAt(4, "broken stream - null reget len=%d num=%d",
							len, num);
				osubj_conn_state = OSUBJ_CONN_EXIT;
				break;
			}
			first_pass = FALSE;
			if (num != len) {
				osubj_reget.off += num;
				break;
			}
			cksum = *(ushort_t *) (osubj_reget.buf + len - OLANG_TAIL_LEN);
			cksum = ONTOHS(cksum);
		} else {
			/* check for new packet */
			len = OLANG_HEAD_LEN - osubj_hdr_off;
			num = recv(osubj_data_sock,
					   (osubj_hdr_buf + osubj_hdr_off), len, OSUBJ_IO_FLAGS);
			if (num > 0)
				osubj_hdr_off += num;
			osubjLogAt(9, "peek at stream num=%d first=%d", num, first_pass);
			if (num != len) {
				if (first_pass) {
					osubjLogAt(4,
							"broken stream - null peek off=%d len=%d num=%d",
							osubj_hdr_off, len, num);
					if (num <= 0) {
						osubj_conn_state = OSUBJ_CONN_EXIT;
						osubj_hdr_off = 0;
					}
				}
				break;
			}
			osubj_hdr_off = 0;
			hbuf = (ushort_t *) osubj_hdr_buf;
			first_pass = FALSE;
			osubj_reget.len = (ushort_t) ONTOHS(hbuf[0]);
			osubj_reget.off = 0;
			osubj_reget.type = (ushort_t) ONTOHS(hbuf[1]);
			osubj_temp_id = (ushort_t) ONTOHS(hbuf[2]);
			if (osubj_reget.len > OLANG_MAX_PACKET) {
				osubjLogAt(4, "broken stream - huge length %d",
							osubj_reget.len);
				osubj_conn_state = OSUBJ_CONN_EXIT;
				break;
			}
			if (!osubj_reget.len) {
				/* get checksum */
				num = 0;
				osubj_reget.buf = NULL;
				num = recv(osubj_data_sock, (void *) csbuf, OLANG_TAIL_LEN,
						   MSG_PEEK | OSUBJ_IO_FLAGS);
				if (num != OLANG_TAIL_LEN)
					break;
				num = recv(osubj_data_sock, (void *) csbuf, OLANG_TAIL_LEN,
						   OSUBJ_IO_FLAGS);
				cksum = csbuf[0];
				cksum = ONTOHS(cksum);
			} else {
				/* get data and checksum */
				len = osubj_reget.len + OLANG_TAIL_LEN - osubj_reget.off;
				if (len & 1)
					len++;

				if (NULL == (osubj_reget.buf = osubjNew(char, len, 4))) {
					/* not enough memory */
					osubjLogAt(3, "cannot allocate length %d", len);
					break;
				}
				num = recv(osubj_data_sock, osubj_reget.buf, len, OSUBJ_IO_FLAGS);
				if (num <= 0) {
					osubj_conn_state = OSUBJ_CONN_EXIT;
					osubjLogAt(4, "broken stream - len=%d got=%d", len, num);
					break;
				}
				if (num != len) {
					osubj_reget.off = len;
					break;
				}
				cksum = *(ushort_t *) (osubj_reget.buf + len - OLANG_TAIL_LEN);
				cksum = ONTOHS(cksum);
			}
		}
		/* if we are here then the packet is ready */
		if (osubjCheckSum(osubj_reget.buf, osubj_reget.len + OLANG_HEAD_LEN)
				!= cksum) {
			osubjLogAt(4, "broken stream - wrong checksum");
			osubj_conn_state = OSUBJ_CONN_EXIT;
			break;
		}
		osubj_alive_get_wd = osubjClock();
		osubjHandleIncoming(OLANG_DATA, osubj_reget.type, osubj_reget.len,
							osubj_reget.buf);
		osubjFree(osubj_reget.buf, 4);
		osubj_reget.buf = NULL;
		osubj_reget.type = osubj_reget.len = osubj_reget.off = -1;
		/* go on to the next packet */
	}
	if (osubj_conn_state == OSUBJ_CONN_EXIT) {
		if (osubj_reget.buf)
			osubjFree(osubj_reget.buf, 4);
		osubj_reget.buf = NULL;
		osubj_reget.type = osubj_reget.len = osubj_reget.off = -1;
	}
}


/*
 * receive a datagram
 */
static void
osubjReceiveDatagram(int kind)
{
	int     n, len, type, blen;
	ushort_t hdr[10], cksum;
	char   *buf;
	struct sockaddr_in *cntl_addr_ptr;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	int     sock;

	if (osubj_conn_state == OSUBJ_CONN_OPEN)
		cntl_addr_ptr = &osubj_tmp_cntl_addr;
	else
		cntl_addr_ptr = &osubj_hub_cntl_addr;
	switch (kind) {
	case OLANG_CONTROL:
		sock = osubj_cntl_sock;
		break;
	case OLANG_COMMAND:
		sock = osubj_cmnd_sock;
		break;
	default:
		return;
	}
	n = recvfrom(sock, (void *) hdr, OLANG_HEAD_LEN, MSG_PEEK,
				 (struct sockaddr *) cntl_addr_ptr, &addr_len);
	if (n != OLANG_HEAD_LEN) {
		/* header error */
		goto REMOVE;
	}
	len = (ushort_t) ONTOHS(hdr[0]);
	type = (ushort_t) ONTOHS(hdr[1]);
	osubj_temp_id = (ushort_t) ONTOHS(hdr[2]);
	if (len > OLANG_MAX_PACKET) {
		/* datagram too big */
		goto REMOVE;
	}
	blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
	if (blen & 1)
		blen++;

	if (NULL == (buf = osubjNew(char, blen, 6))) {
		/* not enough memory */
		goto REMOVE;
	}
	n = recvfrom(sock, buf, blen, 0, NULL, NULL);
	if (n != blen) {
		/* data receive error */
		goto END;
	}
	cksum = *(ushort_t *) (buf + blen - OLANG_TAIL_LEN);
	cksum = ONTOHS(cksum);
	if (cksum != osubjCheckSum(buf, blen - OLANG_TAIL_LEN)) {
		/* checksum fail */
		goto END;
	}
	osubj_alive_get_wd = osubjClock();
	osubjHandleIncoming(kind, type, len, buf + OLANG_HEAD_LEN);
  END:
	osubjFree(buf, 6);
	return;
  REMOVE:
	/* remove datagram from system buffer */
	recvfrom(sock, (void *) hdr, OLANG_HEAD_LEN, 0, NULL, NULL);
	return;
}


/*
 *  accept connection from hub
 */
static oret_t
osubjAcceptConnection()
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	int     sock, rc, on;

	sock = accept(osubj_serv_sock, (struct sockaddr *) &addr, &addr_len);
	if (sock == ERROR)
		return ERROR;
	if (osubj_conn_state == OSUBJ_CONN_OPEN) {
		osubjLogAt(4, "cannot accept more than one data connection");
		close(sock);
		return ERROR;
	}
	/* make socket non-blocking */
#if defined(VXWORKS)
	/* FIXME: autoconf it ! */
	on = 1;
	rc = ioctl(sock, FIONBIO, (int) &on);
	if (rc == ERROR)
		perror("DATA/FIONBIO");
#else /* !VXWORKS */
	rc = fcntl(sock, F_GETFL, 0);
	if (rc == ERROR)
		perror("DATA/O_NONBLOCK/GET");
	else {
		rc = fcntl(sock, F_SETFL, rc | O_NONBLOCK);
		if (rc == ERROR)
			perror("DATA/O_NONBLOCK");
	}
#endif /* VXWORKS */

#if OLANG_PREFER_TCP_NODELAY
	on = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &on, sizeof(on));
#else /* !OLANG_PREFER_TCP_NODELAY */
	osubjLogAt(6, "accept without TCP_NODELAY");
#endif /* OLANG_PREFER_TCP_NODELAY */

	/* find hub control address */
	osubjRecCopy(&addr, &osubj_hub_cntl_addr);
	osubj_hub_cntl_addr.sin_port = 0;	/* will fill the port later */
	/* update structure */
	osubj_data_sock = sock;
	osubj_conn_state = OSUBJ_CONN_OPEN;
	osubj_alive_get_wd = osubj_alive_put_wd = osubjClock();
	return OK;
}


/*
 * check hub availability; send alive messages
 */
static void
osubjBackgroundTask(void)
{
	long    now = osubjClock();

	if (osubj_conn_state == OSUBJ_CONN_OPEN) {
		/* check hub availability */
		if (now - osubj_alive_get_wd > OLANG_DEAD_MS) {
			osubjLogAt(4, "broken stream - hub dead");
			osubj_conn_state = OSUBJ_CONN_EXIT;
		}
	}
	if (osubj_conn_state == OSUBJ_CONN_OPEN) {
		if (now - osubj_alive_put_wd > OLANG_ALIVE_MS) {
			/* free up hanging packets */
			int     i, qty;

			for (qty = i = 0; i < osubj_q_size; i++) {
				OsubjQueuedPacket_t *pkt = &osubj_q_buf[i];

				if (pkt->len <= 0 && pkt->buf != NULL) {
					osubjFree(pkt->buf, 5);
					pkt->buf = NULL;
					qty++;
				}
			}
			if (qty > 0) {
				osubjLogAt(6, "freed up %d hanging packets", qty);
			}
			/* send alive messages */
			osubjLogAt(8, "send heartbeat to hub");
			osubjSendPacket(OK, OLANG_CONTROL, OLANG_ALIVE, 0, NULL);
		}
	}
}


/*
 *  main loop.
 *  returns number of handled events or ERROR for failures.
 */
int
osubjInternalPoll(long timeout, int work_mask)
{
	fd_set  set[2];
	struct timeval to, *pto;
	int     n_events;
	int     nfds, num;
	long    cur, delta;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if ((osubj_polling_mask & work_mask) != 0)
		return ERROR;
	/* FIXME: should have a mutex. otherwise the code is thread-unsafe */
	osubj_polling_mask |= work_mask;
	if (work_mask & OSUBJ_WORK_MONITOR)
		osubjMonitoring();
	cur = osubjClock();
	/* activate periodic tasks */
	if (work_mask & OSUBJ_WORK_CONTROL) {
		delta = cur - osubj_routine_ms;
		if (delta < 0 || delta > OLANG_STAT_MS ||
			delta > OLANG_ALIVE_MS || !osubj_routine_ms) {
			osubj_routine_ms = cur;
			osubjBackgroundTask();
		}
	}

	/* calculate timeout */
	if (timeout < 0) {
		pto = NULL;
	} else {
		if (osubj_resend_suspend_ms && timeout > osubj_resend_suspend_ms - cur) {
			timeout = osubj_resend_suspend_ms - cur;
			if (timeout < 0)
				timeout = 0;
		}
		to.tv_sec = timeout / 1000;
		to.tv_usec = (timeout % 1000) * 1000;
		pto = &to;
	}

	/* prepare descriptors */
	FD_ZERO(set + 0);
	FD_ZERO(set + 1);
	nfds = 0;

	if (work_mask & OSUBJ_WORK_CONTROL) {
		FD_SET(osubj_cntl_sock, set + 0);
		if (nfds < osubj_cntl_sock)
			nfds = osubj_cntl_sock;
	}

	if (work_mask & OSUBJ_WORK_COMMAND) {
		FD_SET(osubj_serv_sock, set + 0);
		if (nfds < osubj_serv_sock)
			nfds = osubj_serv_sock;

		FD_SET(osubj_cmnd_sock, set + 0);
		if (nfds < osubj_cmnd_sock)
			nfds = osubj_cmnd_sock;
	}

	if (work_mask & OSUBJ_WORK_DATA) {
		/* flush all pending data mon replies */
		osubjAddDataMonReply(0, 0);
		/* prepare to poll socket */
		if (osubj_conn_state == OSUBJ_CONN_OPEN) {
			FD_SET(osubj_data_sock, set + 0);
			if (nfds < osubj_data_sock)
				nfds = osubj_data_sock;
			if (osubj_q_len || osubj_reput.len >= 0) {
				if (osubj_resend_suspend_ms &&
					cur - osubj_resend_suspend_ms > 0) {
					osubj_resend_suspend_ms = 0;
					osubjLogAt(8, "unqueuing unfreezed");
				}
				if (!osubj_resend_suspend_ms)
					FD_SET(osubj_data_sock, set + 1);
			}
		}
	}

	/* poll descriptors */
	errno = 0;
	num = select(nfds + 1, set + 0, set + 1, NULL, pto);
	if (num < 0) {
		osubj_polling_mask &= ~work_mask;
		osubjLogAt(2, "bad select timeout=%ld nfds=%d num=%d errno=%d",
					timeout, nfds, num, errno);
		return ERROR;
	}

	n_events = 0;
	if (num > 0) {
		/* handle events */
		if (FD_ISSET(osubj_cntl_sock, set + 0)) {
			osubjReceiveDatagram(OLANG_CONTROL);
			n_events++;
		}

		if (FD_ISSET(osubj_cmnd_sock, set + 0)) {
			osubjReceiveDatagram(OLANG_COMMAND);
			n_events++;
		}

		if (FD_ISSET(osubj_serv_sock, set + 0)) {
			osubjAcceptConnection();
			n_events++;
		}

		if (FD_ISSET(osubj_data_sock, set + 0)
			&& osubj_conn_state != OSUBJ_CONN_EXIT) {
			osubjReceiveStream();
			n_events++;
		}

		if (FD_ISSET(osubj_data_sock, set + 1)
			&& osubj_conn_state != OSUBJ_CONN_EXIT) {
			osubjResendStream();
			n_events++;
		}
	}

	if (osubj_conn_state == OSUBJ_CONN_EXIT) {
		osubjDisconnectData();
		osubj_conn_state = OSUBJ_CONN_NONE;
	}

	osubj_polling_mask &= ~work_mask;
	return n_events;
}


/*
 *   send combined data_mon_reply
 */
oret_t
osubjAddDataMonReply(int type, ulong_t ooid)
{
#if OLANG_PREFER_THROTTLING
	oret_t rc = OK;
	int k = (int) osubj_data_mon_buf[0];

	if ((osubj_data_mon_start != 0 && k > 0 &&
		 osubjClock() - osubj_data_mon_start > OSUBJ_DATA_MON_TTL)
		|| k >= OSUBJ_MAX_DATA_MON_REPLY) {
		osubj_data_mon_buf[0] = OHTONL(k);
		rc = osubjSendPacket(OK, OLANG_DATA, OLANG_DATA_MON_REPLY,
							 k * (sizeof(long) * 2) + sizeof(long),
							 osubj_data_mon_buf);
		osubjLogAt(6, "send data_mon_reply num=%d rc=%d", k, rc);
		osubj_data_mon_buf[0] = 0;
		osubj_data_mon_start = 0;
	}
	if (type != 0) {
		k = ((int) (osubj_data_mon_buf[0]++)) << 1;
		if (!k)
			osubj_data_mon_start = osubjClock();
		osubj_data_mon_buf[k + 1] = OHTONL(type);
		osubj_data_mon_buf[k + 2] = OHTONL(ooid);
	}
	return rc;
#else /* !OLANG_PREFER_THROTTLING */
	ulong_t reply[1];
	if (type == 0)
		return OK;
	reply[0] = OHTONL(ooid);
	return osubjSendPacket(OK, OLANG_DATA, type, 4, (void *) reply);
#endif /* OLANG_PREFER_THROTTLING */
}


/*
 * handle incoming packets
 */
oret_t
osubjHandleIncoming(int kind, int type, int len, char *buf)
{
	ushort_t *wptr = (ushort_t *) buf;
	ushort_t reply[8];
	oret_t rc;
	int     version;
	char    inet_name[32];

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	switch (type) {
	case OLANG_PING_REQ:
		osubjLogAt(7, "reply to ping id=%04xh", osubj_temp_id);
		if (osubj_allow_reconnect) {
			osubjDisconnectData();
			osubj_conn_state = OSUBJ_CONN_NONE;
		}
		version = (ushort_t) ONTOHS(*(ushort_t *) buf);
		if (version != OLANG_VERSION || osubj_conn_state == OSUBJ_CONN_OPEN) {
			reply[0] = OHTONS(OLANG_VERSION);
			reply[1] = 0;
			rc = osubjSendPacket(ERROR, OLANG_CONTROL, OLANG_PING_NAK, 4,
								 reply);
			osubjLogAt(8, "NAK to ping rc=%d version=%04xh", rc, version);
		} else {
			osubj_subj_id = osubj_temp_id;
			reply[0] = OHTONS(version);
			reply[1] = 0;
			rc = osubjSendPacket(OK, OLANG_CONTROL, OLANG_PING_ACK, 4, reply);
			osubjLogAt(9, "ACK to ping rc=%d version=%04xh", rc, version);
		}
		return OK;
	case OLANG_CONN_REQ:
		if (len < 10 || len != 9 + (uchar_t) buf[8]) {
			osubjLogAt(4, "wrong connection request len=%d", len);
			osubj_conn_state = OSUBJ_CONN_EXIT;
			return OK;
		}
		version = (ushort_t) ONTOHS(*(ushort_t *) buf);
		if (version != OLANG_VERSION) {
			osubjLogAt(4, "wrong connection request version=%04xh", version);
			osubj_conn_state = OSUBJ_CONN_EXIT;
			return OK;
		}
		osubj_hub_cntl_addr.sin_port = wptr[1];
		osubj_subj_id = osubj_temp_id;
		inet_name[0] = 0;
#if defined(VXWORKS)
		/* FIXME: autoconf it ! */
		inet_ntoa_b(osubj_hub_cntl_addr.sin_addr, inet_name);
#else /* !VXWORKS */
		strcpy(inet_name, inet_ntoa(osubj_hub_cntl_addr.sin_addr));
#endif /* VXWORKS */
		memcpy(osubj_hub_desc, buf + 9, (int) (uchar_t) buf[8]);
		osubj_hub_desc[(int) (uchar_t) buf[8]] = 0;
		osubjLogAt(4,
				"connected hub (id=%04xh) at %s:%hu aka \"%s\" version=%04xh",
				osubj_subj_id, inet_name, ntohs(osubj_hub_cntl_addr.sin_port),
				osubj_hub_desc, version);
		*(ulong_t *)(void *)reply = OLANG_MAGIC;
		rc = osubjSendPacket(OK, OLANG_DATA, OLANG_CONN_ACK, 4, reply);
		rc = osubjSendSegments(TRUE);
		rc = osubjSendClasses();
		return OK;
	case OLANG_ALIVE:
		osubjLogAt(8, "got heartbeat");
		return OK;
	case OLANG_DATA_REG_REQ:
		osubjLogAt(9, "got data_reg");
		rc = osubjHandleDataReg(kind, type, len, buf);
		return OK;
	case OLANG_DATA_UNREG_REQ:
		osubjLogAt(9, "got data_unreg");
		rc = osubjHandleDataUnreg(kind, type, len, buf);
		return OK;
	case OLANG_DGROUP_UNREG_REQ:
		osubjLogAt(9, "got data_unreg_group");
		rc = osubjHandleDataGroupUnreg(kind, type, len, buf);
		return OK;
	case OLANG_DATA_WRITE_REQ:
		osubjLogAt(9, "got data_write");
		rc = osubjHandleDataWrite(kind, type, len, buf);
		return OK;
	case OLANG_DATA_READ_REQ:
		osubjLogAt(9, "got data_read");
		rc = osubjHandleDataRead(kind, type, len, buf);
		return OK;
	case OLANG_MSG_RECV:
		osubjLogAt(9, "got msg_recv");
		rc = osubjHandleMsgRecv(kind, type, len, buf);
		return OK;
	default:
		osubjLogAt(4, "unknown packet kind=%d type=%04xh len=%d id=%04x",
					kind, type, len, osubj_subj_id);
		return ERROR;
	}
}
