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

  Routines that search locations in a configuration file.

*/

#include "ini-priv.h"
#include <optikus/util.h>		/* for strMask (FIXME: fnmatch ?) */
#include <optikus/conf-mem.h>	/* for oxvzero */
 
#include <string.h>


#define INI_VAR_DESC_SECTION	"CONFIG_VARIABLES:describe"


/*
 * .
 */
int
_iniLocateSection(ini_source_t * source, const char *section)
{
	char   *start;
	char    buffer[128];
	int     c, k, len;
	bool_t  first = TRUE;

	for (;;) {
		for (;;) {
			if ((c = *SCP++) == 0) {
				SCP--;
				return -1;
			}
			if (!(c == '\r' || c == '\n' || first))
				continue;
			first = FALSE;
			while (c == '\r' || c == '\n')
				c = *SCP++;
			while (c == ' ' || c == '\t')
				c = *SCP++;
			if (c == '[')
				break;
			else
				SCP--;
		}
		for (k = 0; k < 126; k++) {
			if ((c = *SCP++) == 0) {
				SCP--;
				_iniMessage(source, "wrong section name format");
				return -3;
			}
			if (strchr(ISNAME, c) != 0)
				buffer[k] = c;
			else if (c == ']')
				break;
			else {
				_iniMessage(source, "invalid section name");
				return -4;
			}
		}
		if (k > 120) {
			_iniMessage(source, "section name too long");
			return -5;
		}
		buffer[k] = 0;
		if (!section || strMask(section, buffer))
			break;
	}
	for (c = *SCP++; c != '\r' && c != '\n' && c; c = *SCP++);
	start = SCP;
	while (c != 0) {
		c = *SCP++;
		if (c == '\r' || c == '\n') {
			while (c == '\r' || c == '\n')
				c = *SCP++;
			while (c == ' ' || c == '\t')
				c = *SCP++;
			if (c == '[') {
				SCP--;
				break;
			}
		}
	}
	if (c == 0)
		SCP--;
	len = (int) (SCP - start);
	SCP = start;
	return len;
}


/*
 * .
 */
int
_iniLocateEntry(ini_source_t * source, const char *entry, bool_t with_name)
{
	char   *start;
	char    buffer[128];
	int     c, k, len;
	bool_t  first = TRUE;

	for (;;) {
		for (;;) {
			if ((c = *SCP++) == 0) {
				SCP--;
				return -1;
			}
			if (!(c == '\r' || c == '\n' || first))
				continue;
			first = FALSE;
			while (c == '\r' || c == '\n')
				c = *SCP++;
			while (c == ' ' || c == '\t')
				c = *SCP++;
			if (c == '[') {
				SCP--;
				return -1;
			}
			if (c == '#' || c == ';' || c == '{' || c == '|')
				continue;
			if (strchr(ISNAME, c)) {
				start = --SCP;
				break;
			}
			_iniMessage(source, "non-allowed character at beginning of line");
		}
		for (k = 0; k < 126; k++) {
			if ((c = *SCP++) == 0) {
				SCP--;
				_iniMessage(source, "wrong entry format");
				return -3;
			}
			if (strchr(ISNAME, c) == 0)
				break;
			buffer[k] = c;
		}
		if (k > 120) {
			_iniMessage(source, "entry name too long");
			return -5;
		}
		while (c == ' ' || c == '\t')
			c = *SCP++;
		if (c != '=') {
			_iniMessage(source, "invalid entry format");
			return -4;
		}
		c = *SCP++;
		buffer[k] = 0;
		if (!entry || strMask(entry, buffer))
			break;
	}
	while (c == ' ' || c == '\t')
		c = *SCP++;
	if (!with_name)
		start = SCP - 1;
	while (c != 0 && c != '\r' && c != '\n')
		c = *SCP++;
	if (c == 0)
		SCP--;
	len = (int) (SCP - start);
	SCP = start;
	return len;
}


/*
 * .
 */
int
_iniGetEntry(ini_source_t * source,
			 const char *section, const char *entry, bool_t with_name)
{
	int     len;

	source->p = source->buffer;
	for (;;) {
		if (_iniLocateSection(source, section) <= 0)
			return -1;
		len = _iniLocateEntry(source, entry, with_name);
		if (len >= 0)
			break;
	}
	return len;
}


/*
 * MOVE THIS TO ini-api.c.
 */
int
iniParam(inidb_t db, const char *entry, inival_t * value)
{
	int     ret_code = ERROR;
	ini_field_t desc = {0};
	ini_source_t *source;

	if (NULL == value)
		return ERROR;
	oxvzero(value);
	if (!db || !entry || !*entry)
		return ERROR;
	source = &db->source;
	ret_code = _iniGetEntry(source, INI_VAR_DESC_SECTION, entry, TRUE);
	if (ret_code <= 0) {
		_iniMessage(source, "parameter description \"%s\" not found", entry);
		return ERROR;
	}
	ret_code = _iniParseFieldDesc(&desc, source);
	if (ret_code != 0)
		return ERROR;
	if (!*desc.section) {
		_iniMessage(source,
					"section for parameter '%s' not specified",
					entry, desc.section);
		ret_code = ERROR;
		goto END;
	}
	ret_code = _iniGetEntry(source, desc.section, entry, FALSE);
	if (ret_code <= 0) {
		ret_code = _iniDefaultValue(&desc, value);
		if (ret_code == 2) {
			_iniMessage(source,
						"unspecified parameter '%s' in section [%s]",
						entry, desc.section);
			ret_code = ERROR;
			goto END;
		}
	} else {
		ret_code = _iniReadField(&desc, source, value, 0);
		if (ret_code == 0)
			ret_code = _iniSkipUntilEol(source);
	}
  END:
	_iniDelFieldDesc(&desc);
	return ret_code >= 0 ? OK : ERROR;
}
