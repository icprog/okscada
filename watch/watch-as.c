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

  Data reading/writing with conversions and waiting.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "watch-priv.h"


/* FIXME: strings should be auto-allocated or this value must be public */
#define OWATCH_MAX_STRING_LEN	512


/*
 * .
 */
static oret_t
owatchReadAs (const char *desc, oval_t * val, char *data,
				int len, int timeout, int *err_ptr)
{
	owop_t  op;
	oval_t  temp_val;
	char    str[OWATCH_MAX_STRING_LEN];

	if (len > 0 && val->type == 's') {
		op = owatchReadByName(desc, &temp_val, data, len);
	} else {
		op = owatchReadByName(desc, &temp_val, str, sizeof(str));
	}
	if (op == OWOP_ERROR) {
		return ERROR;
	}
	if (owatchWaitOp(op, timeout, err_ptr) != OK) {
		owatchCancelOp(op);
		return ERROR;
	}
	return owatchConvertValue(&temp_val, val, data, len);
}

/*
 * .
 */
static oret_t
owatchWriteAs (const char *desc, oval_t * val_ptr, int timeout, int *err_ptr)
{
	owquark_t info;
	oret_t rc;
	owop_t  op;
	oval_t  real_val;
	char    str[OWATCH_MAX_STRING_LEN];
	int     len = sizeof(str);
	int     tmp_err;

	if (NULL == err_ptr)
		err_ptr = &tmp_err;
	*err_ptr = 0;

	op = owatchGetInfoByDesc(desc, &info);
	if (op == OWOP_ERROR) {
		/* FIXME: set *err_ptr */
		return ERROR;
	}

	rc = owatchWaitOp(op, timeout, err_ptr);
	if (rc != OK) {
		owatchDetachOp(op);
		return ERROR;
	}

	real_val.type = info.type;
	real_val.len = info.len;
	real_val.undef = 0;
	real_val.time = 0;
	real_val.undef = 0;

	if (owatchConvertValue(val_ptr, &real_val, str, len) != OK) {
		/* FIXME: set *err_ptr */
		return ERROR;
	}

	op = owatchWriteByName(desc, &real_val);
	if (op == OWOP_ERROR) {
		/* FIXME: set *err_ptr */
		return ERROR;
	}

	return owatchWaitOp(op, timeout, err_ptr);
}

/*
 * .
 */
long
owatchReadAsLong (const char *desc, int timeout, int *err_ptr)
{
	oval_t val;
	oret_t rc;
	int tmp_err;

	if (NULL == err_ptr)
		err_ptr = &tmp_err;
	*err_ptr = 0;
	val.type = 'l';
	rc = owatchReadAs(desc, &val, NULL, 0, timeout, err_ptr);
	return rc == OK ? val.v.v_long : 0;
}

/*
 * .
 */
float
owatchReadAsFloat (const char *desc, int timeout, int *err_ptr)
{
	oval_t val;
	oret_t rc;
	int tmp_err;

	if (NULL == err_ptr)
		err_ptr = &tmp_err;
	*err_ptr = 0;
	val.type = 'f';
	rc = owatchReadAs(desc, &val, NULL, 0, timeout, err_ptr);
	return rc == OK ? val.v.v_float : 0.0;
}

/*
 * .
 */
double
owatchReadAsDouble (const char *desc, int timeout, int *err_ptr)
{
	oval_t val;
	oret_t rc;
	int tmp_err;

	if (NULL == err_ptr)
		err_ptr = &tmp_err;
	*err_ptr = 0;
	val.type = 'd';
	rc = owatchReadAs(desc, &val, NULL, 0, timeout, err_ptr);
	return rc == OK ? val.v.v_double : 0.0;
}

/*
 * .
 */
char   *
owatchReadAsString (const char *desc, int timeout, int *err_ptr)
{
	oval_t val;
	char str[OWATCH_MAX_STRING_LEN];
	int len = sizeof(str);
	char   *res = NULL;
	oret_t rc;
	int tmp_err;

	if (NULL == err_ptr)
		err_ptr = &tmp_err;
	*err_ptr = 0;
	val.type = 's';
	rc = owatchReadAs(desc, &val, str, len, timeout, err_ptr);

	res = (char *) malloc(val.len);
	if (NULL == res) {
		/* FIXME: set *err_ptr */
		return NULL;
	}

	if (val.len > OWATCH_MAX_STRING_LEN) {
		len = val.len;
		val.type = 's';
		owatchReadAs(desc, &val, res, len, timeout, err_ptr);
		/* FIXME: check error code */
	} else {
		strcpy(res, str);
	}

	return res;
}


/*
 * .
 */
oret_t
owatchWriteAsLong (const char *desc, long value, int timeout, int *err_ptr)
{
	oval_t val;

	val.v.v_long = value;
	val.type = 'l';
	return owatchWriteAs(desc, &val, timeout, err_ptr);
}

/*
 * .
 */
oret_t
owatchWriteAsFloat(const char *desc, float value, int timeout, int *err_ptr)
{
	oval_t val;

	val.v.v_float = value;
	val.type = 'f';
	return owatchWriteAs(desc, &val, timeout, err_ptr);
}


/*
 * .
 */
oret_t
owatchWriteAsDouble (const char *desc, double value, int timeout, int *err_ptr)
{
	oval_t val;

	val.v.v_double = value;
	val.type = 'd';
	return owatchWriteAs(desc, &val, timeout, err_ptr);
}


/*
 * .
 */
oret_t
owatchWriteAsString (const char *desc, const char *value, int timeout, int *err_ptr)
{
	oval_t val;

	val.v.v_str = (char *) value;
	val.len = strlen(value);
	val.type = 's';
	return owatchWriteAs(desc, &val, timeout, err_ptr);
}


/*				Connection					*/

oret_t
owatchSetupConnection (const char *hub_host, const char *client_name,
						int conn_timeout, int *err_ptr)
{
	owop_t op;
	int tmp_err;

	if (NULL == err_ptr)
		err_ptr = &tmp_err;
	*err_ptr = 0;

	if (owatchInit(client_name) == ERROR) {
		/* FIXME: set *err_ptr */
		return ERROR;
	}
	op = owatchConnect(hub_host);
	return owatchWaitOp(op, conn_timeout, err_ptr);
}


/*				Commit Points				*/

bool_t
owatchGetAutoCommit()
{
	return TRUE;
}


oret_t
owatchSetAutoCommit(bool_t enable_auto_commit)
{
	return enable_auto_commit ? OK : ERROR;
}


owop_t
owatchCommit()
{
	return OWOP_ERROR;
}


owop_t
owatchCommitAt(long vtu_time)
{
	return OWOP_ERROR;
}


long
owatchGetTime()
{
	return 0;
}
