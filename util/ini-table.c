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

  Parse configuration tables.

*/

#include "ini-priv.h"
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxfree,oxbcopy,oxvzero */
#include <string.h>
#include <stdarg.h>


#define MAX_TABLE_FIELDS 40


/*
 * .
 */
void
_iniCleanFetch(ini_fetch_t * fetch)
{
	int     i;

	if (NULL == fetch->row || fetch->col_qty <= 0)
		return;
	if (NULL != fetch->cols) {
		for (i = 0; i < fetch->col_qty; i++)
			if (fetch->cols[i].type == FPD_STRING)
				oxfree(fetch->row[i].v_str);
	}
	oxbzero(fetch->row, fetch->col_qty * sizeof(inival_t));
}


/*
 * .
 */
void
_iniDelFetch(ini_fetch_t * fetch)
{
	int     i;

	_iniCleanFetch(fetch);
	oxfree(fetch->row);
	if (NULL != fetch->cols) {
		for (i = 0; i < fetch->col_qty; i++)
			_iniDelFieldDesc(&fetch->cols[i]);
	}
	oxfree(fetch->cols);
	_iniDelSource(&fetch->source);
	oxvzero(fetch);
}


/*
 * .
 */
int
_iniSkipMultilineBlanks(ini_source_t * source)
{
	int     rc = OK;

	for (;;) {
		rc = _iniSkipBlanks(source);
		if (rc || *SCP == '\0' || *SCP == '[')
			break;;
		if (*SCP == '\r' || *SCP == '\n')
			SCP++;
		else
			break;;
	}
	return rc;
}


/*
 * .
 */
int
_iniGetRowInternal(ini_source_t * source, ini_fetch_t * fetch)
{
	bool_t  last_col;
	int     k, rc;

	if (fetch->col_qty == 0 || fetch->cols == 0)
		return FALSE;
	_iniCleanFetch(fetch);
	if (fetch->row_qty != IGNORE && fetch->cur_row == fetch->row_qty)
		return FALSE;
	if (_iniSkipMultilineBlanks(source))
		return ERROR;
	if (*SCP == '[' || *SCP == 0)
		return fetch->row_qty == IGNORE ? FALSE : ERROR;
	if (!_iniExpectStr("|", source, TRUE) && !_iniExpectStr("{", source, FALSE))
		return ERROR;
	for (k = 0; k < fetch->col_qty; k++) {
		last_col = (k == fetch->col_qty - 1);
		if (_iniExpectStr(last_col ? "}" : "|", source, TRUE)) {
			rc = _iniDefaultValue(&fetch->cols[k], &fetch->row[k]);
			if (rc < 0)
				return ERROR;
			if (rc == 0)
				continue;
			_iniMessage(source, "value cannot be null");
			return ERROR;
		}
		rc = _iniReadField(&fetch->cols[k], source,
						   &fetch->row[k], last_col ? "|}" : "|");
		if (rc)
			return ERROR;
		if (last_col) {
			if (!_iniExpectStr("|", source, TRUE)
				&& !_iniExpectStr("}", source, FALSE))
				return ERROR;
		} else if (!_iniExpectStr("|", source, FALSE))
			return ERROR;
	}
	if (_iniSkipUntilEol(source))
		return ERROR;
	++fetch->cur_row;
	return fetch->col_qty;
}


/*
 * simple interface for requesting values from a table.
 */
inireq_t
iniAskRow(inidb_t db, const char *table_name)
{
	ini_fetch_t fetch = {};
	ini_field_t *cols = NULL;
	int     i, k, offset, len, rc, qty = 0;
	char    section[128], *b;
	ini_source_t *source;

	if (NULL == db)
		return 0;
	source = &db->source;
	cols = oxnew(ini_field_t, MAX_TABLE_FIELDS);
	strcpy(section, table_name);
	strcat(section, ":describe");
	source->p = source->buffer;
	len = _iniLocateSection(source, section);
	if (len <= 0) {
		_iniMessage(source, "table description [%s] not found", section);
		goto END;
	}
	for (qty = 0; qty < MAX_TABLE_FIELDS; qty++) {
		rc = _iniSkipMultilineBlanks(source);
		if (rc || *SCP == 0 || *SCP == '[')
			break;
		rc = _iniParseFieldDesc(&cols[qty], source);
		if (rc)
			break;
	}
	if (qty >= MAX_TABLE_FIELDS) {
		_iniMessage(source, "too many table fields '%s'", table_name);
		goto END;
	}
	if (rc) {
		_iniMessage(source, "invalid table description '%s'", table_name);
		goto END;
	}
	if (qty == 0) {
		_iniMessage(source, "table description has no fields '%s'", table_name);
		goto END;
	}
	/* check for identical field names */
	for (i = 0; i < qty; i++) {
		for (k = 0; k < i; k++) {
			if (0 == strcmp(cols[i].name, cols[k].name)) {
				_iniMessage(source, "duplicate field name '%s' in table '%s'",
							cols[i].name, table_name);
				goto END;
			}
		}
	}
	strcpy(fetch.name, table_name);

	fetch.cols = oxnew(ini_field_t, qty);
	oxbcopy(cols, fetch.cols, qty * sizeof(ini_field_t));
	fetch.col_qty = qty;
	oxfree(cols);
	cols = NULL;

	fetch.row = oxnew(inival_t, fetch.col_qty);
	source->p = source->buffer;
	len = _iniLocateSection(source, table_name);
	if (len <= 0) {
		_iniMessage(source, "table [%s] not found", table_name);
		goto END;
	}
	fetch.source.first_line = source->first_line;
	b = source->buffer;
	offset = (int) (SCP - b);
	k = 0;
	strcpy(fetch.source.name, source->name);
	for (i = 0; i < source->chunk_qty; i++) {
		if (offset >= source->chunks[i].offset) {
			k = source->chunks[i].offset;
			strcpy(fetch.source.name, source->chunks[i].name);
		}
	}
	for (; b[k] && k < offset; k++) {
		if (b[k] == '\n')
			fetch.source.first_line++;
	}

	fetch.source.buffer = oxnew(char, len + 2);
	oxbcopy(source->buffer + offset, fetch.source.buffer, len);
	fetch.source.buffer[len] = 0;

	source = &fetch.source;
	source->p = source->buffer;
	fetch.row_qty = IGNORE;
	fetch.cur_row = 0;
	for (;;) {
		rc = _iniGetRowInternal(source, &fetch);
		if (rc < 0) {
			_iniMessage(source, "invalid table [%s]", table_name);
			goto END;
		}
		if (!rc)
			break;
	}
	fetch.row_qty = fetch.cur_row;
	fetch.cur_row = 0;
	source->p = source->buffer;

	db->fetches = oxrenew(ini_fetch_t, db->fetch_qty + 1,
							db->fetch_qty, db->fetches);
	oxvcopy(&fetch, &(db->fetches[db->fetch_qty++]));

	return (inireq_t) db->fetch_qty;
  END:
	if (NULL != cols) {
		for (i = 0; i < qty; i++)
			_iniDelFieldDesc(&cols[i]);
	}
	oxfree(cols);
	_iniDelFetch(&fetch);
	return 0;
}


/*
 * simple interface for fetching a table row.
 */
bool_t
iniGetRow(inidb_t db, inireq_t req, ...)
{
	va_list ap;
	inival_t *value;
	int     i, count;
	ini_fetch_t *fetch;

	if (!db || req <= 0 || req > db->fetch_qty)
		return FALSE;
	fetch = db->fetches + req - 1;
	if (fetch->col_qty == 0)
		return FALSE;
	if (fetch->cur_row < fetch->row_qty)
		count = _iniGetRowInternal(&fetch->source, fetch);
	else {
		int     col_qty = fetch->col_qty;
		int     row_qty = fetch->row_qty;

		_iniDelFetch(fetch);
		fetch->col_qty = col_qty;
		fetch->cur_row = fetch->row_qty = row_qty;
		count = 0;
	}
	va_start(ap, req);
	for (i = 0; i < fetch->col_qty; i++) {
		value = va_arg(ap, inival_t *);
		if (value == 0 || value == (inival_t *) - 1)
			break;
		if (count <= 0) {
			oxvzero(value);
			continue;
		}
		switch (fetch->cols[i].type) {
		case FPD_STRING:
			value->v_str = fetch->row[i].v_str;
			break;
		case FPD_INTEGER:
			value->v_int = fetch->row[i].v_int;
			break;
		case FPD_ENUM:
			value->v_enum = fetch->row[i].v_enum;
			break;
		default:
			oxvzero(value);
			break;
		}
	}
	va_end(ap);
	return (count > 0);
}


/*
 * counts number of rows in a table.
 */
int
iniCount(inidb_t d, inireq_t r)
{
	return d && r > 0 && r <= d->fetch_qty ? d->fetches[r - 1].row_qty : 0;
}
