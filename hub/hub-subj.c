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

  Implementation of the network interaction with back-end agents.

*/

#include "hub-priv.h"
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for OMAX,ONTOHS,nonblocking... */
#include <optikus/conf-mem.h>	/* for oxnew,oxbcopy,... */

#include <unistd.h>			/* for close */
#include <stdlib.h>			/* for atoi */
#include <errno.h>			/* for errno,EPIPE */
#include <string.h>			/* for strncmp,... */
#include <netinet/tcp.h>	/* for TCP_NODELAY */


/* TCP/IP connection */

int     ohub_subj_cntl_sock;
struct sockaddr_in ohub_subj_cntl_addr;

bool_t  ohub_subj_is_polling;
long    ohub_subj_routine_ms;
int     ohub_subj_can_immed = 1;	/* 0 = never send packets immediately
									 * 1 = packets less than 800 b
									 * 2 = send any packet immediately first
									 */

bool_t  ohub_subjs_enable;

#define OHUB_SUBJ_Q_SIZE     2500

int     ohub_subj_init_q_size = OHUB_SUBJ_Q_SIZE;

typedef struct
{
	int     type;
	int     len;
	char   *buf;
} OhubQueuedSubjPacket_t;

#define OHUB_SUBJ_NONE  0
#define OHUB_SUBJ_OPEN  1
#define OHUB_SUBJ_WAIT  2
#define OHUB_SUBJ_EXIT  3

typedef struct OhubNetSubj_st
{
	int     subj_state;
	ushort_t subj_id;
	bool_t  needs_rotor;
	int     data_sock;
	struct sockaddr_in cntl_addr;
	struct sockaddr_in data_addr;
	struct sockaddr_in cmnd_addr;
	int     ping_cur_ms;
	long    ping_wd;
	long    alive_get_wd;
	long    alive_put_wd;
	OhubQueuedSubjPacket_t *q_buf;
	int     q_len;
	int     q_size;
	int     q_rd;
	int     q_wr;
	char    hdr_buf[16];
	int     hdr_off;
	int     reget_type;
	int     reget_len;
	int     reget_off;
	char   *reget_buf;
	int     reput_type;
	int     reput_len;
	int     reput_off;
	char   *reput_buf;
	char    url[OHUB_MAX_NAME];
	OhubAgent_t *pagent;
	OhubUnregAccum_t unreg_accum;
} OhubNetSubj_t;

#define OHUB_MAX_SUBJ   32

OhubNetSubj_t *ohub_subjs;
OhubNetSubj_t ohub_cntl_subj;
ushort_t ohub_next_subj_id;


/*
 *  initiate connection with subj
 */
OhubNetSubj_t *
ohubSubjOpen(const char *subj_url, OhubAgent_t * pagent)
{
	char    host[OHUB_MAX_NAME];
	int     port;
	const char *s;
	int     i, n;
	OhubNetSubj_t *pnsubj;

	if (NULL == subj_url || '\0' == *subj_url)
		return NULL;

	/* find idle record */
	if (NULL == ohub_subjs)
		return NULL;
	pnsubj = &ohub_subjs[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_SUBJ; i++, pnsubj++)
		if (pnsubj->subj_state == OHUB_SUBJ_NONE)
			break;
	if (i >= OHUB_MAX_SUBJ)
		return NULL;

	/* allocate send queue */
	pnsubj->q_size = ohub_subj_init_q_size;
	pnsubj->q_buf = oxnew(OhubQueuedSubjPacket_t, pnsubj->q_size);
	if (NULL == pnsubj->q_buf)
		goto FAIL;
	pnsubj->q_len = pnsubj->q_rd = pnsubj->q_wr = 0;

	/* parse URL */
	strcpy(pnsubj->url, subj_url);
	s = pnsubj->url;
	while (*s && *s == ' ')
		s++;
	if (0 == strncmp(s, "olang://", 8))
		s += 8;
	for (n = 0; s[n] && s[n] != ':'; n++);
	if (n >= sizeof(host) || n <= 0)
		goto FAIL;
	memcpy(host, s, n);
	host[n] = 0;
	s += n;
	port = 0;
	if (*s == ':' && s[1])
		port = atoi(s + 1);
	if (port < 0 || port > 65534)
		goto FAIL;
	if (port == 0)
		port = OLANG_SUBJ_PORT;

	/* create control address */
	oxvzero(&pnsubj->cntl_addr);
	if (inetResolveHostName(host, & pnsubj->cntl_addr) < 0) {
		ohubLog(2, "unknown host %s", host);
		goto FAIL;
	}
	pnsubj->cntl_addr.sin_port = htons(port);

	/* create other addresses */
	oxvcopy(&pnsubj->cntl_addr, &pnsubj->data_addr);
	pnsubj->data_addr.sin_port = htons(port + 1);
	oxvcopy(&pnsubj->cntl_addr, &pnsubj->cmnd_addr);
	pnsubj->cmnd_addr.sin_port = htons(port + 1);

	/* reget/reput buffers */
	pnsubj->hdr_off = 0;
	pnsubj->reget_buf = pnsubj->reput_buf = NULL;
	pnsubj->reget_len = pnsubj->reget_off = pnsubj->reget_type = -1;
	pnsubj->reput_len = pnsubj->reput_off = pnsubj->reput_type = -1;
	oxvzero(&pnsubj->unreg_accum);

	/* initiate asynchronous connection */
	pnsubj->subj_state = OHUB_SUBJ_WAIT;
	if ((++ohub_next_subj_id & 0xFF) == 0)
		++ohub_next_subj_id;
	pnsubj->subj_id = (i & 0xFF) | ((ohub_next_subj_id & 0xFF) << 8);
	pnsubj->data_sock = 0;
	pnsubj->alive_get_wd = pnsubj->alive_put_wd = 0;
	pnsubj->ping_cur_ms = OLANG_PING_MIN_MS;
	pnsubj->ping_wd = 0;
	pnsubj->pagent = pagent;
	pnsubj->needs_rotor = FALSE;
	return pnsubj;

  FAIL:
	ohubSubjClose(pnsubj);
	return NULL;
}


/*
 *  close data connection with subj
 */
oret_t
ohubSubjDisconnect(OhubNetSubj_t * pnsubj)
{
	int     i;
	OhubQueuedSubjPacket_t *pkt;

	/* notify agent */
	if (NULL != pnsubj->pagent) {
		ohubSetAgentState(pnsubj->pagent, "");
	}

	/* clear send queue */
	if (pnsubj->q_buf) {
		for (i = 0; i < pnsubj->q_len; i++) {
			pkt = &pnsubj->q_buf[i];
			oxfree(pkt->buf);
			pkt->buf = NULL;
			pkt->len = pkt->type = 0;
		}
	}
	pnsubj->q_rd = pnsubj->q_wr = pnsubj->q_len = 0;

	/* clear packet to be reget */
	pnsubj->hdr_off = 0;
	oxfree(pnsubj->reget_buf);
	pnsubj->reget_buf = NULL;
	pnsubj->reget_len = pnsubj->reget_off = pnsubj->reget_type = -1;

	/* clear packet to be reput */
	oxfree(pnsubj->reput_buf);
	pnsubj->reput_buf = NULL;
	pnsubj->reput_len = pnsubj->reput_len = pnsubj->reput_type = -1;
	oxvzero(&pnsubj->unreg_accum);

	/* close data socket */
	if (pnsubj->data_sock > 0)
		close(pnsubj->data_sock);
	pnsubj->data_sock = 0;
	pnsubj->subj_state = OHUB_SUBJ_WAIT;

	if (NULL != pnsubj->pagent) {
		/* FIXME: duplicates the code above */
		ohubSetAgentState(pnsubj->pagent, "");
	}

	return OK;
}


/*
 *  deallocate subj structure
 */
oret_t
ohubSubjClose(OhubNetSubj_t * pnsubj)
{
	/* close sockets */
	ohubSubjDisconnect(pnsubj);

	/* free the queue */
	oxfree(pnsubj->q_buf);
	pnsubj->q_buf = NULL;
	pnsubj->q_size = 0;

	/* clear variables */
	pnsubj->url[0] = 0;
	oxvzero(&pnsubj->cntl_addr);
	oxvzero(&pnsubj->data_addr);
	oxvzero(&pnsubj->cmnd_addr);
	pnsubj->ping_cur_ms = 0;
	pnsubj->ping_wd = pnsubj->alive_get_wd = pnsubj->alive_put_wd = 0;
	pnsubj->subj_state = OHUB_SUBJ_NONE;
	pnsubj->subj_id = 0;
	pnsubj->pagent = NULL;
	pnsubj->needs_rotor = FALSE;

	return OK;
}


/*
 *  initialize subj structures
 */
oret_t
ohubSubjInitNetwork()
{
	OhubNetSubj_t *pnsubj;
	int     i, rc;
	socklen_t addr_len;

	/* cleanup */
	ohubSubjShutdownNetwork();
	ohub_next_subj_id = (ushort_t) osMsClock();

	/* prepare subj structures */
	if (NULL == (ohub_subjs = oxnew(OhubNetSubj_t, OHUB_MAX_SUBJ)))
		goto FAIL;
	pnsubj = &ohub_subjs[0];
	for (i = 0; i < OHUB_MAX_SUBJ; i++, pnsubj++) {
		oxvzero(pnsubj);
		pnsubj->subj_state = OHUB_SUBJ_NONE;
		pnsubj->reget_len = pnsubj->reget_off = pnsubj->reget_type = -1;
		pnsubj->reput_len = pnsubj->reput_off = pnsubj->reput_type = -1;
	}

	/* create UDP control socket */
	ohub_subj_cntl_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (ohub_subj_cntl_sock == ERROR)
		goto FAIL;
	oxvzero(&ohub_subj_cntl_addr);
	ohub_subj_cntl_addr.sin_port = 0;
	ohub_subj_cntl_addr.sin_family = AF_INET;
	ohub_subj_cntl_addr.sin_addr.s_addr = INADDR_ANY;
	addr_len = sizeof(ohub_subj_cntl_addr);
	rc = bind(ohub_subj_cntl_sock, (struct sockaddr *) &ohub_subj_cntl_addr,
			  addr_len);
	if (rc == ERROR)
		goto FAIL;
	rc = getsockname(ohub_subj_cntl_sock,
					 (struct sockaddr *) &ohub_subj_cntl_addr, &addr_len);
	if (rc == ERROR)
		goto FAIL;
	if (setNonBlocking(ohub_subj_cntl_sock) == ERROR)
		goto FAIL;
	/* setup variables */
	strcpy(ohub_cntl_subj.url, "{CNTL}");
	return OK;

  FAIL:
	ohubSubjShutdownNetwork();
	return ERROR;
}


/*
 *  close connection with all subj's
 */
oret_t
ohubSubjShutdownNetwork()
{
	int     i;

	ohub_subjs_enable = FALSE;
	if (NULL != ohub_subjs) {
		for (i = 0; i < OHUB_MAX_SUBJ; i++)
			ohubSubjClose(&ohub_subjs[i]);
	}
	oxfree(ohub_subjs);
	ohub_subjs = NULL;

	/* close sockets */
	if (ohub_subj_cntl_sock > 0)
		close(ohub_subj_cntl_sock);
	ohub_subj_cntl_sock = 0;

	/* clear variables */
	oxvzero(&ohub_cntl_subj);
	return OK;
}


/*
 * check sum
 */
static ushort_t
ohubSubjCheckSum(char *buf, int nbytes)
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
ohubSubjPokeStream(OhubNetSubj_t * pnsubj, int type, int len, void *dp)
{
	char   *buf;
	int     n, blen;
	ushort_t *wptr;
	ushort_t cksum;

	if (pnsubj->reput_len >= 0) {
		/* retry */
		blen = pnsubj->reput_len - pnsubj->reput_off;
		if (blen > 0)
			n = send(pnsubj->data_sock, pnsubj->reput_buf + pnsubj->reput_off,
					 blen, O_MSG_NOSIGNAL);
		else
			n = blen = 0;
		pnsubj->reput_off += n;
		if (n < blen)
			return ERROR;
		ohubLog(9, "reput TCP type=%04xh len=%d id=%04xh {%s} OK",
				type, len, pnsubj->subj_id, pnsubj->url);
		oxfree(pnsubj->reput_buf);
		pnsubj->reput_buf = NULL;
		pnsubj->reput_off = pnsubj->reput_len = pnsubj->reput_type = -1;
		pnsubj->alive_put_wd = osMsClock();
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
	wptr[2] = OHTONS(pnsubj->subj_id);
	oxbcopy(dp, buf + OLANG_HEAD_LEN, len);
	cksum = ohubSubjCheckSum(buf, len + OLANG_HEAD_LEN);
	*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
	if (blen > 0)
		n = send(pnsubj->data_sock, (void *) buf, blen, O_MSG_NOSIGNAL);
	else
		n = blen = 0;
	if (n == blen) {
		oxfree(buf);
		pnsubj->alive_put_wd = osMsClock();
		ohubLog(9, "sent TCP type=%04xh len=%d {%s} OK", type, len, pnsubj->url);
		return OK;
	}

	/* send error */
	if (errno == EPIPE) {
		ohubLog(4, "broken subj stream {%s} - broken pipe len=%d got=%d",
				pnsubj->url, blen, n);
		pnsubj->subj_state = OHUB_SUBJ_EXIT;
		oxfree(buf);
		return ERROR;
	}
	if (n > 0 && n < OLANG_HEAD_LEN) {
		/* FIXME: not good, the situation is possible under high loads ! */
		ohubLog(4, "broken subj stream {%s} - pipe overflow len=%d got=%d",
				pnsubj->url, blen, n);
		pnsubj->subj_state = OHUB_SUBJ_EXIT;
		oxfree(buf);
		return ERROR;
	}

	if (n <= 0) {
		oxfree(buf);
		return ERROR;
	}

	/* retry later */
	pnsubj->reput_buf = buf;
	pnsubj->reput_off = n;
	pnsubj->reput_len = blen;
	pnsubj->reput_type = type;

	return OK;
}


/*
 *  retry sending queued packets
 */
static void
ohubSubjResendStream(OhubNetSubj_t * pnsubj)
{
	OhubQueuedSubjPacket_t *pkt;

	for (;;) {
		if (pnsubj->reput_len >= 0) {
			if (ohubSubjPokeStream(pnsubj, -1, -1, NULL) != OK)
				break;
		}
		if (!pnsubj->q_len)
			break;
		pkt = &pnsubj->q_buf[pnsubj->q_rd];
		if (ohubSubjPokeStream(pnsubj, pkt->type, pkt->len, pkt->buf) != OK) {
			ohubLog(9,
				"subj unqueuing ERR TCP type=%04xh len=%d qrd=%d qlen=%d {%s}",
				pkt->type, pkt->len, pnsubj->q_rd, pnsubj->q_len, pnsubj->url);
			break;
		}
		if (++pnsubj->q_rd == pnsubj->q_size)
			pnsubj->q_rd = 0;
		pnsubj->q_len--;
		ohubLog(9,
				"subj unqueuing OK TCP type=%04xh len=%d qrd=%d qlen=%d {%s}",
				pkt->type, pkt->len, pnsubj->q_rd, pnsubj->q_len, pnsubj->url);
		pkt->type = pkt->len = 0;
		oxfree(pkt->buf);
		pkt->buf = NULL;
		if (pnsubj->q_len == 0 || pnsubj->reput_len >= 0)
			break;
	}
}


/*
 *  immediately or later send packet to hub
 */
oret_t
ohubSubjSendPacket(OhubNetSubj_t * pnsubj, int kind, int type, int len, void *data)
{
	OhubQueuedSubjPacket_t *pkt;
	ushort_t *wptr, cksum;
	char   *buf, small[16];
	int     blen, num;
	struct sockaddr_in *udp_addr_p;

	if (len < 0 || (len > 0 && !data))
		return ERROR;

	switch (kind) {
	case OLANG_CONTROL:
		udp_addr_p = &pnsubj->cntl_addr;
		break;
	case OLANG_COMMAND:
		udp_addr_p = &pnsubj->cmnd_addr;
		break;
	case OLANG_DATA:
		udp_addr_p = NULL;
		break;
	default:
		return ERROR;
	}

	if (udp_addr_p) {
		/* send a datagram */
		blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
		if (blen & 1)
			blen++;
		if (blen < sizeof(small))
			buf = small;
		else if (NULL == (buf = oxnew(char, blen)))
			return ERROR;

		wptr = (ushort_t *) buf;
		wptr[0] = OHTONS(len);
		wptr[1] = OHTONS(type);
		wptr[2] = OHTONS(pnsubj->subj_id);
		oxbcopy(data, buf + OLANG_HEAD_LEN, len);
		cksum = ohubSubjCheckSum(buf, len + OLANG_HEAD_LEN);
		*(ushort_t *) (buf + blen - OLANG_TAIL_LEN) = OHTONS(cksum);
		num = sendto(ohub_subj_cntl_sock, (void *) buf, blen, 0,
					 (struct sockaddr *) udp_addr_p,
					 sizeof(struct sockaddr_in));
		if (buf != small)
			oxfree(buf);
		if (num == blen) {
			/*owatch_alive_put_wd = osMsClock(); */
			ohubLog(9, "send UDP kind=%d type=%04xh len=%d id=%04xh {%s} OK",
					kind, type, len, pnsubj->subj_id, pnsubj->url);
			return OK;
		}
		ohubLog(5, "cannot UDP kind=%d type=%04xh len=%d id=%04xh"
				" {%s} blen=%d num=%d errno=%d",
				kind, type, len, pnsubj->subj_id, pnsubj->url, blen,
				num, errno);
		return ERROR;
	}

	/* send to stream */
	if (!pnsubj->q_len && pnsubj->reput_len < 0
		&& ohub_subj_can_immed
		&& (len < OLANG_SMALL_PACKET || ohub_subj_can_immed > 1)) {
		if (ohubSubjPokeStream(pnsubj, type, len, data) == OK) {
			pnsubj->alive_put_wd = osMsClock();
			return OK;
		}
	}

	if (pnsubj->q_len == pnsubj->q_size) {
		ohubLog(3,
			"send queue overflow subj {%s} q_len=%d q_size=%d type=%04x len=%d",
			pnsubj->url, pnsubj->q_len, pnsubj->q_size, type, len);
		return ERROR;
	}

	pkt = &pnsubj->q_buf[pnsubj->q_wr];
	if (pkt->buf) {
		ohubLog(6, "hanging subj {%s} pkt type=%04x len=%d qwr=%d qlen=%d",
				pnsubj->url, pkt->type, pkt->len, pnsubj->q_wr, pnsubj->q_len);
		oxfree(pkt->buf);
		pkt->buf = NULL;
	}
	if (len == 0)
		pkt->buf = NULL;
	else {
		if (NULL == (pkt->buf = oxnew(char, len))) {
			ohubLog(2, "cannot allocate %d bytes for TCP send to {%s}",
					len, pnsubj->url);
			return ERROR;
		}
		oxbcopy(data, pkt->buf, len);
	}

	pkt->type = type;
	pkt->len = len;

	if (++pnsubj->q_wr == pnsubj->q_size)
		pnsubj->q_wr = 0;
	++pnsubj->q_len;
	ohubLog(9, "subj queued TCP type=%04xh len=%d qwr=%d qlen=%d {%s}",
			type, len, pnsubj->q_wr, pnsubj->q_len, pnsubj->url);

	return OK;
}


/*
 *  receive stream from TCP socket
 */
static void
ohubSubjReceiveStream(OhubNetSubj_t * pnsubj)
{
	int     num, len;
	ushort_t cksum, cksum2, id, csbuf[8], *hbuf;
	bool_t  first_pass = TRUE;

	/* get packet from TCP */
	for (;;) {
		/* if there was partial packet */
		if (pnsubj->reget_len >= 0) {
			len = pnsubj->reget_len + OLANG_TAIL_LEN - pnsubj->reget_off;
			if (len & 1)
				len++;
			ohubLog(9, "trying to reget off=%d len=%d from {%s}",
					pnsubj->reget_off, pnsubj->reget_len, pnsubj->url);
			num = recv(pnsubj->data_sock, pnsubj->reget_buf + pnsubj->reget_off,
						len, O_MSG_NOSIGNAL);
			if (num <= 0) {
				ohubLog(4, "broken subj stream {%s} - null reget "
						"type=%04x len=%d num=%d err=%d (%s)",
						pnsubj->url, pnsubj->reget_type, pnsubj->reget_len,
						num, errno, strerror(errno));
				pnsubj->subj_state = OHUB_SUBJ_EXIT;
				break;
			}
			first_pass = FALSE;
			if (num != len) {
				pnsubj->reget_off += num;
				break;
			}
			cksum = *(ushort_t *) (pnsubj->reget_buf + len - OLANG_TAIL_LEN);
			cksum = ONTOHS(cksum);
		} else {
			/* check for new packet */
			len = OLANG_HEAD_LEN - pnsubj->hdr_off;
			num = recv(pnsubj->data_sock, (pnsubj->hdr_buf + pnsubj->hdr_off),
						len, O_MSG_NOSIGNAL);
			if (num > 0)
				pnsubj->hdr_off += num;
			ohubLog(9, "peek at stream {%s} off=%d len=%d num=%d first=%d",
					pnsubj->url, pnsubj->hdr_off, len, num, first_pass);
			if (num != len) {
				if (first_pass) {
					ohubLog(4, "broken subj stream {%s} - null peek "
							"off=%d len=%d num=%d errno=%d",
							pnsubj->url, pnsubj->hdr_off, len, num, errno);
					if (num <= 0) {
						pnsubj->subj_state = OHUB_SUBJ_EXIT;
						pnsubj->hdr_off = 0;
					}
				}
				break;
			}
			pnsubj->hdr_off = 0;
			first_pass = FALSE;
			hbuf = (ushort_t *) pnsubj->hdr_buf;
			pnsubj->reget_len = (ushort_t) ONTOHS(hbuf[0]);
			pnsubj->reget_off = 0;
			pnsubj->reget_type = (ushort_t) ONTOHS(hbuf[1]);
			id = ONTOHS(hbuf[2]);
			if (id != pnsubj->subj_id) {
				ohubLog(2, "wrong id %04xh (should be %04xh) from {%s}",
						id, pnsubj->subj_id, pnsubj->url);
			}
			if (pnsubj->reget_len > OLANG_MAX_PACKET) {
				ohubLog(4, "broken subj stream {%s} - huge type=%04x len=%d",
						pnsubj->url, pnsubj->reget_type, pnsubj->reget_len);
				pnsubj->subj_state = OHUB_SUBJ_EXIT;
				break;
			}
			if (!pnsubj->reget_len) {
				/* get checksum */
				num = 0;
				pnsubj->reget_buf = NULL;
				num = recv(pnsubj->data_sock, (void *) csbuf, OLANG_TAIL_LEN,
						   MSG_PEEK | O_MSG_NOSIGNAL);
				if (num != OLANG_TAIL_LEN)
					break;
				num = recv(pnsubj->data_sock, (void *) csbuf, OLANG_TAIL_LEN,
						   O_MSG_NOSIGNAL);
				cksum = csbuf[0];
				cksum = ONTOHS(cksum);
			} else {
				/* get data and checksum */
				len = pnsubj->reget_len + OLANG_TAIL_LEN - pnsubj->reget_off;
				if (len & 1)
					len++;

				if (NULL == (pnsubj->reget_buf = oxnew(char, len))) {
					/* not enough memory */
					ohubLog(2, "cannot allocate length %d for {%s}",
							len, pnsubj->url);
					break;
				}
				num = recv(pnsubj->data_sock, pnsubj->reget_buf,
							len, O_MSG_NOSIGNAL);
				if (num <= 0) {
					pnsubj->subj_state = OHUB_SUBJ_EXIT;
					ohubLog(4,
						"broken subj stream {%s} - at %d type=%04x len=%d",
						pnsubj->url, len, pnsubj->reget_type, pnsubj->reget_len);
					break;
				}
				if (num != len) {
					pnsubj->reget_off = len;
					break;
				}
				cksum = *(ushort_t *) (pnsubj->reget_buf + len - OLANG_TAIL_LEN);
				cksum = ONTOHS(cksum);
			}
		}
		/* if we are here then the packet is ready */
		cksum2 =
			ohubSubjCheckSum(pnsubj->reget_buf,
							 pnsubj->reget_len + OLANG_HEAD_LEN);
		if (cksum2 != cksum) {
			ohubLog(4,
				"broken subj stream {%s} - wrong checksum type=%04x len=%d",
				pnsubj->url, pnsubj->reget_type, pnsubj->reget_len);
			pnsubj->subj_state = OHUB_SUBJ_EXIT;
			break;
		}
		pnsubj->alive_get_wd = osMsClock();
		ohubSubjHandleProtocol(pnsubj, OLANG_DATA,
					pnsubj->reget_type, pnsubj->reget_len, pnsubj->reget_buf);
		oxfree(pnsubj->reget_buf);
		pnsubj->reget_buf = NULL;
		pnsubj->reget_type = pnsubj->reget_len = pnsubj->reget_off = -1;
		/* go on to the next packet */
	}
	if (pnsubj->subj_state == OHUB_SUBJ_EXIT) {
		oxfree(pnsubj->reget_buf);
		pnsubj->reget_buf = NULL;
		pnsubj->reget_type = pnsubj->reget_len = pnsubj->reget_off = -1;
	}
}


/*
 *  send ping to check hub availability
 */
static void
ohubSubjPing(OhubNetSubj_t * pnsubj)
{
	int     interval = pnsubj->ping_cur_ms;
	long    now = osMsClock();
	long    delta = now - pnsubj->ping_wd;
	int     len;
	ushort_t data[2];
	oret_t rc;

	if (!ohub_subjs_enable)
		return;
	if (delta >= 0 && delta < interval && pnsubj->ping_wd)
		return;
	pnsubj->ping_wd = now;
	interval =
		interval * OLANG_PING_COEF2 / OLANG_PING_COEF1 + OLANG_PING_COEF0;
	if (interval > OLANG_PING_MAX_MS)
		interval = OLANG_PING_MAX_MS;
	pnsubj->ping_cur_ms = interval;
	len = 4;					/* 4 bytes of data inside */
	data[0] = OHTONS(OLANG_VERSION);
	data[1] = 0;				/* reserved */
	rc = ohubSubjSendPacket(pnsubj, OLANG_CONTROL, OLANG_PING_REQ,
							sizeof(data), data);
	ohubLog(8, "send ping rc=%d interval=%d to {%s}",
			rc, interval, pnsubj->url);
}


/*
 * receive a datagram
 */
static void
ohubSubjReceiveDatagram(OhubNetSubj_t * pnsubj)
{
	int     n, len, type, blen;
	ushort_t hdr[10], cksum, id;
	char   *buf;
	struct sockaddr_in subj_cntl_addr;
	socklen_t addr_len = sizeof(subj_cntl_addr);

	n = recvfrom(ohub_subj_cntl_sock, (void *) hdr, OLANG_HEAD_LEN, MSG_PEEK,
				 (struct sockaddr *) &subj_cntl_addr, &addr_len);
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
		n = id & 255;
		if (n < 1 || n >= OHUB_MAX_SUBJ) {
			/* wrong subj number */
			ohubLog(2, "wrong subj id=%04xh, subj_no=%d invalid", id, n);
			goto REMOVE;
		}
		pnsubj = &ohub_subjs[n];
		if (pnsubj->subj_state != OHUB_SUBJ_WAIT
			&& pnsubj->subj_state != OHUB_SUBJ_OPEN) {
			ohubLog(2, "wrong subj state id=%04xh, subj_no=%d", id, n);
			goto REMOVE;
		}
		if (id != pnsubj->subj_id) {
			ohubLog(2, "wrong subj id %04xh (should be %04xh) from {%s}",
					id, pnsubj->subj_id, pnsubj->url);
		}
	}
	blen = len + OLANG_HEAD_LEN + OLANG_TAIL_LEN;
	if (blen & 1)
		blen++;

	if (NULL == (buf = oxnew(char, blen))) {
		/* not enough memory */
		goto REMOVE;
	}
	n = recvfrom(ohub_subj_cntl_sock, buf, blen, 0, NULL, NULL);
	if (n != blen) {
		/* data receive error */
		goto END;
	}
	cksum = *(ushort_t *) (buf + blen - OLANG_TAIL_LEN);
	cksum = ONTOHS(cksum);
	if (cksum != ohubSubjCheckSum(buf, blen - OLANG_TAIL_LEN)) {
		/* checksum fail */
		goto END;
	}
	pnsubj->alive_get_wd = osMsClock();
	ohubLog(9, "handle datagram from {%s} type=%04xh len=%d id=%04xh",
			pnsubj->url, type, len, id);
	ohubSubjHandleProtocol(pnsubj, OLANG_CONTROL, type,
							len, buf + OLANG_HEAD_LEN);
  END:
	oxfree(buf);
	return;
  REMOVE:
	/* remove datagram from system buffer */
	recvfrom(ohub_subj_cntl_sock, (char *) hdr, OLANG_HEAD_LEN, 0, NULL, NULL);
	return;
}


/*
 * handle ping reply
 */
static oret_t
ohubSubjHandlePingReply(OhubNetSubj_t * pnsubj, int kind, int type, int len,
						char *data)
{
	int     rc, n, on;
	ushort_t *wptr = (ushort_t *) data;
	char    buf[OHUB_MAX_PATH + 12];

	/* check packet validity */
	if ((type != OLANG_PING_ACK && type != OLANG_PING_NAK)
		|| len != 4 || kind != OLANG_CONTROL
		|| wptr[0] != OHTONS(OLANG_VERSION)) {
		/* packet is wrong */
		return ERROR;
	}
	if (type == OLANG_PING_NAK) {
		ohubLog(2, "subj {%s} is already connected to hub", pnsubj->url);
		pnsubj->ping_cur_ms = OLANG_PING_MAX_MS;
		return OK;
	}
	pnsubj->ping_cur_ms = OLANG_PING_MIN_MS;
	/* open TCP connection */
	if (pnsubj->subj_state == OHUB_SUBJ_OPEN)
		return OK;
	pnsubj->data_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (pnsubj->data_sock == ERROR) {
		pnsubj->data_sock = 0;
		return ERROR;
	}
	if (connect(pnsubj->data_sock,
				(struct sockaddr *) &pnsubj->data_addr,
				sizeof(pnsubj->data_addr)) == ERROR) {
		goto FAIL;
	}

	/* set socket options */
	setNonBlocking(pnsubj->data_sock);
	on = 1;
	setsockopt(pnsubj->data_sock, IPPROTO_TCP, TCP_NODELAY, (void *) &on,
			   sizeof(on));
	/* send connection packet */
	*(ushort_t *) (buf + 0) = OHTONS(OLANG_VERSION);
	*(ushort_t *) (buf + 2) = ohub_subj_cntl_addr.sin_port;
	*(ulong_t *) (buf + 4) = OLANG_MAGIC;
	n = strlen(ohub_hub_desc);
	buf[8] = (uchar_t) n;
	pnsubj->subj_state = OHUB_SUBJ_OPEN;
	strcpy(buf + 9, ohub_hub_desc);
	rc = ohubSubjSendPacket(pnsubj, OLANG_DATA, OLANG_CONN_REQ, 9 + n, buf);
	if (rc != OK)
		goto FAIL;
	pnsubj->alive_get_wd = pnsubj->alive_put_wd = osMsClock();
	return OK;
  FAIL:
	pnsubj->subj_state = OHUB_SUBJ_WAIT;
	if (pnsubj->data_sock > 0)
		close(pnsubj->data_sock);
	pnsubj->data_sock = 0;
	return ERROR;
}


/*
 * check hub availability; send alive messages
 */
void
ohubSubjBackgroundTask(void)
{
	long    now = osMsClock();
	OhubNetSubj_t *pnsubj = &ohub_subjs[0];
	int     i;

	for (i = 0; i < OHUB_MAX_SUBJ; i++, pnsubj++) {
		if (pnsubj->subj_state == OHUB_SUBJ_OPEN) {
			/* check hub availability */
			if (now - pnsubj->alive_get_wd > OLANG_DEAD_MS) {
				ohubLog(4, "broken subj stream {%s} - dead", pnsubj->url);
				pnsubj->subj_state = OHUB_SUBJ_EXIT;
			}
		}
		if (pnsubj->subj_state == OHUB_SUBJ_OPEN) {
			if (now - pnsubj->alive_put_wd > OLANG_ALIVE_MS) {
				/* free up hanging packets */
				int     i, qty;

				for (qty = i = 0; i < pnsubj->q_size; i++) {
					OhubQueuedSubjPacket_t *pkt = &pnsubj->q_buf[i];

					if (pkt->len <= 0 && pkt->buf != 0) {
						oxfree(pkt->buf);
						pkt->buf = NULL;
						qty++;
					}
				}
				if (qty > 0) {
					ohubLog(6, "subj {%s} freed up %d hanging packets",
							pnsubj->url, qty);
				}
				/* send alive messages */
				ohubLog(9, "send heartbeat to {%s}", pnsubj->url);
				ohubSubjSendPacket(pnsubj, OLANG_CONTROL, OLANG_ALIVE, 0, NULL);
				pnsubj->alive_put_wd = now;
				/* FIXME: SubjSendPacket should reset the timout */
			}
		}
	}
}


int
ohubSubjPrepareEvents(fd_set * set, int nfds)
{
	OhubNetSubj_t *pnsubj;
	int     i;
	long    now = osMsClock();

	FD_SET(ohub_subj_cntl_sock, set + 0);
	nfds = OMAX(nfds, ohub_subj_cntl_sock);
	pnsubj = &ohub_subjs[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_SUBJ; i++, pnsubj++) {
		if (pnsubj->unreg_accum.qty > 0
			&& now - pnsubj->unreg_accum.start_ms > OHUB_UNREG_ACCUM_TTL)
			ohubMonFlushUnregAccum(pnsubj, &pnsubj->unreg_accum);
		if (pnsubj->subj_state == OHUB_SUBJ_WAIT) {
			ohubSubjPing(pnsubj);
		}
		if (pnsubj->subj_state == OHUB_SUBJ_OPEN) {
			FD_SET(pnsubj->data_sock, set + 0);
			nfds = OMAX(nfds, pnsubj->data_sock);
			if (pnsubj->q_len || pnsubj->reput_len >= 0) {
				FD_SET(pnsubj->data_sock, set + 1);
			}
		}
	}
	return nfds;
}


int
ohubSubjHandleEvents(fd_set * set, int num)
{
	OhubNetSubj_t *pnsubj;
	int     n_events = 0;
	int     i;

	/* handle events */
	if (FD_ISSET(ohub_subj_cntl_sock, set + 0)) {
		ohubLog(9, "got some subj datagram");
		ohubSubjReceiveDatagram(&ohub_cntl_subj);
		n_events++;
	}
	pnsubj = &ohub_subjs[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_SUBJ; i++, pnsubj++) {
		if (pnsubj->subj_state == OHUB_SUBJ_NONE ||
			pnsubj->subj_state == OHUB_SUBJ_EXIT)
			continue;
		if (FD_ISSET(pnsubj->data_sock, set + 0)) {
			ohubLog(9, "got some packet for {%s}", pnsubj->url);
			ohubSubjReceiveStream(pnsubj);
			n_events++;
		}
		if (pnsubj->subj_state == OHUB_SUBJ_EXIT)
			continue;
		if (FD_ISSET(pnsubj->data_sock, set + 1)) {
			ohubLog(9, "resend some packet to {%s}", pnsubj->url);
			ohubSubjResendStream(pnsubj);
			n_events++;
		}
	}
	return n_events;
}


void
ohubSubjLazyShutdown()
{
	OhubNetSubj_t *pnsubj;
	int     i;

	pnsubj = &ohub_subjs[1];		/* zero descriptor reserved */
	for (i = 1; i < OHUB_MAX_SUBJ; i++, pnsubj++) {
		if (pnsubj->subj_state != OHUB_SUBJ_EXIT)
			continue;
		ohubSubjDisconnect(pnsubj);
		pnsubj->subj_state = OHUB_SUBJ_WAIT;
		ohubLog(4, "disconnected subj {%s}", pnsubj->url);
		ohubSubjPing(pnsubj);
	}
}


/*
 * Finalize connection
 */
oret_t
ohubSubjConnect(OhubNetSubj_t * pnsubj, int kind, int type, int len, char *buf)
{
	ulong_t magic;

	if (len != 4)
		magic = 0;
	else
		magic = *(ulong_t *) buf;
	if (magic == OLANG_MAGIC)
		pnsubj->needs_rotor = FALSE;
	else if (magic == OROTL(OLANG_MAGIC))
		pnsubj->needs_rotor = TRUE;
	else {
		ohubLog(3, "bad connection acknowledge from {%s} "
				"magic=%lxh host=%lxh net=%lxh",
				pnsubj->url, magic, OLANG_MAGIC, OROTL(OLANG_MAGIC));
		pnsubj->subj_state = OHUB_SUBJ_EXIT;
		return OK;
	}
	ohubLog(4, "connected to subj {%s} rotor=%d",
			pnsubj->url, pnsubj->needs_rotor);
	if (NULL != pnsubj->pagent) {
		ohubSetAgentState(pnsubj->pagent, "ON");
	}
	return OK;
}


/*
 * handle incoming packets
 */
oret_t
ohubSubjHandleProtocol(OhubNetSubj_t * pnsubj, int kind, int type,
						int len, char *buf)
{
	switch (type) {
	case OLANG_PING_ACK:
		ohubLog(8, "got ping acknowledge from {%s}", pnsubj->url);
		ohubSubjHandlePingReply(pnsubj, kind, type, len, buf);
		return OK;
	case OLANG_PING_NAK:
		ohubLog(6, "got ping refusal from {%s}", pnsubj->url);
		ohubSubjHandlePingReply(pnsubj, kind, type, len, buf);
		return OK;
	case OLANG_CONN_ACK:
		return ohubSubjConnect(pnsubj, kind, type, len, buf);
	case OLANG_CONN_NAK:
		ohubLog(6, "got connection refusal from {%s}", pnsubj->url);
		pnsubj->subj_state = OHUB_SUBJ_EXIT;
		if (pnsubj->pagent)
			ohubSetAgentState(pnsubj->pagent, "");
		return OK;
	case OLANG_ALIVE:
		ohubLog(9, "got heartbeat from {%s}", pnsubj->url);
		return OK;
	default:
		return ohubAgentHandleIncoming(pnsubj->pagent, kind, type, len, buf);
	}
}


oret_t
ohubSubjSetState(const char *url, bool_t state)
{
	if (*url != '\0')
		ohubLog(1, "subj [%s] is %s", url, state ? "ON" : "OFF");
	return OK;
}


bool_t
ohubSubjNeedsRotor(struct OhubNetSubj_st * pnsubj)
{
	return pnsubj ? pnsubj->needs_rotor : FALSE;
}


int
ohubSubjSendQueueGap(struct OhubNetSubj_st *pnsubj)
{
	return pnsubj ? pnsubj->q_size - pnsubj->q_len : -1;
}


oret_t
ohubSubjTrigger(struct OhubNetSubj_st * pnsubj)
{
	switch (pnsubj->subj_state) {
	case OHUB_SUBJ_WAIT:
		break;
	case OHUB_SUBJ_OPEN:
	case OHUB_SUBJ_NONE:
	case OHUB_SUBJ_EXIT:
	default:
		return ERROR;
	}
	pnsubj->ping_cur_ms = OLANG_PING_MIN_MS;
	pnsubj->ping_wd = 0;
	ohubSubjPing(pnsubj);
	return OK;
}


struct OhubUnregAccum_st *
ohubSubjGetUnregAccum(struct OhubNetSubj_st *pnsubj)
{
	return &pnsubj->unreg_accum;
}
