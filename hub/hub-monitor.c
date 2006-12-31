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

#include "hub-priv.h"
#include <optikus/tree.h>
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for OHTONS,... */
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxbcopy,oxvzero,... */
#include <stdio.h>				/* for sprintf */
#include <string.h>				/* for memcpy,strcat,... */


#define OHUB_USE_TIME	1

#define OHUB_MAX_WATCH_PER_MONITOR	8
#define OHUB_MON_SECURING_LIMIT		32

typedef struct
{
	struct OhubNetWatch_st *nwatch_p;
} OhubWatcherListEntry_t;

typedef struct
{
	int     no;
	ooid_t ooid;
	int     ref_count;
	OhubQuark_t quark;
	oval_t value;
	char   *data_buf;
	int     buf_len;
	OhubWatcherListEntry_t watchers[OHUB_MAX_WATCH_PER_MONITOR];
	int     watch_qty;
	bool_t  securing_flag;
	OhubAgent_t *agent_p;
	struct OhubNetSubj_st *nsubj_p;
} OhubMonitorRecord_t;

typedef struct
{
	OhubMonitorRecord_t *mon_p;
} OhubMonitorListEntry_t;

OhubMonitorListEntry_t *ohub_mons;
int     ohub_mon_qty;
int     ohub_mon_securing_qty;
tree_t  ohub_by_ooid_mon_hash;


/*
 * .
 */
OhubMonitorRecord_t *
ohubFindMonitorByOoid(ooid_t ooid)
{
	int     key = (int) ooid;
	int     no;
	bool_t  found;

	if (!ohub_mons || !ohub_by_ooid_mon_hash || !ooid)
		return NULL;
	found = treeNumFind(ohub_by_ooid_mon_hash, key, &no);
	if (!found || no <= 0 || no >= ohub_mon_qty)
		return NULL;
	if (!ohub_mons[no].mon_p || ohub_mons[no].mon_p->ooid != ooid)
		return NULL;
	return ohub_mons[no].mon_p;
}


OhubMonitorRecord_t *
ohubAllocateMonitor(void)
{
	OhubMonitorRecord_t *pmon;
	OhubMonitorListEntry_t *new_mons;
	int     new_qty, no;

	if (NULL == (pmon = oxnew(OhubMonitorRecord_t, 1)))
		return NULL;
	for (no = 1; no < ohub_mon_qty; no++) {
		if (!ohub_mons[no].mon_p) {
			ohub_mons[no].mon_p = pmon;
			pmon->no = no;
			return pmon;
		}
	}
	if ((no = ohub_mon_qty) <= 0)
		no = 1;

	new_qty = ohub_mon_qty * 2 + 2;
	new_mons = oxrenew(OhubMonitorListEntry_t, new_qty,
						ohub_mon_qty, ohub_mons);
	if (NULL == new_mons) {
		oxfree(pmon);
		return NULL;
	}
	ohub_mons = new_mons;
	ohub_mon_qty = new_qty;

	ohub_mons[no].mon_p = pmon;
	pmon->no = no;
	return pmon;
}


void
ohubDeallocateMonitor(OhubMonitorRecord_t * pmon)
{
	int     no = pmon->no;
	struct OhubNetWatch_st *pnwatch;
	int     i;

	if (pmon->securing_flag) {
		if (--ohub_mon_securing_qty < 0)
			ohub_mon_securing_qty = 0;
	}
	for (i = 0; i < pmon->watch_qty; i++) {
		pnwatch = pmon->watchers[i].nwatch_p;
		pmon->watchers[i].nwatch_p = NULL;
		/* FIXME: perhaps notify this watcher */
	}
	oxfree(pmon->data_buf);
	oxvzero(pmon);
	oxfree(pmon);
	ohub_mons[no].mon_p = NULL;
}


oret_t
ohubMonFlushMonAccum(struct OhubNetWatch_st *pnwatch, OhubMonAccum_t * pacc)
{
	oret_t rc;

	if (pacc->qty <= 0)
		rc = OK;
	else {
		pacc->buf[0] = pacc->qty;
		rc = ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_MONITORING,
								pacc->off, pacc->buf);
		ohubLog(5, "flush monitoring accumulator qty=%d size=%d rc=%d",
				pacc->qty, pacc->off, rc);
	}
	pacc->qty = 0;
	pacc->off = 1;
	pacc->buf[0] = 0;
	pacc->start_ms = 0;
	return rc;
}


oret_t
ohubMonFlushUnregAccum(struct OhubNetSubj_st * pnsubj, OhubUnregAccum_t * pacc)
{
	int     len;
	oret_t rc;

	if (pacc->qty <= 0)
		rc = OK;
	else {
		pacc->buf[0] = OHTONL(pacc->qty);
		len = (pacc->qty + 1) * sizeof(long);
		rc = ohubSubjSendPacket(pnsubj, OLANG_DATA, OLANG_DGROUP_UNREG_REQ,
								len, (char *) pacc->buf);
		ohubLog(5, "flush unreg accumulator qty=%d size=%d rc=%d",
				pacc->qty, len, rc);
	}
	pacc->qty = 0;
	pacc->buf[0] = 0;
	pacc->start_ms = 0;
	return rc;
}


oret_t
ohubAnnounceChange(OhubMonitorRecord_t * pmon,
					struct OhubNetWatch_st * my_watcher)
{
	char    buf[OHUB_MON_ACCUM_SIZE];
	int     buf_len = sizeof(buf);
	ulong_t ulv;
	uchar_t undef;
	char   *b;
	int     i, n;
	int     p = 0;
	int     qty = 0;
	char   *rotor_ptr;
	char    rotor_type;
	short   rotor_len;
	bool_t  enable_rotor, watcher_enable_rotor;
	struct OhubNetWatch_st *nwatch_p;
	OhubMonAccum_t *pacc;

	if (!pmon->watch_qty) {
		ohubLog(10, "change ooid=%lu no watchers", pmon->ooid);
		return OK;
	}
	/* FIXME: better to group changes */
	buf[p++] = 0;
	n = pmon->value.len;
	if (qty > 254 || p + n + 16 > buf_len) {
		ohubLog(10, "change ooid=%lu buffer overflow", pmon->ooid);
		return ERROR;
	}
	undef = pmon->value.undef ? 0x80 : 0x00;
	ohubLog(10, "change ooid=%lu undef=%d n=%d", pmon->ooid, (undef != 0), n);
	ulv = OHTONL(pmon->ooid);
	memcpy((void *) (buf + p), (void *) &ulv, 4);
	p += 4;
	buf[p++] = pmon->value.type;
	if (undef)
		n = 0;
#if defined(OHUB_USE_TIME)
	/* yes time */
	ulv = OHTONL(pmon->value.time);
	if (n < 0x20) {
		buf[p++] = (char) (n | undef | 0x40);
		memcpy((void *) (buf + p), (void *) &ulv, 4);
		p += 4;
	} else {
		buf[p++] = (char) (0x60 | ((n >> 8) & 0x1f) | undef);
		memcpy((void *) (buf + p), (void *) &ulv, 4);
		p += 4;
		buf[p++] = (char) (uchar_t) (n & 0xff);
	}
#else /* !OHUB_USE_TIME */
	/* no time */
	if (n < 0x20) {
		buf[p++] = (char) (n | undef);
	} else {
		buf[p++] = (char) (0x20 | ((n >> 8) & 0x1f) | undef);
		buf[p++] = (char) (uchar_t) (n & 0xff);
	}
#endif /* OHUB_USE_TIME */
	if (n > 0) {
		if (pmon->value.type == 's') {
			b = pmon->value.v.v_str;
			rotor_type = 's';
			rotor_len = n;
			rotor_ptr = buf + p;
			enable_rotor = FALSE;
			memcpy(rotor_ptr, b, n);
		} else {
			b = (char *) &pmon->value.v;
			rotor_type = pmon->value.type;
			rotor_len = n;
			rotor_ptr = buf + p;
			enable_rotor = FALSE;
			memcpy(rotor_ptr, b, n);
		}
		p += n;
	} else {
		rotor_ptr = NULL;
		rotor_type = 0;
		rotor_len = 0;
		enable_rotor = FALSE;
	}
	qty++;
	if (!qty)
		return OK;
	buf[0] = (char) (uchar_t) qty;
	n = 0;
	for (i = 0; i < pmon->watch_qty; i++) {
		nwatch_p = pmon->watchers[i].nwatch_p;
		if (!nwatch_p)
			continue;
		if (my_watcher && nwatch_p != my_watcher)
			continue;
		if (ohubWatchAnnounceBlocked(nwatch_p))
			continue;
		if (rotor_ptr) {
			watcher_enable_rotor = ohubWatchNeedsRotor(nwatch_p);
			if (enable_rotor != watcher_enable_rotor) {
				enable_rotor = watcher_enable_rotor;
				ohubRotate(TRUE, rotor_type, rotor_len, rotor_ptr);
			}
		}
		pacc = ohubWatchGetAccum(nwatch_p);
		if ((pacc->off + p > OHUB_MON_ACCUM_SIZE)
			|| (pacc->qty + qty > 250)
			|| (pacc->qty > 0
				&& osMsClock() - pacc->start_ms > OHUB_MON_ACCUM_TTL))
			ohubMonFlushMonAccum(nwatch_p, pacc);
		if (!pacc->qty)
			pacc->start_ms = osMsClock();
		if (!pacc->off)
			pacc->off = 1;
		oxbcopy(buf + 1, pacc->buf + pacc->off, p - 1);
		pacc->off += p - 1;
		pacc->qty += qty;
		n++;
	}
	if (ohub_verb > 7) {
		if (qty > 1)
			olog("announce %d ooids to %d watchers len=%d", qty, n, p);
		else {
			char    sbuf[80];
			char   *sval = sbuf;

			switch (pmon->value.type) {
			case 'b':
				sprintf(sval, "%ld", (long) pmon->value.v.v_char);
				break;
			case 'h':
				sprintf(sval, "%ld", (long) pmon->value.v.v_short);
				break;
			case 'i':
				sprintf(sval, "%ld", (long) pmon->value.v.v_int);
				break;
			case 'l':
				sprintf(sval, "%ld", (long) pmon->value.v.v_long);
				break;
			case 'B':
				sprintf(sval, "%lu", (ulong_t) pmon->value.v.v_uchar);
				break;
			case 'H':
				sprintf(sval, "%lu", (ulong_t) pmon->value.v.v_ushort);
				break;
			case 'I':
				sprintf(sval, "%lu", (ulong_t) pmon->value.v.v_uint);
				break;
			case 'L':
				sprintf(sval, "%lu", (ulong_t) pmon->value.v.v_ulong);
				break;
			case 'f':
				sprintf(sval, "%g", (double) pmon->value.v.v_float);
				break;
			case 'd':
				sprintf(sval, "%g", (double) pmon->value.v.v_double);
				break;
			default:
				sval = NULL;
				break;
			}
			if (sval)
				olog("announce ooid=%lu desc=[%s] to %d watchers len=%d val=[%s]",
					 pmon->ooid, pmon->quark.path, n, p, sval);
			else
				olog("announce ooid=%lu desc=[%s] to %d watchers len=%d",
					 pmon->ooid, pmon->quark.path, n, p);
		}
	}
	return qty;
}


oret_t
ohubHandleMonitoring(int kind, int type, int len, char *buf,
					  OhubAgent_t * pagent)
{
	OhubMonitorRecord_t *pmon;
	int     i, num, p;
	ooid_t ooid;
	ulong_t ulv;
	uchar_t ucv;
	char    v_type, v_undef;
	void   *v_ptr;
	short   v_len;
	long    v_time;
	char    err = 0;
	int     qty = 0;

	p = 0;
	num = (uchar_t) buf[p++];
	if (num < 1) {
		err = 1;
		goto FAIL;
	}
	for (i = 0; i < num; i++) {
		if (p + 6 > len) {
			err = 2;
			goto FAIL;
		}
		memcpy((void *) &ulv, (void *) (buf + p), 4);
		p += 4;
		ulv = ONTOHL(ulv);
		ooid = (ooid_t) ulv;
		ucv = buf[p++];
		v_type = (char) ucv;
		ucv = buf[p++];
		v_undef = ((ucv & 0x80) != 0);
		v_time = 0;
		if (ucv & 0x40) {
			memcpy((void *) &ulv, (void *) (buf + p), 4);
			p += 4;
			ulv = ONTOHL(ulv);
			v_time = (long) ulv;
		}
		v_len = (short) (ucv & 0x1f);
		if (ucv & 0x20) {
			ucv = buf[p++];
			v_len = (short) (v_len << 8) | (short) ucv;
		}
		if (v_undef) {
			v_ptr = NULL;
			if (v_len) {
				err = 3;
				goto FAIL;
			}
		} else {
			v_ptr = buf + p;
			p += v_len;
		}
		pmon = ohubFindMonitorByOoid(ooid);
		if (NULL == pmon) {
			ohubLog(5, "HMON: unsolicited ooid=%lu", ooid);
			continue;
		}
		ohubLog(11, "change: i=%d num=%d ooid=%lu len=%d undef=%d type='%c'",
				i, num, ooid, v_len, v_undef, v_type);
		if (v_type != pmon->quark.type) {
			/* type mismatch: agent was restarted */
			ohubLog(9, "suspicious change: v_type=%02xh vlen=%d i=%d num=%d"
					" ooid=%lu vtype=%c mytype=%c",
					(uchar_t) v_type, v_len, i, num, ooid,
					v_type ? v_type : '?', pmon->quark.type);
			v_undef = 1;
			v_ptr = NULL;
			v_len = 0;
		}
		if (v_type != 's' && v_len != pmon->quark.len) {
			/* length mismatch: fatal error */
			err = 4;
			goto FAIL;
		}
		if (p > len) {
			err = 5;
			goto FAIL;
		}
		/* we do not compare old and new values here */
		pmon->value.type = v_type;
		pmon->value.undef = v_undef;
		pmon->value.time = v_time;
		if (v_undef) {
			if (v_type != 's') {
				oxvzero(&pmon->value.v);
				pmon->value.len = v_len;
			} else {
				pmon->value.v.v_str = NULL;
				pmon->value.len = 0;
			}
		} else {
			if (v_type != 's') {
				oxbcopy(v_ptr, &pmon->value.v, v_len);
				ohubRotate(ohubSubjNeedsRotor(pagent->nsubj_p),
							v_type, v_len, (char *) &pmon->value.v);
				pmon->value.len = v_len;
			} else if (v_len) {
				/* finite string */
				if (pmon->buf_len >= v_len) {
					if (NULL == pmon->data_buf) {
						pmon->data_buf = oxnew(char, pmon->buf_len);
						if (NULL == pmon->data_buf) {
							err = 6;
							goto FAIL;
						}
					}
				} else {
					/* buffer is not enough */
					oxfree(pmon->data_buf);
					if (NULL == (pmon->data_buf = oxnew(char, v_len))) {
						err = 7;
						goto FAIL;
					}
					pmon->buf_len = v_len;
				}
				/* FIXME: v_len is short, what if it is negative ? */
				oxbcopy(v_ptr, pmon->data_buf, v_len);
				pmon->value.v.v_str = pmon->data_buf;
				pmon->value.len = v_len;
#if 0
				ohubLog(6, "change: i=%d num=%d ooid=%lu len=%d str=\"%s\"",
						i, num, ooid, v_len, pmon->value.v.v_str);
#endif
			} else {
				/* null string */
				pmon->value.v.v_str = NULL;
				pmon->value.len = 0;
			}
		}
		ohubAnnounceChange(pmon, NULL);
		qty++;
	}
	if (p != len) {
		err = 8;
		goto FAIL;
	}
	ohubLog(9, "monitoring qty=%d from <%s>", qty, pagent->agent_name);
	return OK;
  FAIL:
	ohubLog(4, "invalid change from <%s> type=%04xh len=%d err=%d",
			pagent->agent_name, type, len, (int) err);
	return ERROR;
}


oret_t
ohubMonGetValue(ooid_t ooid, oval_t * dst)
{
	const oval_t *src;
	OhubMonitorRecord_t *pmon = ohubFindMonitorByOoid(ooid);

	if (NULL == pmon)
		return ERROR;
	src = &pmon->value;
	if (src->type != 's' || src->undef || src->len <= 0) {
		oxvcopy(src, dst);
		if (src->len <= 0) {
			dst->len = 0;
			dst->undef = 1;
		}
		return OK;
	}
	if (src->len <= 0)
		dst->v.v_str = "";
	else {
		if (NULL == (dst->v.v_str = oxnew(char, src->len)))
			return ERROR;
		memcpy(dst->v.v_str, src->v.v_str, src->len);
	}
	dst->type = src->type;
	dst->undef = src->undef;
	dst->len = src->len;
	dst->time = src->time;
	return OK;
}


oret_t
ohubMonAgentRegister(OhubMonitorRecord_t * pmon)
{
	static const char *me = "monAgentRegister";
	ulong_t ulv;
	ushort_t usv;
	char    buf[24];
	oret_t rc;

	if (!*pmon->agent_p->agent_state) {
		ohubLog(4, "%s: quark ooid=%lu desc=[%s] agent=[%s] is off",
				me, pmon->ooid, pmon->quark.path, pmon->agent_p->agent_name);
		return ERROR;
	}
	if (!pmon->nsubj_p) {
		ohubLog(4, "%s: quark ooid=%lu desc=[%s] agent=[%s] no subj",
				me, pmon->ooid, pmon->quark.path, pmon->agent_p->agent_name);
		return ERROR;
	}
	ulv = (ulong_t) pmon->ooid;
	*(ulong_t *) (buf + 0) = OHTONL(ulv);
	ulv = (ulong_t) pmon->quark.phys_addr;
	*(ulong_t *) (buf + 4) = OHTONL(ulv);
	ulv = (ulong_t) pmon->quark.off;
	*(ulong_t *) (buf + 8) = OHTONL(ulv);
	usv = (ushort_t) pmon->quark.len;
	*(ushort_t *) (buf + 12) = OHTONS(usv);
	buf[14] = pmon->quark.ptr;
	buf[15] = pmon->quark.type;
	buf[16] = (uchar_t) pmon->quark.bit_off;
	buf[17] = (uchar_t) pmon->quark.bit_len;
	rc = ohubSubjSendPacket(pmon->nsubj_p,
							OLANG_DATA, OLANG_DATA_REG_REQ, 18, buf);
	ohubLog(6, "%s: ooid=%lu desc=[%s] agent={%s} rc=%d",
			me, pmon->ooid, pmon->quark.path, pmon->agent_p->agent_name, rc);
	return rc;
}


int
ohubMonRemoveMonitor(OhubMonitorRecord_t * pmon, const char *url)
{
	static const char *me = "monRemoveMonitor";
	oret_t rc;
	ulong_t ulv;
	OhubUnregAccum_t *pacc;

	if (--pmon->ref_count > 0) {
		ohubLog(7, "%s: watcher=[%s] ooid=%lu desc=[%s] refs=%d",
				me, url, pmon->ooid, pmon->quark.path, pmon->ref_count);
		return 0;
	}
	/* FIXME: do a lazy unregistration */
	treeNumAddOrSet(ohub_by_ooid_mon_hash, (int) pmon->ooid, 0);
	if (!*pmon->agent_p->agent_state || !pmon->nsubj_p) {
		ohubLog(7, "%s: quark ooid=%lu desc=[%s] last_watcher=[%s] "
				"agent=[%s] is off", me,
				pmon->ooid, pmon->quark.path, url, pmon->agent_p->agent_name);
		ohubDeallocateMonitor(pmon);
		return 1;
	}
	ulv = pmon->ooid;
	ulv = OHTONL(ulv);
	pacc = ohubSubjGetUnregAccum(pmon->nsubj_p);
	rc = OK;
	if ((pacc->qty >= OHUB_UNREG_ACCUM_SIZE)
		|| (pacc->qty > 0
			&& osMsClock() - pacc->start_ms > OHUB_UNREG_ACCUM_TTL))
		rc = ohubMonFlushUnregAccum(pmon->nsubj_p, pacc);
	if (!pacc->qty)
		pacc->start_ms = osMsClock();
	pacc->buf[1 + pacc->qty++] = ulv;
	ohubLog(6, "%s: ooid=%lu desc=[%s] last_watcher=[%s] subj_rc=%d",
			me, pmon->ooid, pmon->quark.path, url, rc);
	ohubDeallocateMonitor(pmon);
	return 1;
}


int
ohubMonRemoveWatch(struct OhubNetWatch_st *pnwatch, const char *url)
{
	OhubMonitorRecord_t *pmon;
	int     i, k, no, qty = 0, left = 0;

	if (!ohub_mons)
		return 0;
	for (no = 1; no < ohub_mon_qty; no++) {
		pmon = ohub_mons[no].mon_p;
		if (!pmon)
			continue;
		left++;
		for (i = 0; i < OHUB_MAX_WATCH_PER_MONITOR; i++) {
			if (pmon->watchers[i].nwatch_p != pnwatch)
				continue;
			qty++;
			pmon->watchers[i].nwatch_p = NULL;
			if (pmon->watch_qty == i + 1)
				pmon->watch_qty--;
			k = ohubMonRemoveMonitor(pmon, url);
			if (k == 1)
				left--;
			break;
		}
	}
	for (no = 1; no < ohub_mon_qty; no++) {
		pmon = ohub_mons[no].mon_p;
		if (!pmon)
			continue;
	}
	ohubLog(7, "%d monitors left", left);
	return qty;
}


int
ohubMonRenewWatch(struct OhubNetWatch_st *my_watcher, const char *url)
{
	int     qty = 0;
	OhubMonitorRecord_t *pmon;
	int     i, no;
	bool_t  filled;

	if (!ohub_mons || !my_watcher)
		return 0;
	ohubLog(2, "renewing monitors for [%s]", url);
	for (no = 1; no < ohub_mon_qty; no++) {
		pmon = ohub_mons[no].mon_p;
		if (!pmon)
			continue;
		/* FIXME: we could send undefined value */
		filled = !pmon->value.undef && pmon->value.len > 0;
		if (!filled)
			continue;
		for (i = 0; i < pmon->watch_qty; i++) {
			if (pmon->watchers[i].nwatch_p == my_watcher) {
				ohubAnnounceChange(pmon, my_watcher);
				qty++;
				break;
			}
		}
	}
	ohubLog(7, "renew %d monitors for [%s]", qty, url);
	return qty;
}


int
ohubUpdateMonAddresses(OhubDomain_t * pdom, int *mods, int mod_qty)
{
	OhubMonitorRecord_t *pmon;
	int     i, no, mod_no;
	int     qty = 0;

	if (!ohub_mons)
		return 0;
	for (no = 1; no < ohub_mon_qty; no++) {
		pmon = ohub_mons[no].mon_p;
		if (!pmon)
			continue;
		mod_no = pmon->quark.obj_ikey;
		for (i = 0; i < mod_qty; i++) {
			if (mod_no != mods[i])
				continue;
			if (ohubRefineQuarkAddress(pdom, &pmon->quark)) {
				if (ohub_verb > 5) {
					if (pmon->quark.phys_addr == -1)
						olog("mon_adr ooid=%lu desc=[%s] addr=NONE",
							 pmon->ooid, pmon->quark.path);
					else
						olog("mon_adr ooid=%lu desc=[%s] addr=%lxh",
							 pmon->ooid, pmon->quark.path,
							 pmon->quark.phys_addr);
				}
			}
			if (pmon->quark.phys_addr != -1) {
				if (!pmon->securing_flag) {
					pmon->securing_flag = TRUE;
					ohub_mon_securing_qty++;
				}
			}
			qty++;
			break;
		}
	}
	ohubLog(4, "need to secure %d mons", ohub_mon_securing_qty);
	return qty;
}


int
ohubSecureMoreMonitors(OhubDomain_t * pdom)
{
	OhubMonitorRecord_t *pmon;
	int     no, qty, gap, n_ofl, n_err, n_off, n_pend;
	oret_t rc;

	if (!ohub_mons || ohub_mon_securing_qty <= 0)
		return 0;
	qty = n_ofl = n_err = n_off = n_pend = 0;
	for (no = 1; no < ohub_mon_qty; no++) {
		if (qty >= OHUB_MON_SECURING_LIMIT || ohub_mon_securing_qty <= 0)
			break;
		pmon = ohub_mons[no].mon_p;
		if (!pmon || !pmon->securing_flag)
			continue;
		if (!*pmon->agent_p->agent_state || !pmon->nsubj_p) {
			n_off++;
			pmon->securing_flag = FALSE;
			qty++;
			ohub_mon_securing_qty--;
			continue;
		}
		if (pmon->agent_p->cur_reg_count > OHUB_MON_SECURING_LIMIT) {
			n_pend++;
			continue;
		}
		gap = ohubSubjSendQueueGap(pmon->nsubj_p);
		if (gap >= 0 && gap < 10) {
			n_ofl++;
			continue;
		}
		rc = ohubMonAgentRegister(pmon);
		if (rc != OK)
			n_err++;
		pmon->securing_flag = FALSE;
		pmon->agent_p->cur_reg_count++;
		qty++;
		ohub_mon_securing_qty--;
	}
	ohubLog((ohub_mon_securing_qty < 10 || qty > 10 ? 7 : 9),
			"secured %d mons (%d overs, %d errs, %d offs, %d pends), %d left",
			qty, n_ofl, n_err, n_off, n_pend, ohub_mon_securing_qty);
	return qty;
}


oret_t
ohubMonUnregister(ooid_t ooid, struct OhubNetWatch_st * pnwatch, const char *url)
{
	OhubMonitorRecord_t *pmon;
	char    reply[8];
	ulong_t ulv;
	int     i;

	ulv = (ulong_t) ooid;
	*(ulong_t *) (reply + 0) = OHTONL(ulv);
	if (!ooid) {
		goto FAIL;
	}
	pmon = ohubFindMonitorByOoid(ooid);
	if (!pmon) {
		goto FAIL;
	}
	for (i = pmon->watch_qty - 1; i >= 0; i--) {
		if (pmon->watchers[i].nwatch_p == pnwatch) {
			pmon->watchers[i].nwatch_p = NULL;
			if (pmon->watch_qty == i + 1)
				pmon->watch_qty--;
			break;
		}
	}
	if (i < 0) {
		goto FAIL;
	}
	ohubMonRemoveMonitor(pmon, url);
	ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_MON_UNREG_ACK, 4, reply);
	return OK;
  FAIL:
	ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_MON_UNREG_NAK, 4, reply);
	return ERROR;
}


oret_t
ohubMonRegisterInternal(const char *msg, ooid_t ooid,
						 struct OhubNetWatch_st * pnwatch,
						 const char *url, OhubMonitorRecord_t ** pp_mon)
{
	OhubDomain_t *pdom = ohub_pdomain;
	OhubMonitorRecord_t *pmon = NULL;
	char    desc[OHUB_MAX_PATH];
	char    sresult[OHUB_MAX_NAME];
	oret_t rc;
	int     i, key, val;
	OhubModule_t *pmod;
	int     mod_no, gap;

	if (pp_mon)
		*pp_mon = NULL;
	pmon = ohubFindMonitorByOoid(ooid);
	if (NULL == pmon) {
		/* create a new monitor */
		pmon = ohubAllocateMonitor();
		if (!pmon) {
			ohubLog(3, "mon_reg non space for [%s]", url);
			goto FAIL;
		}
		rc = ohubGetInfoByOoid(ooid, &pmon->quark, sresult);
		if (rc != OK) {
			ohubLog(3,
				  "mon_reg not resolved desc=[%s] ooid=%lu for [%s]",
				  desc, ooid, url);
			goto FAIL;
		}
		ohubLog(6, "mon_reg got-quark ooid=%lu desc=[%s] addr=%lxh",
				pmon->quark.ooid, pmon->quark.path, pmon->quark.phys_addr);
		mod_no = pmon->quark.obj_ikey;
		if (mod_no >= 0 && mod_no < pdom->module_qty) {
			pmod = &pdom->modules[mod_no];
			pmon->agent_p = pmod->psubject->pagent;
			pmon->nsubj_p = pmon->agent_p->nsubj_p;
		}
	}
	/* perhaps already attached */
	for (i = 0; i < OHUB_MAX_WATCH_PER_MONITOR; i++) {
		if (pmon->watchers[i].nwatch_p == pnwatch) {
			ohubLog(3,
				"mon_reg already registered [%s] with ooid=%lu desc=[%s]",
				url, ooid, pmon->quark.path);
			goto FAIL;
		}
	}
	/* attach to existing monitor */
	for (i = 0; i < OHUB_MAX_WATCH_PER_MONITOR; i++) {
		if (!pmon->watchers[i].nwatch_p)
			break;
	}
	if (i >= OHUB_MAX_WATCH_PER_MONITOR) {
		ohubLog(3, "mon_reg out of descriptors desc=[%s] ooid=%lu for [%s]",
				desc, ooid, url);
		goto FAIL;
	}
	pmon->watchers[i].nwatch_p = pnwatch;
	if (i >= pmon->watch_qty) {
		pmon->watch_qty = i + 1;
	}
	pmon->ref_count++;
	if (pmon->ref_count == 1) {
		pmon->value.undef = 1;
		pmon->ooid = ooid;
		key = (int) ooid;
		val = pmon->no;
		if (treeNumAddOrSet(ohub_by_ooid_mon_hash, key, val) != OK) {
			ohubLog(2, "cannot hash up mon key=%d val=%d", key, val);
			goto FAIL;
		}
		if (pmon->quark.phys_addr != -1) {
			if (*pmon->agent_p->agent_state && pmon->nsubj_p
				&& !pmon->securing_flag) {
				gap = ohubSubjSendQueueGap(pmon->nsubj_p);
				if (gap < 10
					|| pmon->agent_p->cur_reg_count >
					OHUB_MON_SECURING_LIMIT) {
					ohub_mon_securing_qty++;
					pmon->securing_flag = TRUE;
					ohubLog(6, "data_reg: postponed ooid=%lu desc=[%s]",
							ooid, pmon->quark.path);
				} else {
					rc = ohubMonAgentRegister(pmon);
					pmon->agent_p->cur_reg_count++;
				}
			}
		}
	}
	if (pp_mon)
		*pp_mon = pmon;
	if (ohub_verb > 5) {
		if (pmon->quark.phys_addr == -1)
			olog("%s OK desc=[%s] ooid=%lu watcher=[%s] addr=NONE refs=%d",
				 msg, pmon->quark.path, ooid, url, pmon->ref_count);
		else
			olog("%s OK desc=[%s] ooid=%lu watcher=[%s] "
				 "a=%lxh+%ld/%d bop=%d:%d t=%c p=%c refs=%d",
				 msg, pmon->quark.path, ooid, url,
				 pmon->quark.phys_addr, pmon->quark.off, pmon->quark.len,
				 pmon->quark.bit_off, pmon->quark.bit_len,
				 pmon->quark.type, pmon->quark.ptr, pmon->ref_count);
	}
	return OK;
  FAIL:
	if (pmon && !pmon->ref_count)
		ohubDeallocateMonitor(pmon);
	return ERROR;
}


oret_t
ohubMonRegister(ooid_t ooid, struct OhubNetWatch_st * pnwatch, const char *url)
{
	OhubMonitorRecord_t *pmon = NULL;
	char    reply[4];
	oret_t rc;

	if (ooid)
		rc = ohubMonRegisterInternal("mon_reg", ooid, pnwatch, url, &pmon);
	else {
		rc = ERROR;
		ohubLog(3, "mon_reg null ooid from [%s]", url);

	}
	*(ulong_t *) reply = OHTONL((ulong_t) ooid);
	ohubWatchSendPacket(pnwatch, OLANG_DATA,
						rc == OK ? OLANG_MON_REG_ACK : OLANG_MON_REG_NAK,
						4, reply);
	/* perhaps we can immediately send the value to watcher */
	if (rc == OK && !pmon->value.undef && pmon->value.len > 0)
		ohubAnnounceChange(pmon, pnwatch);
	return rc;
}


oret_t
ohubHandleMonComboRequest(struct OhubNetWatch_st * pnwatch, const char *url,
							int kind, int type, int len, char *buf)
{
	OhubMonitorRecord_t *pmon = NULL;
	ulong_t ulv;
	char    desc[OHUB_MAX_PATH + 4];
	OhubQuark_t quark;
	char    sresult[OHUB_MAX_NAME];
	char    reply[OHUB_MAX_PATH + 72];
	int     n;
	oret_t rc;
	bool_t  announce;

	ulv = *(ulong_t *) buf;
	*(ulong_t *) (reply + 0) = ulv;
	*(ulong_t *) (reply + 4) = (ulong_t) ERROR;
	n = (uchar_t) buf[4];
	if (len < 6 || n <= 0 || n >= OHUB_MAX_PATH) {
		ohubLog(3, "invalid MCREQ len=%d desc_length=%d from [%s]",
				len, n, url);
		return ERROR;
	}
	memcpy(desc, buf + 5, n);
	desc[n] = 0;
	if (n + 5 != len) {
		ohubLog(4, "MCREQ request length mismatch p=%d len=%d from [%s]",
				n + 5, len, url);
		return ERROR;
	}
	rc = ohubGetInfoByDesc(desc, &quark, sresult);
	n = ohubPackInfoReply(reply, 8, rc, &quark, sresult);
	announce = FALSE;
	if (rc == OK && quark.ooid) {
		rc = ohubMonRegisterInternal("mon_reg_combo", quark.ooid, pnwatch,
									url, &pmon);
		if (rc == OK) {
			*(ulong_t *) (reply + 4) = (ulong_t) OK;
			announce = !pmon->value.undef && pmon->value.len > 0;
		}
	}
	rc = ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_MON_COMBO_REPLY,
							n, reply);
	/* perhaps we can immediately send the value to watcher */
	if (announce)
		ohubAnnounceChange(pmon, pnwatch);
	return OK;
}


oret_t
ohubHandleMonRegistration(struct OhubNetWatch_st * pnwatch,
						   const char *url, int kind,
						   int type, int len, char *buf)
{
	ooid_t ooid;
	ulong_t ulv;

	if (len != 4) {
		ohubLog(3, "wrong mon_[un]reg len=%d", len);
		return ERROR;
	}
	ulv = *(ulong_t *) buf;
	ooid = (ooid_t) ONTOHL(ulv);
	switch (type) {
	case OLANG_MON_REG_REQ:
		return ohubMonRegister(ooid, pnwatch, url);
	case OLANG_MON_UNREG_REQ:
		return ohubMonUnregister(ooid, pnwatch, url);
	default:
		return ERROR;
	}
}


int
ohubHandleDataMonReply(OhubAgent_t * pagent, int kind, int type, int len,
						char *buf)
{
	ulong_t *lbuf = (ulong_t *) buf;
	int     i, k, m, n, p, ctype, ptype;
	ooid_t ooid;

	m = (int) ONTOHL(lbuf[0]);
	char    sbuf[180];
	if (m < 1 || m > 4000 || len != (m * 2 + 1) * sizeof(long)) {
		ohubLog(4, "wrong data_mon_reply len=%d m=%d from {%s}",
				len, m, pagent->agent_name);
		return 0;
	}
	n = m * 2 + 1;
	*sbuf = 0;
	p = 0;
	ptype = 0;
	k = 0;
	for (i = 1; i < n; i += 2) {
		ctype = ONTOHL(lbuf[i]);
		ooid = ONTOHL(lbuf[i + 1]);
		switch (ctype) {
		case OLANG_DATA_REG_ACK:
		case OLANG_DATA_REG_NAK:
			if (--pagent->cur_reg_count < 0)
				pagent->cur_reg_count = 0;
			k++;
			break;
		case OLANG_DATA_UNREG_ACK:
		case OLANG_DATA_UNREG_NAK:
			break;
		default:
			ohubLog(5, "wrong type %04x in data_mon_reply m=%d from <%s>",
					ctype, m, pagent->agent_name);
			continue;
		}
		if (ohub_verb < 5)
			continue;
		if (ctype != ptype || p > sizeof(sbuf) - 10) {
			if (p > 0) {
				strcat(sbuf, "]");
				olog(sbuf);
			}
			*sbuf = 0;
			switch (ctype) {
			case OLANG_DATA_REG_ACK:
				strcpy(sbuf, "data_mon_reply/data_reg_ack[ ");
				break;
			case OLANG_DATA_REG_NAK:
				strcpy(sbuf, "data_mon_reply/data_reg_nak[ ");
				break;
			case OLANG_DATA_UNREG_ACK:
				strcpy(sbuf, "data_mon_reply/data_unreg_ack[ ");
				break;
			case OLANG_DATA_UNREG_NAK:
				strcpy(sbuf, "data_mon_reply/data_unreg_nak[ ");
				break;
			}
			p = strlen(sbuf);
		}
		p += sprintf(sbuf + p, "%lu ", ooid);
	}
	if (p > 0) {
		strcat(sbuf, "]");
		olog(sbuf);
	}
	ohubLog(6, "data_mon_reply: m=%d k=%d agent=[%s] rcount=%d",
			m, k, pagent->agent_name, pagent->cur_reg_count);
	return 0;
}


oret_t
ohubInitMonitoring(void)
{
	ohub_by_ooid_mon_hash = treeAlloc(NUMERIC_TREE);
	if (!ohub_by_ooid_mon_hash)
		return ERROR;
	return OK;
}


oret_t
ohubExitMonitoring(void)
{
	int     i;

	if (ohub_mons) {
		for (i = 0; i < ohub_mon_qty; i++) {
			if (ohub_mons[i].mon_p)
				ohubDeallocateMonitor(ohub_mons[i].mon_p);
		}
		oxfree(ohub_mons);
	}
	ohub_mons = NULL;
	ohub_mon_qty = 0;
	ohub_mon_securing_qty = 0;
	if (ohub_by_ooid_mon_hash)
		treeFree(ohub_by_ooid_mon_hash);
	ohub_by_ooid_mon_hash = NULL;
	return OK;
}
