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
#define REMOTE_ID	"remote"
#define LOCAL_ID	"local"
#define MSG2_NUM	100
#define BIGNUM		5000
#define TIMEOUT		3000


pid_t pid;
int msg2_count = 0;
char dst_reply[80];


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


oret_t
remote_klass(ooid_t od, const char *src, const char *dest,
			int klass, int type, void *data, int len, long param)
{
	owop_t op;
	int err;
	switch (type) {
	case 1:
		op = owatchSendMessage(0, src, OLANG_MSG_CLASS_TEST, 21, data, len);
		//olog("got string \"%s\"", (char *)dp);
		err = 0;
		if (owatchWaitOp(op, 0, &err) == ERROR) {
			if (OWATCH_ERR_PENDING != err) {
				olog("cannot send back string echo (op=%xh,err=%d)", op, err);
				exit(1);
			}
			olog("[warning] string echo still pending");
			owatchDetachOp(op);
		}
		break;
	case 2:
		msg2_count++;
		//fprintf(stderr, ".");
		break;
	case 3:
		op = owatchSendMessage(0, src, OLANG_MSG_CLASS_TEST, 23,
							&msg2_count, sizeof(msg2_count));
		//olog("sending count of %d", msg2_count);
		if (owatchWaitOp(op, 0, &err) == ERROR) {
			if (OWATCH_ERR_PENDING != err) {
				olog("cannot send back message count");
				exit(1);
			}
			olog("[warning] message count still pending");
			owatchDetachOp(op);
		}
		break;
	default:
		olog("got unknown type %d", type);
		exit(1);
	}
	strcpy(dst_reply, src);	/* will be used later */
	return OK;
}


void
remote_hup(int sig)
{
	owatchExit();
	puts("exit remote");
	exit(0);
}


void expunge_queue(int sig)
{
	owop_t op;
	ooid_t id;
	char src[80], dest[80];
	int klass, type;
	char buf[128];
	int len;
	int err;
	oret_t rc;

	do {
		len = sizeof(buf);
		op = owatchReceiveMessage(&id, src, dest, &klass, &type,
									buf, &len);
		rc = owatchWaitOp(op, 0, &err);
	} while (rc == OK);
	if (err != OWATCH_ERR_PENDING) {
		olog("unexpected status %d after queue expunge", err);
		exit(1);
	}
	op = owatchSendMessage(0, dst_reply, OLANG_MSG_CLASS_TEST, 26, NULL, 0);
	rc = owatchWaitOp(op, 0, &err);
	if (rc == ERROR) {
		if (OWATCH_ERR_PENDING != err) {
			olog("cannot acknowledge expunge to %s (%d)",
					dst_reply, err);
			exit(1);
		}
		olog("[warning] message count still pending");
		owatchDetachOp(op);
	}
}


int
remote()
{
	oret_t rc;
	int err;

	ologOpen("remote", NULL, 0);
	signal(SIGHUP, remote_hup);
	signal(SIGUSR1, expunge_queue);

	rc = owatchSetupConnection(SERVER, REMOTE_ID, TIMEOUT, &err);
	if (rc == ERROR) {
		olog("cannot initialize remote peer");
		return -1;
	}

	if (owatchAddMsgHandler(OLANG_MSG_CLASS_TEST, remote_klass, 0) == ERROR) {
		olog("cannot setup class handler");
		return -1;
	}
	if (owatchSetMsgQueueLimits(1000, 10000) == ERROR) {
		olog("cannot set queue limits");
		return -1;
	}

	while (1) {
		owatchWork(2000);
	}
	return 0;
}


void
child_problem(int sig)
{
	olog("problem with remote");
	test_end(sig);
}


int
local()
{
	char remote[80], src[80], dest[80], host[80];
	char send_string[80];
	int i, len, blen, err;
	int klass, type;
	char buf[128];
	ooid_t id;
	owop_t op;
	oret_t rc;

	ologOpen("local", NULL, 0);
	signal(SIGCHLD, child_problem);
	usleep(10000);

	/* test #1: connect */

	/* FIXME: We have to know the remote name in advance !
	 *        Server won't tell us ! */
	gethostname(host, sizeof(host));
	sprintf(remote, "%s/%u@%s", REMOTE_ID, pid, host);
	rc = owatchSetupConnection(SERVER, LOCAL_ID, TIMEOUT, &err);
	if (rc == ERROR) {
		olog("cannot connect to server");
		return -1;
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
		//fprintf(stderr, "#");
		op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_TEST, 2, send_string, 0);
		err = 0;
		rc = owatchWaitOp(op, 1, &err);
		if (err != 0) {
			olog("cannot send item to remote (%d)", err);
			return -1;
		}
	}

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
			|| blen != sizeof(msg2_count)) {
		olog("incorrect header of presumed count message");
		return -1;
	}
	if (*(int *)buf != MSG2_NUM) {
		olog("count in received count message %d instead of %d",
			*(int *)buf, MSG2_NUM);
		return -1;
	}

	/* test #4: check how watermarks work */

	for (i = 0; i < BIGNUM; i++) {
		/* overflow the receiving queue */
		op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_THRASH, 5, buf, sizeof(buf));
		owatchDetachOp(op);
		/* let hub work on message */
		owatchWork(i % 20 == 0 ? 1 : 0);
		/* FIXME: strange 'too late' diagnostics here */
	}

	/* check that queue is full */
	owatchWork(50);
	op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_THRASH, 5, buf, sizeof(buf));
	err = 0;
	rc = owatchWaitOp(op, 1, &err);
	if (rc != ERROR) {
		olog("still can unexpectedly send (op=%xh,rc=%d,err=%d)",
				op, rc, err);
		return -1;
	}
	if (err != OWATCH_ERR_RCVR_FULL) {
		olog("unexpected remote overflow (op=%xh,rc=%d,err=%d)",
				op, rc, err);
		return -1;
	}

	/* signal the remote to clean up the receiving queue */
	kill(pid, SIGUSR1);
	blen = sizeof(buf);
	op = owatchReceiveMessage(&id, src, dest, &klass, &type, buf, &blen);
	rc = owatchWaitOp(op, 100, &err);
	if (rc == ERROR) {
		olog("cannot verify the queue is cleaned (%d)", err);
		return -1;
	}
	if (0 != strcmp(src, remote)
			|| klass != OLANG_MSG_CLASS_TEST || type != 26 || blen != 0) {
		olog("incorrect header of presumed expunge message");
		return -1;
	}

	/* check we can send again */
	op = owatchSendMessage(0, remote, OLANG_MSG_CLASS_THRASH, 5, buf, sizeof(buf));
	rc = owatchWaitOp(op, 1, &err);
	if (rc == ERROR) {
		olog("cannot send to remote after expunge (%d)", err);
		return -1;
	}

	/* all tests passed */
	signal(SIGCHLD, SIG_DFL);
	kill(pid, SIGHUP);
	usleep(1);
	puts("exit local");
	return 0;
}


int
main(int argc, char *argv[])
{
	owatch_verb = 3;
	signal(SIGINT, test_end);
	if ((pid = fork()) == -1) {
		perror("fork");
		exit(1);
	}
	if (pid == 0) {
		exit(remote());
	}
	test_end(local());
	return 0;
}

