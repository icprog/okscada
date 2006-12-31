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

  Protocol module serving the quark information requests.

*/

#include "hub-priv.h"
#include <optikus/tree.h>
#include <optikus/conf-net.h>	/* for OHTONL,OHTONS */
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxbcopy,... */
#include <string.h>				/* for memcpy,strcpy,strlen */
#include <stdio.h>				/* for sprintf */


typedef struct
{
	OhubQuark_t *quark_p;
} OhubInfoCacheEntry_t;


OhubInfoCacheEntry_t *ohub_quark_cache;

int     ohub_quark_cache_len;
int     ohub_quark_cache_size;
tree_t  ohub_by_desc_quark_hash;
tree_t  ohub_by_ooid_quark_hash;


/*
 * .
 */
oret_t
ohubFlushInfoCache(void)
{
	int     i;

	if (NULL != ohub_quark_cache) {
		for (i = 0; i < ohub_quark_cache_len; i++) {
			if (ohub_quark_cache[i].quark_p) {
				oxfree(ohub_quark_cache[i].quark_p);
				ohub_quark_cache[i].quark_p = NULL;
			}
		}
		oxfree(ohub_quark_cache);
		ohub_quark_cache = NULL;
	}
	ohub_quark_cache_len = ohub_quark_cache_size = 0;
	if (ohub_by_desc_quark_hash)
		treeFree(ohub_by_desc_quark_hash);
	ohub_by_desc_quark_hash = NULL;
	if (ohub_by_ooid_quark_hash)
		treeFree(ohub_by_ooid_quark_hash);
	ohub_by_ooid_quark_hash = NULL;
	ohub_by_desc_quark_hash = treeAlloc(0);
	ohub_by_ooid_quark_hash = treeAlloc(NUMERIC_TREE);
	return OK;
}


oret_t
ohubFindCachedInfoByDesc(const char *desc, OhubQuark_t * quark_p)
{
	int     no;
	bool_t  found;

	found = treeFind(ohub_by_desc_quark_hash, desc, &no);
	if (!found)
		return ERROR;
	if (quark_p != NULL)
		oxvcopy(ohub_quark_cache[no].quark_p, quark_p);
	return OK;
}


oret_t
ohubFindCachedInfoByOoid(ooid_t ooid, OhubQuark_t * quark_p)
{
	int     key = (int) ooid;
	int     no;
	bool_t  found;

	found = treeNumFind(ohub_by_ooid_quark_hash, key, &no);
	if (!found)
		return ERROR;
	if (quark_p != NULL)
		oxvcopy(ohub_quark_cache[no].quark_p, quark_p);
	return OK;
}


oret_t
ohubAddInfoToCache(OhubQuark_t * quark_p)
{
	OhubInfoCacheEntry_t *new_cache;
	OhubQuark_t *cached_quark_p;
	int     new_size;
	int     no;
	oret_t rc = OK;

	if (ohubFindCachedInfoByDesc(quark_p->path, NULL) == OK) {
		ohubLog(9, "already in cache desc=[%s]", quark_p->path);
		return OK;
	}
	if (ohub_quark_cache_len + 2 > ohub_quark_cache_size) {
		new_size = ohub_quark_cache_size * 3 / 2 + 2;
		new_cache = oxrenew(OhubInfoCacheEntry_t, new_size,
							ohub_quark_cache_len, ohub_quark_cache);
		ohub_quark_cache = new_cache;
		ohub_quark_cache_size = new_size;
	}
	cached_quark_p = oxnew(OhubQuark_t, 1);
	oxvcopy(quark_p, cached_quark_p);
	no = ohub_quark_cache_len++;
	ohub_quark_cache[no].quark_p = cached_quark_p;
	if (treeAdd(ohub_by_desc_quark_hash, quark_p->path, no) != OK) {
		ohubLog(7, "cannot add desc hash desc=\"%s\" ooid=%lu",
				quark_p->path, quark_p->ooid);
		rc = ERROR;
	}
	if (treeNumAdd(ohub_by_ooid_quark_hash, (int) quark_p->ooid, no) != OK) {
		ohubLog(7, "cannot add ooid hash desc=\"%s\" ooid=%lu",
				quark_p->path, quark_p->ooid);
		rc = ERROR;
	}
	if (rc == OK) {
		ohubLog(8, "cached OK desc=[%s] ooid=%lu",
				quark_p->path, quark_p->ooid);
	}
	return rc;
}


oret_t
ohubGetInfoByDesc(const char *desc, OhubQuark_t * quark_p, char *sresult)
{
	OhubDomain_t *pdom = ohub_pdomain;
	oret_t rc;

	if (!desc || !*desc || !quark_p)
		return ERROR;
	if (ohubFindCachedInfoByDesc(desc, quark_p) == OK) {
		ohubRefineQuarkAddress(pdom, quark_p);
		if (sresult)
			strcpy(sresult, "OK");
		return OK;
	}
	rc = ohubFindQuarkByDesc(pdom, desc, quark_p, sresult);
	if (rc == OK)
		ohubAddInfoToCache(quark_p);
	return rc;
}


oret_t
ohubGetInfoByOoid(ooid_t ooid, OhubQuark_t * quark_p, char *sresult)
{
	OhubDomain_t *pdom = ohub_pdomain;
	char    desc[OHUB_MAX_PATH + 4];
	oret_t rc;

	desc[0] = 0;
	if (!ooid || !quark_p)
		return ERROR;
	if (ohubFindCachedInfoByOoid(ooid, quark_p) == OK) {
		ohubRefineQuarkAddress(pdom, quark_p);
		if (sresult)
			strcpy(sresult, "OK");
		return OK;
	}
	if (!ohubFindNameByOoid(ooid, desc)) {
		if (sresult)
			strcpy(sresult, "OOID_NOT_FOUND");
		return ERROR;
	}
	rc = ohubFindQuarkByDesc(pdom, desc, quark_p, sresult);
	if (rc == OK)
		ohubAddInfoToCache(quark_p);
	return rc;
}


int
ohubPackInfoReply(char *reply, int p, oret_t rc,
				   OhubQuark_t * quark_p, char *sresult)
{
	int     n;
	ushort_t usv;

	if (rc != OK) {
		*(ulong_t *) (reply + p) = 0;
		p += 4;
		n = strlen(sresult);
		reply[p++] = (char) n;
		oxbcopy(sresult, reply + p, n);
		if (p & 1)
			reply[p++] = 0;
		return p + n;
	}
	*(ulong_t *) (reply + p) = OHTONL(quark_p->ooid);
	p += 4;
	*(ushort_t *) (reply + p) = OHTONS(quark_p->len);
	p += 2;
	reply[p++] = quark_p->ptr;
	reply[p++] = quark_p->type;
	reply[p++] = quark_p->seg;
	reply[p++] = '.';
	*(ushort_t *) (reply + p) = OHTONS(quark_p->bit_off);
	p += 2;
	*(ushort_t *) (reply + p) = OHTONS(quark_p->bit_len);
	p += 2;
	/*for (s = desc; *s && *s != '@'; s++);
	   if (!*s)  s = desc; */
	n = strlen(quark_p->path);
	usv = (ushort_t) n;
	*(ushort_t *) (reply + p) = OHTONS(usv);
	p += 2;
	oxbcopy(quark_p->path, reply + p, n);
	p += n;
	if (p & 1)
		reply[p++] = 0;
	return p;
}

oret_t
ohubHandleInfoReq(struct OhubNetWatch_st * pnwatch, int kind,
				   int type, int len, char *buf)
{
	char    desc[OHUB_MAX_PATH + 4];
	ulong_t ulv, req_id;
	ooid_t ooid;
	int     n, p = 0;
	OhubQuark_t quark;
	char    sresult[OHUB_MAX_NAME];
	int     rc;
	char    reply[OHUB_MAX_PATH + 64];

	ulv = *(ulong_t *) (buf + p);
	p += 4;
	req_id = OHTONL(ulv);
	switch (type) {
	case OLANG_INFO_BY_DESC_REQ:
		n = (uchar_t) buf[p++];
		if (n <= 0 || n >= OHUB_MAX_PATH) {
			ohubLog(3, "invalid GIBD desc_length=%d", n);
			return ERROR;
		}
		memcpy(desc, buf + p, n);
		desc[n] = 0;
		p += n;
		if (p != len) {
			ohubLog(4, "GIBD request length mismatch p=%d len=%d", p, len);
			return ERROR;
		}
		ohubLog(7, "GIBD id=%lxh desc=[%s] n=%d", req_id, desc, n);
		rc = ohubGetInfoByDesc(desc, &quark, sresult);
		break;
	case OLANG_INFO_BY_OOID_REQ:
		ulv = *(ulong_t *) (buf + p);
		p += 4;
		if (p != len) {
			ohubLog(4, "GIBD request length mismatch p=%d len=%d", p, len);
			return ERROR;
		}
		ooid = (ooid_t) OHTONL(ulv);
		rc = ohubGetInfoByOoid(ooid, &quark, sresult);
		break;
	default:
		return ERROR;
	}
	*(ulong_t *) reply = OHTONL(req_id);
	p = ohubPackInfoReply(reply, 4, rc, &quark, sresult);
	rc = ohubWatchSendPacket(pnwatch, OLANG_DATA,
							OLANG_INFO_BY_ANY_REPLY, p, reply);
	if (rc != OK) {
		ohubLog(4, "GIBD reply packet send failure");
		return ERROR;
	}
	ohubLog(6, "GINFO reply id=%lxh len=%d "
			"ooid=%lu path=[%s] desc=[%s] rc=%d sres=[%s]",
			req_id, p, quark.ooid, quark.path, desc, rc, sresult);
	return OK;
}


/*
 *  MULTIPLEXED DATA REQUESTS
 *  FIXME: not yet ready
 */

typedef struct
{
	int     imin[4];
	int     imax[4];
	int     icur[4];
	int     inum;
	char    desc[200];
	char    format[200];
	int     fcur;
	int     fnum;
	char    result[32];
} OhubRepeater_t;

oret_t
ohubParseMultiple(const char *desc, OhubRepeater_t * prep)
{
	char    spath[200] = { 0 };
	char   *s, *d, *p;
	int     i, n, imin, imax;

	strcpy(spath, desc);
	s = spath;
	d = prep->format;
	i = 0;
	while (*s) {
		if (*s != '[') {
			if (*s == '?' || *s == ':')
				goto SYNTAX;
			*d++ = *s++;
			continue;
		}
		p = ++s;
		n = 0;
		while (*p >= '0' && *p <= '9') {
			n = n * 10 + (*p - '0');
			p++;
		}
		if (p == s)
			goto SYNTAX;
		imin = imax = n;
		if (*p == ':' || (*p == '.' && p[1] == '.')) {
			s = ++p;
			if (*p == '.')
				s = ++p;
			n = 0;
			while (*p >= '0' && *p <= '9') {
				n = n * 10 + (*p - '0');
				p++;
			}
			if (p == s)
				goto SYNTAX;
			imax = n;
		}
		if (*p != ']')
			goto SYNTAX;
		s = p + 1;
		if (imin > imax) {
			strcpy(prep->result, "BADRANGE");
			goto FAIL;
		}
		if (imin == imax) {
			d += sprintf(d, "[%d]", imin);
			continue;
		}
		if (i >= 4) {
			strcpy(prep->result, "MANYIND");
			goto FAIL;
		}
		prep->imin[i] = imin;
		prep->imax[i] = imax;
		i++;
		*d++ = '[';
		*d++ = '%';
		*d++ = 'd';
		*d++ = ']';
	}
	*d = 0;
	prep->inum = i;

	while (i < 4) {
		prep->imin[i] = prep->imax[i] = 0;
		i++;
	}
	for (i = 0; i < 4; i++)
		prep->icur[i] = 0;
	for (i = 0; i < prep->inum; i++)
		prep->icur[i] = prep->imin[i];
	if (prep->inum > 0)
		prep->icur[prep->inum - 1] -= 1;
	strcpy(prep->result, "OK");
	return OK;
  SYNTAX:
	strcpy(prep->result, "SYNERR");
  FAIL:
	return ERROR;
}


oret_t
ohubHandleMonGroupReq(struct OhubNetWatch_st * pnwatch, const char *url,
					   int kind, int type, int len, char *buf)
{
	char    desc[OHUB_MAX_PATH + 4];
	ulong_t ulv, req_id;
	ushort_t usv;
	int     n, p;
	OhubQuark_t quark;
	char    sresult[OHUB_MAX_NAME];
	int     rc;
	char   *reply = 0;
	char   *s;
	OhubRepeater_t rep;

	/* get request */
	p = 0;
	ulv = *(ulong_t *) (buf + p);
	p += 4;
	req_id = OHTONL(ulv);
	n = (uchar_t) buf[p++];
	if (n <= 0 || n >= OHUB_MAX_PATH) {
		ohubLog(3, "invalid GIGM desc_length=%d", n);
		return ERROR;
	}
	memcpy(desc, buf + p, n);
	desc[n] = 0;
	p += n;
	if (p != len) {
		ohubLog(4, "GIBD request length mismatch p=%d len=%d", p, len);
		return ERROR;
	}
	ohubLog(7, "GIBD id=%lxh desc=[%s] n=%d", req_id, desc, n);
	/* parse request */
	rc = ohubParseMultiple(desc, &rep);
	/* allocate reply */
	/* create reply */
	p = 0;
	rc = ohubGetInfoByDesc(desc, &quark, sresult);
	*(ulong_t *) (reply + p) = OHTONL(req_id);
	p += 4;
	if (rc != OK) {
		*(ulong_t *) (reply + p) = 0;
		p += 4;
		n = strlen(sresult);
		reply[p++] = (char) n;
		oxbcopy(sresult, reply + p, n);
		p += n;
	} else {
		*(ulong_t *) (reply + p) = OHTONL(quark.ooid);
		p += 4;
		*(ushort_t *) (reply + p) = OHTONS(quark.len);
		p += 2;
		reply[p++] = quark.ptr;
		reply[p++] = quark.type;
		reply[p++] = quark.seg;
		reply[p++] = '.';
		*(ushort_t *) (reply + p) = OHTONS(quark.bit_off);
		p += 2;
		*(ushort_t *) (reply + p) = OHTONS(quark.bit_len);
		p += 2;
		for (s = desc; *s && *s != '@'; s++);
		if (!*s)
			s = desc;
		n = strlen(quark.path);
		usv = (ushort_t) n;
		*(ushort_t *) (reply + p) = OHTONS(usv);
		p += 2;
		oxbcopy(quark.path, reply + p, n);
		p += n;
	}
	if (p & 1)
		reply[p++] = 0;
	/* send reply */
	rc = ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_MON_GROUP_REPLY, p, reply);
	if (rc != OK) {
		ohubLog(4, "GIBD reply packet send failure");
		return ERROR;
	}
	ohubLog(6, "GIGN reply id=%luh len=%d", req_id, p);
	return OK;
}
