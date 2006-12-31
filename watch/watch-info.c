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

  API to inquire the data quark information.

*/

#include "watch-priv.h"
#include <optikus/tree.h>
#include <optikus/conf-net.h>	/* for OHTONL,... */
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxstrdup,oxvzero,... */
#include <string.h>				/* for strlen,strcpy,strcmp */


typedef struct
{
	char    desc[OWATCH_MAX_PATH];
	owquark_t *quark_p;
} OwatchGetQuark_t;

typedef struct
{
	owquark_t *quark_p;
} OwatchQuarkCacheEntry_t;

OwatchQuarkCacheEntry_t *owatch_quark_cache;

int     owatch_quark_cache_len;
int     owatch_quark_cache_size;
tree_t  owatch_by_desc_quark_hash;
tree_t  owatch_by_ooid_quark_hash;
tree_t  owatch_reject_quark_hash;


oret_t
owatchFlushInfoCache(void)
{
	int     i;

	if (NULL != owatch_quark_cache) {
		for (i = 0; i < owatch_quark_cache_len; i++) {
			oxfree(owatch_quark_cache[i].quark_p);
			owatch_quark_cache[i].quark_p = NULL;
		}
	}
	oxfree(owatch_quark_cache);
	owatch_quark_cache = NULL;
	owatch_quark_cache_len = owatch_quark_cache_size = 0;

	treeFree(owatch_by_desc_quark_hash);
	owatch_by_desc_quark_hash = NULL;
	treeFree(owatch_by_ooid_quark_hash);
	owatch_by_ooid_quark_hash = NULL;
	treeFree(owatch_reject_quark_hash);
	owatch_reject_quark_hash = NULL;

	owatch_by_desc_quark_hash = treeAlloc(0);
	owatch_by_ooid_quark_hash = treeAlloc(NUMERIC_TREE);
	owatch_reject_quark_hash = treeAlloc(0);

	return OK;
}


oret_t
owatchFindCachedInfoByDesc(const char *desc, owquark_t * quark_p)
{
	int     no;
	bool_t  found;

	found = treeFind(owatch_by_desc_quark_hash, desc, &no);
	if (!found)
		return ERROR;
	if (quark_p != NULL)
		oxvcopy(owatch_quark_cache[no].quark_p, quark_p);
	return OK;
}


oret_t
owatchFindCachedInfoByOoid(ulong_t ooid, owquark_t * quark_p)
{
	int     key = (int) ooid;
	int     no;
	bool_t  found;

	found = treeNumFind(owatch_by_ooid_quark_hash, key, &no);
	if (!found)
		return ERROR;
	if (quark_p != NULL)
		oxvcopy(owatch_quark_cache[no].quark_p, quark_p);
	return OK;
}


oret_t
owatchAddInfoToCache(owquark_t * quark_p, bool_t secondary)
{
	OwatchQuarkCacheEntry_t *new_cache;
	owquark_t *cached_quark_p;
	int     new_size;
	int     no;
	oret_t rc = OK;

	if (owatchFindCachedInfoByDesc(quark_p->desc, NULL) == OK) {
		owatchLog(9, "already in cache desc=[%s]", quark_p->desc);
		return OK;
	}
	if (owatch_quark_cache_len + 2 > owatch_quark_cache_size) {
		new_size = owatch_quark_cache_size * 2 + 2;
		new_cache = oxrenew(OwatchQuarkCacheEntry_t, new_size,
							owatch_quark_cache_len, owatch_quark_cache);
		if (NULL == new_cache) {
			owatchLog(1, "quark cache out of memory");
			return ERROR;
		}
		owatch_quark_cache = new_cache;
		owatch_quark_cache_size = new_size;
	}
	cached_quark_p = oxnew(owquark_t, 1);
	oxvcopy(quark_p, cached_quark_p);
	no = owatch_quark_cache_len++;
	owatch_quark_cache[no].quark_p = cached_quark_p;
	if (treeAdd(owatch_by_desc_quark_hash, quark_p->desc, no) != OK) {
		owatchLog(6, "cannot add desc hash desc=\"%s\" ooid=%lu",
					quark_p->desc, quark_p->ooid);
		rc = ERROR;
	}
	if (treeNumAdd(owatch_by_ooid_quark_hash, (int) quark_p->ooid, no) != OK) {
		if (!secondary) {
			owatchLog(6, "cannot add ooid hash desc=\"%s\" ooid=%lu",
						quark_p->desc, quark_p->ooid);
			rc = ERROR;
		}
	}
	if (rc == OK) {
		owatchLog(7, "cached OK desc=[%s] ooid=%lu",
					quark_p->desc, quark_p->ooid);
	}
	return rc;
}


oret_t
owatchUpdateInfoCache(const char *desc, owquark_t * quark_p)
{
	if (quark_p) {
		owatchAddInfoToCache(quark_p, FALSE);
		if (desc != 0 && desc[0] != 0 && strcmp(desc, quark_p->desc) != 0) {
			strcpy(quark_p->desc, desc);
			owatchAddInfoToCache(quark_p, TRUE);
		}
	} else {
		if (desc != 0 && desc[0] != 0)
			treeAdd(owatch_reject_quark_hash, desc, 1);
	}
	return OK;
}


oret_t
owatchUnpackInfoReply(char *buf, int *p_ptr,
					 owquark_t * quark_p, char *ermes, int ermes_len)
{
	int     p = p_ptr ? *p_ptr : 0;
	ushort_t usv;
	ulong_t ulv;
	int     n;

	oxvzero(quark_p);
	*ermes = 0;
	ulv = *(ulong_t *) (buf + p);
	p += 4;
	quark_p->ooid = ONTOHL(ulv);
	if (!quark_p->ooid) {
		/* match not found */
		n = (int) (uchar_t) buf[p++];
		if (n > ermes_len - 2)
			n = ermes_len - 2;
		oxbcopy(buf + p, ermes, n);
		ermes[n] = 0;
		p += n;
		if (p_ptr)
			*p_ptr = p;
		return ERROR;
	}
	/* match found */
	usv = *(ushort_t *) (buf + p);
	p += 2;
	quark_p->len = (int) (ushort_t) ONTOHS(usv);
	quark_p->ptr = buf[p++];
	quark_p->type = buf[p++];
	quark_p->seg = buf[p++];
	p++;
	usv = *(ushort_t *) (buf + p);
	p += 2;
	quark_p->bit_off = (int) (ushort_t) ONTOHS(usv);
	usv = *(ushort_t *) (buf + p);
	p += 2;
	quark_p->bit_len = (int) (ushort_t) ONTOHS(usv);
	usv = *(ushort_t *) (buf + p);
	p += 2;
	n = (int) (ushort_t) ONTOHS(usv);
	if (n > 0 && n < OWATCH_MAX_PATH) {
		memcpy(quark_p->desc, buf + p, n);
		p += n;
		if (p & 1)
			p++;
		if (p_ptr)
			*p_ptr = p;
		return OK;
	}
	if (p_ptr)
		*p_ptr = p;
	return ERROR;
}


oret_t
owatchHandleGetInfoReply(int kind, int type, int len, char *buf)
{
	OwatchGetQuark_t *qrec = NULL;
	owquark_t quark;
	int     p;
	owop_t  op;
	ulong_t ulv;
	long    data1 = 0, data2 = 0;
	int     err;
	char    ermes[40];
	oret_t rc;

	/* deserialize results */
	owatchLog(9, "parse get_info reply len=%d", len);
	oxvzero(&quark);
	p = 0;
	ulv = *(ulong_t *) (buf + p);
	ulv = ONTOHL(ulv);
	p += 4;
	op = (owop_t) ulv;
	if (!owatchIsValidOp(op)) {
		err = OWATCH_ERR_TOOLATE;
		owatchLog(5, "GINFO(%xh) ERR too late", op);
		op = OWOP_ERROR;
		goto FAIL;
	}
	owatchGetOpData(op, &data1, &data2);
	if ((data1 != OWATCH_OPT_DESC_INFO && data1 != OWATCH_OPT_OOID_INFO)
		|| !data2) {
		owatchLog(5, "GINFO(%xh) ERR not GINFO data1=%ld data2=%ld",
					op, data1, data2);
		op = OWOP_ERROR;
		err = OWATCH_ERR_SCREWED;
		goto FAIL;
	}
	qrec = (OwatchGetQuark_t *) data2;
	rc = owatchUnpackInfoReply(buf, &p, &quark, ermes, sizeof(ermes));
	if (!quark.ooid) {
		owatchUpdateInfoCache(qrec->desc, NULL);
		err = OWATCH_ERR_REFUSED;
		owatchLog(5, "GINFO(%xh) [%s] NOT FOUND (%s)",
					op, qrec->desc, ermes);
		goto FAIL;
	}
	if (!quark.desc[0]) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(3, "GINFO(%xh) ERR bad desc ptr=%c type=%c len=%d p=%d",
					op, quark.ptr, quark.type, quark.len, p);
		goto FAIL;
	}
	if (p != len) {
		err = OWATCH_ERR_SCREWED;
		owatchLog(3, "GINFO(%xh) ERR length mismatch p=%d len=%d", op, p, len);
		goto FAIL;
	}
	/* done OK */
	owatchLog(5, "GINFO(%xh) OK ooid=%lu '%c,%c,%c' b+%d/%d/%d [%s]",
				op, quark.ooid, quark.ptr, quark.type, quark.seg,
				quark.bit_off, quark.bit_len, quark.len, quark.desc);
	if (qrec->quark_p)
		oxvcopy(&quark, qrec->quark_p);
	owatchUpdateInfoCache(qrec->desc, &quark);
	oxfree(qrec);
	owatchFinalizeOp(op, OWATCH_ERR_OK);
	return OK;
  FAIL:
	oxfree(qrec);
	if (op != OWOP_ERROR)
		owatchFinalizeOp(op, err);
	return ERROR;
}


int
owatchLazyInfoRequest(owop_t m_op, owop_t s_op, int err_code,
					 long param1, long param2)
{
	char   *desc = (char *) param1;
	ooid_t ooid = (ooid_t) param2;

	if (err_code)
		goto FAIL;
	owatchUnchainOp(m_op);
	if (owatch_verb > 6) {
		if (desc)
			olog("finally send info_req desc=[%s]", desc);
		else
			olog("finally send info_req ooid=%lu", ooid);
	}
	oxfree(desc);
	return OK;
  FAIL:
	oxfree(desc);
	owatchUnchainOp(m_op);
	owatchFinalizeOp(m_op, err_code);
	return ERROR;
}


int
owatchCancelGetInfoRequest(owop_t op, long data1, long data2)
{
	oxfree((OwatchGetQuark_t *) data2);
	return 0;
}


owop_t
owatchNowaitGetInfoByDesc(const char *desc, owquark_t * quark_p)
{
	OwatchGetQuark_t *qrec = NULL;
	int     n, p;
	char    buf[OWATCH_MAX_PATH + 8];
	owop_t  m_op = OWOP_ERROR;
	owop_t  s_op = OWOP_ERROR;
	ulong_t ulv;

	if (!desc || !*desc || !quark_p)
		goto FAIL;
	if (owatchFindCachedInfoByDesc(desc, quark_p) == OK) {
		owatchLog(7, "GIBD(OK) desc=[%s] ooid=%lu",
					quark_p->desc, quark_p->ooid);
		return OWOP_OK;
	}
	if (treeFind(owatch_reject_quark_hash, desc, &n)) {
		owatchLog(6, "GIBD desc=[%s] REJECTED", desc);
		return OWOP_ERROR;
	}
	oxvzero(quark_p);
	qrec = oxnew(OwatchGetQuark_t, 1);
	if (owatchFreeOpCount() < 2)
		goto FAIL;
	if (!qrec)
		goto FAIL;
	strcpy(qrec->desc, desc);
	qrec->quark_p = quark_p;
	m_op = owatchAllocateOp(owatchCancelGetInfoRequest,
						   OWATCH_OPT_DESC_INFO, (long) qrec);
	if (m_op == OWOP_ERROR)
		goto FAIL;
	/* serialize data */
	p = 0;
	ulv = (ulong_t) m_op;
	*(ulong_t *) (buf + p) = OHTONL(ulv);
	p += 4;
	n = strlen(desc);
	if (n > 255 || p + n + 2 > sizeof(buf))
		goto FAIL;
	buf[p++] = (char) (uchar_t) n;
	oxbcopy(desc, buf + p, n);
	p += n;
	s_op = owatchSecureSend(OLANG_DATA, OLANG_INFO_BY_DESC_REQ, p, buf);
	if (s_op == OWOP_ERROR)
		goto FAIL;
	if (s_op != OWOP_OK)
		owatchChainOps(m_op, s_op, owatchLazyInfoRequest, (long) oxstrdup(desc), 0);
	owatchLog(7, "GIBD(%xh) n=%d desc=[%s]", m_op, n, desc);
	return m_op;
  FAIL:
	if (s_op != OWOP_ERROR)
		owatchCancelOp(s_op);
	if (m_op != OWOP_ERROR)
		owatchDeallocateOp(m_op);
	oxfree(qrec);
	if (NULL != quark_p)
		oxvzero(quark_p);
	return OWOP_ERROR;
}


owop_t
owatchNowaitGetInfoByOoid(ooid_t ooid, owquark_t * quark_p)
{
	OwatchGetQuark_t *qrec = NULL;
	int     p;
	char    buf[OWATCH_MAX_PATH + 8];
	owop_t  m_op = OWOP_ERROR;
	owop_t  s_op = OWOP_ERROR;
	ulong_t ulv;

	if (!ooid || NULL == quark_p)
		goto FAIL;
	if (owatchFindCachedInfoByOoid(ooid, quark_p) == OK) {
		owatchLog(7, "GIBP(OK) desc=[%s] ooid=%lu",
					quark_p->desc, quark_p->ooid);
		return OWOP_OK;
	}
	oxvzero(quark_p);
	if (NULL == (qrec = oxnew(OwatchGetQuark_t, 1)))
		goto FAIL;
	qrec->quark_p = quark_p;
	qrec->desc[0] = 0;
	m_op = owatchAllocateOp(owatchCancelGetInfoRequest,
						   OWATCH_OPT_OOID_INFO, (long) qrec);
	if (m_op == OWOP_ERROR)
		goto FAIL;
	/* serialize data */
	p = 0;
	ulv = (ulong_t) m_op;
	*(ulong_t *) (buf + p) = OHTONL(ulv);
	p += 4;
	ulv = (ulong_t) ooid;
	*(ulong_t *) (buf + p) = OHTONL(ulv);
	p += 4;
	s_op = owatchSecureSend(OLANG_DATA, OLANG_INFO_BY_OOID_REQ, p, buf);
	if (s_op == OWOP_ERROR)
		goto FAIL;
	if (s_op != OWOP_OK)
		owatchChainOps(m_op, s_op, owatchLazyInfoRequest, 0, (long) ooid);
	owatchLog(7, "GIBP(%xh) ooid=%d", m_op, (int) ooid);
	return m_op;
  FAIL:
	if (m_op != OWOP_ERROR)
		owatchDeallocateOp(m_op);
	if (s_op != OWOP_ERROR)
		owatchCancelOp(s_op);
	oxfree(qrec);
	if (NULL != quark_p)
		oxvzero(quark_p);
	return OWOP_ERROR;
}


owop_t
owatchGetInfoByDesc(const char *desc, owquark_t * quark_p)
{
	return owatchInternalWaitOp(owatchNowaitGetInfoByDesc(desc, quark_p));
}


owop_t
owatchGetInfoByOoid(ooid_t ooid, owquark_t * quark_p)
{
	return owatchInternalWaitOp(owatchNowaitGetInfoByOoid(ooid, quark_p));
}
