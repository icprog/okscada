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

  Configuration parser utility functions:
     _iniSkipBlanks
     _iniSkipUntilEol
     _iniExpectStr
     _iniReadInteger
     _iniReadString
     _iniReadField

*/

#include "ini-priv.h"
#include <optikus/conf-mem.h>	/* for oxnew,oxfree */
#include <string.h>		/* for strcpy,strlen */
#include <stdlib.h>		/* for strtol,strtoul */


/*
 * .
 */
int
_iniSkipBlanks(ini_source_t * source)
{
	char   *s = SCP;

	/* if until_eol and comments until eol then s will point to eol */
	for (;;) {
		/* skip blanks */
		while (*s == ' ' || *s == '\t')
			s++;
		if (*s == '\r' || *s == '\n')
			break;
		if (*s == ';' || *s == '#') {
			/* skip comment until end of line */
			while (*s != '\n' && *s != '\r' && *s != 0)
				s++;
			break;
		}
		if (*s != '/' || s[1] != '*')
			break;
		/* skip simple comment */
		for (;;) {
			if (*s == 0) {
				SCP = s;
				return 0;
			}
			if (*s == '*' && s[1] == '/') {
				s += 2;
				break;
			}
			if (*s == '\r' || *s == '\n') {
				SCP = s;
				_iniMessage(source,
							"EOL mark encountered while in /*comment*/");
				return -1;
			}
			s++;
		}
	}
	SCP = s;
	return 0;
}


/*
 * .
 */
int
_iniSkipUntilEol(ini_source_t * source)
{
	int     ret_code = 0;
	char    c;

	for (;;) {
		if (_iniSkipBlanks(source))
			return ERROR;
		c = *SCP;
		if (c == 0)
			return ret_code;
		if (c == '\n' || c == '\r' || c == ';' || c == '#')
			break;
		if (c != '/' || SCP[1] != '*') {
			_iniMessage(source, "extra characters on line");
			ret_code = -1;
			break;
		}
		for (;;) {
			c = *SCP;
			if (c == 0)
				return ret_code;
			if (c == '*' && SCP[1] == '/') {
				SCP += 2;
				break;
			}
			if (c == '\r' || c == '\n') {
				_iniMessage(source, "EOL mark encountered within /*comment*/");
				return -1;
			}
			SCP++;
		}
	}
	while (*SCP != '\n' && *SCP != '\r' && *SCP != 0)
		SCP++;
	if (*SCP != 0)
		SCP++;
	return ret_code;
}


/*
 * .
 */
bool_t
_iniExpectStr(char *str_to_expect, ini_source_t * source, bool_t is_quiet)
{
	int     len = strlen(str_to_expect);

	if (_iniSkipBlanks(source))
		return FALSE;
	if (0 == strncasecmp(source->p, str_to_expect, len)) {
		source->p += len;
		return TRUE;
	}
	if (!is_quiet)
		_iniMessage(source, "expected '%s'", str_to_expect);
	return FALSE;
}


/*
 * .
 */
int
_iniReadInteger(int max_digits, int lo_limit, int hi_limit, bool_t unsign,
				ini_source_t * source, int *error_code)
{
	int     digits;
	long    value;
	bool_t  exceed_lo, exceed_hi, have_both_limits;
	bool_t  do_error;
	char   *p, *s;

	do_error = error_code != 0;
	if (error_code != 0)
		*error_code = 0;
	if (_iniSkipBlanks(source)) {
		(*error_code)++;
		return 0;
	}
	s = p = source->p;
	if (*p == '0' && p[1] == 'b') {
		p += 2;
		value = strtol(p, &s, 2);
	} else if (unsign)
		value = (long) strtoul(p, &s, 0);
	else
		value = strtol(p, &s, 0);
	source->p = s;
	if (s == p) {
		value = 0;
		max_digits = lo_limit = hi_limit = IGNORE;
		_iniMessage(source, "wrong integer format");
		if (error_code)
			(*error_code)++;
	}
	if (*p == '-' || *p == '+')
		p++;
	if (*p == '0' && (p[1] == 'x' || p[1] == 'b'))
		p += 2;
	else if (*p == '0')
		p += 1;
	digits = (int) (s - p);
	if (max_digits != IGNORE && digits > max_digits) {
		_iniMessage(source, "exceeded maximum number of digits %d", max_digits);
		if (do_error)
			(*error_code)++;
	}
	if (unsign) {
		exceed_lo = (lo_limit != IGNORE
					 && (ulong_t) value < (ulong_t) lo_limit);
		exceed_hi = (hi_limit != IGNORE
					 && (ulong_t) value > (ulong_t) hi_limit);
	} else {
		exceed_lo = (lo_limit != IGNORE && value < lo_limit);
		exceed_hi = (hi_limit != IGNORE && value > hi_limit);
	}
	have_both_limits = (lo_limit != IGNORE && hi_limit != IGNORE);
	if (have_both_limits && (exceed_lo || exceed_hi))
		_iniMessage(source, "value %d must be in range [%d..%d]", value,
					lo_limit, hi_limit);
	else if (exceed_lo)
		_iniMessage(source, "value %d must be >= %d", value, lo_limit);
	else if (exceed_hi)
		_iniMessage(source, "value %d must be <= %d", value, hi_limit);
	if (exceed_lo) {
		value = lo_limit;
		if (do_error)
			(*error_code)++;
	}
	if (exceed_hi) {
		value = hi_limit;
		if (do_error)
			(*error_code)++;
	}
	if (unsign)
		return (int) (ulong_t) value;
	return (int) value;
}


/*
 * .
 */
char   *
_iniReadStr(bool_t skip_before, const char *delim,
			int max_len, ini_source_t * source, char *buff)
{
	char   *p, *b, *s, quotechar = 0;
	int     k = 0, n, size;
	char    delimiters[64] = " \t\r\n";

	if (skip_before) {
		if (_iniSkipBlanks(source))
			return 0;
	}
	s = source->p;
	if (*s == '\'')
		quotechar = *s++;
	else if (*s == '"')
		quotechar = *s++;
	if (delim)
		strcat(delimiters, delim);
	size = (max_len == IGNORE || max_len <= 0) ? 512 : max_len;
	b = oxnew(char, size + 4);

	for (;;) {
		if (*s == 0 || *s == '\r' || *s == '\n')
			break;
		if (*s == quotechar) {
			s++;
			break;
		}
		if (!quotechar && strchr(delimiters, *s))
			break;
		else if (*s == '\\') {
			switch (s[1]) {
			case 't':
				b[k++] = '\t';
				s += 2;
				break;
			case 'b':
				b[k++] = '\b';
				s += 2;
				break;
			case 'n':
				b[k++] = '\n';
				s += 2;
				break;
			case 'r':
				b[k++] = '\r';
				s += 2;
				break;
			case '\'':
				b[k++] = '\'';
				s += 2;
				break;
			case '"':
				b[k++] = '"';
				s += 2;
				break;
			case '\\':
				b[k++] = '\\';
				s += 2;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				n = (int) strtoul(s + 1, &p, 8);
				b[k++] = n;
				s = p;
				break;
			case 'x':
				n = (int) strtoul(s + 1, &p, 16);
				b[k++] = n;
				s = p;
				break;
			default:
				b[k++] = *s++;
				b[k++] = *s++;
				break;
			}
		} else
			b[k++] = *s++;
		if (k > size)
			break;
	}
	b[k] = 0;
	source->p = s;
	if ((int) strlen(b) >= size) {
		_iniMessage(source, "string length exceeds %d chars", size);
		oxfree(b);
		return NULL;
	}
	if (buff) {
		strcpy(buff, b);
		oxfree(b);
		b = buff;
	}
	return b;
}


/*
 * .
 */
int
_iniReadField(ini_field_t * field, ini_source_t * source,
			  inival_t * value, const char *delim)
{
	int     error_code = 0;
	char    str[64];
	int     i;

	switch (field->type) {
	case FPD_STRING:
		value->v_str =
			_iniReadStr(FALSE, delim, field->d.d_str.max_len, source, NULL);
		if (value->v_str == NULL)
			_iniDefaultValue(field, value);
		break;
	case FPD_INTEGER:
		value->v_int =
			_iniReadInteger(field->d.d_int.max_digits, field->d.d_int.lo_limit,
							field->d.d_int.hi_limit, field->d.d_int.unsign,
							source, &error_code);
		if (error_code)
			_iniDefaultValue(field, value);
		break;
	case FPD_ENUM:
		_iniReadStr(FALSE, delim, 64, source, str);
		_iniDefaultValue(field, value);
		if (error_code)
			break;
		if (field->d.d_enum.ignore_case)
			_iniLowerStr(str);
		for (i = 0; i < field->d.d_enum.enum_qty; i++) {
			if (field->d.d_enum.enums[i].chars_to_cmp == IGNORE) {
				if (!strcmp(str, field->d.d_enum.enums[i].str)) {
					value->v_enum = field->d.d_enum.enums[i].value;
					break;
				}
			} else {
				if (!strncmp(str, field->d.d_enum.enums[i].str,
							 field->d.d_enum.enums[i].chars_to_cmp)) {
					value->v_enum = field->d.d_enum.enums[i].value;
					break;
				}
			}
		}
		if (i == field->d.d_enum.enum_qty) {
			_iniMessage(source, "value is not in enum");
			error_code = -1;
		}
		break;
	case FPD_FLOAT:
	case FPD_NONE:
	default:
		_iniMessage(0, "impossible field type in _iniRreadField");
		error_code = -1;
	}
	return error_code;
}
