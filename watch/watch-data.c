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

  Non-repeating data reading and writing.

*/

#include "watch-priv.h"
#include <optikus/conf-net.h>	/* for ONTOHL,ONTOHS */
#include <optikus/conf-mem.h>	/* for oxbcopy,oxnew,... */
#include <string.h>				/* for memcpy,strcmp */
#include <stdio.h>				/* for sprintf */


OwatchReadOpt_t owatch_optimize_reading = OWATCH_ROPT_NEVER;

typedef struct
{
	owquark_t info;
	oval_t *pvalue;
	char   *data_buf;
	int     buf_len;
} OwatchReadRecord_t;


oret_t
owatchHandleReadReply(int kind, int type, int len, char *buf)
{
	owop_t  op;
	ooid_t ooid;
	ulong_t ulv;
	ushort_t usv;
	char    v_type;
	short   v_len;
	long    time;
	int     err;
	long    data1, data2;
	OwatchReadRecord_t *prrec = NULL;
	oval_t tmp_val;
	oret_t rc;

	if (len < 16) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(5, "READ() tiny len=%d", len);
		return ERROR;
	}
	ulv = *(ulong_t *) (buf + 0);
	ulv = ONTOHL(ulv);
	op = (owop_t) ulv;
	ulv = *(ulong_t *) (buf + 4);
	ulv = ONTOHL(ulv);
	ooid = (ooid_t) ulv;
	ulv = *(ulong_t *) (buf + 8);
	ulv = ONTOHL(ulv);
	time = (long) ulv;
	ulv = *(ulong_t *) (buf + 12);
	ulv = ONTOHL(ulv);
	err = (int) ulv;
	if (!owatchIsValidOp(op)) {
		err = OWATCH_ERR_TOOLATE;
		owatchLog(5, "READ(%xh) too late", op);
		return ERROR;
	}
	data1 = data2 = 0;
	owatchGetOpData(op, &data1, &data2);
	if (data1 != OWATCH_OPT_READ || !data2) {
		owatchLog(5, "READ(%xh) op not READ data1=%ld data2=%ld",
					op, data1, data2);
		return ERROR;
	}
	prrec = (OwatchReadRecord_t *) data2;
	if (err != 0) {
		owatchLog(5, "READ(%xh) ooid=%lu desc=[%s] err=%d",
					op, ooid, prrec->info.desc, err);
		goto FAIL;
	}
	if (len < 21) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(5, "READ(%xh) ooid=%lu desc=[%s] tiny len=%d",
					op, ooid, prrec->info.desc, len);
		goto FAIL;
	}
	v_type = buf[16];
	if (v_type != prrec->info.type) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(5, "READ(%xh) ooid=%lu desc=[%s] type '%c'<>'%c'",
					op, ooid, prrec->info.desc, prrec->info.type,
					v_type ?: '?');
		goto FAIL;
	}
	usv = *(ushort_t *) (buf + 18);
	v_len = (short) ONTOHS(usv);
	if (v_type != 's' && v_len != prrec->info.len) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(5, "READ(%xh) ooid=%lu desc=[%s] v_len %d<>%d",
					op, ooid, prrec->info.desc, (int) v_len,
					(int) prrec->info.len);
		goto FAIL;
	}
	if (len != v_len + 20) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(5, "READ(%xh) ooid=%lu desc=[%s] v_len=%hd len=%d mismatch",
					op, ooid, prrec->info.desc, v_len, len);
		goto FAIL;
	}
	tmp_val.type = v_type;
	tmp_val.time = 0;
	tmp_val.undef = 0;
	tmp_val.len = v_len;
	if (tmp_val.type == 's')
		tmp_val.v.v_str = buf + 20;
	else
		oxbcopy(buf + 20, &tmp_val.v, v_len);
	rc = owatchCopyValue(&tmp_val, prrec->pvalue,
						prrec->data_buf, prrec->buf_len);
	if (rc != OK) {
		err = OWATCH_ERR_INVALID;
		owatchLog(6, "READ(%xh) ooid=%lu desc=[%s] space not enough",
					op, ooid, prrec->info.desc);
		goto FAIL;
	}
	owatchLog(6, "READ(%xh) ooid=%lu desc=[%s] OK", op, ooid, prrec->info.desc);
	owatchFinalizeOp(op, err);
	oxfree(prrec);
	return OK;
  FAIL:
	owatchFinalizeOp(op, err);
	oxfree(prrec);
	return ERROR;
}


int
owatchLazyReadRequest(owop_t m_op, owop_t s_op, int err_code,
					 long param1, long param2)
{
	owatchUnchainOp(m_op);
	if (err_code) {
		owatchLog(7, "read ooid=%ld lazy err=%d", param1, err_code);
	} else {
		owatchLog(7, "finally read ooid=%ld", param1);
	}
	owatchFinalizeOp(m_op, err_code);
	return 0;
}


/*
 *  Registers monitoring so that later values
 *  are sent over network only when updated.
 */
OwatchReadOpt_t
owatchOptimizeReading(const char *desc,
						OwatchReadOpt_t optimization,
						int timeout)
{
	OwatchReadOpt_t prev;
	ooid_t ooid, temp;
	owop_t op;
	int err_code;
	oret_t rc;

	if (NULL == desc || '\0' == *desc
			|| 0 == strcmp(desc, "*") || 0 == strcmp(desc, "*.*")) {
		prev = owatch_optimize_reading;
		switch (optimization) {
		case OWATCH_ROPT_NEVER:
		case OWATCH_ROPT_ALWAYS:
		case OWATCH_ROPT_AUTO:
			owatch_optimize_reading = optimization;
			break;
		default:
			break;
		}
		return prev;
	} else {
		switch (optimization) {
		case OWATCH_ROPT_ALWAYS:
		case OWATCH_ROPT_AUTO:
			ooid = owatchFindMonitorOoidByDesc(desc);
			if (ooid != 0) {
				return OWATCH_ROPT_ALWAYS;
			}
			op = owatchAddMonitorByDesc(desc, &temp, FALSE);
			rc = owatchWaitOp(op, timeout, &err_code);
			if (rc != OK)
				owatchDetachOp(op);
			prev = (rc == OK ? OWATCH_ROPT_NEVER : OWATCH_ROPT_ERROR);
			break;
		case OWATCH_ROPT_NEVER:
			ooid = owatchFindMonitorOoidByDesc(desc);
			if (ooid == 0)
				return OWATCH_ROPT_NEVER;
			rc = owatchRemoveMonitor(ooid);
			prev = (rc == OK ? OWATCH_ROPT_ALWAYS : OWATCH_ROPT_ERROR);
			break;
		default:
			ooid = owatchFindMonitorOoidByDesc(desc);
			prev = (ooid == 0 ? OWATCH_ROPT_NEVER : OWATCH_ROPT_ALWAYS);
			break;
		}
	}
	return prev;
}


int
owatchAutoMonitoringHandler(long ooid, owop_t op, int err_code)
{
	owatchLog(6, "auto-monitoring op=%xh ooid=%lu err=%d",
				op, ooid, err_code);
	return 0;
}


int
owatchReadGotDescStage(owop_t m_op, owop_t s_op, int err_code,
					  long param1, long param2)
{
	OwatchReadRecord_t *prrec = (OwatchReadRecord_t *) param2;
	char    req[12];
	ulong_t ulv;
	owop_t  tmp_op;

	owatchUnchainOp(m_op);
	if (err_code)
		goto FAIL;
	switch (owatch_optimize_reading) {
	case OWATCH_ROPT_ALWAYS:
	case OWATCH_ROPT_AUTO:
		if (owatchFindMonitorByOoid(prrec->info.ooid) == NULL) {
			owatchLog(7, "start auto-monitoring ooid=%lu desc=[%s]",
						prrec->info.ooid, prrec->info.desc);
			tmp_op = owatchAddMonitorByOoid(prrec->info.ooid);
			owatchLocalOpHandler(tmp_op, owatchAutoMonitoringHandler,
								prrec->info.ooid);
		}
		break;
	default:
		break;
	}
	if (owatchGetValue(prrec->info.ooid, prrec->pvalue,
					  prrec->data_buf, prrec->buf_len) == OK) {
		if (prrec->pvalue->undef == 0) {
			owatchLog(6,
					"READ(%xh) ooid=%lu desc=[%s] ival=%x from monitor OK",
					m_op, prrec->info.ooid, prrec->info.desc,
					prrec->pvalue->v.v_int);
			owatchFinalizeOp(m_op, OWATCH_ERR_OK);
			oxfree(prrec);
			return OK;
		}
	}
	ulv = m_op;
	*(ulong_t *) (req + 0) = OHTONL(ulv);
	ulv = prrec->info.ooid;
	*(ulong_t *) (req + 4) = OHTONL(ulv);
	ulv = 0;					/* time not used */
	*(ulong_t *) (req + 8) = OHTONL(ulv);
	s_op = owatchSecureSend(OLANG_DATA, OLANG_OOID_READ_REQ, 12, req);
	if (s_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NETWORK;
		goto FAIL;
	}
	if (s_op != OWOP_OK)
		owatchChainOps(m_op, s_op, owatchLazyReadRequest,
					  (long) prrec->info.ooid, 0);
	owatchLog(7, "ask read ooid=%lu desc=[%s]",
				prrec->info.ooid, prrec->info.desc);
	return OK;
  FAIL:
	owatchLog(7, "read ooid=%ld err=%d", param1, err_code);
	owatchFinalizeOp(m_op, err_code);
	oxfree(prrec);
	return ERROR;
}


int
owatchReadCanceller(owop_t op, long data1, long data2)
{
	oxfree((OwatchReadRecord_t *) data2);
	return 0;
}


owop_t
owatchNowaitReadByAny(const char *a_desc, ooid_t a_ooid,
					 oval_t * pvalue, char *data_buf, int buf_len)
{
	OwatchReadRecord_t *prrec = NULL;
	owop_t  m_op = OWOP_ERROR;
	owop_t  s_op = OWOP_ERROR;
	oret_t rc;
	int     err_code;

	oxvzero(pvalue);
	if (NULL != data_buf && buf_len > 0)
		oxbzero(data_buf, buf_len);
	if (NULL == (prrec = oxnew(OwatchReadRecord_t, 1))) {
		err_code = OWATCH_ERR_NOSPACE;
		goto FAIL;
	}
	prrec->pvalue = pvalue;
	prrec->data_buf = data_buf;
	prrec->buf_len = buf_len;
	err_code = OWATCH_ERR_NOSPACE;
	m_op = owatchAllocateOp(owatchReadCanceller, OWATCH_OPT_READ, (long) prrec);
	if (m_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NOSPACE;
		goto FAIL;
	}
	if (a_desc)
		s_op = owatchNowaitGetInfoByDesc(a_desc, &prrec->info);
	else
		s_op = owatchNowaitGetInfoByOoid(a_ooid, &prrec->info);
	if (s_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NOINFO;
		goto FAIL;
	}
	rc = owatchChainOps(m_op, s_op, owatchReadGotDescStage, 0, (long) prrec);
	if (rc != OK) {
		err_code = OWATCH_ERR_INTERNAL;
		goto FAIL;
	}
	/* FIXME: got to cancel all requests if hub is dead */
	/*        or buffer up requests at hub */
	return m_op;
  FAIL:
	oxfree(prrec);
	if (s_op != OWOP_ERROR)
		owatchCancelOp(s_op);
	if (m_op != OWOP_ERROR)
		owatchFinalizeOp(m_op, err_code);
	return OWOP_ERROR;
}


owop_t
owatchRead(ooid_t ooid, oval_t * pvalue, char *data_buf, int buf_len)
{
	owop_t  op;

	if (!ooid || !pvalue || buf_len < 0)
		return OWOP_ERROR;
	op = owatchNowaitReadByAny(NULL, ooid, pvalue, data_buf, buf_len);
	return owatchInternalWaitOp(op);
}


owop_t
owatchReadByName(const char *desc, oval_t * pvalue, char *data_buf, int buf_len)
{
	owop_t  op;

	if (!desc || !*desc || !pvalue || buf_len < 0)
		return OWOP_ERROR;
	op = owatchNowaitReadByAny(desc, 0, pvalue, data_buf, buf_len);
	return owatchInternalWaitOp(op);
}


typedef struct
{
	owquark_t info;
	oval_t src_val;
	oval_t dst_val;
	char    str_buf[4];
} OwatchWriteRecord_t;


oret_t
owatchHandleWriteReply(int kind, int type, int len, char *buf)
{
	owop_t  op;
	ooid_t ooid;
	ulong_t ulv;
	int     err;

	if (len != 12) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(5, "WRITE() invalid len=%d", len);
		return ERROR;
	}
	ulv = *(ulong_t *) (buf + 0);
	ulv = ONTOHL(ulv);
	op = (owop_t) ulv;
	ulv = *(ulong_t *) (buf + 4);
	ulv = ONTOHL(ulv);
	ooid = (ooid_t) ulv;
	ulv = *(ulong_t *) (buf + 8);
	ulv = ONTOHL(ulv);
	err = (int) ulv;
	if (!owatchIsValidOp(op)) {
		err = OWATCH_ERR_TOOLATE;
		owatchLog(5, "WRITE(%xh) too late", op);
		return ERROR;
	}
	owatchLog(6, "WRITE(%xh) code=%d", op, err);
	owatchFinalizeOp(op, err);
	return OK;
}


int
owatchLazyWriteRequest(owop_t m_op, owop_t s_op, int err_code,
					  long param1, long param2)
{
	owatchUnchainOp(m_op);
	if (err_code) {
		owatchLog(7, "write ooid=%ld lazy err=%d", param1, err_code);
	} else {
		owatchLog(7, "finally wrote ooid=%ld", param1);
	}
	owatchFinalizeOp(m_op, err_code);
	return 0;
}


int
owatchWriteGotDescStage(owop_t m_op, owop_t s_op, int err_code,
					   long param1, long param2)
{
	OwatchWriteRecord_t *pwrec = (OwatchWriteRecord_t *) param2;
	char   *buf = NULL;
	int     p;
	ulong_t ulv;
	int     n;
	uchar_t undef = 0;
	char   *b;
	oret_t rc;

	owatchUnchainOp(m_op);
	if (err_code)
		goto FAIL;
	n = pwrec->dst_val.len = pwrec->info.len;
	if (NULL == (buf = oxnew(char, n + 32))) {
		err_code = OWATCH_ERR_NOSPACE;
		goto FAIL;
	}
	ulv = m_op;
	*(ulong_t *) (buf + 0) = OHTONL(ulv);
	ulv = pwrec->info.ooid;
	*(ulong_t *) (buf + 4) = OHTONL(ulv);
	p = 8;
	buf[p++] = pwrec->info.type;
	if (undef)
		n = 0;
	if (n < 0x20) {
		buf[p++] = (char) (n | undef);
	} else {
		buf[p++] = (char) (0x20 | ((n >> 8) & 0x1f) | undef);
		buf[p++] = (char) (uchar_t) (n & 0xff);
	}
	if (n > 0) {
		pwrec->dst_val.type = pwrec->info.type;
		if (pwrec->src_val.type == pwrec->dst_val.type) {
			if (pwrec->src_val.type == 's')
				memcpy(buf + p, pwrec->str_buf, n);
			else {
				b = (char *) &pwrec->src_val.v;
				memcpy(buf + p, b, n);
			}
		} else if (pwrec->dst_val.type == 's') {
			b = buf + p;
			rc = owatchConvertValue(&pwrec->src_val, &pwrec->dst_val, b, n);
			if (rc != OK) {
				err_code = OWATCH_ERR_INVALID;
				goto FAIL;
			}
		} else {
			rc = owatchConvertValue(&pwrec->src_val, &pwrec->dst_val, NULL, 0);
			if (rc != OK) {
				err_code = OWATCH_ERR_INVALID;
				goto FAIL;
			}
			b = (char *) &pwrec->dst_val.v;
			memcpy(buf + p, b, n);
		}
		p += n;
	}
	s_op = owatchSecureSend(OLANG_DATA, OLANG_OOID_WRITE_REQ, p, buf);
	if (s_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NETWORK;
		goto FAIL;
	}
	if (s_op != OWOP_OK)
		owatchChainOps(m_op, s_op, owatchLazyWriteRequest,
					  (long) pwrec->info.ooid, 0);
	owatchLog(7, "ask write ooid=%lu desc=[%s]",
				pwrec->info.ooid, pwrec->info.desc);
	owatchSetOpData(m_op, OWATCH_OPT_WRITE, 0);
	oxfree(buf);
	oxfree(pwrec);
	return OK;
  FAIL:
	owatchSetOpData(m_op, OWATCH_OPT_WRITE, 0);
	oxfree(pwrec);
	oxfree(buf);
	owatchLog(7, "write ooid=%ld err=%d", param1, err_code);
	owatchFinalizeOp(m_op, err_code);
	return ERROR;
}


int
owatchWriteCanceller(owop_t op, long data1, long data2)
{
	oxfree((OwatchWriteRecord_t *) data2);
	return 0;
}


oret_t
owatchFastFormsWrite(const char *desc, ooid_t ooid,
				   char type, int timeout, char *data, int size)
{
	static const char *me = "owatchFastFormsWrite";
	char   *buf = NULL;
	int     p, n, err_code;
	ulong_t ulv;
	uchar_t undef = 0;
	oret_t rc;
	owop_t  op = OWOP_ERROR;
	owquark_t info;

	if (type != 0 && ooid != 0) {
		n = size;
		if (desc == 0) {
			sprintf(info.desc, "<%lu>", ooid);
			desc = info.desc;
		}
	} else if (ooid != 0) {
		if (timeout == 0) {
			if (owatchFindCachedInfoByOoid(ooid, &info) != OK) {
				owatchLog(2, "%s(ooid=%lu) size=%d info not found",
							me, ooid, size);
				goto FAIL;
			}
		} else {
			op = owatchGetInfoByOoid(ooid, &info);
			if (owatchWaitOp(op, timeout, &err_code) != OK) {
				owatchLog(2, "%s(ooid=%lu) size=%d GINFO error=%s",
							me, ooid, size, owatchErrorString(err_code));
				goto FAIL;
			}
		}
		desc = info.desc;
		n = info.len;
		type = info.type;
	} else if (desc && *desc) {
		if (timeout == 0) {
			if (owatchFindCachedInfoByDesc(desc, &info) != OK) {
				owatchLog(2, "%s(desc=%s) size=%d info not found",
							me, desc, size);
				goto FAIL;
			}
		} else {
			op = owatchGetInfoByDesc(desc, &info);
			if (owatchWaitOp(op, timeout, &err_code) != OK) {
				owatchLog(2, "%s(desc=%s) size=%d GINFO error=%s",
							me, desc, size, owatchErrorString(err_code));
				goto FAIL;
			}
		}
		ooid = info.ooid;
		n = info.len;
		type = info.type;
	} else {
		goto FAIL;
	}
	if (n != size) {
		owatchLog(2, "%s(ooid=%lu, desc=%s) size=%d mismatches %d",
					me, ooid, desc, size, n);
		goto FAIL;
	}
	if (NULL == (buf = oxnew(char, n + 32)))
		goto FAIL;
	op = owatchAllocateOp(owatchWriteCanceller, OWATCH_OPT_WRITE, 0);
	ulv = op;
	*(ulong_t *) (buf + 0) = OHTONL(ulv);
	ulv = ooid;
	*(ulong_t *) (buf + 4) = OHTONL(ulv);
	p = 8;
	buf[p++] = type;
	if (undef)
		n = 0;
	if (n < 0x20) {
		buf[p++] = (char) (n | undef);
	} else {
		buf[p++] = (char) (0x20 | ((n >> 8) & 0x1f) | undef);
		buf[p++] = (char) (uchar_t) (n & 0xff);
	}
	if (n > 0) {
		memcpy(buf + p, data, n);
		p += n;
	}
	rc = owatchSendPacket(OLANG_DATA, OLANG_OOID_WRITE_REQ, p, buf);
	if (rc != OK) {
		owatchLog(2, "%s(ooid=%lu, desc=%s) network error",
					me, ooid, desc);
		err_code = OWATCH_ERR_NETWORK;
		goto FAIL;
	}
	owatchLog(7, "%s: fast ask write ooid=%lu desc=[%s]",
				me, ooid, desc);
	oxfree(buf);
	if (timeout == 0)
		rc = OK;
	else
		rc = owatchWaitOp(op, timeout, &err_code);
	owatchDeallocateOp(op);
	if (rc != OK) {
		owatchLog(2, "%s(ooid=%lu, desc=%s) write error %s",
					me, ooid, info.desc, owatchErrorString(err_code));
	}
	return rc;
  FAIL:
	if (op != OWOP_ERROR)
		owatchDeallocateOp(op);
	oxfree(buf);
	return ERROR;
}


owop_t
owatchNowaitWriteByAny(const char *a_desc, ooid_t a_ooid, const oval_t * pval)
{
	OwatchWriteRecord_t *pwrec = NULL;
	owop_t  m_op = OWOP_ERROR;
	owop_t  s_op = OWOP_ERROR;
	oret_t rc;
	int     err_code;
	int     len = owatchGetValueLength(pval) + 4;
	pwrec = (OwatchWriteRecord_t *) oxnew(char,
										  sizeof(OwatchWriteRecord_t) + len);
	if (NULL == pwrec) {
		err_code = OWATCH_ERR_NOSPACE;
		goto FAIL;
	}
	err_code = OWATCH_ERR_NOSPACE;
	m_op = owatchAllocateOp(owatchWriteCanceller, OWATCH_OPT_WRITE, (long) pwrec);
	if (m_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NOSPACE;
		goto FAIL;
	}
	rc = owatchCopyValue(pval, &pwrec->src_val, pwrec->str_buf, len);
	if (rc != OK) {
		err_code = OWATCH_ERR_INVALID;
		goto FAIL;
	}
	if (a_desc)
		s_op = owatchNowaitGetInfoByDesc(a_desc, &pwrec->info);
	else
		s_op = owatchNowaitGetInfoByOoid(a_ooid, &pwrec->info);
	if (s_op == OWOP_ERROR) {
		err_code = OWATCH_ERR_NOINFO;
		goto FAIL;
	}
	rc = owatchChainOps(m_op, s_op, owatchWriteGotDescStage, 0, (long) pwrec);
	if (rc != OK) {
		err_code = OWATCH_ERR_INTERNAL;
		goto FAIL;
	}
	/* FIXME: got to cancel all requests if hub is dead */
	/*        or buffer up requests at hub */
	return m_op;
  FAIL:
	oxfree(pwrec);
	if (s_op != OWOP_ERROR)
		owatchCancelOp(s_op);
	if (m_op != OWOP_ERROR)
		owatchFinalizeOp(m_op, err_code);
	return OWOP_ERROR;
}


owop_t
owatchWrite(ooid_t ooid, const oval_t * pval)
{
	owop_t  op;

	if (!ooid || !pval || pval->undef)
		return OWOP_ERROR;
	op = owatchNowaitWriteByAny(NULL, ooid, pval);
	return owatchInternalWaitOp(op);
}


owop_t
owatchWriteByName(const char *desc, const oval_t * pval)
{
	owop_t  op;

	if (!desc || !*desc || !pval || pval->undef)
		return OWOP_ERROR;
	op = owatchNowaitWriteByAny(desc, 0, pval);
	return owatchInternalWaitOp(op);
}
