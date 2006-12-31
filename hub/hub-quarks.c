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

  Operations on quarks: search, book, fetch/store in flat files etc.

*/

#include "hub-priv.h"
#include <optikus/conf-mem.h>	/* for oxfree,oxvzero */
#include <stdio.h>				/* for FILE,sscanf */
#include <string.h>				/* for strcpy,strchr,strcat,strlen */


#define OHUB_MAX_LINE_LEN  256


typedef struct
{
	void   *ptr;
	int     size;
	int     qty;
	int     max_qty;
} OhubDynamicArray_t;

#define arrPtr(a,t,n)     (((t*)(a).ptr)+(n))


static void
arrGrow(OhubDynamicArray_t * ap, int gap)
{
	int     qty = ap->qty;
	void   *ptr;

	ap->qty += gap;
	if (ap->qty >= ap->max_qty) {
		while (ap->max_qty <= ap->qty)
			ap->max_qty = ap->max_qty * 4 / 3 + 2;
		if (NULL == (ptr = oxcalloc(ap->max_qty, ap->size)))
			ologAbort("malGrow: not enough memory for grow");
		if (0 != qty && NULL != ap->ptr)
			oxbcopy(ap->ptr, ptr, qty * ap->size);
		if (NULL != ap->ptr)
			free(ap->ptr);
		ap->ptr = ptr;
	}
}


/*
 * .
 */
oret_t
ohubLoadModuleQuarks(OhubModule_t * pmod)
{
	FILE   *file;
	OhubRecord_t *prh, *prh2;
	char    buf[OHUB_MAX_LINE_LEN + 4];
	char    path[OHUB_MAX_PATH + 4], glob_name[OHUB_MAX_PATH + 4];
	char   *s;
	int     k, n, lineno;
	char    c;
	OhubDynamicArray_t aquark = { .size = sizeof(OhubRecord_t) };
	OhubDynamicArray_t apath = { .size = sizeof(char) };
	OhubDynamicArray_t aglob = { .size = sizeof(char) };

	oxfree(pmod->records);
	pmod->records = NULL;
	pmod->record_qty = 0;
	oxfree(pmod->glob_heap);
	pmod->glob_heap = NULL;
	pmod->glob_heap_len = pmod->glob_heap_size = 0;
	oxfree(pmod->path_heap);
	pmod->path_heap = NULL;
	pmod->path_heap_len = pmod->path_heap_size = 0;
	treeFree(pmod->quarks_byname);
	treeFree(pmod->globals_byname);
	pmod->quarks_byname = NULL;
	pmod->globals_byname = NULL;
	file = fopen(pmod->quark_file, "r");
	if (file == NULL) {
		olog("error: cannot read quark from \"%s\"", pmod->quark_file);
		return ERROR;
	}

	arrGrow(&aquark, 1);
	pmod->quarks_byname = treeAlloc(0);
	pmod->globals_byname = treeAlloc(0);

	/* parse lines */
	lineno = 0;
	while (fgets(buf, OHUB_MAX_LINE_LEN, file) != NULL) {
		for (s = buf; *s; s++)
			if (*s == '\n')
				lineno++;
		ohubTrim(buf);
		/* skip comments and blank lines */
		c = *buf;
		if (c == 0 || c == ';' || c == '#')
			continue;
		/* parse buffer */
		prh = arrPtr(aquark, OhubRecord_t, pmod->record_qty);
		*path = *glob_name = 0;
		oxvzero(prh);
		n = sscanf(buf,
				   /*k|path    |pt|ty|adr|off|le|bo|bl|c0|c1|c2|c3|d0 |d1 |d2 |d3 |glbname|ga |se|oi */
				   "%d|%140[^|]|%c|%c|%ld|%ld|%d|%d|%d|%d|%d|%d|%d|%hd|%hd|%hd|%hd|%140[^|]|%ld|%c|%d",
				   &prh->ikey, path, &prh->ptr, &prh->type,
				   &prh->adr, &prh->off, &prh->len, &prh->bit_off,
				   &prh->bit_len, &prh->coef[0], &prh->coef[1], &prh->coef[2],
				   &prh->coef[3], &prh->dim[0], &prh->dim[1], &prh->dim[2],
				   &prh->dim[3], glob_name, &prh->glob_adr, &prh->seg,
				   &prh->obj_ikey);
		if (n != 21 || !*path || !*glob_name) {
			olog("ERROR[%s:%d]: wrong line format: \"%s\"",
				 pmod->quark_file, lineno, buf);
			continue;
		}
		prh->ukey = pmod->unit_no;
		if (prh->ukey != prh->obj_ikey) {
			olog("WARN [%s:%d]: obj_ikey=%d, but ukey=%d for path=\"%s\"",
				 pmod->quark_file, lineno, prh->obj_ikey, prh->ukey, path);
			prh->ukey = prh->obj_ikey;
		}
		if (!SEG_IS_VALID(prh->seg)) {
			olog("WARN [%s:%d]: invalid segment=%02xh for path=\"%s\"",
				 pmod->quark_file, lineno, (int) (uchar_t) prh->seg, path);
			prh->seg = '?';
		}
		if (prh->bit_off < 0 || prh->bit_off > 255
			|| prh->bit_len < 0 || prh->bit_len > 255) {
			if (!(strchr(path, ':') && strchr(path, '!')))
				olog("WARN [%s:%d]: invalid bitoff=%d bitlen=%d path=\"%s\"",
					 pmod->quark_file, lineno, prh->bit_off, prh->bit_len,
					 path);
			prh->bit_off = prh->bit_len = 0;
		}

		/* redefine some fields */
		prh->ikey = pmod->record_qty;
		prh->obj_ikey = pmod->ikey;
		prh->glob_ikey = 0;
		prh->next_no = -1;

		/* append names to heaps */
		n = strlen(path) + 1;
		arrGrow(&apath, n);

		prh->path_off = pmod->path_heap_len;
		strcpy(arrPtr(apath, char, prh->path_off), path);

		pmod->path_heap_len += n;

		n = strlen(glob_name) + 1;
		arrGrow(&aglob, n);

		prh->glob_off = pmod->glob_heap_len;
		strcpy(arrPtr(aglob, char, prh->glob_off), glob_name);

		pmod->glob_heap_len += n;

		/* add to quark hash */
		if (treeFind(pmod->quarks_byname, path, &k)) {
			ohubLog(5, "WARN [%s:%d] duplicate quark \"%s\"",
					pmod->quark_file, lineno, path);
			continue;
		}
		treeAdd(pmod->quarks_byname, path, pmod->record_qty);

		/* add to globals hash */
		if (treeFind(pmod->globals_byname, glob_name, &k)) {
			prh2 = arrPtr(aquark, OhubRecord_t, k);
			while (prh2->next_no >= 0)
				prh2 = arrPtr(aquark, OhubRecord_t, prh2->next_no);
			prh2->next_no = pmod->record_qty;
		} else {
			k = pmod->record_qty;
			treeAdd(pmod->globals_byname, glob_name, k);
		}

		/* add to array */
		arrGrow(&aquark, 1);
		pmod->record_qty++;
	}

	fclose(file);
	pmod->records = arrPtr(aquark, OhubRecord_t, 0);
	pmod->path_heap = arrPtr(apath, char, 0);
	pmod->glob_heap = arrPtr(aglob, char, 0);

	pmod->path_heap_size = apath.max_qty;
	pmod->glob_heap_size = aglob.max_qty;

	ohubLog(4, "INFO[%s]: %d records for \"%s\" (%dk/%dk/%dk)",
			ohubShortenPath(pmod->quark_file, path, 0),
			pmod->record_qty, pmod->nick_name,
			pmod->record_qty * sizeof(OhubRecord_t) / 1024,
			pmod->path_heap_size / 1024, pmod->glob_heap_size / 1024);
	return OK;
}


oret_t
ohubLoadDomainQuarks(struct OhubDomain_st * pdom)
{
	OhubModule_t *pmod;
	int     i;
	oret_t rc = OK;

	if (pdom == NULL)
		return ERROR;
	for (i = 0; i < pdom->module_qty; i++) {
		pmod = &pdom->modules[i];
		if (!pmod->psubject->pagent->enable)
			continue;
		if (ohubLoadModuleQuarks(pmod) != OK)
			rc = ERROR;
	}
	return rc;
}


/*
 *  convert pforms-formatted data path into address/offset
 */
static OhubRecord_t *
ohubFindCanonicalRecord(OhubDomain_t * pdom,
						 const char *module,
						 const char *subject,
						 const char *path, OhubModule_t ** pmod_ptr)
{
	OhubModule_t *pmod;
	int     i, k;

	pmod = &pdom->modules[0];
	for (i = 0; i < pdom->module_qty; i++, pmod++) {
		if (*module && strcmp(module, pmod->nick_name) != 0)
			continue;
		if (*subject && strcmp(subject, pmod->subject_name) != 0)
			continue;
		if (treeFind(pmod->quarks_byname, path, &k)) {
			if (k < pmod->record_qty) {
				if (pmod_ptr)
					*pmod_ptr = pmod;
				return &pmod->records[k];
			}
			return NULL;
		}
	}
	return NULL;
}


int
ohubFindQuarkByDesc(OhubDomain_t * pdom, const char *spath,
					 OhubQuark_t * presult, char *sresult)
{
	OhubRecord_t *prh;
	OhubModule_t *pmod;
	int     i, k, n, ind[4];
	char   *s, *d, *p, *tpath;
	char    bpath[OHUB_MAX_PATH + 4];
	char    xpath[OHUB_MAX_PATH + 4];
	char   *subject, *module;

	oxvzero(presult);
	*sresult = 0;
	if (strlen(spath) + 2 > sizeof(bpath)) {
		if (sresult)
			strcpy(sresult, "PATH_TOO_LONG");
		return -1;
	}
	if (strchr(spath, '?') || strchr(spath, '!')) {
		if (sresult)
			strcpy(sresult, "WRONG_METACHAR");
		return -3;
	}

	/* extract subject and module */
	strcpy(bpath, spath);
	ohubTrim(bpath);
	subject = module = "";
	tpath = bpath;
	s = strchr(bpath, '@');
	if (s) {
		p = strchr(bpath, '/');
		if (p && p < s) {
			/* both subject and module */
			k = (int) (p - bpath);
			subject = tpath;
			subject[k] = 0;
			n = (int) (s - p);
			module = p + 1;
			module[n - 1] = 0;
			tpath = s + 1;
		} else {
			/* only subject */
			n = (int) (s - bpath);
			subject = bpath;
			subject[n] = 0;
			tpath = s + 1;
		}
	}

	/* extract indexes */
	strcpy(xpath, tpath);
	s = d = tpath;
	i = 0;
	while (*s) {
		if (*s == '[' && s[1] >= '0' && s[1] <= '9') {
			p = s + 1;
			n = 0;
			while (*p >= '0' && *p <= '9') {
				n = n * 10 + (*p - '0');
				p++;
			}
			if (*p == ']') {
				if (i >= 4) {
					if (sresult)
						strcpy(sresult, "TOO_MANY_INDEXES");
					return -2;
				}
				ind[i++] = n;
				s = p + 1;
				*d++ = '?';
				continue;
			}
		}
		*d++ = *s++;
	}
	*d = 0;
	while (i < 4)
		ind[i++] = 0;

	/* search quark by canonical name. FIXME: order is significant !  */
	prh = ohubFindCanonicalRecord(pdom, module, subject, tpath, &pmod);
	if (!prh) {
		/* try to find it as a string */
		strcat(tpath, "+");
		prh = ohubFindCanonicalRecord(pdom, module, subject, tpath, &pmod);
	}
	if (!prh) {
		if (sresult)
			sprintf(sresult, "NOT_FOUND(%s/%s)", subject, module);
		return -3;
	}

	ohubRecordToQuark(pdom, prh, presult);
	sprintf(presult->path, "%s/%s@%s",
			pmod->subject_name, pmod->nick_name, xpath);
	ohubLog(8, "quarkByDesc found "
			"dsc=[%s] p=%c t=%c d=[%d,%d,%d,%d] c=[%d,%d,%d,%d]",
			pmod->path_heap + prh->path_off, prh->ptr, prh->type,
			prh->dim[0], prh->dim[1], prh->dim[2], prh->dim[3],
			prh->coef[0], prh->coef[1], prh->coef[2], prh->coef[3]);

	/* crunch offset from indexes */
	for (i = 0; i < 4; i++) {
		k = ind[i];
		n = presult->dim[i];
		if ((n == 0 && k != 0) || (n != 0 && k >= n) || k < 0) {
			if (sresult)
				sprintf(sresult, "OUT_OF_RANGE_%d", i);
			return -4;
		}
		presult->off += k * presult->coef[i];
		presult->ind[i] = k;
	}

	ohubRefineQuarkAddress(pdom, presult);

	if (sresult)
		strcpy(sresult, "OK");
	presult->ooid = ohubMakeStaticOoid(pdom, presult);
	return 0;
}


/* crunch physical addr */
bool_t
ohubRefineQuarkAddress(OhubDomain_t * pdom, OhubQuark_t * quark_p)
{
	long    seg_addr, phys_addr = -1;
	int     mod_no = quark_p->obj_ikey;
	char    seg = quark_p->seg;

	if (mod_no >= 0 && mod_no < pdom->module_qty && SEG_IS_VALID(seg)) {
		seg_addr = pdom->modules[mod_no].segment_addr[SEG_NAME_2_NO(seg)];
		if (seg_addr != -1)
			phys_addr = seg_addr + quark_p->adr;
	}
	if (phys_addr != quark_p->phys_addr) {
		quark_p->phys_addr = phys_addr;
		return TRUE;
	}
	return FALSE;
}


/*
 * primitive type length in bytes
 */
int
ohubType2Len(char type)
{
	switch (type) {
	case 'b':	return 1;				/* char, signed char  */
	case 'B':	return 1;				/* unsigned char      */
	case 'h':	return 2;				/* short              */
	case 'H':	return 2;				/* unsigned short     */
	case 'i':	return 4;				/* int                */
	case 'I':	return 4;				/* unsigned int       */
	case 'l':	return 4;				/* long               */
	case 'L':	return 4;				/* unsigned long      */
	case 'q':	return 8;				/* long long          */
	case 'Q':	return 8;				/* unsigned long long */
	case 'f':	return 4;				/* float              */
	case 'd':	return 8;				/* double             */
	case 'D':	return 16;				/* long double        */
	case 'p':	return 4;				/* (*func)()          */
	case 'v':	return 0;				/* void               */
	case 's':	return 0;				/* string, [[un]signed] char* */
	case 'E':	return 4;				/* enumeration        */
	}
	return 0;	/* unknown / invalid */
}


char *
ohubQuarkToString(OhubQuark_t * pq, char *pbuf)
{
	static char sbuf[256];
	char   *s;

	if (!pbuf)
		pbuf = sbuf;
	s = pbuf;
	s += sprintf(s, "#%d \"%s\" %c:%c @0x%lx +%ld,%d b+%d,%d ",
				 pq->ikey, pq->path,
				 pq->ptr, pq->type, pq->adr,
				 pq->off, pq->len, pq->bit_off, pq->bit_len);
	s += sprintf(s, "c=[%d,%d,%d,%d] d=[%d,%d,%d,%d] ",
				 pq->coef[0], pq->coef[1], pq->coef[2], pq->coef[3],
				 pq->dim[0], pq->dim[1], pq->dim[2], pq->dim[3]);
	s += sprintf(s, "i=[%d,%d,%d,%d] ",
				 pq->ind[0], pq->ind[1], pq->ind[2], pq->ind[3]);
	s += sprintf(s, "*%d", pq->glob_ikey);
	return pbuf;
}


oret_t
ohubRecordToQuark(OhubDomain_t *pdom, OhubRecord_t *rec_p, OhubQuark_t *quark_p)
{
	OhubModule_t *pmod;
	int     i;

	pmod = &pdom->modules[rec_p->obj_ikey];
	quark_p->ukey = rec_p->ukey;
	quark_p->ikey = rec_p->ikey;
	quark_p->adr = rec_p->adr;
	quark_p->off0 = quark_p->off = rec_p->off;
	quark_p->len = rec_p->len;
	quark_p->ptr = rec_p->ptr;
	quark_p->type = rec_p->type;
	quark_p->seg = rec_p->seg;
	quark_p->spare1 = rec_p->spare1;
	quark_p->obj_ikey = rec_p->obj_ikey;
	quark_p->bit_off = rec_p->bit_off;
	quark_p->bit_len = rec_p->bit_len;
	for (i = 0; i < 4; i++)
		quark_p->dim[i] = rec_p->dim[i];
	for (i = 0; i < 4; i++)
		quark_p->coef[i] = rec_p->coef[i];
	quark_p->glob_ikey = rec_p->glob_ikey;
	quark_p->glob_adr = rec_p->glob_adr;
	for (i = 0; i < 4; i++)
		quark_p->ind[i] = 0;
	quark_p->ooid = 0;
	quark_p->phys_addr = 0;
	quark_p->next_no = rec_p->next_no;
	strcpy(quark_p->glob_name, pmod->glob_heap + rec_p->glob_off);
	strcpy(quark_p->path, pmod->path_heap + rec_p->path_off);
	return OK;
}
