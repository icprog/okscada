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

  Data monitoring module.

*/

#include "watch-priv.h"
#include <optikus/tree.h>
#include <optikus/conf-net.h>	/* for ONTOHL */
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxbcopy,... */
#include <string.h>				/* for strlen,strcpy */


#define OWATCH_FAST_REGISTRATION 1

typedef struct OwatchMonitorRecord_st
{
	int     no;
	ooid_t  ooid;
	int     ref_count;
	int     active_count;
	int     update_count;
	int     incoming_count;
	bool_t  registered;
	owquark_t info;
	oval_t  value;
	char   *data_buf;
	int     buf_len;
	long    user_data;
	owop_t  reg_ops[OWATCH_MAX_OP_PER_MONITOR];
	int     reg_op_qty;
	owop_t  val_ops[OWATCH_MAX_OP_PER_MONITOR];
	int     val_op_qty;
	owop_t  secure_op;
	ooid_t *user_ooid_buf;
} OwatchMonitorRecord_t;


typedef struct
{
	OwatchDataHandler_t phand;
	long    param;
} OwatchDataHandlerRecord_t;


typedef struct
{
	OwatchMonitorRecord_t *mon_p;
} OwatchMonitorListEntry_t;


typedef struct
{
	ooid_t ooid;
	owquark_t info;
} OwatchOoidAndInfo_t;


OwatchMonitorListEntry_t *owatch_mons;
int     owatch_mon_qty;
tree_t  owatch_by_ooid_mon_hash;

int     owatch_mon_update_count;

OwatchDataHandlerRecord_t *owatch_data_handlers;

bool_t  owatch_monitoring_active;

int     owatch_mon_secure_count;

bool_t  owatch_mon_all_blocked;


static void
owatchLazyCallDataHandlers(OwatchMonitorRecord_t * pmon)
{
	if (owatch_handlers_are_lazy) {
		owatchRecordLazyCall(1, (long) pmon->ooid);
	} else {
		int     i, n = 0;
		OwatchDataHandlerRecord_t *hrec;

		hrec = &owatch_data_handlers[0];
		for (i = 0; i < OWATCH_MAX_DATA_HANDLERS; i++, hrec++) {
			if (hrec->phand != NULL) {
				(*hrec->phand) (hrec->param, pmon->ooid, &pmon->value);
				n++;
			}
		}
	}
}


void
owatchCallDataHandlers(long p)
{
	OwatchMonitorRecord_t *pmon = owatchFindMonitorByOoid((ooid_t) p);
	int     i, n = 0;
	OwatchDataHandlerRecord_t *hrec;

	if (!pmon)
		return;
	hrec = &owatch_data_handlers[0];
	for (i = 0; i < OWATCH_MAX_DATA_HANDLERS; i++, hrec++) {
		if (hrec->phand != NULL) {
			(*hrec->phand) (hrec->param, pmon->ooid, &pmon->value);
			n++;
		}
	}
}


int
owatchAnnounceMonitor(OwatchMonitorRecord_t * pmon, bool_t later)
{
	int     i;

	if (!pmon->incoming_count)
		return -1;
	if (pmon->val_op_qty) {
		for (i = 0; i < pmon->val_op_qty; i++) {
			if (pmon->val_ops[i]) {
				owatchFinalizeOp(pmon->val_ops[i], OWATCH_ERR_OK);
				pmon->val_ops[i] = 0;
			}
		}
	}
	if (owatch_monitoring_active && pmon->active_count && !later) {
		owatchLazyCallDataHandlers(pmon);
		return 1;
	} else if (pmon->active_count) {
		pmon->update_count++;
		owatch_mon_update_count++;
		return -2;
	} else {
		owatch_mon_update_count += 1 - pmon->update_count;
		pmon->update_count = 1;
		return -3;
	}
}


OwatchMonitorRecord_t *
owatchFindMonitorByOoid(ooid_t ooid)
{
	int     key = (int) ooid;
	int     no = 0;
	bool_t  found;

	if (!owatch_mons || !owatch_by_ooid_mon_hash || !ooid)
		return NULL;
	found = treeNumFind(owatch_by_ooid_mon_hash, key, &no);
	if (!found || no <= 0 || no >= owatch_mon_qty)
		return NULL;
	if (!owatch_mons[no].mon_p || owatch_mons[no].mon_p->ooid != ooid)
		return NULL;
	return owatch_mons[no].mon_p;
}


OwatchMonitorRecord_t *
owatchAllocateMonitor(void)
{
	OwatchMonitorRecord_t *pmon;
	OwatchMonitorListEntry_t *new_mons;
	int     new_qty, no;

	if (NULL == (pmon = oxnew(OwatchMonitorRecord_t, 1)))
		return NULL;
	for (no = 1; no < owatch_mon_qty; no++) {
		if (NULL == owatch_mons[no].mon_p) {
			owatch_mons[no].mon_p = pmon;
			pmon->no = no;
			return pmon;
		}
	}
	no = owatch_mon_qty;
	if (no <= 0)
		no = 1;

	new_qty = owatch_mon_qty * 2 + 2;
	new_mons = oxrenew(OwatchMonitorListEntry_t, new_qty,
						owatch_mon_qty, owatch_mons);
	if (NULL == new_mons) {
		oxfree(pmon);
		return NULL;
	}
	owatch_mons = new_mons;
	owatch_mon_qty = new_qty;

	owatch_mons[no].mon_p = pmon;
	pmon->no = no;
	pmon->secure_op = OWOP_NULL;
	return pmon;
}


void
owatchDeallocateMonitor(OwatchMonitorRecord_t * pmon)
{
	int     no = pmon->no;

	if (pmon->secure_op != OWOP_NULL && pmon->secure_op != OWOP_SELF)
		owatchCancelOp(pmon->secure_op);
	owatch_mons[no].mon_p = NULL;
	oxfree(pmon->data_buf);
	oxvzero(pmon);
	oxfree(pmon);
}


oret_t
owatchHandleMonitoring(int kind, int type, int len, char *buf)
{
	OwatchMonitorRecord_t *pmon;
	int     i, num, p;
	ooid_t ooid;
	ulong_t ulv;
	uchar_t ucv;
	char    v_type, v_undef;
	void   *v_ptr;
	short   v_len;
	long    v_time;

	p = 0;
	num = (uchar_t) buf[p++];
	if (num < 1) {
		owatchLog(4, "invalid change: num=%d", num);
		goto FAIL;
	}
	if (owatch_mon_all_blocked) {
		owatchLog(5, "monitoring change blocked");
		return OK;
	}
	for (i = 0; i < num; i++) {
		if (p + 6 > len) {
			owatchLog(4, "invalid change: bof1 p=%d len=%d i=%d num=%d",
						p, len, i, num);
			goto FAIL;
		}
		oxbcopy(buf + p, &ulv, 4);
		p += 4;
		ulv = ONTOHL(ulv);
		ooid = (ooid_t) ulv;
		ucv = buf[p++];
		v_type = (char) ucv;
		ucv = buf[p++];
		v_undef = ((ucv & 0x80) != 0);
		v_time = 0;
		if (ucv & 0x40) {
			oxbcopy(buf + p, &ulv, 4);
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
			if (v_len) {
				owatchLog(4,
					"invalid change: v_undef=%d v_len=%d i=%d num=%d ooid=%lu",
					v_undef, v_len, i, num, ooid);
				goto FAIL;
			}
			v_ptr = NULL;
		} else {
			v_ptr = buf + p;
			p += v_len;
		}
		pmon = owatchFindMonitorByOoid(ooid);
		if (!pmon) {
			owatchLog(5, "HMON: unsolicited ooid %lu i=%d num=%d",
						ooid, i, num);
			continue;
		}
		if (v_type != pmon->info.type) {
			/* type mismatch: agent was restarted */
			owatchLog(6, "suspicious change: v_type=%02xh vlen=%d i=%d "
						"num=%d ooid=%lu vtype=%c mytype=%c",
						(uchar_t) v_type, v_len, i, num, ooid,
						v_type ?: '?', pmon->info.type);
			v_undef = 1;
			v_ptr = NULL;
			v_len = 0;
		}
		if (v_type != 's' && !v_undef && v_len != pmon->info.len) {
			/* length mismatch: fatal error */
			owatchLog(4, "invalid change: v_type=%c v_len=%d need_len=%d "
						"i=%d num=%d ooid=%lu",
						v_undef, v_len, pmon->info.len, i, num, ooid);
			goto FAIL;
		}
		if (p > len) {
			owatchLog(4,
					"invalid change: bof2 p=%d len=%d i=%d num=%d ooid=%lu",
					p, len, i, num, ooid);
			goto FAIL;
		}
		/* FIXME: should we compare old and new values ? */
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
				/* non-string */
				oxbcopy(v_ptr, &pmon->value.v, v_len);
				pmon->value.len = v_len;
			} else if (v_len) {
				/* finite string */
				if (pmon->buf_len >= v_len) {
					if (NULL == pmon->data_buf) {
						pmon->data_buf = oxnew(char, pmon->buf_len);
						if (NULL == pmon->data_buf) {
							owatchLog(3,
									"invalid change: out of memory (1) len=%d",
									pmon->buf_len);
							goto FAIL;
						}
					}
				} else {
					/* buffer is not enough */
					oxfree(pmon->data_buf);
					if (NULL == (pmon->data_buf = oxnew(char, v_len))) {
						owatchLog(3,
							"invalid change: out of memory (2) len=%d", v_len);
						goto FAIL;
					}
					pmon->buf_len = v_len;
				}
				oxbcopy(v_ptr, pmon->data_buf, v_len);
				pmon->value.v.v_str = pmon->data_buf;
				pmon->value.len = v_len;
			} else {
				/* null string */
				pmon->value.v.v_str = NULL;
				pmon->value.len = 0;
			}
		}
		pmon->incoming_count++;
		owatchAnnounceMonitor(pmon, FALSE);
	}
	if (p != len) {
		owatchLog(4, "invalid change: mismatch p=%d len=%d i=%d num=%d",
					p, len, i, num);
		goto FAIL;
	}
	owatchLog(11, "change OK num=%d", num);
	return OK;
  FAIL:
	return ERROR;
}


oret_t
owatchRemoveMonitorByRef(OwatchMonitorRecord_t * pmon)
{
	ulong_t ulv;
	owop_t  op;
	int     i;

	if (--pmon->ref_count > 0) {
		owatchLog(7, "RMMON: decrease ooid=%lu ref=%d",
					pmon->ooid, pmon->ref_count);
		return OK;
	}
	/* send packet to unregister monitor */
	if (pmon->registered && owatchIsConnected()) {
		ulv = OHTONL(pmon->ooid);
		owatchSendPacket(OLANG_DATA, OLANG_MON_UNREG_REQ, 4, &ulv);
	}
	/* cancel pending operations */
	for (i = pmon->reg_op_qty - 1; i >= 0; i--) {
		op = pmon->reg_ops[i];
		pmon->reg_ops[i] = 0;
		if (op)
			owatchCancelOp(op);
	}
	for (i = pmon->val_op_qty - 1; i >= 0; i--) {
		op = pmon->val_ops[i];
		pmon->val_ops[i] = 0;
		if (op)
			owatchCancelOp(op);
	}
	treeNumAddOrSet(owatch_by_ooid_mon_hash, (int) pmon->ooid, 0);
	owatchLog(7, "RMMON: removed ooid=%lu", pmon->ooid);
	owatchDeallocateMonitor(pmon);
	return OK;
}


int
owatchMonitorAdditionCanceller(owop_t op, long data1, long data2)
{
	OwatchMonitorRecord_t *pmon = (OwatchMonitorRecord_t *) data2;
	int     i;
	oret_t rc;

	if (NULL == pmon)
		return 0;

	for (i = pmon->reg_op_qty - 1; i >= 0; i--) {
		if (pmon->reg_ops[i] == op) {
			pmon->reg_ops[i] = 0;
			if (pmon->reg_op_qty == i + 1)
				pmon->reg_op_qty--;
		}
	}
	/* want to force sending unregistration packet to hub,
	 * because registration can already be processed,
	 * while we run this code */
	pmon->registered = TRUE;
	rc = owatchRemoveMonitorByRef(pmon);
	return rc == OK ? 0 : -1;
}


oret_t
owatchHandleRegistration(int kind, int type, int len, char *buf)
{
	OwatchMonitorRecord_t *pmon;
	ulong_t ulv;
	ooid_t ooid;
	int     i;
	owop_t  op;

	if (len != 4) {
		owatchLog(3, "wrong registration type=%x len=%d", type, len);
		return ERROR;
	}
	ulv = *(ulong_t *) buf;
	ooid = (ooid_t) ONTOHL(ulv);
	if (!ooid) {
		owatchLog(3, "wrong registration type=%x null ooid", type);
		return ERROR;
	}
	switch (type) {
	case OLANG_MON_REG_ACK:
		pmon = owatchFindMonitorByOoid(ooid);
		if (NULL == pmon) {
			owatchLog(4, "mon_reg_ack: ooid %lu not registered", ooid);
			return ERROR;
		}
		if (pmon->registered) {
			owatchLog(6, "mon_reg_ack: ooid %lu REregistered", ooid);
			/* FIXME: do we need to finalize operation here ? */
			return ERROR;
		}
		pmon->registered = TRUE;
		for (i = 0; i < pmon->reg_op_qty; i++) {
			op = pmon->reg_ops[i];
			pmon->reg_ops[i] = 0;
			if (op)
				owatchFinalizeOp(op, OWATCH_ERR_OK);
		}
		owatchLog(6, "mon_reg_ack: registered ooid %lu", ooid);
		pmon->reg_op_qty = 0;
		return OK;
	case OLANG_MON_REG_NAK:
		pmon = owatchFindMonitorByOoid(ooid);
		if (!pmon) {
			owatchLog(4, "mon_reg_nak: ooid %lu not registered", ooid);
			return ERROR;
		}
		for (i = 0; i < pmon->reg_op_qty; i++) {
			op = pmon->reg_ops[i];
			pmon->reg_ops[i] = 0;
			if (op)
				owatchFinalizeOp(op, OWATCH_ERR_REFUSED);
		}
		pmon->reg_op_qty = 0;
		owatchRemoveMonitorByRef(pmon);
		owatchLog(6, "mon_reg_nak: unregistered ooid %lu", ooid);
		return OK;
	case OLANG_MON_UNREG_ACK:
	case OLANG_MON_UNREG_NAK:
		return OK;
	default:
		return ERROR;
	}
}


int
owatchLazyMonRegRequest(owop_t m_op, owop_t s_op, int err_code,
					   long param1, long param2)
{
	owatchUnchainOp(m_op);
	if (err_code) {
		owatchLog(7, "mon_reg ooid=%ld lazy err=%d", param1, err_code);
		owatchFinalizeOp(m_op, err_code);
	} else {
		owatchLog(7, "finally send mon_reg ooid=%ld", param1);
	}
	return 0;
}


owop_t
owatchStickToExistingMonitor(ooid_t ooid, owop_t op, bool_t dont_monitor)
{
	OwatchMonitorRecord_t *pmon;
	int     i;

	pmon = owatchFindMonitorByOoid(ooid);
	if (!pmon)
		return OWOP_NULL;
	if (pmon->registered) {
		pmon->ref_count++;
		if (!dont_monitor)
			pmon->active_count++;
		if (op != OWOP_ERROR)
			owatchFinalizeOp(op, OWATCH_ERR_OK);
		owatchAnnounceMonitor(pmon, TRUE);
		owatchLog(7, "STEM ooid=%lu ref=%d. OK",
					pmon->info.ooid, pmon->ref_count);
		return OWOP_OK;
	}
	if (op == OWOP_ERROR) {
		op = owatchAllocateOp(owatchMonitorAdditionCanceller,
							 OWATCH_OPT_WAIT_MON, (long) pmon);
		if (op == OWOP_ERROR)
			return OWOP_ERROR;
	}
	for (i = 0; i < OWATCH_MAX_OP_PER_MONITOR; i++) {
		if (!pmon->reg_ops[i]) {
			pmon->reg_ops[i] = op;
			if (i >= pmon->reg_op_qty)
				pmon->reg_op_qty = i + 1;
			pmon->ref_count++;
			if (!dont_monitor)
				pmon->active_count++;
			break;
		}
	}
	if (i >= OWATCH_MAX_OP_PER_MONITOR)
		owatchFinalizeOp(op, OWATCH_ERR_NOSPACE);
	return op;
}


oret_t
owatchMonHashUp(OwatchMonitorRecord_t * pmon)
{
	int     key = (int) pmon->ooid;
	int     val = pmon->no;
	bool_t  found;

	if (treeNumAddOrSet(owatch_by_ooid_mon_hash, key, val) != OK) {
		owatchLog(2, "MICR: ! cannot hash up monitor key=%d", key);
		return ERROR;
	}
	found = treeNumFind(owatch_by_ooid_mon_hash, key, &val);
	if (!found) {
		owatchLog(2, "MICR: ! did not hash up monitor key=%d", key);
		return ERROR;
	}
	if (val != pmon->no) {
		owatchLog(2,
			  "MICR: ! ivalid hash up monitor key=%d val=%d no=%d",
			  key, val, pmon->no);
		return ERROR;
	}
	owatchLog(11, "OK hash up monitor found=%d key=%d val=%d no=%d",
				found, key, val, pmon->no);
	return OK;
}


int
owatchAddMonitorGotDescStage(owop_t m_op, owop_t s_op, int err_code,
							long param1, long param2)
{
	OwatchMonitorRecord_t *pmon = (OwatchMonitorRecord_t *) param2;
	ulong_t ulv;
	owop_t  op;

	owatchLog(10, "ooid=%lu stage2 m_op=%xh s_op=%xh err=%d",
				pmon->info.ooid, m_op, s_op, err_code);
	owatchUnchainOp(m_op);
	if (err_code)
		goto FAIL;
	if (pmon->user_ooid_buf) {
		*(pmon->user_ooid_buf) = pmon->info.ooid;
		pmon->user_ooid_buf = NULL;
	}
	owatchLog(11, "ooid=%lu looking to stick to monitor at stage2",
				pmon->info.ooid);
	op = owatchStickToExistingMonitor(pmon->info.ooid, m_op, !pmon->active_count);
	if (op != OWOP_NULL) {
		owatchLog(7, "ooid=%lu stuck to existing monitor at stage2",
					pmon->info.ooid);
		owatchDeallocateMonitor(pmon);
		owatchLog(11, "ooid=%lu deallocated temporary monitor at stage2",
					pmon->info.ooid);
		return 0;
	}
	pmon->ooid = pmon->info.ooid;
	if (owatchMonHashUp(pmon) != OK) {
		err_code = OWATCH_ERR_INTERNAL;
		goto FAIL;
	}
	ulv = OHTONL(pmon->ooid);
	s_op = owatchSecureSend(OLANG_DATA, OLANG_MON_REG_REQ, 4, &ulv);
	if (s_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NETWORK;
		goto FAIL;
	}
	if (s_op != OWOP_OK)
		owatchChainOps(m_op, s_op, owatchLazyMonRegRequest, (long) pmon->ooid, 0);
	owatchLog(7, "ask registration ooid=%lu no=%d desc=[%s] "
				"m_op=%xh secsnd_op=%xh",
				pmon->ooid, pmon->no, pmon->info.desc, m_op, s_op);
	return OK;
  FAIL:
	if (NULL != pmon)
		owatchDeallocateMonitor(pmon);
	owatchLog(7, "mon_reg ooid=%ld err=%d", param1, err_code);
	owatchFinalizeOp(m_op, err_code);
	return ERROR;
}


owop_t
owatchNowaitAddMonitorByOoid(ooid_t ooid)
{
	OwatchMonitorRecord_t *pmon;
	owop_t  m_op, s_op, op;
	oret_t rc;
	int     err_code;

	op = owatchStickToExistingMonitor(ooid, OWOP_ERROR, FALSE);
	if (op != OWOP_NULL)
		return op;
	m_op = s_op = OWOP_ERROR;
	pmon = owatchAllocateMonitor();
	if (!pmon) {
		err_code = OWATCH_ERR_NOSPACE;
		goto FAIL;
	}
	err_code = OWATCH_ERR_NOSPACE;
	m_op = owatchAllocateOp(owatchMonitorAdditionCanceller,
							OWATCH_OPT_OOID_MON, (long) pmon);
	if (m_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NOSPACE;
		goto FAIL;
	}
	pmon->active_count++;
	pmon->reg_ops[0] = m_op;
	pmon->reg_op_qty = 1;
	pmon->ooid = ooid;
	pmon->user_ooid_buf = NULL;
	pmon->registered = FALSE;
	pmon->ref_count = 1;
	oxvzero(&pmon->value);
	pmon->value.undef = 1;
	s_op = owatchNowaitGetInfoByOoid(ooid, &pmon->info);
	if (s_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NOINFO;
		goto FAIL;
	}
	owatchLog(10,
			"AMBP waiting for info stage1 m_op=%xh s_op=%xh ooid=%lu ref=%d",
			m_op, s_op, ooid, pmon->ref_count);
	rc = owatchChainOps(m_op, s_op, owatchAddMonitorGotDescStage, 0, (long) pmon);
	if (rc != OK) {
		err_code = OWATCH_ERR_INTERNAL;
		goto FAIL;
	}
	return m_op;
  FAIL:
	if (pmon)
		owatchDeallocateMonitor(pmon);
	if (s_op != OWOP_ERROR)
		owatchCancelOp(s_op);
	if (m_op != OWOP_ERROR)
		owatchFinalizeOp(m_op, err_code);
	return OWOP_ERROR;
}


ooid_t
owatchFindMonitorOoidByDesc(const char *desc)
{
	owquark_t info;
	OwatchMonitorRecord_t *pmon;
	oret_t rc;

	rc = owatchFindCachedInfoByDesc(desc, &info);
	if (rc != OK)
		return 0;
	pmon = owatchFindMonitorByOoid(info.ooid);
	if (pmon == NULL)
		return 0;
	return info.ooid;
}


oret_t
owatchHandleMonComboReply(int kind, int type, int len, char *buf)
{
	OwatchMonitorRecord_t *pmon = NULL;
	owquark_t info;
	int     p, i;
	owop_t  m_op, s_op;
	ulong_t ulv;
	long    data1 = 0, data2 = 0;
	int     err, mon_rc;
	char    ermes[40];
	oret_t rc;

	/* deserialize results */
	owatchLog(9, "parse mon_combo_reply len=%d", len);
	oxvzero(&info);
	ulv = *(ulong_t *) (buf + 0);
	m_op = (owop_t) ONTOHL(ulv);
	ulv = *(ulong_t *) (buf + 4);
	mon_rc = (int) ONTOHL(ulv);
	p = 8;
	if (!owatchIsValidOp(m_op)) {
		err = OWATCH_ERR_TOOLATE;
		owatchLog(5, "MICR(%xh) ERR too late", m_op);
		m_op = OWOP_ERROR;
		goto FAIL;
	}
	owatchGetOpData(m_op, &data1, &data2);
	if (data1 != OWATCH_OPT_COMBO_MON || !data2) {
		owatchLog(5, "MICR(%xh) ERR not MICR data1=%ld data2=%ld",
					m_op, data1, data2);
		m_op = OWOP_ERROR;
		err = OWATCH_ERR_SCREWED;
		goto FAIL;
	}
	pmon = (OwatchMonitorRecord_t *) data2;
	rc = owatchUnpackInfoReply(buf, &p, &info, ermes, sizeof(ermes));
	if (!info.ooid) {
		owatchUpdateInfoCache(pmon->info.desc, NULL);
		err = OWATCH_ERR_REFUSED;
		owatchLog(5, "MICR(%xh) [%s] NOT FOUND [%s]",
					m_op, pmon->info.desc, ermes);
		goto FAIL;
	}
	if (!info.desc[0]) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(3, "MICR(%xh) ERR bad desc ptr=%c type=%c len=%d p=%d",
					m_op, info.ptr, info.type, info.len, p);
		goto FAIL;
	}
	if (p != len) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(3, "MICR(%xh) ERR length mismatch p=%d len=%d",
					m_op, p, len);
		goto FAIL;
	}
	/* INFO OK */
	owatchLog(5, "MICR(%xh) OK ooid=%lu '%c,%c,%c' b+%d/%d/%d [%s]",
				m_op, info.ooid, info.ptr, info.type, info.seg,
				info.bit_off, info.bit_len, info.len, info.desc);
	owatchUpdateInfoCache(pmon->info.desc, &info);
	oxvcopy(&info, &pmon->info);
	if (pmon->user_ooid_buf) {
		*(pmon->user_ooid_buf) = pmon->info.ooid;
		pmon->user_ooid_buf = NULL;
	}

	/* try to stick */
	owatchLog(11, "MICR: ooid=%lu looking to stick to monitor at stage2",
				pmon->info.ooid);
	s_op = owatchStickToExistingMonitor(pmon->info.ooid, m_op,
									   !pmon->active_count);
	if (s_op != OWOP_NULL) {
		owatchLog(7, "MICR: ooid=%lu stuck to existing monitor at stage2",
					pmon->info.ooid);
		owatchDeallocateMonitor(pmon);
		owatchLog(11, "MICR: ooid=%lu deallocated temporary monitor at stage2",
					pmon->info.ooid);
		return OK;				/* finalize operation ? */
	}

	/* cannot stick - really new monitor */
	pmon->ooid = pmon->info.ooid;
	if (mon_rc != 0) {
		owatchLog(6, "MICR: mon_reg_nak: unregistered [%s]", pmon->info.desc);
		err = OWATCH_ERR_REFUSED;
		goto FAIL;
	}

	/* hash it up */
	err = OWATCH_ERR_INTERNAL;
	if (owatchMonHashUp(pmon) != OK)
		goto FAIL;

	if (pmon->registered) {
		owatchLog(6, "MICR: REregistered [%s]", pmon->info.desc);
	} else {
		owatchLog(6, "MICR: registered [%s]", pmon->info.desc);
	}
	pmon->registered = TRUE;
	for (i = 0; i < pmon->reg_op_qty; i++) {
		if (pmon->reg_ops[i])
			owatchFinalizeOp(pmon->reg_ops[i], OWATCH_ERR_OK);
		pmon->reg_ops[i] = 0;
	}
	pmon->reg_op_qty = 0;

	return OK;
  FAIL:
	owatchLog(7, "MICR: err=%d", err);
	if (pmon) {
		for (i = 0; i < pmon->reg_op_qty; i++) {
			owop_t  op = pmon->reg_ops[i];

			pmon->reg_ops[i] = 0;
			if (op) {
				owatchFinalizeOp(op, OWATCH_ERR_REFUSED);
				if (op == m_op)
					m_op = OWOP_ERROR;
			}
		}
		pmon->reg_op_qty = 0;
		pmon->ref_count = 0;
		owatchRemoveMonitorByRef(pmon);
	}
	if (m_op != OWOP_ERROR)
		owatchFinalizeOp(m_op, err);
	return ERROR;
}


int
owatchLazyMonComboReq(owop_t m_op, owop_t s_op, int err_code,
					 long param1, long param2)
{
	char   *desc = (char *) param1;

	if (err_code)
		goto FAIL;
	owatchUnchainOp(m_op);
	owatchLog(7, "finally send mon_combo_req desc=[%s]", desc);
	oxfree(desc);
	return OK;
  FAIL:
	oxfree(desc);
	owatchUnchainOp(m_op);
	owatchFinalizeOp(m_op, err_code);
	return ERROR;
}


owop_t
owatchNowaitAddMonitorByDesc(const char *desc,
							ooid_t * ooid_buf, bool_t dont_monitor)
{
	owquark_t info;
	OwatchMonitorRecord_t *pmon;
	owop_t  m_op, s_op, op;
	oret_t rc;
	int     err_code;
	int     n;
	char    buf[OWATCH_MAX_PATH + 8];

	if (!desc || !*desc)
		return OWOP_ERROR;
	n = strlen(desc);
	if (n > 255 || n + 4 > sizeof(buf))
		return OWOP_ERROR;
	info.ooid = 0;
	if (ooid_buf)
		*ooid_buf = 0;
	rc = owatchFindCachedInfoByDesc(desc, &info);
	if (rc == OK) {
		if (ooid_buf)
			*ooid_buf = info.ooid;
		op = owatchStickToExistingMonitor(info.ooid, OWOP_ERROR, dont_monitor);
		if (op != OWOP_NULL) {
			owatchLog(7, "AMBD desc=\"%s\" stuck to ooid=%lu op=%xh dont=%d",
						desc, info.ooid, op, dont_monitor);
			return op;
		}
		owatchLog(7, "AMBD desc=[%s] did not stick", desc);
		op = owatchNowaitAddMonitorByOoid(info.ooid);
		return op;
	}
	owatchLog(7, "AMBD desc=[%s] not found in cache - going on", desc);
	m_op = s_op = OWOP_ERROR;
	err_code = OWATCH_ERR_NOSPACE;
	pmon = owatchAllocateMonitor();
	if (!pmon)
		goto FAIL;
#if OWATCH_FAST_REGISTRATION
	/* FIXME: check for rejected description */
	m_op = owatchAllocateOp(owatchMonitorAdditionCanceller,	/* really ? */
						   OWATCH_OPT_COMBO_MON, (long) pmon);
	if (m_op == OWOP_ERROR)
		goto FAIL;
	if (!dont_monitor)
		pmon->active_count++;
	strcpy(pmon->info.desc, desc);
	pmon->reg_ops[0] = m_op;
	pmon->reg_op_qty = 1;
	pmon->ooid = 0;
	pmon->user_ooid_buf = ooid_buf;
	pmon->value.undef = 1;
	pmon->registered = FALSE;
	pmon->ref_count = 1;

	/* serialize data */
	n = strlen(desc);
	*(ulong_t *) buf = OHTONL((ulong_t) m_op);
	buf[4] = (char) (uchar_t) n;
	oxbcopy(desc, buf + 5, n);
	err_code = OWATCH_ERR_INTERNAL;
	s_op = owatchSecureSend(OLANG_DATA, OLANG_MON_COMBO_REQ, n + 5, buf);
	if (s_op == OWOP_ERROR)
		goto FAIL;
	if (s_op != OWOP_OK)
		owatchChainOps(m_op, s_op, owatchLazyMonComboReq, (long) oxstrdup(desc), 0);
	owatchLog(10,
			"AMBD waiting for info stage1 m_op=%xh s_op=%xh desc=[%s] ref=%d",
			m_op, s_op, desc, pmon->ref_count);
#else /* !OWATCH_FAST_REGISTRATION */
	m_op = owatchAllocateOp(owatchMonitorAdditionCanceller,
						   OWATCH_OPT_DESC_MON, (long) pmon);
	if (m_op == OWOP_ERROR)
		goto FAIL;
	if (!dont_monitor)
		pmon->active_count++;
	pmon->reg_ops[0] = m_op;
	pmon->reg_op_qty = 1;
	pmon->ooid = 0;
	pmon->user_ooid_buf = ooid_buf;
	pmon->value.undef = 1;
	pmon->registered = FALSE;
	pmon->ref_count = 1;
	err_code = OWATCH_ERR_NOINFO;
	s_op = owatchNowaitGetInfoByDesc(desc, &pmon->info);
	if (s_op == OWOP_ERROR)
		goto FAIL;
	owatchLog(10,
			"AMBD waiting for info stage1 m_op=%xh s_op=%xh desc=[%s] ref=%d",
			m_op, s_op, desc, pmon->ref_count);
	err_code = OWATCH_ERR_INTERNAL;
	rc = owatchChainOps(m_op, s_op, owatchAddMonitorGotDescStage, 0, (long) pmon);
	if (rc != OK)
		goto FAIL;
#endif /* OWATCH_FAST_REGISTRATION */
	return m_op;
  FAIL:
	if (pmon)
		owatchDeallocateMonitor(pmon);
	if (s_op != OWOP_ERROR)
		owatchCancelOp(s_op);
	if (m_op != OWOP_ERROR)
		owatchFinalizeOp(m_op, err_code);
	return OWOP_ERROR;
}


int
owatchMonCheckInfo(long param1, owop_t op, int err_code)
{
	OwatchOoidAndInfo_t *ppai = (OwatchOoidAndInfo_t *) param1;
	ooid_t ooid = ppai->ooid;
	owquark_t *pinfo = &ppai->info;
	owquark_t *pinfo0;
	OwatchMonitorRecord_t *pmon = 0;
	ulong_t ulv;
	oret_t rc;

	pmon = owatchFindMonitorByOoid(ooid);
	if (err_code)
		owatchLog(5, "cannot compare mon ooid=%lu err=%d", ooid, err_code);
	else if (NULL == pmon)
		owatchLog(5, "cannot compare mon ooid=%lu mon=NULL", ooid);
	else {
		pinfo0 = &pmon->info;
		if (pinfo->type == pinfo0->type &&
			pinfo->len == pinfo0->len && pinfo->bit_len == pinfo0->bit_len) {
			/* type OK - register monitor */
			ulv = OHTONL(pmon->ooid);
			rc = owatchSendPacket(OLANG_DATA, OLANG_MON_REG_REQ, 4, &ulv);
			owatchLog(7, "compare mon OK ooid=%lu desc=[%s] send_rc=%d",
						ooid, pmon->info.desc, rc);
		} else {
			/* type differs - value is undefined */
			owatchLog(6, "compare mon ERR ooid=%lu desc=[%s]",
						ooid, pmon->info.desc);
			pmon->value.undef = 1;
			/* FIXME: announce undefined value */
		}
	}
	if (NULL != pmon)
		pmon->secure_op = OWOP_NULL;
	oxfree(ppai);
	return OK;
}


int
owatchMonSecureStart()
{
	OwatchMonitorRecord_t *pmon;
	int     i;

	owatch_mon_secure_count = 0;
	for (i = 0; i < owatch_mon_qty; i++) {
		pmon = owatch_mons[i].mon_p;
		if (!pmon || !pmon->registered)
			continue;
		pmon->secure_op = OWOP_SELF;
		owatch_mon_secure_count++;
	}
	owatchLog(5, "securing %d mons", owatch_mon_secure_count);
	owatchMonSecureMore();
	return owatch_mon_secure_count;
}


int
owatchMonSecureMore()
{
	OwatchMonitorRecord_t *pmon;
	OwatchOoidAndInfo_t *ppai;
	owop_t  op;
	int     i, qty;

	if (!owatch_mon_secure_count)
		return 0;
	if (!owatchIsConnected()) {
		for (i = 0; i < owatch_mon_qty; i++) {
			pmon = owatch_mons[i].mon_p;
			if (!pmon || !pmon->registered)
				continue;
			if (pmon->secure_op != OWOP_NULL && pmon->secure_op != OWOP_SELF)
				owatchCancelOp(pmon->secure_op);
			pmon->secure_op = OWOP_NULL;
		}
		owatch_mon_secure_count = 0;
		return 0;
	}
	qty = 0;
	i = 0;
	while (owatchFreeOpCount() > 4 && owatch_mon_secure_count > 0) {
		for (; i < owatch_mon_qty; i++) {
			pmon = owatch_mons[i].mon_p;
			if (!pmon || !pmon->registered || pmon->secure_op != OWOP_SELF)
				continue;
			ppai = oxnew(OwatchOoidAndInfo_t, 1);
			ppai->ooid = pmon->ooid;
			op = owatchNowaitGetInfoByDesc(pmon->info.desc, &ppai->info);
			switch (op) {
			case OWOP_OK:
				ologAbort("cannot compare mon cache not flushed");
				pmon->secure_op = OWOP_NULL;
				break;
			case OWOP_ERROR:
				owatchLog(5, "cannot compare mon ooid=%lu GINFO ERR",
							pmon->ooid);
				pmon->secure_op = OWOP_NULL;
				oxfree(ppai);
				break;
			default:
				owatchLocalOpHandler(op, owatchMonCheckInfo, (long) ppai);
				pmon->secure_op = op;
				qty++;
				break;
			}
			owatch_mon_secure_count--;
			break;
		}
	}
	owatchLog(6, "wait for %d mons, still have %d mons to secure",
				qty, owatch_mon_secure_count);
	if (owatch_mon_secure_count <= 0)
		owatchLog(5, "all monitors secured");
	return qty;
}


owop_t
owatchAddMonitorByOoid(ooid_t ooid)
{
	owop_t  op = owatchNowaitAddMonitorByOoid(ooid);

	return owatchInternalWaitOp(op);
}


owop_t
owatchAddMonitorByDesc(const char *desc, ooid_t * ooid_buf, bool_t dont_monitor)
{
	owop_t  op = owatchNowaitAddMonitorByDesc(desc, ooid_buf, dont_monitor);

	return owatchInternalWaitOp(op);
}


oret_t
owatchEnableMonitoring(bool_t enable)
{
	enable = (enable != 0);
	if (owatch_monitoring_active == enable)
		return OK;
	owatch_monitoring_active = enable;
	owatchLog(6, "EnableMonitoring=%d mon_qty=%d", enable, owatch_mon_qty);
	owatchLazyDataHandlers();
	return OK;
}


static int
owatchMonBlockHandler(long param, owop_t op, int err_code)
{
	owatchLog(7, "BlockMonitoring=%d err=%d", (int) param, err_code);
	return 0;
}


oret_t
owatchBlockMonitoring(bool_t block, bool_t renew_if_unblock)
{
	owop_t  op;
	char    buf[2];

	block = (block != 0);
	renew_if_unblock = (renew_if_unblock != 0);
	if (owatch_mon_all_blocked == block)
		return OK;
	buf[0] = (char) block;
	buf[1] = (char) renew_if_unblock;
	op = owatchSecureSend(OLANG_DATA, OLANG_BLOCK_ANNOUNCE, 2, buf);
	owatchLog(6, "BlockMonitoring=%d renew=%d mon_qty=%d op=%x",
				block, renew_if_unblock, owatch_mon_qty, op);
	if (op == OWOP_ERROR)
		return ERROR;
	owatchLocalOpHandler(op, owatchMonBlockHandler, block);
	owatch_mon_all_blocked = block;
	return OK;
}


int
owatchLazyDataHandlers(void)
{
	OwatchMonitorRecord_t *pmon;
	int     i;

	if (!owatch_monitoring_active || !owatch_mon_update_count)
		return 0;
	if (owatch_mon_update_count < 0) {
		owatchLog(3, "oops! mon_update_count=%d", owatch_mon_update_count);
		owatch_mon_update_count = 0;
		return 0;
	}
	for (i = 0; i < owatch_mon_qty; i++) {
		pmon = owatch_mons[i].mon_p;
		if (!pmon || !pmon->update_count)
			continue;
		owatchLog(9, "lazy monitoring desc=[%s] ucount=%d",
					pmon->info.desc, pmon->update_count);
		owatchLazyCallDataHandlers(pmon);
		/* FIXME: if it was updated several times, do we need a queue ? */
		owatch_mon_update_count -= pmon->update_count;
		pmon->update_count = 0;
	}
	return 0;
}


oret_t
owatchInitMonitoring(void)
{
	owatch_mon_update_count = 0;
	owatch_mon_secure_count = 0;
	owatch_mon_all_blocked = FALSE;
	owatch_data_handlers = oxnew(OwatchDataHandlerRecord_t,
								OWATCH_MAX_DATA_HANDLERS);
	if (NULL == owatch_data_handlers)
		return ERROR;
	if (NULL == (owatch_by_ooid_mon_hash = treeAlloc(NUMERIC_TREE)))
		return ERROR;
	return OK;
}


oret_t
owatchExitMonitoring(void)
{
	int     i;

	owatch_mon_update_count = owatch_mon_secure_count = 0;
	owatch_mon_all_blocked = FALSE;

	if (owatch_data_handlers) {
		for (i = 0; i < OWATCH_MAX_DATA_HANDLERS; i++)
			owatch_data_handlers[i].phand = NULL;
	}
	oxfree(owatch_data_handlers);
	owatch_data_handlers = NULL;

	treeFree(owatch_by_ooid_mon_hash);
	owatch_by_ooid_mon_hash = NULL;

	owatchRemoveAllMonitors();
	return OK;
}


oret_t
owatchRemoveAllMonitors()
{
	int     i;

	if (NULL == owatch_mons) {
		owatch_mon_qty = 0;
		/* FIXME: compare this with documentation */
		return ERROR;
	}
	for (i = 0; i < owatch_mon_qty; i++) {
		if (owatch_mons[i].mon_p)
			owatchRemoveMonitorByRef(owatch_mons[i].mon_p);
	}
	oxfree(owatch_mons);
	owatch_mons = NULL;
	owatch_mon_qty = 0;
	return OK;
}


oret_t
owatchRemoveMonitor(ooid_t ooid)
{
	OwatchMonitorRecord_t *pmon = owatchFindMonitorByOoid(ooid);

	return pmon ? owatchRemoveMonitorByRef(pmon) : ERROR;
}


oret_t
owatchGetValue(ooid_t ooid, oval_t * pvalue, char *data_buf, int buf_len)
{
	OwatchMonitorRecord_t *pmon = owatchFindMonitorByOoid(ooid);
	if (NULL == pmon)
		return ERROR;
	/* FIXME: Reference manual to declare this a non-blocking function ? */
	return owatchCopyValue(&pmon->value, pvalue, data_buf, buf_len);
}


oret_t
owatchAddDataHandler(const char *filter, OwatchDataHandler_t phand, long param)
{
	int     i;

	if (!owatch_data_handlers || !phand)
		return ERROR;
	for (i = 0; i < OWATCH_MAX_DATA_HANDLERS; i++) {
		if (!owatch_data_handlers[i].phand) {
			owatch_data_handlers[i].param = param;
			owatch_data_handlers[i].phand = phand;
			return OK;
		}
	}
	return ERROR;
}


oret_t
owatchRemoveDataHandler(OwatchDataHandler_t phand)
{
	int     i, n;

	if (NULL == owatch_data_handlers || NULL == phand)
		return ERROR;
	for (i = n = 0; i < OWATCH_MAX_ALIVE_HANDLERS; i++) {
		if (owatch_data_handlers[i].phand == phand) {
			owatch_data_handlers[i].phand = NULL;
			n++;
		}
	}
	return n ? OK : ERROR;
}


oret_t
owatchFlushMonitor(ooid_t ooid)
{
	OwatchMonitorRecord_t *pmon = owatchFindMonitorByOoid(ooid);

	if (!pmon)
		return ERROR;
	owatch_mon_update_count -= pmon->update_count;
	pmon->update_count = 0;
	return OK;
}


oret_t
owatchFlushAllMonitors()
{
	int     i;

	if (!owatch_mons)
		return ERROR;
	for (i = 0; i < owatch_mon_qty; i++) {
		if (owatch_mons[i].mon_p)
			owatch_mons[i].mon_p->update_count = 0;
	}
	owatch_mon_update_count = 0;
	return OK;
}


owop_t
owatchInternalRenewMonitor(OwatchMonitorRecord_t * pmon)
{
	if (pmon->incoming_count) {
		owatchAnnounceMonitor(pmon, TRUE);
		return OWOP_OK;
	}
	return OWOP_ERROR;
}


owop_t
owatchRenewMonitor(ooid_t ooid)
{
	OwatchMonitorRecord_t *pmon = owatchFindMonitorByOoid(ooid);

	if (!pmon)
		return ERROR;
	return owatchInternalRenewMonitor(pmon);
}


oret_t
owatchRenewAllMonitors()
{
	int     i;

	if (!owatch_mons)
		return ERROR;
	for (i = 0; i < owatch_mon_qty; i++) {
		if (owatch_mons[i].mon_p)
			owatchInternalRenewMonitor(owatch_mons[i].mon_p);
	}
	return OK;
}


oret_t
owatchSetMonitorData(ooid_t ooid, long user_data)
{
	OwatchMonitorRecord_t *pmon = owatchFindMonitorByOoid(ooid);

	if (!pmon)
		return ERROR;
	pmon->user_data = user_data;
	return OK;
}


oret_t
owatchGetMonitorData(ooid_t ooid, long *user_data_p)
{
	OwatchMonitorRecord_t *pmon = owatchFindMonitorByOoid(ooid);

	if (NULL == pmon || NULL == user_data_p)
		return ERROR;
	*user_data_p = pmon->user_data;
	return OK;
}
