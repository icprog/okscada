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
#include <math.h>

#include <optikus/watch.h>
#include <optikus/log.h>


#define TEST_SERVER		"localhost"
#define TEST_TIMEOUT	2000	/* 2 seconds */
#define TOLERABLE_RELATIVE_ERROR	10e-9

int     all_change_qty;
int     data_count;
int		print_all;

int		timeout = TEST_TIMEOUT;


void
test_end(int sig)
{
	if (print_all)
		fprintf(stderr, "sig=%d\n", sig);
	owatchExit();
	exit(sig ? 1 : 0);
}

struct checked_data {
	char *	desc;
	int		is_out;
	double  val;
	char *	str;
}
checked_data[] = {
	{ "sample@strinc", 0, 'x', NULL },
	{ "sample@strinh", 0, -5, NULL },
	{ "sample@strini", 0, 65537, NULL },
	{ "sample@strinf", 0, -5.02, NULL },
	{ "sample@strind", 0, 833, NULL },
	{ "sample@strout", 1, 0, "c=x h=-5 i=65537 f=-5.02 d=833.00" },
	{ NULL }
};

int
clear_data(const char *desc, int is_str)
{
	char buf[80] = {};
	oret_t rc;
	char *str;
	int err, err2;

	/* write 0 */
	err = 0;
	if (is_str)
		rc = owatchWriteAsString(desc, buf, timeout, &err);
	else
		rc = owatchWriteAsLong(desc, 0, timeout, &err);
	if (rc != OK) {
		olog("writing 0 to %s failed with error %d", desc, err);
		return 1;
	}
	/* check that data contains 0 */
	if (is_str) {
		err = 0;
		str = owatchReadAsString(desc, timeout, &err);
		if (str == NULL) {
			olog("reading string from %s failed with error %d", desc, err);
			return 1;
		}
		if (*str) {
			olog("string in %s is not zeroed", desc);
			return 1;
		}
	} else {
		err = err2 = 0;
		if (owatchReadAsLong(desc, timeout, &err) != 0
				|| owatchReadAsDouble(desc, timeout, &err) != 0.0) {
			olog("data in %s is not zeroed", desc);
			return 1;
		}
		if (err) {
			olog("reading long from %s failed with error %d", desc, err);
			return 1;
		}
		if (err) {
			olog("reading double from %s failed with error %d", desc, err);
			return 1;
		}
	}
	return 0;
}

int check_data(const char *desc, double val, const char *str)
{
	char *s;
	long l;
	float f;
	double d, e;
	int err = 0;
	if (str != NULL) {
		s = owatchReadAsString(desc, timeout, &err);
		if (err) {
			olog("reading string from %s failed with error %d", desc, err);
			return 1;
		}
		if (s == NULL) {
			olog("invalid returned string reading from %s", desc);
			return 1;
		}
		if (strcmp(s, str) != 0) {
			olog("unexpected string when reading from %s ...", desc);
			olog("  ... expected \"%s\"", str);
			olog("  ... got \"%s\"", s);
			return 1;
		}
	} else {
		l = owatchReadAsLong(desc, timeout, &err);
		if (err) {
			olog("reading long from %s failed with error %d", desc, err);
			return 1;
		}
		if (l != (long) val) {
			olog("unexpected long data when reading from %s ...", desc);
			olog(" ... expected %ld", (long) val);
			olog(" ... got %ld", l);
			return 1;
		}
		f = owatchReadAsFloat(desc, timeout, &err);
		if (err) {
			olog("reading float from %s failed with error %d", desc, err);
			return 1;
		}
		if (f != (float) val) {
			olog("unexpected float data when reading from %s ...", desc);
			olog(" ... expected %f", (float) val);
			olog(" ... got %f, delta %g", f, (float)val - f);
			return 1;
		}
		d = owatchReadAsDouble(desc, timeout, &err);
		if (err) {
			olog("reading double from %s failed with error %d", desc, err);
			return 1;
		}
		if (d != val) {
			/* Conversions back forth can involve errors! */
			e = fabs((d - val) / val);
			if (e > TOLERABLE_RELATIVE_ERROR) {
				olog("unexpected double data when reading from %s ...", desc);
				olog(" ... expected %f", val);
				olog(" ... got %f, relative error %g", d, e);
				return 1;
			}
		}
	}
	return 0;
}

int
test(void)
{
	int		i, err;
	char *	desc;

	if (owatchSetupConnection(TEST_SERVER, "test06watchAs", timeout, &err)) {
		olog("connection to hub failed with error %d", err);
		return 1;
	}
	/* clear outputs */
	for (i = 0; checked_data[i].desc; i++) {
		if (checked_data[i].is_out) {
			if (clear_data(checked_data[i].desc, checked_data[i].str != NULL)) {
				return 1;
			}
		}
	}
	/* write inputs */
	for (i = 0; checked_data[i].desc; i++) {
		if (checked_data[i].is_out)
			continue;
		/* Write real value */
		desc = checked_data[i].desc;
		err = 0;
		if (checked_data[i].str) {
			if (owatchWriteAsString(desc, checked_data[i].str, timeout, &err)) {
				olog("writing string to %s failed with error %d", desc, err);
				return 1;
			}
		} else {
			if (owatchWriteAsLong(desc, (long)checked_data[i].val,
									timeout, &err)) {
				olog("writing long to %s failed with error %d", desc, err);
				return 1;
			}
			if (owatchWriteAsDouble(desc, checked_data[i].val,
									timeout, &err)) {
				olog("writing double to %s failed with error %d", desc, err);
				return 1;
			}
		}
		/* Check that value was really written */
		if (check_data(checked_data[i].desc, checked_data[i].val,
						checked_data[i].str)) {
			return 1;
		}
	}
	/* wait for output */
	if (owatchWork(TEST_TIMEOUT)) {
		olog("wait failed");
		return 1;
	}
	/* check outputs */
	for (i = 0; checked_data[i].desc; i++) {
		if (!checked_data[i].is_out)
			continue;
		if (check_data(checked_data[i].desc, checked_data[i].val,
						checked_data[i].str)) {
			return 1;
		}
	}
	/* passed */
	return 0;
}

int
main(int argc, char *argv[])
{
	ologOpen("test06watchAs", NULL, 0);
	ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
	signal(SIGINT, test_end);
	test_end(test());
	return 0;
}
