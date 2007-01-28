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

  Handling of the network protocol with front end clients.

  Note: the UDP control port is shared between front ends and
        back end agents.

*/

#include "hub-priv.h"
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for OMAX,ONTOHS,nonblocking,... */
#include <optikus/conf-mem.h>	/* for oxnew,oxbcopy,... */

#include <unistd.h>			/* for close */
#include <stdlib.h>			/* for atoi */
#include <errno.h>			/* for errno,EPIPE */
#include <string.h>			/* for strncmp,... */
#include <netinet/in.h>		/* for sockaddr_in */
#include <netinet/tcp.h>	/* for TCP_NODELAY */
#include <arpa/inet.h>		/* for inet_ntoa */


int     ohub_watch_serv_sock;
int     ohub_watch_cntl_sock;

long    ohub_watch_routine_ms;

int     ohub_watch_can_immed = 1;	/* 0 = never send packets immediately
									 * 1 = packets less than 800 b
									 * 2 = send any packet immediately first
									 */

bool_t  ohub_watchers_enable;

#define OHUB_WATCH_Q_SIZE     2500

int     ohub_watch_init_q_size = OHUB_WATCH_Q_SIZE;

typedef struct
{
	int     type;
	int     len;
	char   *buf;
} ohubQueuedWatchPacket_t;

#define OHUB_WATCH_NONE  0
#define OHUB_WATCH_WAIT  1
#define OHUB_WATCH_OPEN  2
#define OHUB_WATCH_EXIT  3

typedef struct OhubNetWatch_st
{
	int     watch_state;
	ushort_t watch_id;
	bool_t  needs_rotor;
	int     data_sock;
	long    alive_get_wd;
	long    alive_put_wd;
	bool_t  announce_blocked;
	int     version;
	/* send queue */
	ohubQueuedWatchPacket_t *q_buf;
	int     q_len;
	int     q_size;
	int     q_rd;
	int     q_wr;
	/* packet to be reget */
	char    hdr_buf[16];
	int     hdr_off;
	int     reget_type;
	int     reget_len;
	int     reget_off;
	char   *reget_buf;
	/* packet to be resent */
	int     reput_type;
	int     reput_len;
	int     reput_off;
	char   *reput_buf;
	/* for UDP packets */
	struct sockaddr_in cntl_addr;
	char    url[OHUB_MAX_NAME];
	char    watch_desc[OHUB_MAX_PATH];
	/* for monitoring */
	OhubMonAccum_t accum;
} OhubNetWatch_t;

#define OHUB_MAX_WATCH   32

OhubNetWatch_t *ohub_watchers;
OhubNetWatch_t ohub_cntl_watch;
ushort_t ohub_next_watch_id;


/*
 *  initialize server sockets
 */
oret_t
ohubWatchInitNetwork(int serv_port)
{
	int     i, rc;
	struct sockaddr_in cntl_addr, serv_addr;
	OhubNetWatch_t *pnwatch;
	int     on = 1;

	/* cleanup */
	ohub_watchers_enable = FALSE;
	if (serv_port == 0)
		serv_port = OLANG_HUB_PORT;
	ohubWatchersShutdownNetwork();
	if (OHUB_MAX_WATCH > 255) {		/* FIXME: document this limit */
		ologAbort("cannot be more than 255 watchers");
		goto FAIL;
	}
	ohub_next_watch_id = (ushort_t) osMsClock();

	/* prepare watcher structures */
	if (NULL == (ohub_watchers = oxnew(OhubNetWatch_t, OHUB_MAX_WATCH)))
		goto FAIL;
	pnwatch = &ohub_watchers[0];
	for (i = 0; i < OHUB_MAX_WATCH; i++, pnwatch++) {
		oxvzero(pnwatch);
		pnwatch->watch_state = OHUB_WATCH_NONE;
		pnwatch->reget_len = pnwatch->reget_off = pnwatch->reget_type = -1;
		pnwatch->reput_len = pnwatch->reput_off = pnwatch->reput_type = -1;
	}

	/* create UDP socket */
	ohub_watch_cntl_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ohub_watch_cntl_sock == ERROR) {
		perror("cannot open control socket");
		goto FAIL;
	}
	rc = setsockopt(ohub_watch_cntl_sock, SOL_SOCKET, SO_REUSEADDR,
					(char *) &on, sizeof(on));
	if (rc == ERROR)
		perror("CNTL/SO_REUSEADDR");
	oxvzero(&cntl_addr);
	cntl_addr.sin_port = htons(serv_port);
	cntl_addr.sin_family = AF_INET;
	cntl_addr.sin_addr.s_addr = INADDR_ANY;
	rc = bind(ohub_watch_cntl_sock,
			  (struct sockaddr *) &cntl_addr, sizeof(cntl_addr));
	if (rc == ERROR) {
		perror("cannot bind control socket");
		goto FAIL;
	}

	if (setNonBlocking(ohub_watch_cntl_sock) == ERROR)
		goto FAIL;

	/* create TCP accepting socket */
	ohub_watch_serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ohub_watch_serv_sock == ERROR) {
		perror("cannot open server socket");
		goto FAIL;
	}
	rc = setsockopt(ohub_watch_serv_sock, SOL_SOCKET, SO_REUSEADDR,
					(char *) &on, sizeof(on));
	if (rc == ERROR)
		perror("SERV/SO_REUSEADDR");
	oxvzero(&serv_addr);
	serv_addr.sin_port = htons(serv_port + 1);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	rc = bind(ohub_watch_serv_sock,
			  (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (rc == ERROR) {
		perror("cannot bind server socket");
		goto FAIL;
	}
	rc = listen(ohub_watch_serv_sock, OLANG_CONNECT_QTY);
	if (rc == ERROR) {
		perror("cannot listen on server socket");
		goto FAIL;
	}
	if (setNonBlocking(ohub_watch_serv_sock) == ERROR)
		goto FAIL;

	/* setup variables */
	strcpy(ohub_cntl_watch.url, "[CNTL]");
	ohubLog(8, "watch network init ports=%d,%d serv_sock=%d cntl_sock=%d",
		serv_port + 1, serv_port, ohub_watch_serv_sock, ohub_watch_cntl_sock);
	return OK;

  FAIL:
	ohubWatchersShutdownNetwork();
	return ERROR;
}


/*
 *  initialize client structure
 */
OhubNetWatch_t *
ohubWatchOpen(void)
{
	OhubNetWatch_t *pnwatch;
	int     i;

	/* find idle record */
	if (!ohub_watchers)
		return NULL;
	pnwatch = &ohub_watchers[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_WATCH; i++, pnwatch++)
		if (pnwatch->watch_state == OHUB_WATCH_NONE)
			break;
	if (i >= OHUB_MAX_WATCH)
		return NULL;
	/* connection */
	pnwatch->watch_state = OHUB_WATCH_WAIT;
	pnwatch->announce_blocked = FALSE;
	if ((++ohub_next_watch_id & 0xFF) == 0)
		++ohub_next_watch_id;
	pnwatch->watch_id = (i & 0xFF) | ((ohub_next_watch_id & 0xFF) << 8);
	pnwatch->data_sock = 0;
	pnwatch->alive_get_wd = pnwatch->alive_put_wd = 0;
	pnwatch->q_len = pnwatch->q_rd = pnwatch->q_wr = 0;
	/* send queue */
	pnwatch->q_size = ohub_watch_init_q_size;
	pnwatch->q_buf = oxnew(ohubQueuedWatchPacket_t, pnwatch->q_size);
	if (NULL == pnwatch->q_buf) {
		pnwatch->q_size = 0;
		pnwatch->watch_state = OHUB_WATCH_NONE;
		return NULL;
	}
	pnwatch->q_len = pnwatch->q_rd = pnwatch->q_wr = 0;
	/* reget/reput buffers */
	pnwatch->hdr_off = 0;
	pnwatch->reget_buf = pnwatch->reput_buf = NULL;
	pnwatch->reget_len = pnwatch->reget_off = pnwatch->reget_type = -1;
	pnwatch->reput_len = pnwatch->reput_off = pnwatch->reput_type = -1;
	/* other variables */
	pnwatch->needs_rotor = FALSE;
	oxvzero(&pnwatch->cntl_addr);
	strcpy(pnwatch->url, "?");
	pnwatch->accum.off = 1;
	return pnwatch;
}


/*
 *  close client
 */
void
ohubWatchClose(OhubNetWatch_t * pnwatch)
{
	ohubQueuedWatchPacket_t *packet;
	int     i;

	/* close data socket */
	if (pnwatch->data_sock > 0) {
		close(pnwatch->data_sock);
		ohubLog(4, "disconnected watcher [%s]", pnwatch->url);
		ohubMonRemoveWatch(pnwatch, pnwatch->url);
		ohubCancelWatchData(pnwatch, pnwatch->url);
		ohubClearSubjectWaiters(pnwatch, pnwatch->url);
		ohubRegisterMsgNode(pnwatch, NULL, OHUB_MSGNODE_REMOVE,
							pnwatch->watch_id, pnwatch->watch_desc);
	}
	pnwatch->data_sock = 0;

	/* clear send queue */
	if (pnwatch->q_buf) {
		for (i = 0; i < pnwatch->q_size; i++) {
			packet = &pnwatch->q_buf[i];
			oxfree(packet->buf);
			packet->buf = NULL;
			packet->len = packet->type = 0;
		}
	}
	oxfree(pnwatch->q_buf);
	pnwatch->q_buf = NULL;
	pnwatch->q_rd = pnwatch->q_wr = pnwatch->q_len = 0;
	pnwatch->q_size = 0;

	/* clear packet to be reget */
	pnwatch->hdr_off = 0;
	oxfree(pnwatch->reget_buf);
	pnwatch->reget_buf = NULL;
	pnwatch->reget_len = pnwatch->reget_off = pnwatch->reget_type = -1;

	/* clear packet to be reput */
	oxfree(pnwatch->reput_buf);
	pnwatch->reput_buf = NULL;
	pnwatch->reput_len = pnwatch->reput_len = pnwatch->reput_type = -1;

	/* clear state */
	pnwatch->needs_rotor = FALSE;
	oxvzero(&pnwatch->cntl_addr);
	pnwatch->alive_get_wd = pnwatch->alive_put_wd = 0;
	pnwatch->watch_id = 0;
	pnwatch->watch_state = OHUB_WATCH_NONE;
}


/*
 *  close all network connections with clients
 */
oret_t
ohubWatchersShutdownNetwork(void)
{
	int     i;

	if (NULL != ohub_watchers) {
		for (i = 0; i < OHUB_MAX_WATCH; i++)
			ohubWatchClose(&ohub_watchers[i]);
	}
	oxfree(ohub_watchers);
	ohub_watchers = NULL;

	/* close sockets */
	if (ohub_watch_serv_sock > 0)
		close(ohub_watch_serv_sock);
	ohub_watch_serv_sock = 0;
	if (ohub_watch_cntl_sock > 0)
		close(ohub_watch_cntl_sock);
	ohub_watch_cntl_sock = 0;

	/* clear variables */
	oxvzero(&ohub_cntl_watch);
	ohub_watch_routine_ms = 0;
	return OK;
}


/*
 * check sum
 */
static ushort_t
ohubWatchCheckSum(char *buf, int nbytes)
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
 *  send data over network to client
 */
static oret_t
ohubWatchPokeStream(OhubNetWatch_t * pnwatch, int type, int len, void *dp)
{
	char   *buf;
	int     n, blen;
	ushort_t *wptr, cksum;

	if (pnwatch->reput_len >= 0) {
		/* retry */
		blen = pnwatch->reput_len - pnwatch->reput_off;
		if (blen > 0)
			n = send(pnwatch->data_sock, pnwatch->reput_buf + pnwatch->reput_off,
					 blen, O_MSG_NOSIGNAL);
		else
			n = blen = 0;
		pnwatch->reput_off += n;
		if (n < blen)
			return ERROR;
		oxfree(pnwatch->reput_buf);
		pnwatch->reput_buf = NULL;
		pnwatch->reput_off = pnwatch->reput_len = pnwatch->reput_type = -1;
		pnwatch->alive_put_wd = osMsClock();
	}
	if (len < 0)
		return OK;
	if (len > 0 && !dp)
		return ERROR;
	blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
	if ((blen & 1) != 0)
		blen++;
	if (NULL == (buf = oxnew(char, blen)))
		return ERROR;

	wptr = (ushort_t *) buf;
	wptr[0] = OHTONS(len);
	wptr[1] = OHTONS(type);
	wptr[2] = OHTONS(pnwatch->watch_id);
	oxbcopy(dp, buf + OLANG_HEAD_LEN, len);
	cksum = ohubWatchCheckSum(buf, len + OLANG_HEAD_LEN);
	*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
	if (blen > 0)
		n = send(pnwatch->data_sock, (void *) buf, blen, O_MSG_NOSIGNAL);
	else
		n = blen = 0;
	if (n == blen) {
		oxfree(buf);
		pnwatch->alive_put_wd = osMsClock();
		ohubLog(9, "send immed OK type=%04xh len=%d id=%04xh blen=%d",
				type, len, pnwatch->watch_id, blen);
		return OK;
	}

	/* send error */
	if (errno == EPIPE) {
		ohubLog(4, "broken watch stream [%s] - broken pipe len=%d got=%d",
				pnwatch->url, blen, n);
		pnwatch->watch_state = OHUB_WATCH_EXIT;
		oxfree(buf);
		return ERROR;
	}
	if (n > 0 && n < OLANG_HEAD_LEN) {
		ohubLog(4, "broken watch stream [%s] - pipe overflow len=%d got=%d",
				pnwatch->url, blen, n);
		pnwatch->watch_state = OHUB_WATCH_EXIT;
		oxfree(buf);
		return ERROR;
	}

	if (n <= 0) {
		oxfree(buf);
		return ERROR;
	}

	/* retry later */
	pnwatch->reput_buf = buf;
	pnwatch->reput_off = n;
	pnwatch->reput_len = blen;
	pnwatch->reput_type = type;

	return OK;
}


/*
 *  retry sending queued packets
 */
static void
ohubWatchResendStream(OhubNetWatch_t * pnwatch)
{
	ohubQueuedWatchPacket_t *pkt;

	for (;;) {
		if (pnwatch->reput_len >= 0) {
			if (ohubWatchPokeStream(pnwatch, -1, -1, NULL) != OK)
				break;
		}
		if (!pnwatch->q_len)
			break;
		pkt = &pnwatch->q_buf[pnwatch->q_rd];
		if (ohubWatchPokeStream(pnwatch, pkt->type, pkt->len, pkt->buf) != OK) {
			ohubLog(9,
				"watch unqueuing ERR TCP type=%04xh len=%d qrd=%d qlen=%d [%s]",
				pkt->type, pkt->len, pnwatch->q_rd, pnwatch->q_len, pnwatch->url);
			break;
		}
		if (++pnwatch->q_rd == pnwatch->q_size)
			pnwatch->q_rd = 0;
		pnwatch->q_len--;
		ohubLog(9,
			"watch unqueuing OK TCP type=%04xh len=%d qrd=%d qlen=%d [%s]",
			pkt->type, pkt->len, pnwatch->q_rd, pnwatch->q_len, pnwatch->url);
		pkt->type = pkt->len = 0;
		oxfree(pkt->buf);
		pkt->buf = NULL;
		if (pnwatch->q_len == 0 || pnwatch->reput_len >= 0)
			break;
	}
}


/*
 *  immediately or later send packet to client
 */
oret_t
ohubWatchSendPacket(OhubNetWatch_t * pnwatch, int kind, int type, int len, void *data)
{
	ohubQueuedWatchPacket_t *pkt;
	ushort_t *wptr, cksum;
	char   *buf, small[16];
	int     blen, num, i;

	if (len < 0 || (len > 0 && NULL == data))
		return ERROR;

	if (pnwatch == NULL) {
		/* broadcast */
		num = 0;
		pnwatch = &ohub_watchers[1];
		for (i = 1; i < OHUB_MAX_WATCH; i++, pnwatch++)
			if (pnwatch->watch_state == OHUB_WATCH_OPEN)
				if (ohubWatchSendPacket(pnwatch, kind, type, len, data) != OK)
					num++;
		ohubLog(9, "broadcast kind=%d type=%04xh len=%d to %d watchers",
				kind, type, len, num);
		return num ? ERROR : OK;
	}

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
				ohubLog(2, "cannot send to [%s] no memory for %d bytes",
						pnwatch->url, blen);
				return ERROR;
			}
		}
		wptr = (ushort_t *) buf;
		wptr[0] = OHTONS(len);
		wptr[1] = OHTONS(type);
		wptr[2] = OHTONS(pnwatch->watch_id);
		oxbcopy(data, buf + OLANG_HEAD_LEN, len);
		cksum = ohubWatchCheckSum(buf, len + OLANG_HEAD_LEN);
		*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
		num = sendto(ohub_watch_cntl_sock, (void *) buf, blen, 0,
					 (struct sockaddr *) &pnwatch->cntl_addr,
					 sizeof(pnwatch->cntl_addr));
		if (buf != small)
			oxfree(buf);
		if (num == blen) {
			pnwatch->alive_put_wd = osMsClock();
			ohubLog(9, "send UDP kind=%d type=%04xh len=%d id=%04xh "
					"to [%s] %08lxh:%hu OK",
					kind, type, len, pnwatch->watch_id, pnwatch->url,
					ONTOHL(pnwatch->cntl_addr.sin_addr.s_addr),
					ONTOHS(pnwatch->cntl_addr.sin_port));
			return OK;
		}
		ohubLog(5, "cannot UDP kind=%d type=%04xh len=%d id=%04xh to [%s], "
				"blen=%d num=%d errno=%d %08lxh:%hu",
				kind, type, len, pnwatch->watch_id, pnwatch->url, blen, num,
				errno, ONTOHL(pnwatch->cntl_addr.sin_addr.s_addr),
				ONTOHS(pnwatch->cntl_addr.sin_port));
		return ERROR;
	case OLANG_DATA:
		break;
	default:
		return ERROR;
	}

	/* send to stream */
	if (pnwatch->watch_state != OHUB_WATCH_OPEN)
		return ERROR;

	if (!pnwatch->q_len && pnwatch->reput_len < 0
		&& ohub_watch_can_immed
		&& (len < OLANG_SMALL_PACKET || ohub_watch_can_immed > 1)) {
		if (ohubWatchPokeStream(pnwatch, type, len, data) == OK)
			return OK;
	}

	if (pnwatch->q_len == pnwatch->q_size) {
		ohubLog(3, "send queue overflow watcher [%s] "
				"q_len=%d q_size=%d type=%04x len=%d",
				pnwatch->url, pnwatch->q_len, pnwatch->q_size, type, len);
		return ERROR;
	}

	pkt = &pnwatch->q_buf[pnwatch->q_wr];
	if (pkt->buf) {
		ohubLog(6, "hanging watcher [%s] pkt type=%04x len=%d qwr=%d qlen=%d",
			pnwatch->url, pkt->type, pkt->len, pnwatch->q_wr, pnwatch->q_len);
		oxfree(pkt->buf);
		pkt->buf = NULL;
	}
	if (len == 0)
		pkt->buf = NULL;
	else {
		if (NULL == (pkt->buf = oxnew(char, len)))
			return ERROR;
		oxbcopy(data, pkt->buf, len);
	}

	pkt->len = len;
	pkt->type = type;

	if (++pnwatch->q_wr == pnwatch->q_size)
		pnwatch->q_wr = 0;
	++pnwatch->q_len;
	ohubLog(9, "watcher queued TCP type=%04xh len=%d qwr=%d qlen=%d [%s]",
		 	type, len, pnwatch->q_wr, pnwatch->q_len, pnwatch->url);

	return OK;
}


/*
 *  receive stream from client
 */
static void
ohubWatchReceiveStream(OhubNetWatch_t * pnwatch)
{
	int     num, len;
	ushort_t id, cksum, cksum2, csbuf[8], *hbuf;
	bool_t  first_pass = TRUE;

	/* get packet from TCP */
	for (;;) {
		/* if there was partial packet */
		if (pnwatch->reget_len >= 0) {
			len = pnwatch->reget_len + OLANG_TAIL_LEN - pnwatch->reget_off;
			if (len & 1)
				len++;
			ohubLog(9, "trying to reget from [%s] off=%d len=%d",
					pnwatch->url, pnwatch->reget_off, pnwatch->reget_len);
			num = recv(pnwatch->data_sock,
						pnwatch->reget_buf + pnwatch->reget_off, len,
						O_MSG_NOSIGNAL);
			if (num <= 0) {
				ohubLog(4,
						"broken watch stream [%s] - null reget len=%d got=%d",
						pnwatch->url, len, num);
				pnwatch->watch_state = OHUB_WATCH_EXIT;
				break;
			}
			first_pass = FALSE;
			if (num != len) {
				pnwatch->reget_off += num;
				break;
			}
			cksum = *(ushort_t *) (pnwatch->reget_buf + len - OLANG_TAIL_LEN);
			cksum = ONTOHS(cksum);
		} else {
			/* check for new packet */
			len = OLANG_HEAD_LEN - pnwatch->hdr_off;
			num = recv(pnwatch->data_sock,
					   (pnwatch->hdr_buf + pnwatch->hdr_off), len, O_MSG_NOSIGNAL);
			if (num > 0)
				pnwatch->hdr_off += num;
			ohubLog(9, "peek at stream from [%s] off=%d len=%d num=%d first=%d",
					pnwatch->url, pnwatch->hdr_off, len, num, first_pass);
			if (num != len) {
				if (first_pass) {
					ohubLog(4, "broken watch stream [%s] - "
							"null peek off=%d len=%d num=%d errno=%d",
							pnwatch->url, pnwatch->hdr_off, len, num, errno);
					if (num <= 0) {
						pnwatch->watch_state = OHUB_WATCH_EXIT;
						pnwatch->hdr_off = 0;
					}
				}
				break;
			}
			pnwatch->hdr_off = 0;
			first_pass = FALSE;
			hbuf = (ushort_t *) pnwatch->hdr_buf;
			pnwatch->reget_len = (ushort_t) ONTOHS(hbuf[0]);
			pnwatch->reget_off = 0;
			pnwatch->reget_type = (ushort_t) ONTOHS(hbuf[1]);
			id = (ushort_t) ONTOHS(hbuf[2]);
			if (id != pnwatch->watch_id) {
				if (id == 0 && pnwatch->reget_type == OLANG_CONN_REQ) {
					/* connection request - id should be 0 */
				} else {
					ohubLog(2,
						"wrong TCP watch id %04xh (should be %04xh) from [%s]",
						id, pnwatch->watch_id, pnwatch->url);
				}
			}
			if (pnwatch->reget_len > OLANG_MAX_PACKET) {
				pnwatch->watch_state = OHUB_WATCH_EXIT;
				break;
			}
			if (!pnwatch->reget_len) {
				/* get checksum */
				num = 0;
				pnwatch->reget_buf = NULL;
				num = recv(pnwatch->data_sock, (void *) csbuf, OLANG_TAIL_LEN,
						   MSG_PEEK | O_MSG_NOSIGNAL);
				if (num != OLANG_TAIL_LEN)
					break;
				num =
					recv(pnwatch->data_sock, (void *) csbuf, OLANG_TAIL_LEN,
						 O_MSG_NOSIGNAL);
				cksum = ONTOHS(csbuf[0]);
			} else {
				/* get data and checksum */
				len = pnwatch->reget_len + OLANG_TAIL_LEN - pnwatch->reget_off;
				if (len & 1)
					len++;
				if (NULL == (pnwatch->reget_buf = oxnew(char, len))) {
					/* not enough memory */
					ohubLog(4, "no memory for stream length %d from [%s]",
							len, pnwatch->url);
					break;
				}
				num = recv(pnwatch->data_sock, pnwatch->reget_buf, len,
							O_MSG_NOSIGNAL);
				if (num <= 0) {
					pnwatch->watch_state = OHUB_WATCH_EXIT;
					ohubLog(4, "broken watch stream [%s] - len=%d num=%d",
							pnwatch->url, len, num);
					break;
				}
				if (num != len) {
					pnwatch->reget_off = len;
					break;
				}
				cksum = *(ushort_t *) (pnwatch->reget_buf + len - OLANG_TAIL_LEN);
				cksum = ONTOHS(cksum);
			}
		}
		/* if we are here then the packet is ready */
		cksum2 =
			ohubWatchCheckSum(pnwatch->reget_buf,
							 pnwatch->reget_len + OLANG_HEAD_LEN);
		if (cksum != cksum2) {
			pnwatch->watch_state = OHUB_WATCH_EXIT;
			break;
		}
		pnwatch->alive_get_wd = osMsClock();
		ohubLog(9, "handle stream from [%s] type=%04xh len=%d id=%04xh",
				pnwatch->url, pnwatch->reget_type, pnwatch->reget_len,
				pnwatch->watch_id);
		ohubWatchHandleProtocol(pnwatch, OLANG_DATA, pnwatch->reget_type,
								pnwatch->reget_len, pnwatch->reget_buf);
		oxfree(pnwatch->reget_buf);
		pnwatch->reget_buf = NULL;
		pnwatch->reget_type = pnwatch->reget_len = pnwatch->reget_off = -1;
		/* go on to the next packet */
	}
	if (pnwatch->watch_state == OHUB_WATCH_EXIT) {
		oxfree(pnwatch->reget_buf);
		pnwatch->reget_buf = NULL;
		pnwatch->reget_type = pnwatch->reget_len = pnwatch->reget_off = -1;
	}
}


/*
 *   find a watcher record
 */
oret_t
ohubWatchGetById(ushort_t id, struct OhubNetWatch_st **nwatch_pp, const char *who)
{
	int     n = id & 255;
	OhubNetWatch_t *pnwatch;

	if (n < 1 || n >= OHUB_MAX_WATCH) {
		/* wrong watcher number */
		ohubLog(2, "[%s]: wrong watch id %04xh, watch_no=%d invalid",
				who ?: "", id, n);
		return ERROR;
	}
	pnwatch = &ohub_watchers[n];
	if (pnwatch->watch_state != OHUB_WATCH_WAIT
			&& pnwatch->watch_state != OHUB_WATCH_OPEN) {
		ohubLog(2, "[%s]: wrong watch state id=%04xh, watch_no=%d",
				who ?: "", id, n);
		return ERROR;
	}
	if (id != pnwatch->watch_id) {
		ohubLog(2, "[%s]: wrong watch id=%04xh (should be %04xh) url=[%s]",
				who ?: "", id, pnwatch->watch_id, pnwatch->url);
		return ERROR;
	}
	if (nwatch_pp)
		*nwatch_pp = pnwatch;
	return OK;
}


/*
 *   receive a datagram
 */
static void
ohubWatchReceiveDatagram(OhubNetWatch_t * pnwatch)
{
	int     n, len, type, blen;
	ushort_t hdr[10], cksum, cksum2, id;
	char   *buf;
	struct sockaddr_in tmp_cntl_addr;
	socklen_t addr_len = sizeof(tmp_cntl_addr);
	struct sockaddr_in *watch_cntl_addr_ptr;
	oret_t rc;

	if (pnwatch->watch_state == OHUB_WATCH_OPEN)
		watch_cntl_addr_ptr = &tmp_cntl_addr;	/* if not open then record it */
	else
		watch_cntl_addr_ptr = &pnwatch->cntl_addr;	/* if open then already found */
	n = recvfrom(ohub_watch_cntl_sock, (void *) hdr, OLANG_HEAD_LEN, MSG_PEEK,
				 (struct sockaddr *) watch_cntl_addr_ptr, &addr_len);
	if (n != OLANG_HEAD_LEN) {
		/* header error */
		goto REMOVE;
	}
	len = (ushort_t) ONTOHS(hdr[0]);
	type = (ushort_t) ONTOHS(hdr[1]);
	id = ONTOHS(hdr[2]);
	if (len > OLANG_MAX_PACKET) {
		/* datagram too big */
		goto REMOVE;
	}
	if (id != 0) {
		rc = ohubWatchGetById(id, &pnwatch, "UDP");
		if (rc != OK)
			goto REMOVE;
	}
	blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
	if (blen & 1)
		blen++;
	if (NULL == (buf = oxnew(char, blen))) {
		/* not enough memory */
		ohubLog(4, "huge datagram size len=%d blen=%d", len, blen);
		goto REMOVE;
	}
	n = recvfrom(ohub_watch_cntl_sock, buf, blen, 0, NULL, NULL);
	if (n != blen) {
		/* data receive error */
		ohubLog(4, "invalid datagram size len=%d blen=%d n=%d", len, blen, n);
		goto END;
	}
	cksum = *(ushort_t *) (buf + blen - OLANG_TAIL_LEN);
	cksum = ONTOHS(cksum);
	cksum2 = ohubWatchCheckSum(buf, blen - OLANG_TAIL_LEN);
	if (cksum != cksum2) {
		/* checksum fail */
		ohubLog(4, "invalid datagram checksum len=%d blen=%d "
				"cs1=%04x cs2=%04x id=%04xh", len, blen, cksum, cksum2, id);
		goto END;
	}
	pnwatch->alive_get_wd = osMsClock();
	ohubLog(9, "handle datagram from [%s] type=%04xh len=%d id=%04xh",
			pnwatch->url, type, len, id);
	ohubWatchHandleProtocol(pnwatch, OLANG_CONTROL, type, len,
							buf + OLANG_HEAD_LEN);
  END:
	oxfree(buf);
	return;
  REMOVE:
	/* remove datagram from system buffer */
	ohubLog(4, "removing invalid datagram");
	recvfrom(ohub_watch_cntl_sock, (char *) hdr, OLANG_HEAD_LEN, 0, NULL, NULL);
	return;
}


/*
 *  accept connection from client
 */
static oret_t
ohubWatchAcceptConnection()
{
	OhubNetWatch_t *pnwatch;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	int     sock, on;

	sock = accept(ohub_watch_serv_sock, (struct sockaddr *) &addr, &addr_len);
	if (sock == ERROR)
		return ERROR;
	pnwatch = ohubWatchOpen();
	if (!pnwatch) {
		close(sock);
		return ERROR;
	}

	/* set socket options */
	setNonBlocking(sock);
	on = 1;
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *) &on, sizeof(on));

	/* find client control address */
	oxvcopy(&addr, &pnwatch->cntl_addr);
	pnwatch->cntl_addr.sin_port = 0;	/* will fill the port later */
	/* update structure */
	pnwatch->data_sock = sock;
	sprintf(pnwatch->url, "<%s:%hu>",
			inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	pnwatch->watch_state = OHUB_WATCH_OPEN;
	pnwatch->alive_get_wd = pnwatch->alive_put_wd = osMsClock();
	return OK;
}


/*
 * check client availability; send alive messages
 */
void
ohubWatchBackgroundTask(void)
{
	int     i;
	long    now = osMsClock();
	OhubNetWatch_t *pnwatch = &ohub_watchers[0];

	for (i = 0; i < OHUB_MAX_WATCH; i++, pnwatch++) {
		if (pnwatch->watch_state == OHUB_WATCH_OPEN) {
			/* check client availability */
			if (now - pnwatch->alive_get_wd > OLANG_DEAD_MS) {
				pnwatch->watch_state = OHUB_WATCH_EXIT;
				olog("[%s] is dead", pnwatch->url);
				ohubWatchClose(pnwatch);
			}
		}
		if (pnwatch->watch_state == OHUB_WATCH_OPEN) {
			/* free up hanging packets */
			int     i, qty;

			for (qty = i = 0; i < pnwatch->q_size; i++) {
				ohubQueuedWatchPacket_t *pkt = &pnwatch->q_buf[i];

				if (pkt->len <= 0 && pkt->buf != 0) {
					oxfree(pkt->buf);
					pkt->buf = NULL;
					qty++;
				}
			}
			if (qty > 0) {
				ohubLog(6, "watch [%s] freed up %d hanging packets",
						pnwatch->url, qty);
			}
			/* send alive messages */
			if (now - pnwatch->alive_put_wd > OLANG_ALIVE_MS) {
				ohubWatchSendPacket(pnwatch, OLANG_CONTROL, OLANG_ALIVE, 0, NULL);
				ohubLog(9, "send heartbeat to [%s]", pnwatch->url);
				pnwatch->alive_put_wd = now;
				/* FIXME: WatchSendPacket should reset the timeout */
			}
		}
	}
}


/*
 *  fill array with watch network descriptors
 */
int
ohubWatchPrepareEvents(fd_set * set, int nfds)
{
	OhubNetWatch_t *pnwatch;
	int     i;

	FD_SET(ohub_watch_cntl_sock, set + 0);
	nfds = OMAX(nfds, ohub_watch_cntl_sock);
	FD_SET(ohub_watch_serv_sock, set + 0);
	nfds = OMAX(nfds, ohub_watch_serv_sock);

	pnwatch = &ohub_watchers[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_WATCH; i++, pnwatch++) {

		if (pnwatch->accum.qty > 0
			&& osMsClock() - pnwatch->accum.start_ms > OHUB_MON_ACCUM_TTL)
			ohubMonFlushMonAccum(pnwatch, &pnwatch->accum);

		if (pnwatch->watch_state != OHUB_WATCH_OPEN)
			continue;
		FD_SET(pnwatch->data_sock, set + 0);
		nfds = OMAX(nfds, pnwatch->data_sock);
		if (pnwatch->q_len || pnwatch->reput_len >= 0) {
			FD_SET(pnwatch->data_sock, set + 1);
		}
	}
	return nfds;
}


/*
 *  handle watch network events
 */
int
ohubWatchHandleEvents(fd_set * set, int num)
{
	OhubNetWatch_t *pnwatch;
	int     i, n_events = 0;

	if (FD_ISSET(ohub_watch_cntl_sock, set + 0)) {
		ohubLog(9, "got some watch datagram");
		ohubWatchReceiveDatagram(&ohub_cntl_watch);
		n_events++;
	}

	if (FD_ISSET(ohub_watch_serv_sock, set + 0)) {
		ohubLog(9, "got some watch connection request");
		ohubWatchAcceptConnection();
		n_events++;
	}

	pnwatch = &ohub_watchers[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_WATCH; i++, pnwatch++) {
		switch (pnwatch->watch_state) {
		case OHUB_WATCH_NONE:
		case OHUB_WATCH_EXIT:
			continue;
		}
		if (FD_ISSET(pnwatch->data_sock, set + 0)) {
			ohubLog(9, "got some packet for [%s]", pnwatch->url);
			ohubWatchReceiveStream(pnwatch);
			n_events++;
		}
		if (pnwatch->watch_state == OHUB_WATCH_EXIT)
			continue;
		if (FD_ISSET(pnwatch->data_sock, set + 1)) {
			ohubLog(9, "resend some packet to [%s]", pnwatch->url);
			ohubWatchResendStream(pnwatch);
			n_events++;
		}
	}
	return n_events;
}


/*
 *  lazy watch close
 */
void
ohubWatchLazyShutdown(void)
{
	OhubNetWatch_t *pnwatch;
	int     i;

	pnwatch = &ohub_watchers[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_WATCH; i++, pnwatch++) {
		if (pnwatch->watch_state == OHUB_WATCH_EXIT)
			ohubWatchClose(pnwatch);
	}
}


/*
 * handle connection request
 */
oret_t
ohubWatchConnect(OhubNetWatch_t * pnwatch, int kind,
				int type, int len, char *buf)
{
	ushort_t reply[8];
	ulong_t magic;
	oret_t rc;

	if (len < 10 || len != 9 + (uchar_t) buf[8])
		magic = 0;
	else
		magic = *(ulong_t *) (buf + 4);

	pnwatch->version = (ushort_t) ONTOHS(*(ushort_t *) buf);
	if (pnwatch->version != OLANG_VERSION)
		magic = 0;

	if (magic == OLANG_MAGIC) {
		pnwatch->needs_rotor = FALSE;
	}
	else if (magic == OROTL(OLANG_MAGIC)) {
		pnwatch->needs_rotor = TRUE;
	}	
	else {
		ohubLog(4, "bad connection request from [%s] magic=%lxh version=%04xh",
				pnwatch->url, magic, pnwatch->version);
		pnwatch->watch_state = OHUB_WATCH_EXIT;
		return OK;
	}

	pnwatch->cntl_addr.sin_port = *(ushort_t *) (buf + 2);
	oxbcopy(buf + 9, pnwatch->watch_desc, (int) (uchar_t) buf[8]);
	pnwatch->watch_desc[(int) (uchar_t) buf[8]] = 0;
	ohubLog(9, "acknowledge connection to [%s] cntl_port=%d",
			pnwatch->url, ntohs(pnwatch->cntl_addr.sin_port));
	sprintf(pnwatch->url, "%s:%hu",
			inet_ntoa(pnwatch->cntl_addr.sin_addr),
			ONTOHS(pnwatch->cntl_addr.sin_port));
	rc = ohubSendSubjects(pnwatch, TRUE, pnwatch->url);

	*(ulong_t *)(void *)reply = OLANG_MAGIC;
	rc = ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_CONN_ACK, 4, reply);

	rc = ohubRegisterMsgNode(pnwatch, NULL, OHUB_MSGNODE_ADD,
							pnwatch->watch_id, pnwatch->watch_desc);

	ohubLog(4, "connected watcher [%s] aka \"%s\" rotor=%d version=%04x",
			pnwatch->url, pnwatch->watch_desc, pnwatch->needs_rotor,
			pnwatch->version);

	return OK;
}


/*
 * handle incoming packets
 */
oret_t
ohubWatchHandleProtocol(OhubNetWatch_t * pnwatch, int kind,
						int type, int len, char *buf)
{
	ushort_t reply[8];
	oret_t rc;
	bool_t  f;

	switch (type) {
	case OLANG_PING_REQ:
		if (!ohub_watchers_enable) {
			ohubLog(8, "ping reply disabled");
			return OK;
		}
		pnwatch->version = (ushort_t) ONTOHS(*(ushort_t *) buf);
		if (pnwatch->version != OLANG_VERSION) {
			ohubLog(6, "incompatible watch [%s] version=%04x",
					pnwatch->url, pnwatch->version);
			return OK;
		}
		reply[0] = OHTONS(pnwatch->version);
		reply[1] = 0;
		rc = ohubWatchSendPacket(pnwatch, OLANG_CONTROL, OLANG_PING_ACK,
								4, reply);
		ohubLog(8, "reply to ping rc=%d version=%04x", rc, pnwatch->version);
		return OK;
	case OLANG_CONN_REQ:
		return ohubWatchConnect(pnwatch, kind, type, len, buf);
	case OLANG_ALIVE:
		ohubLog(9, "got heartbeat from [%s]", pnwatch->url);
		return OK;
	case OLANG_BLOCK_ANNOUNCE:
		f = (len > 0 ? (pnwatch->announce_blocked = buf[0] != 0)
					: pnwatch->announce_blocked);
		ohubLog(7, "announce block = %d for [%s]",
				pnwatch->announce_blocked, pnwatch->url);
		if (pnwatch->announce_blocked && !f) {
			if (len > 1 && buf[1] != 0)
				ohubMonRenewWatch(pnwatch, pnwatch->url);
		}
		return OK;
	default:
		return ohubWatchHandleIncoming(pnwatch, pnwatch->watch_id,
								pnwatch->watch_desc, kind, type, len, buf);
	}
}


bool_t
ohubWatchAnnounceBlocked(struct OhubNetWatch_st * pnwatch)
{
	return pnwatch ? pnwatch->announce_blocked : TRUE;
}


bool_t
ohubWatchNeedsRotor(struct OhubNetWatch_st * pnwatch)
{
	return pnwatch ? pnwatch->needs_rotor : FALSE;
}


int
ohubWatchSendQueueGap(struct OhubNetWatch_st *pnwatch)
{
	return pnwatch ? pnwatch->q_size - pnwatch->q_len : -1;
}


OhubMonAccum_t *
ohubWatchGetAccum(struct OhubNetWatch_st * pnwatch)
{
	return &pnwatch->accum;
}
