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

  Demonstration of the Optikus messaging API.

*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include <optikus/watch.h>
#include <optikus/lang.h>
#include <optikus/log.h>


#define SERVER		"localhost"
#define LOCAL_ID	"local"
#define REMOTE_ID	"sample"
#define MSG2_NUM	100
#define BIGNUM		5000
#define TIMEOUT		3000

pid_t pid;
int print_all;
int echo_count;


void
test_end(int sig)
{
	signal(SIGCHLD, SIG_DFL);
	if (pid) {
		kill(pid, SIGKILL);
	}
	pid = 0;
	owatchExit();
	exit(sig ? 1 : 0);
}


void
timed_out(int sig)
{
	olog("timed out");
	test_end(-1);
}


oret_t
echo_handler(ooid_t od, const char *src, const char *dest,
			int klass, int type, void *data, int len, long param)
{
	if (type != OLANG_MSG_TYPE_ECHO) {
		olog("invalid echo message %d", type);
		test_end(-1);
		return ERROR;
	}
	echo_count++;
	return OK;
}


int
local()
{
	char *remote = REMOTE_ID;
	char src[80], dest[80];
	char send_string[80];
	int i, len, blen, err, count, expected;
	int klass, type;
	char buf[128];
	ooid_t id;
	owop_t op;
	oret_t rc;

	/* test #1: connect */

	if (owatchSetupConnection(SERVER, LOCAL_ID, TIMEOUT, &err) != OK) {
		olog("cannot connect to server");
		return -1;
	}
	if (owatchAddMsgHandler(OLANG_MSG_CLASS_ECHO, echo_handler, 0) != OK) {
		olog("cannot register message handler");
		return 1;
	}

	/* test #2: send a string and check echo */

	len = 1 + sprintf(send_string, "test<%s>", remote);
	op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_TEST, 1, send_string, len);
	rc = owatchWaitOp(op, 1, &err);
	if (rc == ERROR) {
		olog("cannot send string to remote");
		return -1;
	}
	blen = sizeof(buf);
	op = owatchReceiveMessage(&id, src, dest, &klass, &type, buf, &blen);
	rc = owatchWaitOp(op, 100, &err);
	if (rc == ERROR) {
		olog("cannot receive string echo from remote (%d)",
				err);
		return -1;
	}
	if (0 != strcmp(src, remote)
			|| klass != OLANG_MSG_CLASS_TEST || type != 21 || blen != len) {
		olog("incorrect header of presumed string echo");
		return -1;
	}
	if (0 != memcmp(buf, send_string, len)) {
		olog("strings don't match");
		return -1;
	}

	/* test #3: send a number of messages in a row and count them */

	for (i = 0; i < MSG2_NUM; i++) {
		/* ask to increase counter */
		if (print_all)
			fprintf(stderr, "#");
		op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_TEST, 2, send_string, 0);
		err = 0;
		rc = owatchWaitOp(op, 1, &err);
		if (err != 0) {
			olog("cannot send item to remote (%d)", err);
			return -1;
		}
	}
	if (print_all)
		fprintf(stderr, ".\n");

	/* request the total */
	op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_TEST, 3, send_string, 0);
	rc = owatchWaitOp(op, 1, &err);
	if (rc == ERROR) {
		olog("cannot send request to remote (%d)", err);
		return -1;
	}
	blen = sizeof(buf);
	op = owatchReceiveMessage(&id, src, dest, &klass, &type, buf, &blen);
	rc = owatchWaitOp(op, 100, &err);
	if (rc == ERROR) {
		olog("cannot receive count from remote (%d)", err);
		return -1;
	}
	if (0 != strcmp(src, remote) || klass != OLANG_MSG_CLASS_TEST || type != 23
			|| blen != sizeof(int)) {
		olog("incorrect header of presumed count message");
		return -1;
	}
	if (*(int *)buf != MSG2_NUM) {
		olog("count in received count message %d instead of %d",
			*(int *)buf, MSG2_NUM);
		return -1;
	}

	/* check we can send again */
	op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_THRASH, 5, buf, sizeof(buf));
	rc = owatchWaitOp(op, 1, &err);
	if (rc == ERROR) {
		olog("cannot send to remote after expunge (%d)", err);
		return -1;
	}

	/* test #4: send a number of acknowledgeble messages and count replies */

	signal(SIGALRM, timed_out);
	alarm(10);
	expected = 0;

	for (i = 0; i < MSG2_NUM; i++) {
		op = owatchSendMessage(0, remote,
				OLANG_MSG_CLASS_SEND, OLANG_MSG_TYPE_SEND, send_string, 0);
		err = 0;
		rc = owatchWaitOp(op, 1, &err);
		if (err == OWATCH_ERR_PENDING)
			rc = owatchWaitOp(op, 10, &err);
		if (err != 0) {
			olog("cannot send item to remote (%d)", err);
			return -1;
		}
		expected++;
	}

	for (i = 0; i < MSG2_NUM; i++) {
		op = owatchSendCtlMsg(remote, i, i, "%d%f%lf", i, (float)i, (double)i);
		err = 0;
		rc = owatchWaitOp(op, 1, &err);
		if (err == OWATCH_ERR_PENDING)
			rc = owatchWaitOp(op, 10, &err);
		if (err != 0) {
			olog("cannot send control message %d to remote (%d)", i, err);
			return -1;
		}
		expected++;
	}

	while (owatchGetAckCount(FALSE) < expected)
		owatchWork(100);
	count = owatchGetAckCount(FALSE);
	if (count != expected) {
		olog("incorrect acknowledge count %d, expected %d", count, expected);
		return -1;
	}
	ologIf(print_all, "all %d messages acknowledged", expected);
	if (echo_count < expected)
		owatchWork(100);
	if (echo_count != expected) {
		olog("incorrect echo count %d, expected %d", echo_count, expected);
		return -1;
	}
	ologIf(print_all, "all %d messages echoed", expected);

	/* all tests passed */
	return 0;
}


int
main(int argc, char *argv[])
{
	if (argc > 1)
		owatch_verb = atoi(argv[1]);
	if (0 == owatch_verb)
		owatch_verb = 1;
	if (argc > 2)
		print_all = atoi(argv[2]);
	ologOpen("test08subjmsg", NULL, 0);
	signal(SIGINT, test_end);
	test_end(local());
	return 0;
}

