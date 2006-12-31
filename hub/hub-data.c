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

  Single shot reading and writing of data.

*/

#include "hub-priv.h"
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for OHTONL,... */
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxbcopy,... */
#include <string.h>				/* for memcpy */


#define OHUB_WRITE_SECURING_LIMIT   32

typedef struct OhubDataRecord_st
{
	bool_t  is_busy;
	int     no;
	bool_t  is_write;
	ulong_t id;
	ooid_t ooid;
	ulong_t op;
	OhubQuark_t quark;
	oval_t val;
	char   *data_buf;
	int     buf_len;
	OhubAgent_t *pagent;
	struct OhubNetWatch_st *pnwatch;
	bool_t  securing_flag;
} OhubDataRecord_t;


OhubDataRecord_t *ohub_data_chain;
int     ohub_data_chain_size;
int     ohub_data_chain_len;
ulong_t ohub_next_data_id;

int     ohub_write_securing_qty;


/*
 * .
 */
OhubDataRecord_t *
ohubAllocateData(void)
{
	int     no, new_size;
	OhubDataRecord_t *new_chain, *pdata;

	for (no = 1; no < ohub_data_chain_size; no++)
		if (!ohub_data_chain[no].is_busy)
			break;
	if (no >= ohub_data_chain_size) {
		new_size = ohub_data_chain_size * 2 + 4;
		if (new_size > 65535)
			new_size = 65535;
		if (new_size == ohub_data_chain_size)
			return NULL;
		new_chain = oxrenew(OhubDataRecord_t, new_size,
							ohub_data_chain_size, ohub_data_chain);
		if (NULL == new_chain)
			return NULL;
		ohub_data_chain = new_chain;
		ohub_data_chain_size = new_size;
	}
	pdata = &ohub_data_chain[no];
	oxvzero(pdata);
	pdata->is_busy = TRUE;
	pdata->no = no;
	if ((++ohub_next_data_id & 0xFFFF) == 0)
		++ohub_next_data_id;
	pdata->id = ((ohub_next_data_id & 0xFFFF) << 16) | no;
	ohub_data_chain_len++;
	return pdata;
}


oret_t
ohubDeallocateData(OhubDataRecord_t * pdata)
{
	if (NULL == pdata)
		return ERROR;
	if (pdata->securing_flag) {
		if (--ohub_write_securing_qty < 0)
			ohub_write_securing_qty = 0;
	}
	oxfree(pdata->data_buf);
	oxvzero(pdata);
	ohub_data_chain_len--;
	return OK;
}


OhubDataRecord_t *
ohubFindData(ulong_t id)
{
	int     no = id & 65535;
	OhubDataRecord_t *pdata;

	if (no <= 0 || no >= ohub_data_chain_size || !ohub_data_chain)
		return NULL;
	pdata = &ohub_data_chain[no];
	return (pdata->is_busy && pdata->id == id ? pdata : NULL);
}


oret_t
ohubCompleteWriteReply(OhubDataRecord_t * pdata, int err)
{
	char    reply[12];
	oret_t rc;

	if (!pdata || !pdata->pnwatch || pdata->is_write != TRUE)
		return ERROR;
	*(ulong_t *) (reply + 0) = OHTONL(pdata->op);
	*(ulong_t *) (reply + 4) = OHTONL(pdata->ooid);
	*(ulong_t *) (reply + 8) = OHTONL((ulong_t) err);
	rc = ohubWatchSendPacket(pdata->pnwatch, OLANG_DATA,
							OLANG_OOID_WRITE_REPLY, 12, reply);
	ohubLog(err ? 3 : 6,
			"ooid_write: ooid=%lu desc=[%s] err=%d send_rc=%d",
			pdata->ooid, pdata->quark.path, err, rc);
	ohubDeallocateData(pdata);
	return rc;
}


oret_t
ohubSendWriteRequest(OhubDataRecord_t * pdata)
{
	int     n;
	char    req_buf[128];
	char   *req = NULL;
	ulong_t ulv;
	ushort_t usv;
	oret_t rc;

	if (NULL == pdata || pdata->is_write != TRUE)
		return ERROR;
	n = pdata->val.len + 48;
	if (n < sizeof(req_buf)) {
		req = req_buf;
	} else {
		if (NULL == (req = oxnew(char, n))) {
			ohubLog(3, "ooid_write: cannot allocate n=%d ooid=%lu desc=[%s]",
					n, pdata->ooid, pdata->quark.path);
			return ERROR;
		}
	}

	ulv = pdata->id;
	*(ulong_t *) (req + 0) = OHTONL(ulv);
	ulv = pdata->val.time;
	*(ulong_t *) (req + 4) = OHTONL(ulv);
	ulv = pdata->quark.phys_addr;
	*(ulong_t *) (req + 8) = OHTONL(ulv);
	ulv = pdata->quark.off;
	*(ulong_t *) (req + 12) = OHTONL(ulv);
	usv = pdata->val.len;
	*(ushort_t *) (req + 16) = OHTONS(usv);
	*(req + 18) = pdata->quark.ptr;
	*(req + 19) = pdata->quark.type;
	*(req + 20) = (char) pdata->quark.bit_off;
	*(req + 21) = (char) pdata->quark.bit_len;
	*(ushort_t *) (req + 22) = 0;
	/* FIXME: pdata->val.len is short, "negative" values are "lost" */
	if (pdata->val.type == 's')
		oxbcopy(pdata->val.v.v_str, req + 24, pdata->val.len);
	else {
		oxbcopy((void *) &pdata->val.v, req + 24, pdata->val.len);
		ohubRotate(ohubSubjNeedsRotor(pdata->pagent->nsubj_p),
					pdata->quark.type, pdata->val.len, req + 24);
	}
	n = 24 + pdata->val.len;
	rc = ohubSubjSendPacket(pdata->pagent->nsubj_p, OLANG_DATA,
							OLANG_DATA_WRITE_REQ, n, req);
	if (rc != OK) {
		ohubLog(3, "ooid_write-snd_ER ooid=%lu desc=[%s] agent=[%s]"
				" pl=%d id=%lxh hang=%d pnwatch=%p",
				pdata->ooid, pdata->quark.path, pdata->pagent->agent_name,
				n, pdata->id, ohub_data_chain_len, pdata->pnwatch);
	} else {
		ohubLog(7, "ooid_write-snd_OK ooid=%lu desc=[%s] agent=[%s]"
				" pl=%d id=%lxh hang=%d pnwatch=%p",
				pdata->ooid, pdata->quark.path, pdata->pagent->agent_name,
				n, pdata->id, ohub_data_chain_len, pdata->pnwatch);
	}
	if (req != req_buf)
		oxfree(req);
	return rc;
}


oret_t
ohubSendSecureWriteRequest(OhubDataRecord_t * pdata)
{
	oret_t rc;
	int     gap;

	if (NULL == pdata || pdata->is_write != TRUE)
		return ERROR;
	if (NULL == pdata->pagent || !*pdata->pagent->agent_state
			|| NULL == pdata->pagent->nsubj_p)
		return ERROR;
	if (pdata->securing_flag)
		return OK;
	gap = ohubSubjSendQueueGap(pdata->pagent->nsubj_p);
	if (gap < 10 || pdata->pagent->cur_write_count > OHUB_WRITE_SECURING_LIMIT) {
		ohub_write_securing_qty++;
		pdata->securing_flag = TRUE;
		ohubLog(6, "ooid_write: postponed ooid=%lu desc=[%s]",
				pdata->ooid, pdata->quark.path);
		return OK;
	}
	rc = ohubSendWriteRequest(pdata);
	pdata->pagent->cur_write_count++;
	return rc;
}


int
ohubSecureMoreWrites(OhubDomain_t * pdom)
{
	OhubDataRecord_t *pdata;
	int     no, qty, gap, n_ofl, n_err, n_off, n_pend;
	oret_t rc;

	/* FIXME: order of writes is broken !!!!!
	 *        need a queue, not flagging !!!!!
	 */
	if (ohub_write_securing_qty <= 0
		|| !ohub_data_chain || ohub_data_chain_size <= 0)
		return 0;
	qty = n_ofl = n_err = n_off = n_pend = 0;
	for (no = 1; no < ohub_data_chain_size; no++) {
		if (qty >= OHUB_WRITE_SECURING_LIMIT || ohub_write_securing_qty <= 0)
			break;
		pdata = &ohub_data_chain[no];
		if (!pdata->is_busy || !pdata->is_write || !pdata->securing_flag)
			continue;
		if (!*pdata->pagent->agent_state || !pdata->pagent->nsubj_p) {
			if (pdata->securing_flag == ERROR) {
				ohubDeallocateData(pdata);
			}
			n_off++;
			continue;
		}
		if (pdata->pagent->cur_write_count > OHUB_WRITE_SECURING_LIMIT) {
			n_pend++;
			continue;
		}
		gap = ohubSubjSendQueueGap(pdata->pagent->nsubj_p);
		if (gap >= 0 && gap < 10) {
			n_ofl++;
			continue;
		}
		rc = ohubSendWriteRequest(pdata);
		if (rc != OK)
			n_err++;
		if (pdata->securing_flag == ERROR) {
			pdata->pagent->cur_write_count++;
			ohubDeallocateData(pdata);
		} else {
			pdata->securing_flag = FALSE;
			ohub_write_securing_qty--;
			pdata->pagent->cur_write_count++;
		}
		qty++;
	}
	ohubLog((ohub_write_securing_qty < 20 || qty > 10 ? 7 : 9),
			"securing %d writes (%d overs, %d errs, %d offs, %d pends),"
			" still have %d, %d hangs",
			qty, n_ofl, n_err, n_off, n_pend, ohub_write_securing_qty,
			ohub_data_chain_len);
	return qty;
}


oret_t
ohubHandleOoidWrite(struct OhubNetWatch_st * pnwatch,
					ushort_t watch_id, const char *url,
					int kind, int type, int len, char *buf)
{
	OhubDomain_t *pdom = ohub_pdomain;
	char    sresult[OHUB_MAX_NAME];
	int     p;
	oret_t rc;
	int     err;
	uchar_t ucv;
	ulong_t ulv;
	int     mod_no;
	OhubModule_t *pmod;
	OhubDataRecord_t *pdata;

	pdata = ohubAllocateData();
	if (!pdata) {
		ohubLog(3, "ooid_write: cannot allocate data for [%s]", url);
		/* FIXME: need robust OLANG error codes */
		return ERROR;
	}
	pdata->pnwatch = pnwatch;
	pdata->is_write = TRUE;
	/* parse out ID */
	if (len < 7) {
		ohubLog(3, "ooid_write: bad len=%d from [%s]", len, url);
		/* FIXME: need robust OLANG error codes */
		err = ERROR;
		goto FAIL;
	}
	ulv = *(ulong_t *) (buf + 0);
	pdata->op = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 4);
	pdata->ooid = (ooid_t) ONTOHL(ulv);
	p = 8;
	/* find quark */
	rc = ohubGetInfoByOoid(pdata->ooid, &pdata->quark, sresult);
	if (rc != OK) {
		ohubLog(3, "ooid_write: not resolved ooid=%lu from [%s]",
				pdata->ooid, url);
		err = ERROR;
		goto FAIL;
	}
	/* check validity */
	ucv = buf[p++];
	pdata->val.type = (char) ucv;
	ucv = buf[p++];
	pdata->val.undef = ((ucv & 0x80) != 0);
	pdata->val.time = 0;
	if (ucv & 0x40) {
		memcpy((void *) &ulv, (void *) (buf + p), 4);
		p += 4;
		pdata->val.time = (long) ONTOHL(ulv);
	}
	pdata->val.len = (short) (ucv & 0x1f);
	if (ucv & 0x20) {
		ucv = buf[p++];
		pdata->val.len = (short) (pdata->val.len << 8) | (short) ucv;
	}
	if (pdata->val.undef) {
		ohubLog(3, "ooid_write: must be defined desc=[%s] ooid=%lu from [%s]",
				pdata->quark.path, pdata->ooid, url);
		err = ERROR;
		goto FAIL;
	}
	if (pdata->val.type != pdata->quark.type) {
		ohubLog(3, "ooid_write: type mismatch desc=[%s] ooid=%lu from [%s]"
				" must=%c have=%02xh(%c)",
				pdata->quark.path, pdata->ooid, url, pdata->quark.type,
				(int) (uchar_t) pdata->val.type,
				pdata->val.type ? pdata->val.type : '?');
		err = ERROR;
		goto FAIL;
	}
	if (pdata->val.type == 's') {
		if (pdata->val.len > pdata->quark.len) {
			/* FIXME: what about pointer strings ? */
			ohubLog(3, "ooid_write: gross string desc=[%s] ooid=%lu from [%s]",
					pdata->quark.path, pdata->ooid, url);
			err = ERROR;
			goto FAIL;
		}
	} else {
		if (pdata->val.len != pdata->quark.len) {
			ohubLog(3,
					"ooid_write: length mismatch desc=[%s] ooid=%lu from [%s]",
					pdata->quark.path, pdata->ooid, url);
			err = ERROR;
			goto FAIL;
		}
	}
	/* FIXME: pdata->val.len is short, a range of "negative" values is lost */
	if (pdata->val.len <= 0) {
		ohubLog(3, "ooid_write: zero length desc=[%s] ooid=%lu from [%s]",
				pdata->quark.path, pdata->ooid, url);
		err = ERROR;
		goto FAIL;
	}
	if (p + pdata->val.len != len) {
		ohubLog(3,
			"ooid_write: packet size mismatch desc=[%s] ooid=%lu from [%s]",
			pdata->quark.path, pdata->ooid, url);
		err = ERROR;
		goto FAIL;
	}
	/* get value */
	if (pdata->val.type == 's') {
		if (NULL == (pdata->data_buf = oxnew(char, pdata->val.len))) {
			ohubLog(2, "ooid_write: cannot allocate "
					"vlen=%d desc=[%s] ooid=%lu from [%s]",
					pdata->val.len, pdata->quark.path, pdata->ooid, url);
			err = ERROR;
			goto FAIL;
		}
		pdata->buf_len = pdata->val.len;
		memcpy(pdata->data_buf, buf + p, pdata->val.len);
		pdata->val.v.v_str = pdata->data_buf;
	} else {
		memcpy((void *) &pdata->val.v, buf + p, pdata->val.len);
		ohubRotate(ohubWatchNeedsRotor(pnwatch),
					pdata->val.type, pdata->val.len, (char *) &pdata->val.v);
	}
	/* find appropriate subj */
	mod_no = pdata->quark.obj_ikey;
	if (mod_no < 0 || mod_no >= pdom->module_qty) {
		ohubLog(2, "ooid_write: quark ooid=%lu desc=[%s] invalid mod_no=%d",
				pdata->ooid, pdata->quark.path, mod_no);
		err = ERROR;
		goto FAIL;
	}
	pmod = &pdom->modules[mod_no];
	pdata->pagent = pmod->psubject->pagent;
	if (NULL == pdata->pagent->nsubj_p) {
		ohubLog(3, "ooid_write: quark ooid=%lu desc=[%s] agent=[%s] no subj",
				pdata->ooid, pdata->quark.path, pdata->pagent->agent_name);
		err = ERROR;
		goto FAIL;
	}
	if (*pdata->pagent->agent_state && pdata->quark.phys_addr != -1) {
		rc = ohubSendSecureWriteRequest(pdata);
		if (rc != OK) {
			err = ERROR;
			goto FAIL;
		}
	} else {
		ohubLog(5, "ooid_write: hang=%d ooid=%lu desc=[%s] agent=[%s]"
				" id=%lxh pnwatch=%p",
				ohub_data_chain_len, pdata->ooid, pdata->quark.path,
				pdata->pagent->agent_name, pdata->id, pdata->pnwatch);
	}
	return OK;
  FAIL:
	ohubCompleteWriteReply(pdata, err);
	return ERROR;
}


oret_t
ohubHandleDataWrite(OhubAgent_t * pagent,
					 int kind, int type, int len, char *buf)
{
	OhubDataRecord_t *pdata;
	ulong_t id;
	oret_t rc;
	int     err;

	if (len != 6) {
		ohubLog(3, "DWRITE invalid len=%d from <%s>", len, pagent->agent_name);
		return ERROR;
	}
	id = ONTOHL(*(ulong_t *) (buf + 0));
	err = (int) (short) ONTOHS(*(ushort_t *) (buf + 4));
	pdata = ohubFindData(id);
	if (!pdata || pdata->is_write != TRUE) {
		ohubLog(5, "DWRITE invalid id=%lxh from <%s> p=%p is_w=%d",
				id, pagent->agent_name, pdata, pdata ? pdata->is_write : 0);
		return ERROR;
	}
	rc = ohubCompleteWriteReply(pdata, err);
	return rc;
}


oret_t
ohubCompleteReadReply(OhubDataRecord_t * pdata, int err)
{
	int     plen;
	char    reply_buf[128];
	char   *reply;
	oret_t rc;

	if (NULL == pdata || NULL == pdata->pnwatch || pdata->is_write != FALSE)
		return ERROR;
	plen = err ? 16 : 20 + pdata->val.len;
	if (plen < sizeof(reply_buf))
		reply = reply_buf;
	else {
		if (NULL == (reply = oxnew(char, plen))) {
			ohubLog(3,
				"ooid_read: ooid=%lu desc=[%s] err=%d cannot allocate n=%d",
				pdata->ooid, pdata->quark.path, err, plen);
			ohubDeallocateData(pdata);
			return ERROR;
		}
	}
	*(ulong_t *) (reply + 0) = OHTONL(pdata->op);
	*(ulong_t *) (reply + 4) = OHTONL(pdata->ooid);
	*(ulong_t *) (reply + 8) = OHTONL((ulong_t) pdata->val.time);
	*(ulong_t *) (reply + 12) = OHTONL((ulong_t) err);
	if (!err) {
		*(reply + 16) = pdata->val.type;
		*(reply + 17) = 0;
		*(ushort_t *) (reply + 18) = OHTONS((ushort_t) pdata->val.len);
		if (pdata->val.type == 's')
			oxbcopy(pdata->val.v.v_str, reply + 20, pdata->val.len);
		else {
			oxbcopy((void *) &pdata->val.v, reply + 20, pdata->val.len);
			ohubRotate(ohubWatchNeedsRotor(pdata->pnwatch),
						pdata->val.type, pdata->val.len, reply + 20);
		}
	}
	rc = ohubWatchSendPacket(pdata->pnwatch, OLANG_DATA,
							OLANG_OOID_READ_REPLY, plen, reply);
	ohubLog((err ? 4 : 6),
			"ooid_read: ooid=%lu desc=[%s] err=%d plen=%d snd_rc=%d",
			pdata->ooid, pdata->quark.path, err, plen, rc);
	if (reply != reply_buf)
		oxfree(reply);
	ohubDeallocateData(pdata);
	return rc;
}


oret_t
ohubSendReadRequest(OhubDataRecord_t * pdata)
{
	ulong_t ulv;
	ushort_t usv;
	char    req[24];
	oret_t rc;

	if (NULL == pdata || pdata->is_write != FALSE)
		return ERROR;
	ulv = pdata->id;
	*(ulong_t *) (req + 0) = OHTONL(ulv);
	ulv = pdata->val.time;
	*(ulong_t *) (req + 4) = OHTONL(ulv);
	ulv = pdata->quark.phys_addr;
	*(ulong_t *) (req + 8) = OHTONL(ulv);
	ulv = pdata->quark.off;
	*(ulong_t *) (req + 12) = OHTONL(ulv);
	usv = pdata->val.len;
	*(ushort_t *) (req + 16) = OHTONS(usv);
	*(req + 18) = pdata->quark.ptr;
	*(req + 19) = pdata->quark.type;
	*(req + 20) = (char) pdata->quark.bit_off;
	*(req + 21) = (char) pdata->quark.bit_len;
	*(ushort_t *) (req + 22) = 0;
	rc = ohubSubjSendPacket(pdata->pagent->nsubj_p, OLANG_DATA,
							OLANG_DATA_READ_REQ, 24, req);
	if (rc != OK) {
		ohubLog(3, "ooid_read-snd_ER ooid=%lu desc=[%s] agent=[%s] id=%lxh",
				pdata->ooid, pdata->quark.path, pdata->pagent->agent_name,
				pdata->id);
	} else {
		ohubLog(7, "ooid_read-snd_OK ooid=%lu desc=[%s] agent=[%s] id=%lxh",
				pdata->ooid, pdata->quark.path, pdata->pagent->agent_name,
				pdata->id);
	}
	return rc;
}


oret_t
ohubHandleOoidRead(struct OhubNetWatch_st * pnwatch,
				   ushort_t watch_id, const char *url,
				   int kind, int type, int len, char *buf)
{
	OhubDomain_t *pdom = ohub_pdomain;
	char    sresult[OHUB_MAX_NAME];
	oret_t rc;
	int     err;
	ulong_t ulv;
	int     mod_no;
	OhubModule_t *pmod;
	OhubDataRecord_t *pdata;

	pdata = ohubAllocateData();
	if (!pdata) {
		ohubLog(3, "ooid_read: cannot allocate data for [%s]", url);
		/* FIXME: need robust OLANG error codes */
		return ERROR;
	}
	pdata->pnwatch = pnwatch;
	pdata->is_write = FALSE;
	/* parse out ID */
	if (len != 12) {
		ohubLog(3, "ooid_read: bad len=%d from [%s]", len, url);
		/* FIXME: need robust OLANG error codes */
		err = ERROR;
		goto FAIL;
	}
	ulv = *(ulong_t *) (buf + 0);
	pdata->op = ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 4);
	pdata->ooid = (ooid_t) ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 8);
	pdata->val.time = (long) ONTOHL(ulv);
	/* try to read data from existing monitor */
	if (ohubMonGetValue(pdata->ooid, &pdata->val) == OK) {
		if (pdata->val.undef == 0) {
			if (pdata->val.type == 's') {
				pdata->data_buf = pdata->val.v.v_str;
				pdata->buf_len = pdata->val.len;
			}
			ohubLog(7, "ooid_read got monitor ooid=%lu desc=[%s] op=%lxh"
					" v(l=%d,u=%d,t=%c) OK",
					pdata->ooid, pdata->quark.path, pdata->op,
					pdata->val.len, pdata->val.undef,
					pdata->val.type ? pdata->val.type : '?');
			ohubCompleteReadReply(pdata, 0);
			return OK;
		}
	}
	/* find a quark */
	rc = ohubGetInfoByOoid(pdata->ooid, &pdata->quark, sresult);
	if (rc != OK) {
		ohubLog(3, "ooid_read: not resolved ooid=%lu watcher=[%s] sres=[%s]",
				pdata->ooid, url, sresult);
		err = ERROR;
		goto FAIL;
	}
	pdata->val.type = pdata->quark.type;
	pdata->val.len = pdata->quark.len;
	pdata->val.undef = 1;
	/* find appropriate subj */
	mod_no = pdata->quark.obj_ikey;
	if (mod_no < 0 || mod_no >= pdom->module_qty) {
		ohubLog(2, "ooid_read: quark ooid=%lu desc=[%s] invalid mod_no=%d",
				pdata->ooid, pdata->quark.path, mod_no);
		err = ERROR;
		goto FAIL;
	}
	pmod = &pdom->modules[mod_no];
	pdata->pagent = pmod->psubject->pagent;
	if (!pdata->pagent->nsubj_p) {
		ohubLog(3, "ooid_read: quark ooid=%lu desc=[%s] agent=[%s] no subj",
				pdata->ooid, pdata->quark.path, pdata->pagent->agent_name);
		err = ERROR;
		goto FAIL;
	}
	if (*pdata->pagent->agent_state && pdata->quark.phys_addr != -1) {
		rc = ohubSendReadRequest(pdata);
		if (rc != OK) {
			err = ERROR;
			goto FAIL;
		}
	} else {
		ohubLog(5, "ooid_read: quark ooid=%lu desc=[%s] agent=[%s] "
				"id=%lxh hanging %d",
				pdata->ooid, pdata->quark.path, pdata->pagent->agent_name,
				pdata->id, ohub_data_chain_len);
	}
	return OK;
  FAIL:
	ohubCompleteReadReply(pdata, err);
	return ERROR;
}


oret_t
ohubHandleDataRead(OhubAgent_t * pagent,
					int kind, int type, int len, char *buf)
{
	short   v_len;
	oret_t rc;
	ulong_t id;
	int     err;
	ulong_t ulv;
	ushort_t usv;
	OhubDataRecord_t *pdata;
	int     n;

	if (len < 6) {
		ohubLog(3, "DREAD tiny len=%d from <%s>", len, pagent->agent_name);
		return ERROR;
	}
	ulv = *(ulong_t *) (buf + 0);
	id = ONTOHL(ulv);
	usv = *(ushort_t *) (buf + 4);
	err = (int) (short) ONTOHS(usv);
	pdata = ohubFindData(id);
	if (!pdata || pdata->is_write != FALSE) {
		ohubLog(5, "DREAD invalid id=%lxh from <%s>", id, pagent->agent_name);
		return ERROR;
	}
	if (err) {
		v_len = 0;
		n = 6;
	} else {
		usv = *(ushort_t *) (buf + 6);
		v_len = (short) ONTOHS(usv);
		if (v_len != pdata->quark.len) {
			ohubLog(5, "DREAD id=%lxh length mismatch %hd <> %d from <%s>",
					id, v_len, pdata->quark.len, pagent->agent_name);
			ohubCompleteReadReply(pdata, -1);
			return ERROR;
		}
		n = 8 + v_len;
	}
	if (n != len) {
		ohubLog(3, "DREAD invalid len=%d from <%s> vlen=%hd",
				len, pagent->agent_name, v_len);
		ohubCompleteReadReply(pdata, -1);
		return ERROR;
	}
	/* FIXME: v_len is short, a good range of "negative" sizes is lost */
	if (v_len <= 0) {
		pdata->val.undef = 1;
		pdata->val.type = pdata->quark.type;
		pdata->val.v.v_ulong = 0;
	} else {
		pdata->val.undef = 0;
		pdata->val.type = pdata->quark.type;
		if (pdata->val.type == 's') {
			pdata->val.len = pdata->buf_len = v_len;
			if (NULL == (pdata->data_buf = oxnew(char, v_len))) {
				ohubCompleteReadReply(pdata, -1);
				return ERROR;
			}
			memcpy(pdata->data_buf, buf + 8, v_len);
			pdata->val.v.v_str = pdata->data_buf;
		} else {
			memcpy((void *) &pdata->val.v, buf + 8, v_len);
			ohubRotate(ohubSubjNeedsRotor(pagent->nsubj_p),
						pdata->val.type, v_len, (char *) &pdata->val.v);
		}
	}
	rc = ohubCompleteReadReply(pdata, err);
	return rc;
}


int
ohubCancelAgentData(OhubAgent_t * pagent)
{
	OhubDataRecord_t *pdata;
	int     no, qty = 0;

	if (NULL == pagent)
		return 0;
	pdata = &ohub_data_chain[0];
	for (no = 0; no < ohub_data_chain_size; no++, pdata++) {
		if (pdata->is_busy && pdata->pagent == pagent) {
			if (pdata->is_write) {
				ohubCompleteWriteReply(pdata, -1);
			} else {
				ohubCompleteReadReply(pdata, -1);
			}
			qty++;
		}
	}
	ohubLog(4, "cancelled %d datas, %d hanging, agent=[%s]",
			qty, ohub_data_chain_len, pagent->agent_name);
	return qty;
}


int
ohubCancelWatchData(struct OhubNetWatch_st *pnwatch, const char *url)
{
	OhubDataRecord_t *pdata;
	int     no, qty = 0, secured_qty = 0;

	if (!pnwatch)
		return 0;
	pdata = &ohub_data_chain[0];
	for (no = 0; no < ohub_data_chain_size; no++, pdata++) {
		if (pdata->is_busy && pdata->pnwatch == pnwatch) {
			if (pdata->securing_flag && *pdata->pagent->agent_state
				&& pdata->pagent->nsubj_p) {
				pdata->securing_flag = ERROR;	/* it means: pre-zombie */
				secured_qty++;
			} else {
				ohubDeallocateData(pdata);
				qty++;
			}
		}
	}
	ohubLog(4, "data cancelled=%d secured=%d hang=%d watcher=%p=[%s]",
			qty, secured_qty, ohub_data_chain_len, pnwatch, url);
	return qty;
}


int
ohubUpdateDataAddresses(OhubDomain_t * pdom, int *mods, int mod_qty)
{
	OhubDataRecord_t *pdata;
	OhubModule_t *pmod;
	int     i, no, mod_no;
	char    seg;
	long    seg_addr, phys_addr;
	oret_t rc;
	int     qty = 0;

	if (!ohub_data_chain)
		return 0;
	pdata = &ohub_data_chain[0];
	for (no = 0; no < ohub_data_chain_size; no++, pdata++) {
		if (!pdata->is_busy)
			continue;
		mod_no = pdata->quark.obj_ikey;
		for (i = 0; i < mod_qty; i++) {
			if (mod_no != mods[i])
				continue;
			pmod = &pdom->modules[mod_no];
			seg = pdata->quark.seg;
			if (!SEG_IS_VALID(seg))
				phys_addr = -1;
			else {
				seg_addr = pmod->segment_addr[SEG_NAME_2_NO(seg)];
				if (seg_addr == -1)
					phys_addr = -1;
				else
					phys_addr = seg_addr + pdata->quark.adr;
			}
			if (phys_addr != pdata->quark.phys_addr) {
				pdata->quark.phys_addr = phys_addr;
				if (ohub_verb > 5) {
					if (phys_addr == -1)
						olog("data_adr wr=%d ooid=%lu desc=[%s] addr=NONE",
							pdata->is_write, pdata->ooid, pdata->quark.path);
					else
						olog("data_adr wr=%d ooid=%lu desc=[%s] addr=%lxh",
							pdata->is_write, pdata->ooid, pdata->quark.path,
							phys_addr);
				}
			}
			if (pdata->quark.phys_addr != -1 && *pdata->pagent->agent_state) {
				if (pdata->is_write) {
					rc = ohubSendSecureWriteRequest(pdata);
					if (rc != OK)
						ohubCompleteWriteReply(pdata, -1);
				} else {
					rc = ohubSendReadRequest(pdata);
					if (rc != OK)
						ohubCompleteReadReply(pdata, -1);
				}
				qty++;
			}
		}
	}
	ohubLog(4, "updated %d datas, %d hanging", qty, ohub_data_chain_len);
	return qty;
}


oret_t
ohubInitData(void)
{
	ohubExitData();
	ohub_next_data_id = osMsClock();
	return OK;
}


oret_t
ohubExitData(void)
{
	int     no;
	OhubDataRecord_t *pdata;

	if (NULL != (pdata = ohub_data_chain)) {
		for (no = 0; no < ohub_data_chain_size; no++, pdata++) {
			if (pdata->is_write) {
				ohubCompleteWriteReply(pdata, -1);
			} else {
				ohubCompleteReadReply(pdata, -1);
			}
		}
	}
	oxfree(ohub_data_chain);
	ohub_data_chain = NULL;
	ohub_data_chain_size = ohub_data_chain_len = 0;
	ohub_write_securing_qty = 0;
	return OK;
}
