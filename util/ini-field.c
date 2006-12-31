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

  Parser of field descriptions:
       _iniParseFieldDesc
       _iniDelFieldDesc
       _iniDefaultValue

*/

#include "ini-priv.h"
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxfree,oxvzero */
#include <string.h>


#define FPD_BOOL   333


/*
 * .
 */
int
_iniParseFieldDesc(ini_field_t * field, ini_source_t * source)
{
	char    type_buf[64], tmp_buf[64], *ss;
	int     error_code;
	int     i, max_lo_limit, max_hi_limit;
	int     no, max_enums = 20;
	enum_slot_desc_t *enums;

	oxvzero(field);

	if (NULL == source || NULL == source->p)
		goto END;

	/* get name */
	ss = _iniReadStr(TRUE, "\t =;/", 64, source, field->name);
	if (!ss)
		goto END;
	if (!*field->name) {
		_iniMessage(source, "field name not described");
		goto END;
	}
	if (!_iniExpectStr("=", source, FALSE))
		goto END;

	ss = _iniReadStr(TRUE, "\t ([{;/", 64, source, type_buf);
	if (NULL == ss)
		goto END;
	_iniLowerStr(type_buf);
	if (!strcmp(type_buf, "string"))
		field->type = FPD_STRING;
	else if (!strcmp(type_buf, "integer"))
		field->type = FPD_INTEGER;
	else if (!strcmp(type_buf, "enum"))
		field->type = FPD_ENUM;
	else if (!strcmp(type_buf, "bool")) {
		field->type = FPD_BOOL;
	} else {
		_iniMessage(source, "unexpected field type '%s'", type_buf);
		goto END;
	}
	if (field->type == FPD_STRING) {
		/*********** parse string description ***********/
		field->d.d_str.max_len = IGNORE;
		field->d.d_str.def = NULL;
		for (;;) {
			if (_iniExpectStr("(", source, TRUE)) {
				field->d.d_str.max_len =
					_iniReadInteger(4, 1, 256, FALSE, source, &error_code);
				if (error_code)
					goto END;
				if (!_iniExpectStr(")", source, FALSE))
					goto END;
			} else if (_iniExpectStr("!0", source, TRUE))
				field->not_null = TRUE;
			else if (_iniExpectStr("section", source, TRUE)) {
				if (!_iniExpectStr("=", source, FALSE))
					goto END;
				ss = _iniReadStr(TRUE, 0, 64, source, field->section);
				if (NULL == ss)
					goto END;
			} else if (_iniExpectStr("default", source, TRUE)) {
				if (!_iniExpectStr("=", source, FALSE))
					goto END;
				if (NULL == (ss = _iniReadStr(TRUE, 0, 64, source, tmp_buf)))
					goto END;
				oxfree(field->d.d_str.def);
				field->d.d_str.def = oxstrdup(tmp_buf);
			} else
				break;
		}
	} else if (field->type == FPD_INTEGER) {
		/*********** parse integer description ***********/
		max_lo_limit = max_hi_limit = -1;
		field->d.d_int.max_digits = IGNORE;
		field->d.d_int.lo_limit = IGNORE;
		field->d.d_int.hi_limit = IGNORE;
		field->d.d_int.def_val = 0;
		field->d.d_int.out_base = 10;
		field->d.d_int.unsign = FALSE;
		for (;;) {
			if (_iniExpectStr("(", source, TRUE)) {
				field->d.d_int.max_digits =
					_iniReadInteger(3, 1, 10, FALSE, source, &error_code);
				if (error_code)
					goto END;
				if (!_iniExpectStr(")", source, TRUE))
					goto END;
			} else if (_iniExpectStr("[", source, TRUE)) {
				field->d.d_int.lo_limit =
					_iniReadInteger(10, IGNORE, IGNORE, FALSE, source,
									&error_code);
				if (error_code)
					goto END;
				if (!_iniExpectStr(":", source, TRUE))
					if (!_iniExpectStr("..", source, FALSE))
						goto END;
				field->d.d_int.hi_limit =
					_iniReadInteger(10, IGNORE, IGNORE, FALSE, source,
									&error_code);
				if (error_code)
					goto END;
				if (!_iniExpectStr("]", source, FALSE))
					goto END;
			} else if (_iniExpectStr("!0", source, TRUE))
				field->not_null = TRUE;
			else if (_iniExpectStr("signed", source, TRUE))
				field->d.d_int.unsign = FALSE;
			else if (_iniExpectStr("unsigned", source, TRUE))
				field->d.d_int.unsign = TRUE;
			else if (_iniExpectStr("hex", source, TRUE))
				field->d.d_int.out_base = 16;
			else if (_iniExpectStr("decimal", source, TRUE))
				field->d.d_int.out_base = 10;
			else if (_iniExpectStr("binary", source, TRUE))
				field->d.d_int.out_base = 2;
			else if (_iniExpectStr("octal", source, TRUE))
				field->d.d_int.out_base = 8;
			else if (_iniExpectStr("section", source, TRUE)) {
				if (!_iniExpectStr("=", source, FALSE))
					goto END;
				ss = _iniReadStr(TRUE, 0, 64, source, field->section);
				if (!ss)
					goto END;
			} else if (_iniExpectStr("default", source, TRUE)) {
				if (!_iniExpectStr("=", source, FALSE))
					goto END;
				field->d.d_int.def_val = _iniReadInteger(IGNORE, IGNORE, IGNORE,
														 field->d.d_int.unsign,
														 source, &error_code);
				if (error_code)
					goto END;
			} else
				break;
		}
		error_code = 0;
		if (field->d.d_int.max_digits != IGNORE &&
			(field->d.d_int.max_digits < 1 || field->d.d_int.max_digits > 10)) {
			_iniMessage(source, "wrong value max_digits =  %d",
						field->d.d_int.max_digits);
			error_code += 1;
		}
		if (field->d.d_int.lo_limit > field->d.d_int.hi_limit) {
			_iniMessage(source, "wrong integer limits [%d : %d] (%d > %d)",
						field->d.d_int.lo_limit, field->d.d_int.hi_limit,
						field->d.d_int.lo_limit, field->d.d_int.hi_limit);
			error_code += 1;
		}
		if (field->d.d_int.max_digits != IGNORE &&
			(field->d.d_int.lo_limit != IGNORE
			 || field->d.d_int.hi_limit != IGNORE)) {
			max_hi_limit = 1;
			for (i = 0; i < field->d.d_int.max_digits; i++)
				max_hi_limit *= 10;
			max_lo_limit = -max_hi_limit;
			if (field->d.d_int.lo_limit != IGNORE
				&& field->d.d_int.hi_limit != IGNORE
				&& (field->d.d_int.lo_limit < max_lo_limit
					|| field->d.d_int.hi_limit > max_hi_limit)) {
				_iniMessage(source,
							"wrong integer limits [%d..%d] (for %d digits)",
							field->d.d_int.lo_limit, field->d.d_int.hi_limit,
							field->d.d_int.max_digits);
				error_code += 1;
			} else if (field->d.d_int.lo_limit != IGNORE &&
					   field->d.d_int.lo_limit < max_lo_limit) {
				_iniMessage(source,
							"wrong integer limits [%d..] (for %d digits)",
							field->d.d_int.lo_limit, field->d.d_int.max_digits);
				error_code += 1;
			} else if (field->d.d_int.hi_limit != IGNORE &&
					   field->d.d_int.hi_limit > max_hi_limit) {
				_iniMessage(source,
							"wrong integer limits [..%d] (for %d digits)",
							field->d.d_int.hi_limit, field->d.d_int.max_digits);
				error_code += 1;
			}
		}
		if (error_code)
			goto END;
	} else if (field->type == FPD_ENUM || field->type == FPD_BOOL) {
		/* parse enumeration description */
		if (field->type == FPD_ENUM) {
			field->d.d_enum.enum_qty = 0;
			field->d.d_enum.shorting = FALSE;
			field->d.d_enum.ignore_case = TRUE;
			field->d.d_enum.def_val = -1;
			field->d.d_enum.out_base = 10;
			field->d.d_enum.max_len = 1;
			field->d.d_enum.enums = oxnew(enum_slot_desc_t, max_enums);
		} else {
			field->d.d_enum.enum_qty = 8;
			field->d.d_enum.shorting = field->d.d_enum.ignore_case = TRUE;
			field->d.d_enum.def_val = FALSE;
			field->d.d_enum.out_base = 10;
			field->d.d_enum.max_len = 5;
			field->d.d_enum.enums = oxnew(enum_slot_desc_t, 8);
			enums = field->d.d_enum.enums;
			for (i = 0; i <= 7; i++)
				enums[i].ignore_case = TRUE;
			for (i = 0; i <= 7; i++)
				enums[i].chars_to_cmp = 1;
			strcpy(enums[0].str, "true");
			strcpy(enums[1].str, "yes");
			strcpy(enums[2].str, "on");
			enums[2].chars_to_cmp = 2;
			strcpy(enums[3].str, "1");
			for (i = 0; i <= 3; i++)
				enums[i].value = TRUE;
			strcpy(enums[4].str, "false");
			strcpy(enums[5].str, "no");
			strcpy(enums[6].str, "off");
			enums[6].chars_to_cmp = 2;
			strcpy(enums[7].str, "0");
		}
		for (;;) {
			if (_iniExpectStr("short", source, TRUE))
				field->d.d_enum.shorting = TRUE;
			else if (_iniExpectStr("full", source, TRUE))
				field->d.d_enum.shorting = FALSE;
			else if (_iniExpectStr("case", source, TRUE))
				field->d.d_enum.ignore_case = TRUE;
			else if (_iniExpectStr("nocase", source, TRUE))
				field->d.d_enum.ignore_case = FALSE;
			else if (_iniExpectStr("!0", source, TRUE))
				field->not_null = TRUE;
			else if (_iniExpectStr("hex", source, TRUE))
				field->d.d_enum.out_base = 16;
			else if (_iniExpectStr("decimal", source, TRUE))
				field->d.d_enum.out_base = 10;
			else if (_iniExpectStr("binary", source, TRUE))
				field->d.d_enum.out_base = 2;
			else if (_iniExpectStr("octal", source, TRUE))
				field->d.d_int.out_base = 8;
			else if (_iniExpectStr("default", source, TRUE)) {
				if (!_iniExpectStr("=", source, FALSE))
					goto END;
				field->d.d_enum.def_val =
					_iniReadInteger(IGNORE, IGNORE, IGNORE, FALSE, source,
									&error_code);
				if (error_code)
					goto END;
			} else if (_iniExpectStr("section", source, TRUE)) {
				if (!_iniExpectStr("=", source, FALSE))
					goto END;
				ss = _iniReadStr(TRUE, 0, 64, source, field->section);
				if (!ss)
					goto END;
			} else
				break;
		}
		if (field->type == FPD_ENUM) {
			if (!_iniExpectStr("{", source, FALSE))
				goto END;
			enums = field->d.d_enum.enums;
			no = 0;
			for (;;) {
				enums[no].chars_to_cmp = IGNORE;
				enums[no].ignore_case = FALSE;
				ss = _iniReadStr(TRUE, "\t =;/", 64, source, enums[no].str);
				if (!ss)
					goto END;
				if (!_iniExpectStr("=", source, TRUE))
					break;
				enums[no].value =
					_iniReadInteger(5, IGNORE, IGNORE, FALSE, source,
									&error_code);
				if (error_code)
					goto END;
				if (field->d.d_enum.enum_qty >= max_enums) {
					int old_max = max_enums;
					max_enums += max_enums / 2;
					field->d.d_enum.enums = oxrenew(enum_slot_desc_t,
													max_enums, old_max,
													field->d.d_enum.enums);
				}
				field->d.d_enum.enum_qty++;
				no++;
				if (!_iniExpectStr(",", source, TRUE))
					break;
			}
			if (!_iniExpectStr("}", source, FALSE))
				goto END;
			if (field->d.d_enum.enum_qty == 0) {
				_iniMessage(source, "need at least one enumeration slot");
				goto END;
			}
			/* max length */
			for (i = 0; i < field->d.d_enum.enum_qty; i++)
				if ((int) strlen(enums[i].str) > field->d.d_enum.max_len)
					field->d.d_enum.max_len = strlen(enums[i].str);
			/* ignore case ? */
			if (field->d.d_enum.ignore_case) {
				for (i = 0; i < field->d.d_enum.enum_qty; i++) {
					_iniLowerStr(enums[i].str);
					enums[i].ignore_case = 1;
				}
			}
			/* short names ? */
			if (field->d.d_enum.shorting) {
				for (i = 0; i < field->d.d_enum.enum_qty; i++) {
					int     j, k, n, char_qty = 1;

					for (j = 0; j < field->d.d_enum.enum_qty; j++) {
						if (j == i)
							continue;
						n = strlen(enums[j].str);
						k = strlen(enums[i].str);
						if (n > k)
							n = k;
						for (k = 0; k < n; k++)
							if (enums[i].str[k] != enums[j].str[k])
								break;
						if (k + 1 > char_qty)
							char_qty = k + 1;
					}
					if (char_qty > (int) strlen(enums[i].str)) {
						_iniMessage(source,
									"ambiguous enumeration slot name '%s'",
									enums[i].str);
						goto END;
					}
					enums[i].chars_to_cmp = char_qty;
				}
			}
		}
		field->type = FPD_ENUM;
	}
	if (_iniSkipUntilEol(source) != 0)
		goto END;
	return 0;
  END:
	_iniDelFieldDesc(field);
	return -1;
}


/*
 * .
 */
void
_iniDelFieldDesc(ini_field_t * field)
{
	switch (field->type) {
	case FPD_STRING:
		oxfree(field->d.d_str.def);
		field->d.d_str.def = NULL;
		break;
	case FPD_ENUM:
		oxfree(field->d.d_enum.enums);
		field->d.d_enum.enums = NULL;
		break;
	default:
		break;
	}
	oxvzero(field);
}


/*
 * .
 */
int
_iniDefaultValue(ini_field_t * field, inival_t * value)
{
	switch (field->type) {
	case FPD_STRING:
		value->v_str = oxstrdup(field->d.d_str.def ? : "");
		break;
	case FPD_INTEGER:
		value->v_int = field->d.d_int.def_val;
		break;
	case FPD_ENUM:
		value->v_enum = field->d.d_enum.def_val;
		break;
	case FPD_FLOAT:
	case FPD_NONE:
	default:
		_iniMessage(0, "impossible field type in _iniDefaultValue");
		return -1;
	}
	return field->not_null ? 2 : 0;
}
