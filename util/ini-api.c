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

  Configuration parser API:
       iniAskRow
       iniGetRow
       iniCount
       iniOpen
       iniClose

*/

#include "ini-priv.h"
#include <optikus/conf-mem.h>	/* for oxnew,oxfree,oxbcopy */
#include <string.h>


static ini_source_t * ini_cache;
static int  ini_cache_len;


/*
 * open the configuration file.
 */
inidb_t
iniOpen(const char *ini_file, const char *describing, int unused_msg_kind)
{
	inidb_t db;
	FILE   *file;
	int     no, len;
	char   *buf;

	if (!ini_file || !*ini_file)
		return 0;
	no = iniCache(ini_file, 0);
	if (no < 0) {
		file = fopen(ini_file, "r");
		if (!file)
			return 0;
		fseek(file, 0, SEEK_END);
		len = (int) ftell(file);
		if (len <= 0) {
			fclose(file);
			return 0;
		}
		fseek(file, 0, 0);
		buf = oxnew(char, len + 2);
		fread(buf, len, 1, file);
		fclose(file);
		buf[len] = 0;
		no = iniCache(ini_file, buf);
		oxfree(buf);
		if (no < 0)
			return 0;
	}
	db = oxnew(ini_database_t, 1);
	strcpy(db->source.name, ini_file);
	db->source.buffer = oxstrdup(ini_cache[no].buffer);
	if (describing)
		iniPassport(db, describing);
	return db;
}


/*
 * register string buffer as a virtual configuration file.
 */
int
iniCache(const char *ini_file, char *buffer)
{
	ini_source_t *cache;
	int     num = ini_cache_len;
	int     i = num;
	char   *b;

	if (ini_cache) {
		for (i = 0; i < num; i++) {
			if (!strcmp(ini_file, ini_cache[i].name))
				break;
		}
	}
	if (i < num) {
		if (buffer != 0)
			goto END;
		return i;
	}
	if (!buffer)
		return -1;
	cache = oxnew(ini_source_t, num + 1);
	if (num > 0 && ini_cache) {
		oxbcopy(ini_cache, cache, num * sizeof(ini_source_t));
		oxfree(ini_cache);
	}
	ini_cache = cache;
	ini_cache_len = num + 1;
	strcpy(ini_cache[num].name, ini_file);
  END:
	oxfree(ini_cache[i].buffer);
	b = oxnew(char, strlen(buffer) + 2);

	strcpy(b, buffer);
	strcat(b, "\n");
	ini_cache[i].buffer = b;
	return i;
}


/*
 * remove all virtual configuration files.
 */
oret_t
iniFlush(void)
{
	int     num = ini_cache_len;
	int     i;

	if (NULL != ini_cache) {
		for (i = 0; i < num; i++)
			oxfree(ini_cache[i].buffer);
	}
	oxfree(ini_cache);
	ini_cache = NULL;
	ini_cache_len = 0;
	return OK;
}


/*
 * register additional field descriptions for an open file.
 */
oret_t
iniPassport(inidb_t db, const char *describing)
{
	return iniAugment(db, describing, "PASSPORT");
}


/*
 * .
 */
oret_t
iniAugment(inidb_t db, const char *describing, const char *name)
{
	int     len, n, i;
	char   *s;
	ini_source_t *source;
	ini_chunk_t *chunks;

	if (NULL == db || NULL == describing || '\0' == *describing)
		return ERROR;
	source = &db->source;
	len = strlen(describing) + 1;
	s = oxnew(char, len + strlen(source->buffer) + 4);
	strcpy(s, describing);
	strcat(s, "\n");
	strcat(s, source->buffer);
	oxfree(source->buffer);
	source->buffer = s;
	n = source->chunks ? source->chunk_qty : 0;
	chunks = oxnew(ini_chunk_t, n + 2);
	if (n > 0)
		oxbcopy(source->chunks, &chunks[1], n * sizeof(ini_chunk_t));
	else
		strcpy(chunks[n = 1].name, source->name);
	sprintf(chunks[0].name, "%s-%p", name ? name : "STRING", describing);
	for (i = 1; i <= n; i++)
		chunks[i].offset += len;
	oxfree(source->chunks);
	source->chunks = chunks;
	source->chunk_qty = n + 1;
	return OK;
}


/*
 * close configuration file.
 */
void
iniClose(inidb_t db)
{
	if (NULL != db) {
		iniClean(db);
		_iniDelSource(&db->source);
		oxvzero(db);
		oxfree(db);
	}
}


/*
 * remove all cached data kept for configuration file.
 */
void
iniClean(inidb_t db)
{
	int     i;

	if (NULL != db) {
		if (NULL != db->fetches) {
			for (i = 0; i < db->fetch_qty; i++)
				_iniDelFetch(&db->fetches[i]);
		}
		oxfree(db->fetches);
		db->fetches = NULL;
		db->fetch_qty = 0;
	}
}
