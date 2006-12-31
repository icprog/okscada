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

  This module is to provide API compatibility with old fashioned PFORMS OOIDs.

  TO BE REFACTORED.

*/


#include "hub-priv.h"
#include <optikus/tree.h>
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxfree */
#include <string.h>				/* for memcpy,strlen,strcpy */
#include <stdio.h>				/* for fopen,fscanf,fprintf */


ooid_t  ohub_ooid_seq;
tree_t  ohub_by_path_hash;
tree_t  ohub_by_ooid_hash;

char   *ohub_path_heap_buf;
int     ohub_path_heap_size;
int     ohub_path_heap_off;


/* FIXME: remove this */
ooid_t
ohubMakeStaticOoid_old(OhubDomain_t * pdom, OhubQuark_t * quark_p)
{
	long    mul, key;
	ooid_t ooid;
	int     i, n;

	mul = 0;
	for (i = 1; i < 4; i++) {
		mul += quark_p->ind[i - 1];
		n = quark_p->dim[i];
		if (n > 0)
			mul *= n;
	}
	mul += quark_p->ind[i - 1];
	key = quark_p->ikey;
	if (mul >= 0 && mul < OHUB_MAX_MUL && key >= 0 && key < OHUB_MAX_KEY) {
		ooid = (ooid_t) (mul * 1 + key * OHUB_MAX_KEY);
		return ooid;
	}
	return 0;
}


/*
 * .
 */
oret_t
ohubAddOoidPath(const char *path, ooid_t ooid)
{
	char   *new_heap_buf;
	int     new_heap_size;
	int     len = strlen(path) + 1;
	char   *str;
	int     off, i_ooid;
	oret_t rc = OK;

	if (ohub_path_heap_off + len + 4 > ohub_path_heap_size) {
		new_heap_size = ohub_path_heap_size * 4 / 3 + len + 4;
		new_heap_buf = oxrenew(char, new_heap_size,
								ohub_path_heap_off, ohub_path_heap_buf);
		if (NULL == new_heap_buf)
			ologAbort("no space for quark path heap");
		ohub_path_heap_buf = new_heap_buf;
		ohub_path_heap_size = new_heap_size;
	}
	off = ohub_path_heap_off;
	ohub_path_heap_off += len + 4;
	str = ohub_path_heap_buf + off;
	memcpy(str, path, len);
	memcpy(str + len, &ooid, 4);
	i_ooid = (int) ooid;
	ohubLog(10, "ooid_add(\"%s\")=%d", str, i_ooid);
	if (treeAdd(ohub_by_path_hash, str, i_ooid) != OK) {
		ohubLog(2, "cannot add path hash path=\"%s\" ooid=%lu", str, ooid);
		rc = ERROR;
	}
	if (treeNumAdd(ohub_by_ooid_hash, i_ooid, off) != OK) {
		ohubLog(2, "cannot add ooid hash path=\"%s\" ooid=%lu", str, ooid);
		rc = ERROR;
	}
	return rc;
}


int
ohubLoadOoidHash(const char *file_name)
{
	char    path[OHUB_MAX_PATH + 4];
	ooid_t ooid;
	ulong_t ulv;
	FILE   *file;
	int     num, qty = 0;

	file = fopen(file_name, "r");
	if (!file) {
		ohubLog(4, "cannot read OOID cache \"%s\"", file_name);
		return 0;
	}
	for (;;) {
		num = fscanf(file, " %lu %s", &ulv, path);
		if (num != 2)
			break;
		ooid = (ooid_t) ulv;
		ohubAddOoidPath(path, ooid);
		if ((int) (ooid - ohub_ooid_seq) > 0)
			ohub_ooid_seq = ooid;
		qty++;
	}
	fclose(file);
	ohubLog(7, "done reading %d OOIDs from \"%s\"", qty, file_name);
	ohubLog(8, "path_hash_depth=%d ooid_hash_depth=%d",
			treeGetDepth(ohub_by_path_hash), treeGetDepth(ohub_by_ooid_hash));
	return qty;
}


int
ohubSaveOoidHash(const char *file_name)
{
	char   *path;
	ulong_t ooid;
	FILE   *file;
	int     len, off = 0, qty = 0;

	file = fopen(file_name, "w");
	if (!file) {
		ohubLog(4, "cannot write OOID cache \"%s\"", file_name);
		return 0;
	}
	while (off < ohub_path_heap_off) {
		path = ohub_path_heap_buf + off;
		len = strlen(path) + 1;
		memcpy(&ooid, path + len, 4);
		off += len + 4;
		fprintf(file, "%-4lu %s\n", ooid, path);
		qty++;
	}
	fclose(file);
	ohubLog(7, "done writing %d OOIDs to \"%s\"", qty, file_name);
	return qty;
}


oret_t
ohubInitOoidFactory(const char *file_name)
{
	ohub_ooid_seq = 0;
	ohub_by_path_hash = treeAlloc(0);
	ohub_by_ooid_hash = treeAlloc(NUMERIC_TREE);
	ohub_path_heap_size = 20;
	ohub_path_heap_buf = oxnew(char, 20);

	ohub_path_heap_off = 0;
	if (file_name && *file_name)
		ohubLoadOoidHash(file_name);
	return OK;
}


oret_t
ohubExitOoidFactory(const char *file_name)
{
	if (file_name && *file_name)
		ohubSaveOoidHash(file_name);
	treeFree(ohub_by_path_hash);
	ohub_by_path_hash = NULL;
	treeFree(ohub_by_ooid_hash);
	ohub_by_ooid_hash = NULL;
	oxfree(ohub_path_heap_buf);
	ohub_path_heap_buf = NULL;
	ohub_path_heap_off = ohub_path_heap_size = 0;
	return OK;
}


ooid_t
ohubFindOoidByName(const char *path, bool_t can_add)
{
	int     val;
	ooid_t ooid;
	bool_t  found;

	found = treeFind(ohub_by_path_hash, path, &val);
	if (found)
		ooid = (ooid_t) val;
	else {
		ooid = 0;
		if (can_add) {
			if (++ohub_ooid_seq == 0)
				++ohub_ooid_seq;
			ooid = ohub_ooid_seq;
			ohubAddOoidPath(path, ooid);
		}
	}
	return ooid;
}


const char *
ohubFindNameByOoid(ooid_t ooid, char *path_buf)
{
	int     key = (int) ooid;
	int     off;
	bool_t  found;

	found = treeNumFind(ohub_by_ooid_hash, key, &off);
	if (!found) {
		path_buf[0] = 0;
		return NULL;
	}
	strcpy(path_buf, ohub_path_heap_buf + off);
	return path_buf;
}


ooid_t
ohubMakeStaticOoid(OhubDomain_t * pdom, OhubQuark_t * quark_p)
{
	ooid_t ooid;

	ooid = ohubFindOoidByName(quark_p->path, TRUE);
	ohubLog(7, "got_ooid(\"%s\")=%lu", quark_p->path, ooid);
	return ooid;
}
