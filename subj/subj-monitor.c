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

  Data change monitoring.

*/

#include "subj-priv.h"
#include <optikus/conf-net.h>	/* for OHTONL,... */
#include <string.h>				/* for memcpy,strncmp */

#if defined(HAVE_CONFIG_H)
#include <config.h>				/* for VXWORKS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS)
#include <timers.h>				/* for time stamping */
#endif


#define OSUBJ_MON_BUF_SIZE  2048

int     osubj_mon_buf_size = OSUBJ_MON_BUF_SIZE;

typedef struct OsubjMonitorRecord_st
{
	struct OsubjMonitorRecord_st *next;
	struct OsubjMonitorRecord_st *prev;
	ulong_t ooid;
	long    adr;
	long    off;
	int     len;
	int     boff;
	int     blen;
	char    ptr;
	char    type;
	uchar_t undef;
	char    hibit;
	int     force_count;
	char    buf[4];				/* must be the last */
} OsubjMonitorRecord_t;


#define NULL_ADDR(a)   ((long)(a) <= 0 && (long)(a) >= -8)

OsubjMonitorRecord_t *osubj_first_mon;
OsubjMonitorRecord_t *osubj_last_mon;
OsubjMonitorRecord_t *osubj_cur_mon;

int     osubj_mon_qty;

void   *osubj_ever_write[OSUBJ_MAX_EVER_WRITE];
bool_t  osubj_ban_write;

long    osubj_sel_address;

int     osubj_dbg[100];

/*		rate limiting		*/

int     osubj_mon_max_sent_bytes;
int     osubj_mon_max_polled_qty;
int     osubj_mon_max_changed_qty;
long    osubj_mon_max_time_ms;

/* 		time stamping		*/

bool_t  osubj_use_time = TRUE;
ulong_t osubj_cur_time;

ulong_t (*osubj_time_func)(void);


/*
 * .
 */
ulong_t
osubjGetTime(void)
{
	if (osubj_time_func) {
		return (*osubj_time_func) ();
	} else {
#if defined(VXWORKS)
		struct timespec ts;
		ulong_t sec, usec, val;

		clock_gettime(CLOCK_REALTIME, &ts);
		sec = ts.tv_sec;
		sec %= 1000;
		usec = ts.tv_nsec;
		usec /= 1000L;
		val = sec * 1000000 + usec;
		return val;
#else /* !VXWORKS */
		return 0;
#endif /* VXWORKS */
	}
}


/*
 * .
 */
OsubjMonitorRecord_t *
osubjAllocateMonitor(int data_len)
{
	int     mon_len;
	OsubjMonitorRecord_t *pmon;
	char   *ptr;

	mon_len = sizeof(OsubjMonitorRecord_t) + data_len * 2;
	ptr = osubjNew(char, mon_len, 7);

	if (!ptr)
		return NULL;
	pmon = (OsubjMonitorRecord_t *) ptr;
	osubjLogAt(12, "alloc data_len=%d mon_len=%d ptr=%p",
				data_len, mon_len, ptr);
	pmon->len = data_len;
	/* add to last end */
	if (!osubj_first_mon)
		osubj_first_mon = pmon;
	pmon->prev = osubj_last_mon;
	if (osubj_last_mon)
		osubj_last_mon->next = pmon;
	osubj_last_mon = pmon;
	osubj_mon_qty++;
	return pmon;
}


oret_t
osubjDeallocateMonitor(OsubjMonitorRecord_t * pmon)
{
	if (!pmon)
		return ERROR;
	if (osubj_first_mon == pmon)
		osubj_first_mon = pmon->next;
	if (osubj_last_mon == pmon)
		osubj_last_mon = pmon->prev;
	if (pmon->next)
		pmon->next->prev = pmon->prev;
	if (pmon->prev)
		pmon->prev->next = pmon->next;
	osubjFree(pmon, 7);
	osubj_mon_qty--;
	return OK;
}


oret_t
osubjClearMonitoring(void)
{
	while (osubj_first_mon)
		osubjDeallocateMonitor(osubj_first_mon);
	osubj_first_mon = osubj_last_mon = osubj_cur_mon = NULL;
	osubj_mon_qty = 0;
	osubjLogAt(7, "monitoring cleared");
	return OK;
}


oret_t
osubjMonitoring()
{
	OsubjMonitorRecord_t *pmon;
	char   *mbuf;
	int     buf_len;
	ulong_t ulv, ulm;
	ushort_t usv, usm;
	uchar_t ucv, ucm;
	ulong_t a;
	uchar_t undef;
	char   *b;
	int     n;
	int     k;
	int     p;
	int     qty, polled_qty, changed_qty, sent_bytes;
	long    start_ms, delta_ms;
	oret_t rc;
	int     pkt_type = OLANG_MONITORING;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	osubj_dbg[0]++;
	if (0 == osubj_mon_qty)
		return OK;
	buf_len = osubj_mon_buf_size;
	if (NULL == (mbuf = osubjNew(char, buf_len, 8))) {
		osubjLogAt(3, "memory exhausted for monitoring buffer");
		return ERROR;
	}
	osubj_cur_time = osubjGetTime();
	start_ms = osubjClock();
	mbuf[0] = 0;
	p = 1;
	qty = polled_qty = changed_qty = sent_bytes = 0;
	if (NULL == (pmon = osubj_cur_mon))
		pmon = osubj_first_mon;
	for (; NULL != pmon; pmon = pmon->next) {
		if (0 == pmon->ooid)
			continue;
		osubj_dbg[1]++;
		n = pmon->len;
		if (n + 16 > buf_len) {
			/* data won't fit in buffer */
			osubj_dbg[2]++;
			continue;
		}
		if (qty > 254 || p + n + 16 > buf_len) {
			/* send packet */
			mbuf[0] = (char) (uchar_t) qty;
			if (osubjIsConnected() && qty > 0) {
				osubj_dbg[3]++;
				rc = osubjSendPacket(OK, OLANG_DATA, pkt_type, p, mbuf);
				pkt_type = OLANG_MONITORING;
				sent_bytes += p;
			}
			/* start new buffer */
			osubj_dbg[4]++;
			p = 0;
			qty = 0;
			mbuf[p++] = 0;
			if (osubj_mon_max_sent_bytes > 0
				&& sent_bytes > osubj_mon_max_sent_bytes) {
				osubj_dbg[70]++;
				break;
			}
		}
		/* limit poll rate */
		if (osubj_mon_max_polled_qty > 0
			&& polled_qty > osubj_mon_max_polled_qty) {
			osubj_dbg[71]++;
			break;
		}
		if (osubj_mon_max_changed_qty > 0
			&& changed_qty > osubj_mon_max_changed_qty) {
			osubj_dbg[72]++;
			break;
		}
		if (osubj_mon_max_time_ms > 0 && (polled_qty % 10 == 0)) {
			delta_ms = osubjClock();
			delta_ms -= start_ms;
			if (delta_ms > osubj_mon_max_time_ms) {
				osubj_dbg[73]++;
				break;
			}
		}
		/* actual read */
		polled_qty++;
		b = pmon->buf;
		undef = 0x80;
		a = pmon->adr;
		switch (pmon->ptr) {
		case 'D':
			/* off is always 0 here */
			if (NULL_ADDR(a))
				break;
			undef = 0;
			break;
		case 'I':
			if (NULL_ADDR(a))
				break;
			a += pmon->off;
			if (a & 3)
				memcpy((void *) &a, (void *) a, 4);
			else
				a = *(ulong_t *) a;
			if (NULL_ADDR(a))
				break;
			undef = 0;
			break;
		case 'R':
			if (NULL_ADDR(a))
				break;
			if (a & 3)
				memcpy((void *) &a, (void *) a, 4);
			else
				a = *(ulong_t *) a;
			a += pmon->off;
			if (NULL_ADDR(a))
				break;
			undef = 0;
			break;
		case 'P':
			if (NULL_ADDR(a))
				break;
			if (a & 3)
				memcpy((void *) &a, (void *) a, 4);
			else
				a = *(ulong_t *) a;
			a += pmon->off;
			if (NULL_ADDR(a))
				break;
			if (a & 3)
				memcpy((void *) &a, (void *) a, 4);
			else
				a = *(ulong_t *) a;
			if (NULL_ADDR(a))
				break;
			undef = 0;
			break;
		default:
			break;
		}
		if (!undef) {
			/* NOTE: try to read data as aligned, in single transaction */
			if (!pmon->blen) {
				switch (n) {
				case 4:
					*(ulong_t *) (b + 4) = *(ulong_t *) b;
					if (a & 3)
						memcpy(b, (void *) a, 4);
					else
						*(ulong_t *) b = *(ulong_t *) a;
					break;
				case 2:
					*(ushort_t *) (b + 2) = *(ushort_t *) b;
					if (a & 1)
						memcpy(b, (void *) a, 2);
					else
						*(ushort_t *) b = *(ushort_t *) a;
					break;
				case 1:
					b[1] = b[0];
					*(uchar_t *) b = *(uchar_t *) a;
					break;
				default:
					memcpy(b + n, b, n);
					memcpy(b, (void *) a, n);
					break;
				}
			} else {
				switch (n) {
				case 4:
					*(ulong_t *) (b + 4) = *(ulong_t *) b;
					if (a & 3)
						memcpy((void *) &ulv, (void *) a, 4);
					else
						ulv = *(ulong_t *) a;
					if (pmon->boff)
						ulv >>= pmon->boff;
					ulm = (1 << pmon->blen) - 1;
					ulv &= ulm;
					if (pmon->hibit && (ulv & (1 << (pmon->blen - 1))))
						ulv |= ~ulm;
					*(ulong_t *) b = ulv;
					break;
				case 2:
					*(ushort_t *) (b + 2) = *(ushort_t *) b;
					if (a & 1)
						memcpy((void *) &usv, (void *) a, 2);
					else
						usv = *(ushort_t *) a;
					if (pmon->boff)
						usv >>= pmon->boff;
					usm = (1 << pmon->blen) - 1;
					usv &= usm;
					if (pmon->hibit && (usv & (1 << (pmon->blen - 1))))
						usv |= ~usm;
					*(ushort_t *) b = usv;
					break;
				case 1:
					b[1] = b[0];
					ucv = *(uchar_t *) a;
					if (pmon->boff)
						ucv >>= pmon->boff;
					ucm = (1 << pmon->blen) - 1;
					ucv &= ucm;
					if (pmon->hibit && (ucv & (1 << (pmon->blen - 1))))
						ucv |= ~ucm;
					*(uchar_t *) b = ucv;
					break;
				default:
					undef = 1;
					break;
				}
			}
		}
		if (pmon->force_count)
			pmon->force_count--;
		else if (undef && pmon->undef)
			continue;
		else if (!undef && !pmon->undef) {
			if (pmon->type == 's')
				k = strncmp(b, b + n, n);
			else {
				switch (n) {
				case 4:
					k = *(long *) b - *(long *) (b + 4);
					break;
				case 2:
					k = *(short *) b - *(short *) (b + 2);
					break;
				case 1:
					k = b[0] - b[1];
					break;
				default:
					k = memcmp(b, b + n, n);
					break;
				}
			}
			if (k == 0)
				continue;
		}
		changed_qty++;
		pmon->undef = undef;
		ulv = OHTONL(pmon->ooid);
		memcpy((void *) (mbuf + p), (void *) &ulv, 4);
		p += 4;
		mbuf[p++] = pmon->type;
		/* no time */
		if (undef)
			n = 0;
		if (osubj_use_time) {
			ulv = OHTONL(osubj_cur_time);
			if (n < 0x20) {
				mbuf[p++] = (char) (n | undef | 0x40);
				memcpy(mbuf + p, (void *) &ulv, 4);
				p += 4;
			} else {
				mbuf[p++] = (char) (0x20 | ((n >> 8) & 0x1f) | undef | 0x40);
				memcpy(mbuf + p, (void *) &ulv, 4);
				p += 4;
				mbuf[p++] = (char) (uchar_t) (n & 0xff);
			}
		} else {
			if (n < 0x20)
				mbuf[p++] = (char) (n | undef);
			else {
				mbuf[p++] = (char) (0x20 | ((n >> 8) & 0x1f) | undef);
				mbuf[p++] = (char) (uchar_t) (n & 0xff);
			}
		}
		if (n > 0) {
			memcpy(mbuf + p, b, n);
			p += n;
		}
		if (osubj_sel_address && a == osubj_sel_address) {
			if ((osubj_sel_address & 3) == 0) {
				osubjLogAt(5, "eeee sel_address=%ld",
							*(long *) osubj_sel_address);
			}
			pkt_type = OLANG_SEL_MONITORING;
		}
		qty++;
	}
	osubj_cur_mon = pmon;
	if (qty <= 0)
		rc = OK;
	else {
		mbuf[0] = (char) (uchar_t) qty;
		if (!osubjIsConnected())
			rc = ERROR;
		else {
			rc = osubjSendPacket(OK, OLANG_DATA, pkt_type, p, mbuf);
			pkt_type = OLANG_MONITORING;
			sent_bytes += p;
		}
	}
	osubjFree(mbuf, 8);
	return rc;
}


oret_t
osubjUnregisterMonitoring(ulong_t ooid)
{
	OsubjMonitorRecord_t *pmon;
	oret_t rc;

	/* removal search from last to first */
	for (pmon = osubj_last_mon; pmon; pmon = pmon->prev)
		if (pmon->ooid == ooid)
			break;
	if (pmon) {
		if (osubj_cur_mon == pmon)
			osubj_cur_mon = pmon->next;
		osubjDeallocateMonitor(pmon);
		rc = osubjAddDataMonReply(OLANG_DATA_UNREG_ACK, ooid);
		osubjLogAt(6, "data_unreg OK ooid=%lu send_rc=%d", ooid, rc);
		return rc;
	}
	rc = osubjAddDataMonReply(OLANG_DATA_UNREG_NAK, ooid);
	osubjLogAt(4, "data_unreg NOT_FOUND ooid=%lu send_rc=%d", ooid, rc);
	return ERROR;
}


oret_t
osubjHandleDataGroupUnreg(int kind, int type, int len, char *buf)
{
	ulong_t ulv, ooid, *lbuf;
	int     i, qty = 0;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (len < 8) {
		osubjLogAt(2, "invalid data_group_unreg packet len=%d", len);
		return ERROR;
	}
	lbuf = (ulong_t *) buf;
	ulv = lbuf[0];
	qty = (int) ONTOHL(ulv);
	if (qty < 1 || len != (qty + 1) * sizeof(long)) {
		osubjLogAt(2, "invalid data_group_unreg packet len=%d qty=%d", len, qty);
		return ERROR;
	}
	for (i = 1; i <= qty; i++) {
		ulv = lbuf[i];
		ooid = ONTOHL(ulv);
		if (!ooid) {
			osubjLogAt(2, "invalid data_unreg_group packet %dth ooid=NULL", i);
			continue;
		}
		osubjUnregisterMonitoring(ooid);
	}
	osubjLogAt(5, "data_group_unreg unregistered %d monitors", qty);
	return OK;
}


oret_t
osubjHandleDataUnreg(int kind, int type, int len, char *buf)
{
	ulong_t ulv, ooid;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (len != 4) {
		if (osubj_verb > 1)
		osubjLogAt(2, "invalid data_unreg packet len=%d", len);
		return ERROR;
	}
	ulv = *(ulong_t *) buf;
	ooid = ONTOHL(ulv);
	if (!ooid) {
		osubjLogAt(2, "invalid data_unreg packet ooid=NULL");
		return ERROR;
	}
	return osubjUnregisterMonitoring(ooid);
}


oret_t
osubjHandleDataReg(int kind, int type, int len, char *buf)
{
	OsubjMonitorRecord_t *pmon;
	ulong_t ulv;
	ushort_t usv;
	ulong_t ooid;
	long    v_adr, v_off;
	short   v_len;
	char    v_ptr, v_type;
	uchar_t v_boff, v_blen;
	oret_t rc;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (len != 18) {
		osubjLogAt(2, "invalid data_reg packet len=%d", len);
		return ERROR;
	}
	ulv = *(ulong_t *) (buf + 0);
	ooid = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 4);
	v_adr = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 8);
	v_off = ONTOHL(ulv);
	usv = *(ushort_t *) (buf + 12);
	usv = ONTOHS(usv);
	v_len = (short) usv;
	v_ptr = buf[14];
	v_type = buf[15];
	v_boff = buf[16];
	v_blen = buf[17];
	if (!ooid) {
		osubjLogAt(2, "invalid data_reg packet ooid=NULL");
		goto FAIL;
	}
	/* addition search from first to last */
	for (pmon = osubj_first_mon; pmon; pmon = pmon->next)
		if (pmon->ooid == ooid)
			break;
	if (pmon) {
		osubjLogAt(4, "data_reg ERR already got ooid=%lu", ooid);
		pmon = NULL;
		goto FAIL;
	}
	if (v_ptr != 'D' && v_ptr != 'I' && v_ptr != 'R' && v_ptr != 'P') {
		osubjLogAt(4, "data_reg ERR ooid=%lu invalid ptr=%02xh",
					ooid, (int) (uchar_t) v_ptr);
		goto FAIL;
	}
	if (v_len <= 0 || v_len >= 4192) {
		osubjLogAt(4, "data_reg ERR ooid=%lu invalid data_len=%hd", ooid, v_len);
		goto FAIL;
	}
	if (NULL == (pmon = osubjAllocateMonitor(v_len))) {
		osubjLogAt(2, "data_reg packet ooid=%lu no memory", ooid);
		goto FAIL;
	}
	if (v_ptr == 'D') {
		v_adr += v_off;
		v_off = 0;
	}
	pmon->ooid = ooid;
	pmon->adr = v_adr;
	pmon->off = v_off;
	pmon->len = v_len;
	pmon->boff = v_boff;
	pmon->blen = v_blen;
	pmon->ptr = v_ptr;
	pmon->type = v_type;
	pmon->undef = 0;
	switch (pmon->type) {
	case 'b':
	case 'h':
	case 'i':
	case 'l':
	case 'q':
		pmon->hibit = 1;
		break;
	default:
		pmon->hibit = 0;
		break;
	}
	pmon->force_count = 1;
	rc = osubjAddDataMonReply(OLANG_DATA_REG_ACK, ooid);
	osubjLogAt(6,
			"data_reg OK ooid=%lu a=%lxh+%ld/%hd b%d:%d p=%c t=%c snd_rc=%d",
			ooid, v_adr, v_off, v_len, v_boff, v_blen, v_ptr, v_type, rc);
	return OK;
  FAIL:
	rc = osubjAddDataMonReply(OLANG_DATA_REG_NAK, ooid);
	return ERROR;
}


oret_t
osubjHandleDataWrite(int kind, int type, int len, char *buf)
{
	ulong_t ulv, ulvd, ulvm;
	ushort_t usv, usvd, usvm;
	uchar_t ucv, ucvd, ucvm;
	ulong_t id, spare_time;
	char    v_ptr, v_type;
	oret_t rc;
	ulong_t a, adr;
	long    off;
	int     boff, blen;
	short   err;
	char   *b;
	int     i, n;
	char    reply[16];

	/* parse input */
	osubj_dbg[40]++;
	if (!osubj_initialized || osubj_aborted) {
		osubj_dbg[41]++;
		return ERROR;
	}
	if (len < 25) {
		osubjLogAt(2, "data_write: tiny len=%d", len);
		osubj_dbg[42]++;
		return ERROR;
	}
	ulv = *(ulong_t *) (buf + 4);
	spare_time = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 8);
	a = adr = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 12);
	off = ONTOHL(ulv);
	usv = *(ushort_t *) (buf + 16);
	n = ONTOHS(usv);
	v_ptr = buf[18];
	v_type = buf[19];
	boff = (uchar_t) buf[20];
	blen = (uchar_t) buf[21];
	b = buf + 24;
	osubj_dbg[50]++;
	osubj_dbg[51] = a;
	osubj_dbg[52] = off;
	osubj_dbg[53] = n;
	osubj_dbg[54] = v_ptr;
	osubj_dbg[55] = v_type;
	osubj_dbg[56] = boff;
	osubj_dbg[57] = blen;
	if (len != 24 + n) {
		osubjLogAt(2, "data_write: invalid len=%d data_len=%d", len, n);
		err = -1;
		osubj_dbg[43]++;
		return ERROR;
	}
	/* prepare reply header */
	ulv = *(ulong_t *) reply = *(ulong_t *) buf;	/* id */
	id = ONTOHL(ulv);
	osubj_dbg[58] = id;
	/* check validity */
	if (n <= 0 || n >= 4192) {
		osubjLogAt(4, "data_write ERR id=%lu invalid data_len=%d", id, n);
		err = -1;
		osubj_dbg[44]++;
		goto END;
	}
	/* actual write */
	err = -127;					/* FIXME: need OLANG code for null pointers */
	switch (v_ptr) {
	case 'D':
		a += off;
		if (NULL_ADDR(a))
			break;
		err = 0;
		break;
	case 'I':
		if (NULL_ADDR(a))
			break;
		a += off;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		if (NULL_ADDR(a))
			break;
		err = 0;
		break;
	case 'R':
		if (NULL_ADDR(a))
			break;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		a += off;
		if (NULL_ADDR(a))
			break;
		err = 0;
		break;
	case 'P':
		if (NULL_ADDR(a))
			break;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		a += off;
		if (NULL_ADDR(a))
			break;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		if (NULL_ADDR(a))
			break;
		err = 0;
		break;
	default:
		osubjLogAt(4, "data_write ERR id=%lu invalid ptr=%02xh",
					id, (int) (uchar_t) v_ptr);
		err = -1;
		osubj_dbg[45]++;
		goto END;
	}
	if (err) {
		/* null pointers */
		osubj_dbg[46]++;
		goto END;
	}
	/* write protection */
	if (osubj_ban_write) {
		for (i = 0; i < OSUBJ_MAX_EVER_WRITE; i++)
			if (a && osubj_ever_write[i] == (void *) a)
				break;
		if (i >= OSUBJ_MAX_EVER_WRITE) {
			osubj_dbg[98]++;
			err = -1;
			goto END;
		}
	}
	if (!blen) {
		/* NOTE: try to change data as aligned, in single transaction */
		osubj_dbg[47]++;
		switch (n) {
		case 4:
			if ((long) b & 3)
				memcpy((void *) &ulv, b, 4);
			else
				ulv = *(ulong_t *) b;
			if (a & 3)
				memcpy((void *) a, (void *) &ulv, 4);
			else
				*(ulong_t *) a = ulv;
			break;
		case 2:
			if ((long) b & 1)
				memcpy((void *) &usv, b, 2);
			else
				usv = *(ushort_t *) b;
			if (a & 1)
				memcpy((void *) a, (void *) &usv, 2);
			else
				*(ushort_t *) a = usv;
			break;
		case 1:
			*(uchar_t *) a = *(uchar_t *) b;
			break;
		default:
			memcpy((void *) a, b, n);
		}
	} else {
		osubj_dbg[48]++;
		switch (n) {
		case 4:
			ulvm = (1 << blen) - 1;
			if ((long) b & 3)
				memcpy((void *) &ulvd, b, 4);
			else
				ulvd = *(ulong_t *) b;
			if (a & 3)
				memcpy((void *) &ulv, (void *) a, 4);
			else
				ulv = *(ulong_t *) a;
			ulvd &= ulvm;
			if (boff)
				ulvd <<= boff, ulvm <<= boff;
			ulv &= ~ulvm;
			ulv |= ulvd;
			if (a & 3)
				memcpy((void *) a, (void *) &ulv, 4);
			else
				*(ulong_t *) a = ulv;
			break;
		case 2:
			usvm = (1 << blen) - 1;
			osubj_dbg[60] = usvm;
			if ((long) b & 1)
				memcpy((void *) &usvd, b, 2);
			else
				usvd = *(ushort_t *) b;
			osubj_dbg[61] = usvd;
			if (a & 1)
				memcpy((void *) &usv, (void *) a, 2);
			else
				usv = *(ushort_t *) a;
			osubj_dbg[62] = usv;
			usvd &= usvm;
			osubj_dbg[63] = usvd;
			if (boff)
				usvd <<= boff, usvm <<= boff;
			osubj_dbg[64] = usvd;
			osubj_dbg[65] = usvm;
			usv &= ~usvm;
			osubj_dbg[66] = usv;
			usv |= usvd;
			osubj_dbg[67] = usv;
			if (a & 1)
				memcpy((void *) a, (void *) &usv, 2);
			else
				*(ushort_t *) a = usv;
			osubj_dbg[68] = a;
			break;
		case 1:
			ucvm = (1 << blen) - 1;
			ucvd = *(uchar_t *) b;
			ucv = *(uchar_t *) a;
			ucvd &= ucvm;
			if (boff)
				ucvd <<= boff, ucvm <<= boff;
			ucv &= ~ucvm;
			ucv |= ucvd;
			*(uchar_t *) a = ucv;
			break;
		default:
			osubjLogAt(4, "data_write ERR id=%lu invalid bitop data_len=%d",
						id, n);
			err = -1;
			goto END;
		}
	}
  END:
	/* send results */
	*(ushort_t *) (reply + 4) = OHTONS((ushort_t) err);
	rc = osubjSendPacket(OK, OLANG_DATA, OLANG_DATA_WRITE_REPLY, 6, reply);
	osubjLogAt(6,
			"data_write OK id=%lu a=%lxh+%ld n=%d p=%c t=%c err=%d snd_rc=%d",
			id, adr, off, n, v_ptr, v_type, err, rc);
	return OK;
}


oret_t
osubjHandleDataRead(int kind, int type, int len, char *buf)
{
	ulong_t ulv, ulm;
	ushort_t usv, usm;
	uchar_t ucv, ucm;
	ulong_t spare_time;
	char    v_ptr, v_type;
	oret_t rc;
	ulong_t id;
	ulong_t a, adr;
	long    off;
	int     boff, blen;
	short   err;
	char   *b;
	int     n;
	int     plen;
	uchar_t undef;
	char    hibit;
	char    reply_buf[128];
	char   *reply = reply_buf;

	/* parse input */
	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (len != 24) {
		osubjLogAt(2, "data_read: invalid len=%d", len);
		return ERROR;
	}
	ulv = *(ulong_t *) buf;
	id = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 4);
	spare_time = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 8);
	a = adr = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 12);
	off = ONTOHL(ulv);
	usv = *(ushort_t *) (buf + 16);
	n = (int) (short) ONTOHS(usv);
	v_ptr = buf[18];
	v_type = buf[19];
	boff = (uchar_t) buf[20];
	blen = (uchar_t) buf[21];
	osubj_dbg[10]++;
	osubj_dbg[11] = id;
	osubj_dbg[12] = a;
	osubj_dbg[13] = off;
	osubj_dbg[14] = n;
	osubj_dbg[15] = v_ptr;
	osubj_dbg[16] = v_type;
	osubj_dbg[17] = boff;
	osubj_dbg[18] = blen;
	/* check validity */
	undef = 0x80;
	if (n <= 0 || n >= 4192) {
		osubjLogAt(4, "data_read ERR id=%lxh invalid data_len=%d", id, n);
		err = -2;
		goto END;
	}
	/* allocate buffer */
	if (n + 32 < sizeof(reply_buf))
		reply = reply_buf;
	else {
		if (NULL == (reply = osubjNew(char, n + 32, 8))) {
			osubjLogAt(3, "data_read ERR id=%lxh cannot allocate data_len=%d",
						id, n);
			reply = reply_buf;
			err = -3;
			goto END;
		}
	}
	/* preread */
	err = -127;					/* FIXME: need OLANG code for null pointers */
	switch (v_ptr) {
	case 'D':
		a += off;
		if (NULL_ADDR(a))
			break;
		undef = 0;
		break;
	case 'I':
		if (NULL_ADDR(a))
			break;
		a += off;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		if (NULL_ADDR(a))
			break;
		undef = 0;
		break;
	case 'R':
		if (NULL_ADDR(a))
			break;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		a += off;
		if (NULL_ADDR(a))
			break;
		undef = 0;
		break;
	case 'P':
		if (NULL_ADDR(a))
			break;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		a += off;
		if (NULL_ADDR(a))
			break;
		if (a & 3)
			memcpy((void *) &a, (void *) a, 4);
		else
			a = *(ulong_t *) a;
		if (NULL_ADDR(a))
			break;
		undef = 0;
		break;
	default:
		break;
	}
	if (undef)
		goto END;
	osubj_dbg[18]++;
	osubj_dbg[19] = a;
	/* guess offset into buffer */
	b = reply + 8;
	/* NOTE: try to read data as aligned, in single transaction */
	if (!blen) {
		osubj_dbg[20]++;
		switch (n) {
		case 4:
			if (a & 3)
				memcpy(b, (void *) a, 4);
			else
				*(ulong_t *) b = *(ulong_t *) a;
			break;
		case 2:
			if (a & 1)
				memcpy(b, (void *) a, 2);
			else
				*(ushort_t *) b = *(ushort_t *) a;
			break;
		case 1:
			*(uchar_t *) b = *(uchar_t *) a;
			break;
		default:
			memcpy(b, (void *) a, n);
			break;
		}
	} else {
		osubj_dbg[21]++;
		switch (v_type) {
		case 'b':
		case 'h':
		case 'i':
		case 'l':
		case 'q':
			hibit = 1;
			break;
		default:
			hibit = 0;
			break;
		}
		osubj_dbg[22] = hibit;
		switch (n) {
		case 4:
			if (a & 3)
				memcpy((void *) &ulv, (void *) a, 4);
			else
				ulv = *(ulong_t *) a;
			if (boff)
				ulv >>= boff;
			ulm = (1 << blen) - 1;
			ulv &= ulm;
			if (hibit && (ulv & (1 << (blen - 1))))
				ulv |= ~ulm;
			*(ulong_t *) b = ulv;
			break;
		case 2:
			if (a & 1)
				memcpy((void *) &usv, (void *) a, 2);
			else
				usv = *(ushort_t *) a;
			osubj_dbg[23] = a;
			osubj_dbg[24] = usv;
			if (boff)
				usv >>= boff;
			osubj_dbg[25] = usv;
			usm = (1 << blen) - 1;
			osubj_dbg[26] = usm;
			usv &= usm;
			osubj_dbg[27] = usv;
			if (hibit && (usv & (1 << (blen - 1))))
				usv |= ~usm;
			osubj_dbg[28] = usv;
			*(ushort_t *) b = usv;
			break;
		case 1:
			ucv = *(uchar_t *) a;
			if (boff)
				ucv >>= boff;
			ucm = (1 << blen) - 1;
			ucv &= ucm;
			if (hibit && (ucv & (1 << (blen - 1))))
				ucv |= ~ucm;
			*(uchar_t *) b = ucv;
			break;
		default:
			undef = 1;
			err = -4;
			break;
		}
	}
	*(ushort_t *) (reply + 6) = OHTONS((ushort_t) n);
	err = 0;
  END:
	osubj_dbg[29] = err;
	*(ulong_t *) reply = *(ulong_t *) buf;	/* id */
	*(ushort_t *) (reply + 4) = OHTONS((ushort_t) err);
	plen = err ? 6 : 8 + n;
	rc = osubjSendPacket(OK, OLANG_DATA, OLANG_DATA_READ_REPLY, plen, reply);
	osubjLogAt(6,
			"data_read id=%lxh a=%lxh+%ld n=%d p=%c t=%c err=%d pl=%d OK=%d",
			id, adr, off, n, v_ptr, v_type, err, plen, rc);
	if (reply != reply_buf)
		osubjFree(reply, 8);
	return OK;
}


oret_t
osubjEverWrite(void *adr, bool_t enable)
{
	int     i;

	if (enable) {
		for (i = 0; i < OSUBJ_MAX_EVER_WRITE; i++) {
			if (osubj_ever_write[i] == adr)
				return OK;
		}
		for (i = 0; i < OSUBJ_MAX_EVER_WRITE; i++) {
			if (osubj_ever_write[i] == 0) {
				osubj_ever_write[i] = adr;
				return OK;
			}
		}
	} else {
		for (i = 0; i < OSUBJ_MAX_EVER_WRITE; i++) {
			if (osubj_ever_write[i] == adr) {
				osubj_ever_write[i] = 0;
				return OK;
			}
		}
	}
	return ERROR;
}

