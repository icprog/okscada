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

  TO BE REMOVED.

*/

#include <optikus/conf-os.h>
#include <stdio.h>

int     ev1, tid1, tid2, tid3, cue1, tid4, wd1;

void
fun2()
{
	int     msg = 200;

	while (1) {
		osEventWait(ev1, -1);
		fprintf(stderr, "[ev1/2]");
		osTCuePut(cue1, msg, 0);
		msg++;
	}
}

void
fun3()
{
	int     msg = 300;

	while (1) {
		osEventWait(ev1, -1);
		fprintf(stderr, "[ev1/3]");
		osTCuePut(cue1, msg, 0);
		msg++;
	}
}

void
wdfun(int param)
{
	int     msg;

	fprintf(stderr, "[!W%d!]", param);
	msg = -param;
	osTCuePut(cue1, msg, 0);
}

void
fun4()
{
	int     msg;

	while (1) {
		msg = -1;
		osTCueGet(cue1, msg, -1);
		fprintf(stderr, "[M=%d]", msg);
		if (msg == 200 || msg == 301) {
			fprintf(stderr, "[wd(%d)...]", msg);
			osWdogStart(wd1, 200, (proc_t) wdfun, msg);
		}
	}
}

void
fun1()
{
	int     msg = 100;
	int     i;

	for (i = 0; i < 6; i++) {
		char    b[40];
		char   *s = osTaskName(0, b);

		if (s == 0)
			s = "FUN";
		if (i == 2) {
			fprintf(stderr, "suspend fun\n");
			osTaskSuspend(0);
		}
		fprintf(stderr, "<f1/%d>", msg);
		osTCuePut(cue1, msg, 0);
		msg++;
		fprintf(stderr, "[f1+]");
		osMsSleep(100);
		fprintf(stderr, "[f1-]");
		osEventPulse(ev1);
		fprintf(stderr, "%s/%d->%d\n", s, osTaskId(0), i);
	}
}

int
main()
{
	int     msg = 0;
	int     i;
	int     mutex, rc1, rc2, rc3, rc4;

	ev1 = osEventCreate("event1", TRUE);
	cue1 = osTCueCreate("cue1", int, 20, TRUE);

	wd1 = osWdogCreate("wdog1");
	fprintf(stderr, "[cre_tid1]");
	tid1 = osTaskCreate("task1", 100, (proc_t) fun1, 0, 0, 0, 0, 0);
	fprintf(stderr, "[cre_tid2]");
	tid2 = osTaskCreate("task2", 70, (proc_t) fun2, 0, 0, 0, 0, 0);
	fprintf(stderr, "[cre_tid3]");
	tid3 = osTaskCreate("task3", 60, (proc_t) fun3, 0, 0, 0, 0, 0);
	fprintf(stderr, "[cre_tid4]");
	tid4 = osTaskCreate("task4", 80, (proc_t) fun4, 0, 0, 0, 0, 0);
	fprintf(stderr,
			"\ncreated tid1=%d tid2=%d tid3=%d tid4=%d cue1=%d ev1=%d\n", tid1,
			tid2, tid3, tid4, cue1, ev1);
	fprintf(stderr, "test recursive mutex\n");
	mutex = osMutexCreate("name", TRUE, TRUE);
	rc1 = osMutexGet(mutex, -1);
	rc2 = osMutexGet(mutex, -1);
	rc3 = osMutexRelease(mutex);
	rc4 = osMutexRelease(mutex);
	fprintf(stderr, "recursive mutex: rc1=%d rc2=%d rc3=%d rc4=%d\n",
			rc1, rc2, rc3, rc4);
	for (i = 0; i < 6; i++) {
		char    b[40];
		char   *s = osTaskName(0, b);

		if (s == 0)
			s = "MAIN";
		fprintf(stderr, "%s/0->%d\n", s, i);
		if (i == 2) {
			fprintf(stderr, "increase priority\n");
			osTaskPrio(tid2, 50);
		}
		if (i == 4) {
			fprintf(stderr, "resume fun\n");
			osTaskResume(tid1);
		}
		fprintf(stderr, "[m+]");
		osMsSleep(100);
		fprintf(stderr, "[m-]");
		osTCuePut(cue1, msg, 0);
		msg++;
	}
	fprintf(stderr, "\n");
	return 0;
}
