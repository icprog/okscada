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
#include <optikus/util.h>


#define SERVER		"localhost"
#define TIMEOUT		200
#define NUMVALS		1000
#define NUMMONS		100
#define DEADLINE	600
#define SLOW_LIMIT	30

int  timeout = TIMEOUT;
int  print_all;
int  numvals;
int  nummons;
int  deadline;

const char *arrays[] = {
	"c_char", "c_uchar", "c_short", "c_ushort", "c_int", "c_uint",
	"c_long", "c_ulong", "c_float", "c_double", "c_bool", "c_string"
};
#define NUMARRAYS	(sizeof(arrays) / sizeof(arrays[0]))

const char *mon_arrays[] = {
	"n_char", "n_uchar", "n_short", "n_ushort", "n_int", "n_uint",
	"n_long", "n_ulong", "n_float", "n_double", "n_bool", "n_string"
};
#define NUM_MON_ARS	(sizeof(arrays) / sizeof(arrays[0]))

int    array_ok[NUMARRAYS];
int    num_ok;
int    slow_problem;
double data_count;
long   start;
int    monitoring;

void stress_enable(int, int);


void
test_end(int sig)
{
	if (print_all) {
		printf("%s=%d\n", (sig > 0 ? "signal" : "result"), sig);
		printf("work time: %ld sec, slow errors: %d, data count: %g\n",
				start ? (osMsClock() - start) / 1000 : 0,
				slow_problem, data_count);
	}
	stress_enable(0, 0);
	osMsSleep(100);
	owatchExit();
	exit(sig ? 1 : 0);
}

int
data_handler(long param, ooid_t id, const oval_t *pval)
{
	data_count += 1;
	return 0;
}

int
read_val(const char *array, int index)
{
	char desc[32];
	int err = 0;
	sprintf(desc, "%s[%d]", array, index);
	int val = (int) owatchReadAsLong(desc, timeout, &err);
	if (err) {
		olog("reading long from %s failed with error %d", desc, err);
		test_end(-1);
	}
	return val;
}

void
check_val(const char *array, int index, int expected)
{
	int val;
	int i;
	char desc[32];
	
	sprintf(desc, "%s[%d]", array, index);
	i = 0;
	while(1) {
		val = read_val(array, index);
		if (val == expected)
			break;
		i++;
		if (!slow_problem) {
			olog("FIXME: known problem with slow writing");
			slow_problem++;
		}
		if (i < SLOW_LIMIT)
			continue;
		olog("unexpected value from %s : want %d got %d after %d attempts",
				desc, expected, val, i);
		test_end(-1);
	}
}

void
write_val(const char *array, int index, int new_val, int check)
{
	char desc[32];
	int err = 0;
	sprintf(desc, "%s[%d]", array, index);
	if (owatchWriteAsLong(desc, new_val, timeout, &err) != OK) {
		olog("writing %d to %s failed with error %d", new_val, desc, err);
		test_end(-1);
	}
	if (check && !monitoring)
		check_val(array, index, new_val);
	if (print_all >= 2)
		printf("wrote %s := %d\n", desc, new_val);
}

void
stress_enable(int enable, int check)
{
	const char *desc = "stress_test1";
	int  val, err, verb;

	if (check) {
		err = 0;
		if (owatchWriteAsLong(desc, enable, timeout, &err)) {
			olog("writing %d to %s failed with error %d", enable, desc, err);
			test_end(-1);
		}
		err = 0;
		val = (int) owatchReadAsLong(desc, timeout, &err);
		if (err) {
			olog("reading long from %s failed with error %d", desc, err);
			test_end(-1);
		}
		if (val != enable) {
			olog("strange value from %s : want %d got %d", desc, enable, val);
			test_end(-1);
		}
	} else {
		verb = owatch_verb;
		owatch_verb = 0;
		owatchWriteAsLong(desc, enable, timeout, &err);
		owatch_verb = verb;
	}
}

void
add_monitor(const char *array, int index)
{
	char desc[32];
	oret_t  rc;
	owop_t  op;
	ooid_t  id;
	int     err;

	sprintf(desc, "%s[%d]", array, index);
	op = owatchAddMonitorByDesc(desc, &id, FALSE);
	if (op == OWOP_ERROR) {
		olog("request of %s monitoring failed", desc);
		test_end(-1);
	}

	err = 0;
	rc = owatchWaitOp(op, timeout, &err);
	if (rc != OK) {
		owatchCancelOp(op);
		olog("request of %s monitoring failed, error=%d", desc, err);
		test_end(-1);
	}
}

int
loop_step()
{
	int  i, j, val, okval;

	if (osMsClock() - start > deadline * 1000) {
		olog("reached deadline time %d seconds", deadline);
		test_end(-1);
	}

	for (i = 0; i < NUMARRAYS; i++) {
		for (j = 0; j < numvals+1; j+=2) {
			val = read_val(arrays[i], j);
			if (print_all >= 3)
				printf("read %s[%d]: %d\n", arrays[i], j, val);
			if (val != 0)
				write_val(arrays[i], j+1, val+1, 1);
		}
	}

	for (i = 0; i < NUMARRAYS; i++) {
		val = read_val(arrays[i], numvals);
		switch(i) {
			case 0:
				okval = (char) numvals;
				break;
			case 1:
				okval = (uchar_t) numvals;
				break;
			default:
				okval = numvals;
				break;
		}
		if (val != 0) {
			if (array_ok[i])
				continue;
			array_ok[i] = 1;
			num_ok++;
			if (print_all)
				printf("array %s ok val[%d]=%d=%d (%d of %d left)\n",
						arrays[i], numvals, val, okval,
						NUMARRAYS - num_ok, NUMARRAYS);
		}
	}
	return (num_ok == NUMARRAYS);
}

int
test_variant(void)
{
	long test_start;
	int  i, j, err;

	/* connect */
	if (print_all)
		printf("connecting with monitoring=%d...\n",
				monitoring);
	err = 0;
	if (owatchSetupConnection(SERVER, "test10stress1", timeout, &err)) {
		olog("connection failed with error %d", err);
		test_end(-1);
	}
	if (nummons > 0) {
		if (owatchAddDataHandler("", data_handler, 0) != OK) {
			olog("cannot setup data handler");
			test_end(-1);
		}
	}

	/* clear arrays */
	if (print_all)
		printf("clear arrays of %d values\n", numvals);
	stress_enable(0, 1);
	for (i = 0; i < NUMARRAYS; i++) {
		for (j = 0; j < numvals+1; j++) {
			write_val(arrays[i], j, 0, 1);
			if (monitoring)
				add_monitor(arrays[i], j);
		}
	}

	/* set up monitoring */
	if (print_all)
		printf("set up %d monitored values\n", nummons);
	for (i = 0; i < NUM_MON_ARS; i++) {
		for (j = 0; j < nummons; j++) {
			add_monitor(mon_arrays[i], j);
		}
	}

	/* set up incremented chains */
	if (print_all)
		printf("set up chains\n");
	for (i = 0; i < NUMARRAYS; i++) {
		write_val(arrays[i], 1, 1, 1);
	}

	/* looping */
	test_start = osMsClock();
	if (print_all)
		printf("waiting for results\n");
	stress_enable(2, 1);
	while(!loop_step());

	/* passed */
	stress_enable(0, 1);
	owatchExit();
	printf("passed with monitoring=%d in %ld sec\n",
			monitoring, (osMsClock() - test_start) / 1000);
	return 0;
}

int
test(void)
{
	int ret;
	start = osMsClock();
	monitoring = 0;
	ret = test_variant();
	if (0 == ret) {
		monitoring = 1;
		ret = test_variant();
	}
	printf("done in %ld sec\n", (osMsClock() - start) / 1000);
	return ret;
}

int
main(int argc, char *argv[])
{
	if (argc > 1)
		numvals = atoi(argv[1]);
	if (numvals <= 0)
		numvals = NUMVALS;
	if (argc > 2)
		nummons = atoi(argv[2]);
	if (nummons <= 0 && 0 != strcmp(argv[2], "0"))
		nummons = NUMMONS;
	if (argc > 3)
		deadline = atoi(argv[3]);
	if (deadline <= 0)
		deadline = DEADLINE;
	if (argc > 4)
		print_all = atoi(argv[4]);
	if (print_all)
		printf("numvals=%d nummons=%d deadline=%dsec\n",
				numvals, nummons, deadline);
	ologOpen("test10stress1", NULL, 0);
	ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
	signal(SIGINT, test_end);
	test_end(test());
	return 0;
}
