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

  Demonstration of remote logging.

*/

#include <optikus/watch.h>
#include <optikus/subj.h>
#include <optikus/log.h>
#include <optikus/util.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>


#define SERVER			"localhost"
#define CONN_TIMEOUT	2000
#define DISK_TIMEOUT	200
#define DELAY			50

#define LOCAL_FILE		"test09-rlog.log"
#define REMOTE_FILE		"ohub.log"

#define SAMPLE_SUBJECT	"sample"
#define SAMPLE_SYMTABLE	"."
#define SAMPLE_MODULE	SAMPLE_SYMTABLE


int		print_all = 0;
int		use_sync  = 0;


/*
 * .
 */
void
do_sync()
{
	if (use_sync) {
		if (print_all)
			printf("syncing...\n");
		sync();
		if (print_all)
			printf("sync done\n");
	}
}


/*
 * .
 */
void
test_end(int sig)
{
	if (print_all)
		fprintf(stderr, "sig=%d\n", sig);
	owatchExit();
	osubjExit();
	exit(sig ? 1 : 0);
}


bool_t
last_in_nowait(const char *msg, const char *path)
{
	FILE   *f;
	char    line[1024], last[1024];
	bool_t  is_in;

	f = fopen(path, "r");
	if (NULL == f) {
		printf("cannot open file `%s'\n", path);
		test_end(-1);
	}

	*line = *last = '\0';
	while (fgets(line, sizeof(line), f) != NULL) {
		strcpy(last, line);
	}

	is_in = (strstr(last, msg) != NULL);
	fclose(f);
	return is_in;
}


bool_t
last_in(const char *msg, const char *path, int method)
{
	bool_t  is_in;
	long tstart = osMsClock();

	do {
		is_in = last_in_nowait(msg, path);
		if (is_in)
			break;
		switch(method) {
		case 1:
			owatchWork(DELAY);
			break;
		case 2:
		case 3:
			osubjSleep(DELAY);
			break;
		default:
			osMsSleep(DELAY);
			break;
		}
	} while(osMsClock() - tstart < DISK_TIMEOUT);

	return is_in;
}


int
check_log(int method, const char *msg, bool_t is_local, bool_t is_remote)
{
	const char *me;

	switch(method) {
	case 1:
		me = "owatch";
		olog(msg);
		break;
	case 2:
		me = "osubj";
		olog(msg);
		break;
	case 3:
		me = "osubj/subjLog";
		osubjLog(msg);
		break;
	default:
		test_end(-1);
		return -1;
	}

	do_sync();

	if (print_all)
		printf("checking local=%d remote=%d msg=\"%s\"\n",
				is_local, is_remote, msg);

	if (is_local != last_in(msg, LOCAL_FILE, method)) {
		printf("%s: the message `%s' unexpectedly %s in local log `%s'\n",
				me, msg, is_local ? "absent" : "appears", LOCAL_FILE);
		test_end(-1);
	}
	if (is_remote != last_in(msg, REMOTE_FILE, method)) {
		printf("%s: the message `%s' unexpectedly %s in remote log `%s'\n",
				me, msg, is_local ? "absent" : "appears", REMOTE_FILE);
		test_end(-1);
	}
	return 0;
}


int
test()
{
	owop_t  op;
	oret_t rc;
	int     err_code;
	long    tstart;

	ologOpen("test09rlog", LOCAL_FILE, 0);
	ologFlags(OLOG_DATE | OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
	do_sync();

	/* watch logging API */

	if (owatchInit("test09rlog")) {
		printf("watch init failed\n");
		return 1;
	}
	op = owatchConnect(SERVER);
	if (op == OWOP_ERROR) {
		printf("failed starting watch connection to %s\n", SERVER);
		return 1;
	}
	rc = owatchWaitOp(op, CONN_TIMEOUT, &err_code);
	if (rc == ERROR) {
		printf("watch connection to %s failed\n", SERVER);
		return 1;
	}

	check_log(1, "1st owatch string, local only", TRUE, FALSE);
	owatchRemoteLog(TRUE);
	check_log(1, "2nd owatch string, local+remote", TRUE, TRUE);
	owatchRemoteLog(FALSE);
	check_log(1, "3rd owatch string, just local", TRUE, FALSE);

	/* subject logging API */

	if (osubjInit(0, OSUBJ_WORK_ALL, 0, 0, NULL, NULL)) {
		printf("subj init failed\n");
		return 1;
	}
	if (osubjAddSubject(SAMPLE_SUBJECT, SAMPLE_SYMTABLE)) {
		printf("cannot register subject name\n");
		return 1;
	}
	tstart = osMsClock();
	while (!osubjIsConnected() && osMsClock() - tstart < CONN_TIMEOUT) {}
	if (!osubjIsConnected()) {
		printf("subject connection to %s failed\n", SERVER);
		return 1;
	}

	check_log(2, "1st osubj string, local only", TRUE, FALSE);
	osubjRemoteLog(TRUE);
	check_log(2, "2nd osubj string, local+remote", TRUE, TRUE);
	osubjRemoteLog(FALSE);
	check_log(2, "3rd osubj string, just local", TRUE, FALSE);

	check_log(3, "4th osubj string, local only", TRUE, FALSE);
	osubjRemoteLog(TRUE);
	check_log(3, "5th osubj string, local+remote", TRUE, TRUE);
	osubjRemoteLog(FALSE);
	check_log(3, "6th osubj string, just local", TRUE, FALSE);

	/* passed */
	return 0;
}


int
main(int argc, char *argv[])
{
	signal(SIGINT, test_end);
	test_end(test());
	return 0;
}
