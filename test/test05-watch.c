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

  Demonstration example of the Optikus fron-end client API.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <optikus/watch.h>
#include <optikus/log.h>


#define TEST_SERVER		"localhost"
#define TEST_TIMEOUT	2000	/* 2 seconds */

int     all_change_qty = 0;
int     data_count = 0;
int		print_all = 0;


int
dataHandler(long param, ooid_t ooid, const oval_t * pval)
{
	owquark_t info;
	char    str_buf[80];
	oval_t tmp_val;

	all_change_qty++;
	owatchGetInfoByOoid(ooid, &info);
	tmp_val.type = 's';
	strcpy(str_buf, "???");
	owatchConvertValue(pval, &tmp_val, str_buf, sizeof(str_buf));
	if (++data_count % 10000 == 0 && print_all) {
		olog("ooid=%ld name=\"%s\" type='%c' undef=%d len=%d value=\"%s\"",
			 ooid, info.desc, pval->type, pval->undef, pval->len, str_buf);
	}
	return 0;
}


int
aliveHandler(long param, const char *name, const char *state)
{
	if (print_all)
		printf("subject=[%s] state=[%s]\n", name, state);
	return 0;
}


void
test_end(int sig)
{
	if (print_all)
		fprintf(stderr, "sig=%d\n", sig);
	owatchEnableMonitoring(FALSE);
	owatchRemoveDataHandler(dataHandler);
	owatchDisconnect();
	owatchExit();
	exit(sig ? 1 : 0);
}


char *
monitored_data[] = {
	"param1", "param2", "param3",
	NULL
};

struct checked_data {
	char *	desc;
	oval_t	val;
}
checked_data[] = {
	{ "sample@strinc", { 'b', 0, 1,  0, {.v_char = 'x'} }},
	{ "sample@strinh", { 'h', 0, 2,  0, {.v_short = -5} }},
	{ "sample@strini", { 'i', 0, 4,  0, {.v_int = 65537} }},
	{ "sample@strinf", { 'f', 0, 4,  0, {.v_float = -5.02} }},
	{ "sample@strind", { 'd', 0, 8,  0, {.v_double = 833} }},
	{ "sample@strout", { 's', 1, 64, 0,
	{ .v_str = "c=x h=-5 i=65537 f=-5.02 d=833.00" } }},
	{ NULL, { 0, 0, 0, 0 } }
};


int
test()
{
	oval_t val;
	ooid_t ooid;
	owop_t  op;
	oret_t rc;
	owquark_t info;
	int     err_code;
	char    buf[80];
	int		i, j;
	char *	p;
	char *	desc;

	if (owatchInit("test05watch")) {
		olog("init failed");
		return 1;
	}
	op = owatchConnect(TEST_SERVER);
	if (op == OWOP_ERROR) {
		olog("failed starting connection to %s", TEST_SERVER);
		return 1;
	}
	rc = owatchWaitOp(op, TEST_TIMEOUT, &err_code);
	if (rc == ERROR) {
		olog("connection to %s failed", TEST_SERVER);
		return 1;
	}
	if (owatchAddDataHandler("", dataHandler, 0)) {
		olog("AddDataHandler failed");
		return 1;
	}
	if (owatchAddAliveHandler(aliveHandler, 0)) {
		olog("AddAliveHandler failed");
		return 1;
	}
	for (i = 0; monitored_data[i]; i++) {
		op = owatchAddMonitorByDesc(monitored_data[i], &ooid, FALSE);
		if (op == OWOP_ERROR) {
			olog("AddMonitorByDesc(%s) failed", monitored_data[i]);
			return 1;
		}
	}
	err_code = ERROR; /* assume the worst */
	rc = owatchWaitOp(OWOP_ALL, TEST_TIMEOUT, &err_code);
	if (rc == ERROR || err_code == ERROR) {
		olog("waiting for param1/param2/param2 failed");
		return 1;
	}
	if (owatchGetSubjectList(buf, sizeof(buf))) {
		olog("GetSubjectList failed");
		return 1;
	}
	if (strstr(buf, "sample") == NULL) {
		olog("sample subject not active");
		return 1;
	}
	if (print_all)
		printf("subjects: [%s]\n", buf);
	if (owatchEnableMonitoring(TRUE)) {
		olog("EnableMonitoring failed");
		return 1;
	}
	err_code = ERROR; /* assume the worst */
	rc = owatchWaitOp(OWOP_ALL, TEST_TIMEOUT, &err_code);
	if (rc == ERROR || err_code == ERROR) {
		olog("waiting for monitoring failed");
		return 1;
	}
	for (i = 0; checked_data[i].desc; i++) {
		/* Verify data description */
		desc = checked_data[i].desc;
		op = owatchGetInfoByDesc(desc, &info);
		if (op == OWOP_ERROR) {
			olog("cannot start getting info for %s", desc);
			return 1;
		}
		rc = owatchWaitOp(op, TEST_TIMEOUT, &err_code);
		if (rc != OK) {
			olog("cannot get info for %s failed", desc);
			return 1;
		}
		if (info.type != checked_data[i].val.type) {
			olog("expected type '%c' for data '%s', got '%c'",
				checked_data[i].val.type, desc, info.type);
			return 1;
		}
		if (info.len != checked_data[i].val.len) {
			olog("expected length %d for data '%s', got %d",
				checked_data[i].val.len, desc, info.len);
			return 1;
		}
		/* write 0 */
		memset((char *)&val, 0, sizeof(val));
		val.type = info.type;
		val.len = info.len;
		if (info.type == 's') {
			memset(buf, 0, sizeof(buf));
			val.v.v_str = buf;
		}
		val.undef = 0;
		op = owatchWriteByName(desc, &val);
		if (op == OWOP_ERROR) {
			olog("cannot start writing 0 to %s", desc);
			return 1;
		}
		if (owatchWaitOp(op, TEST_TIMEOUT, &err_code)) {
			olog("cannot write 0 to %s", desc);
			return 1;
		}
		/* check that data contains 0 */
		op = owatchReadByName(desc, &val, buf, sizeof(buf));
		if (op == OWOP_ERROR) {
			olog("cannot start reading 0 from %s", desc);
			return 1;
		}
		if (owatchWaitOp(op, TEST_TIMEOUT, &err_code)) {
			olog("cannot read 0 from %s", desc);
			return 1;
		}
		switch (info.type) {
		case 's':
			if (val.v.v_str != buf) {
				olog("expected string buffer %p for %s, got %p",
					buf, desc, val.v.v_str);
				return 1;
			}
			p = buf;
			break;
		default:
			p = (char *) &val.v;
			break;
		}
		for (j = 0; j < info.len; j++) {
			if (p[j] != 0) {
				olog("data %s not zeroed", desc);
				return 1;
			}
		}
		/* Write real value */
		if (!checked_data[i].val.undef) {
			op = owatchWriteByName(desc, &checked_data[i].val);
			if (op == OWOP_ERROR) {
				olog("cannot start writing data to %s", desc);
				return 1;
			}
			if (owatchWaitOp(op, TEST_TIMEOUT, &err_code)) {
				olog("cannot write data to %s", desc);
				return 1;
			}
		}
		/* Check that value was really written */
		if (checked_data[i].val.undef) {
			/* This is calculated value. Wait a little. */
			if (owatchWork(TEST_TIMEOUT)) {
				olog("Work failed");
				return 1;
			}
		}
		op = owatchReadByName(desc, &val, buf, sizeof(buf));
		if (op == OWOP_ERROR) {
			olog("cannot start reading data from %s", desc);
			return 1;
		}
		if (owatchWaitOp(op, TEST_TIMEOUT, &err_code)) {
			olog("cannot read data from %s", desc);
			return 1;
		}
		switch (info.type) {
		case 's':
			if (val.v.v_str != buf) {
				olog("expected string buffer %p for %s, got %p",
					buf, desc, val.v.v_str);
				return 1;
			}
			if (strcmp(buf, checked_data[i].val.v.v_str) != 0) {
				olog("unexpected value for string %s ...", desc);
				olog(" ... wanted \"%s\"", checked_data[i].val.v.v_str);
				olog(" ... got \"%s\"", buf);
				return 1;
			}
			break;
		default:
			if (memcmp((char *)&val.v, (char *)&checked_data[i].val.v,
						info.len) != 0) {
				olog("unexpected value for data %s", desc);
				return 1;
			}
			break;
		}
	}
	/* passed */
	return 0;
}

int
main(int argc, char *argv[])
{
	ologOpen("test05watch", NULL, 0);
	ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
	owatch_verb = argc > 1 ? atoi(argv[1]) : 0;
	if (0 == owatch_verb)
		owatch_verb = 1;
	if (print_all)
		printf("verb=%d\n", owatch_verb);
	signal(SIGINT, test_end);
	test_end(test());
	return 0;
}
