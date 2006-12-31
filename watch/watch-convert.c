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

  Conversion between data formats.

*/

#include "watch-priv.h"
#include <optikus/conf-mem.h>	/* for oxvcopy,oxvzero */
#include <stdio.h>		/* for sprintf,sscanf */
#include <string.h>		/* for strlen,strcpy */


int
owatchGetValueLength(const oval_t * pval)
{
	switch (pval->type) {
	case 'b':	return 1;			/* char, signed char  */
	case 'B':	return 1;			/* unsigned char      */
	case 'h':	return 2;			/* short              */
	case 'H':	return 2;			/* unsigned short     */
	case 'i':	return 4;			/* int                */
	case 'I':	return 4;			/* unsigned int       */
	case 'l':	return 4;			/* long               */
	case 'L':	return 4;			/* unsigned long      */
	case 'q':	return 8;			/* long long          */
	case 'Q':	return 8;			/* unsigned long long */
	case 'f':	return 4;			/* float              */
	case 'd':	return 8;			/* double             */
	case 'D':	return 16;			/* long double        */
	case 'p':	return 4;			/* (*func)()          */
	case 'v':	return 0;			/* void               */
	case 's':	return pval->len;	/* string, [[un]signed] char* */
	case 'E':	return 4;			/* enumeration        */
	}
	return 0;
}


static oret_t
convertSignedChar(signed char v, char t, oval_t * p)
{
	switch (t) {
	case 'b':	p->v.v_char = v;		break;
	case 'B':	p->v.v_uchar = v;		break;
	case 'h':	p->v.v_short = v;		break;
	case 'H':	p->v.v_ushort = v;		break;
	case 'i':	p->v.v_int = v;			break;
	case 'I':	p->v.v_uint = v;		break;
	case 'l':	p->v.v_long = v;		break;
	case 'L':	p->v.v_ulong = v;		break;
	case 'f':	p->v.v_float = v;		break;
	case 'd':	p->v.v_double = v;		break;
	case 'E':	p->v.v_enum = v;		break;
	default:
		return ERROR;
	}
	return OK;
}

/* FIXME: overflows and data trims should be catched ! */

static oret_t
convertUnsignedChar(unsigned char v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) v;
		break;
	case 'B':
		p->v.v_uchar = v;
		break;
	case 'h':
		p->v.v_short = v;
		break;
	case 'H':
		p->v.v_ushort = v;
		break;
	case 'i':
		p->v.v_int = v;
		break;
	case 'I':
		p->v.v_uint = v;
		break;
	case 'l':
		p->v.v_long = v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertSignedShort(short v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) (ushort_t) v;
		break;
	case 'h':
		p->v.v_short = v;
		break;
	case 'H':
		p->v.v_ushort = v;
		break;
	case 'i':
		p->v.v_int = v;
		break;
	case 'I':
		p->v.v_uint = v;
		break;
	case 'l':
		p->v.v_long = v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertUnsignedShort(unsigned short v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) (uchar_t) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) v;
		break;
	case 'h':
		p->v.v_short = (short) v;
		break;
	case 'H':
		p->v.v_ushort = v;
		break;
	case 'i':
		p->v.v_int = v;
		break;
	case 'I':
		p->v.v_uint = v;
		break;
	case 'l':
		p->v.v_long = v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertSignedInt(int v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) (unsigned) v;
		break;
	case 'h':
		p->v.v_short = (short) v;
		break;
	case 'H':
		p->v.v_ushort = (ushort_t) (unsigned) v;
		break;
	case 'i':
		p->v.v_int = v;
		break;
	case 'I':
		p->v.v_uint = v;
		break;
	case 'l':
		p->v.v_long = v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertUnsignedInt(unsigned int v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (char) (uchar_t) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) v;
		break;
	case 'h':
		p->v.v_short = (short) (ushort_t) v;
		break;
	case 'H':
		p->v.v_ushort = (ushort_t) v;
		break;
	case 'i':
		p->v.v_int = v;
		break;
	case 'I':
		p->v.v_uint = v;
		break;
	case 'l':
		p->v.v_long = v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertSignedLong(long v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) (ulong_t) v;
		break;
	case 'h':
		p->v.v_short = (short) v;
		break;
	case 'H':
		p->v.v_ushort = (ushort_t) (ulong_t) v;
		break;
	case 'i':
		p->v.v_int = (int) v;
		break;
	case 'I':
		p->v.v_uint = (unsigned) (ulong_t) v;
		break;
	case 'l':
		p->v.v_long = v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertUnsignedLong(unsigned long v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) (uchar_t) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) v;
		break;
	case 'h':
		p->v.v_short = (short) (ushort_t) v;
		break;
	case 'H':
		p->v.v_ushort = (ushort_t) v;
		break;
	case 'i':
		p->v.v_int = (int) (unsigned) v;
		break;
	case 'I':
		p->v.v_uint = (unsigned) v;
		break;
	case 'l':
		p->v.v_long = (long) v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertFloat(float v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) v;
		break;
	case 'h':
		p->v.v_short = (short) v;
		break;
	case 'H':
		p->v.v_ushort = (ushort_t) v;
		break;
	case 'i':
		p->v.v_int = (int) v;
		break;
	case 'I':
		p->v.v_uint = (unsigned) v;
		break;
	case 'l':
		p->v.v_long = (long) v;
		break;
	case 'L':
		p->v.v_ulong = (ulong_t) v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = (double) v;
		break;
	case 'E':
		p->v.v_enum = (int) v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertDouble(double v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) v;
		break;
	case 'h':
		p->v.v_short = (short) v;
		break;
	case 'H':
		p->v.v_ushort = (ushort_t) v;
		break;
	case 'i':
		p->v.v_int = (int) v;
		break;
	case 'I':
		p->v.v_uint = (unsigned) v;
		break;
	case 'l':
		p->v.v_long = (long) v;
		break;
	case 'L':
		p->v.v_ulong = (ulong_t) v;
		break;
	case 'f':
		p->v.v_float = (float) v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = (int) v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertEnum(int v, char t, oval_t * p)
{
	switch (t) {
	case 'b':
		p->v.v_char = (signed char) v;
		break;
	case 'B':
		p->v.v_uchar = (uchar_t) (unsigned) v;
		break;
	case 'h':
		p->v.v_short = (short) v;
		break;
	case 'H':
		p->v.v_ushort = (ushort_t) (unsigned) v;
		break;
	case 'i':
		p->v.v_int = v;
		break;
	case 'I':
		p->v.v_uint = v;
		break;
	case 'l':
		p->v.v_long = v;
		break;
	case 'L':
		p->v.v_ulong = v;
		break;
	case 'f':
		p->v.v_float = v;
		break;
	case 'd':
		p->v.v_double = v;
		break;
	case 'E':
		p->v.v_enum = v;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static oret_t
convertFromString(const char *str, char type, oval_t * pval)
{
	int     n;
	int     i;
	unsigned int u;

	if (!str)
		return ERROR;
	switch (type) {
	case 'b':
		n = sscanf(str, "%d", &i);
		if (n <= 0 || i < -128 || i > 127)
			return ERROR;
		pval->v.v_char = (char) i;
		break;
	case 'B':
		n = sscanf(str, "%u", &u);
		if (n <= 0 || u > 255)
			return ERROR;
		pval->v.v_uchar = (unsigned char) u;
		break;
	case 'h':
		n = sscanf(str, "%hd", &pval->v.v_short);
		if (n <= 0)
			return ERROR;
		break;
	case 'H':
		n = sscanf(str, "%hu", &pval->v.v_ushort);
		if (n <= 0)
			return ERROR;
		break;
	case 'i':
		n = sscanf(str, "%d", &pval->v.v_int);
		if (n <= 0)
			return ERROR;
		break;
	case 'I':
		n = sscanf(str, "%u", &pval->v.v_uint);
		if (n <= 0)
			return ERROR;
		break;
	case 'l':
		n = sscanf(str, "%ld", &pval->v.v_long);
		if (n <= 0)
			return ERROR;
		break;
	case 'L':
		n = sscanf(str, "%lu", &pval->v.v_ulong);
		if (n <= 0)
			return ERROR;
		break;
	case 'f':
		n = sscanf(str, "%g", &pval->v.v_float);
		if (n <= 0)
			return ERROR;
		break;
	case 'd':
		n = sscanf(str, "%lg", &pval->v.v_double);
		if (n <= 0)
			return ERROR;
		break;
	case 'E':
		n = sscanf(str, "%d", &i);
		if (n <= 0)
			return ERROR;
		pval->v.v_enum = i;
		break;
	default:
		return ERROR;
	}
	return OK;
}

static int
convertToString(char type, const oval_t * pval, char *buf, int buf_len)
{
	char    tmp[80];
	int     tmp_len;

	if (!buf || buf_len < 1)
		return -1;
	switch (type) {
	case 'b':
		sprintf(tmp, "%d", (int) pval->v.v_char);
		break;
	case 'B':
		sprintf(tmp, "%u", (unsigned) pval->v.v_uchar);
		break;
	case 'h':
		sprintf(tmp, "%d", (int) pval->v.v_short);
		break;
	case 'H':
		sprintf(tmp, "%u", (unsigned) pval->v.v_ushort);
		break;
	case 'i':
		sprintf(tmp, "%d", pval->v.v_int);
		break;
	case 'I':
		sprintf(tmp, "%u", pval->v.v_uint);
		break;
	case 'l':
		sprintf(tmp, "%ld", pval->v.v_long);
		break;
	case 'L':
		sprintf(tmp, "%lu", pval->v.v_ulong);
		break;
	case 'f':
		sprintf(tmp, "%g", (double) pval->v.v_float);
		break;
	case 'd':
		sprintf(tmp, "%g", pval->v.v_double);
		break;
	case 'E':
		sprintf(tmp, "%d", (int) pval->v.v_enum);
		break;
	default:
		return -1;
	}
	tmp_len = strlen(tmp) + 1;
	if (tmp_len > buf_len)
		return -1;
	strcpy(buf, tmp);
	return tmp_len;
}

oret_t
owatchConvertValue(const oval_t * src_val, oval_t * dst_val,
				  char *data_buf, int data_buf_len)
{
	oret_t rc = ERROR;
	char    src_type, dst_type;
	int     len;

	if (src_val == NULL || dst_val == NULL)
		return ERROR;
	src_type = src_val->type;
	dst_type = dst_val->type;
	if (dst_type == 's') {
		if (src_type == 's') {
			if (src_val->v.v_str == NULL) {
				dst_val->v.v_str = NULL;
				dst_val->len = 0;
				dst_val->time = src_val->time;
				if (data_buf && data_buf_len)
					*data_buf = 0;
				return OK;
			}
			if (src_val->len == 0) {
				dst_val->v.v_str = data_buf;
				dst_val->len = 0;
				dst_val->time = src_val->time;
				if (data_buf && data_buf_len)
					*data_buf = 0;
				return OK;
			}
			if (src_val->len < 0)
				return ERROR;
			if (data_buf == NULL || data_buf_len < src_val->len)
				return ERROR;
			oxbcopy(src_val->v.v_str, data_buf, src_val->len);
			if (src_val->len < data_buf_len)
				data_buf[src_val->len] = 0;
			dst_val->v.v_str = data_buf;
			dst_val->len = src_val->len;
			dst_val->time = src_val->time;
			return OK;
		}
		len = convertToString(src_type, src_val, data_buf, data_buf_len);
		if (len < 0)
			return ERROR;
		dst_val->v.v_str = data_buf;
		dst_val->len = len;
		dst_val->time = src_val->time;
		return OK;
	}
	switch (src_type) {
	case 'b':
		rc = convertSignedChar(src_val->v.v_char, dst_type, dst_val);
		break;
	case 'B':
		rc = convertUnsignedChar(src_val->v.v_uchar, dst_type, dst_val);
		break;
	case 'h':
		rc = convertSignedShort(src_val->v.v_short, dst_type, dst_val);
		break;
	case 'H':
		rc = convertUnsignedShort(src_val->v.v_ushort, dst_type, dst_val);
		break;
	case 'i':
		rc = convertSignedInt(src_val->v.v_int, dst_type, dst_val);
		break;
	case 'I':
		rc = convertUnsignedInt(src_val->v.v_uint, dst_type, dst_val);
		break;
	case 'l':
		rc = convertSignedLong(src_val->v.v_long, dst_type, dst_val);
		break;
	case 'L':
		rc = convertUnsignedLong(src_val->v.v_ulong, dst_type, dst_val);
		break;
	case 'f':
		rc = convertFloat(src_val->v.v_float, dst_type, dst_val);
		break;
	case 'd':
		rc = convertDouble(src_val->v.v_double, dst_type, dst_val);
		break;
	case 'E':
		rc = convertEnum(src_val->v.v_enum, dst_type, dst_val);
		break;
	case 's':
		rc = convertFromString(src_val->v.v_str, dst_type, dst_val);
		break;
	default:
		return ERROR;
	}
	if (rc == ERROR)
		return ERROR;
	switch (dst_val->type) {
	case 'b':
		dst_val->len = 1;
		break;
	case 'B':
		dst_val->len = 1;
		break;
	case 'h':
		dst_val->len = 2;
		break;
	case 'H':
		dst_val->len = 2;
		break;
	case 'i':
		dst_val->len = 4;
		break;
	case 'I':
		dst_val->len = 4;
		break;
	case 'l':
		dst_val->len = 4;
		break;
	case 'L':
		dst_val->len = 4;
		break;
	case 'q':
		dst_val->len = 16;
		break;
	case 'Q':
		dst_val->len = 16;
		break;
	case 'f':
		dst_val->len = 4;
		break;
	case 'd':
		dst_val->len = 8;
		break;
	case 'D':
		dst_val->len = 16;
		break;
	case 'p':
		dst_val->len = 4;
		break;
	case 'E':
		dst_val->len = 4;
		break;
	case 'v':
		return ERROR;
	case 's':
		return ERROR;
	default:
		return ERROR;
	}
	dst_val->time = src_val->time;
	return OK;
}


oret_t
owatchCopyValue(const oval_t * src, oval_t * dst, char *buf, int buf_len)
{
	oret_t rc;

	if (NULL == dst)
		return ERROR;
	if (src->len < 0)
		return ERROR;
	if (src->type != 's') {
		if (src->len > sizeof(ovariant_t))
			return ERROR;
		oxvcopy(src, dst);
		return OK;
	}
	rc = OK;
	if (src->undef) {
		oxvzero(&dst->v);
	} else {
		if (src->len != 0) {
			if (NULL == buf || buf_len < src->len) {
				if (buf != NULL && buf_len > 0)
					memcpy(buf, src->v.v_str, buf_len);
				rc = ERROR;
			} else {
				memcpy(buf, src->v.v_str, src->len);
				if (src->len < buf_len)
					buf[src->len] = '\0';
				dst->v.v_str = buf;
			}
		} else {
			dst->v.v_str = buf;
			if (buf && buf_len > 0)
				*buf = '\0';
		}
	}
	dst->type = src->type;
	dst->undef = src->undef;
	dst->len = src->len;
	dst->time = src->time;
	return rc;
}
