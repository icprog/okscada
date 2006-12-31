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

  User visible agent API.

*/

#include "subj-priv.h"
#include <optikus/conf-net.h>	/* for OHTONL */

#if defined(HAVE_CONFIG_H)
#include <config.h>				/* for VXWORKS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS)

#include <memLib.h>
#include <timers.h>
#include <moduleLib.h>

PART_ID osubj_part_id;

#else /* !VXWORKS */

#include <stdlib.h>		/* for malloc,free */
#include <string.h>		/* for strlen,... */
#include <sys/time.h>	/* for gettimeofday */
#include <pthread.h>	/* for sched_param,pthread_create,... */
#include <dlfcn.h>		/* for dlopen,dlsym */

void   *osubj_dl_handle;

#endif /* VXWORKS vs UNIX */


int     osubj_verb = 1;
int     osubj_spin;
bool_t  osubj_aborted;
bool_t  osubj_initialized;

OsubjSubject_t *osubj_subjects;
int osubj_subject_qty;


/*
 *  Thread control.
 */

int     osubj_task_period = 50;
int     osubj_task_mask;
int     osubj_task_qty;
int     osubj_task_prio = 95;
long    osubj_task_now;

#if OSUBJ_THREADING == 1

#if defined(VXWORKS)
int     osubj_task_id;
#else /* !VXWORKS */
pthread_t osubj_task_thread;
int     osubj_old_task_prio = -1;
#define VXWORKS2POSIX(p)  (((p) < 1) ? 1 : ((p) > 99) ? 99 : (100 - (p)))
#endif /* VXWORKS vs UNIX */

void   *
osubjTask(void *dummy_arg)
{
	long    now;

	osubj_task_now = osubjClock();
	while (osubj_task_mask) {
#if defined(VXWORKS)
		osTaskPrio(0, osubj_task_prio);
#else /* !VXWORKS */
		if (osubj_task_prio > 255)
			osubj_task_prio = 255;
		if (osubj_task_prio >= 0 && osubj_task_prio != osubj_old_task_prio) {
			struct sched_param sched_param = { 0 };
			osubj_old_task_prio = osubj_task_prio;
			sched_param.sched_priority = VXWORKS2POSIX(osubj_task_prio);
			pthread_setschedparam(osubj_task_thread, SCHED_FIFO, &sched_param);
		}
#endif /* VXWORKS vs UNIX */
		now = osubjClock();
		while (osubj_task_now - now <= 0)
			osubj_task_now += osubj_task_period;
		if (!osubj_initialized || osubj_aborted)
			osubjSleep(osubj_task_now - now);
		osubjWork(osubj_task_now - now, osubj_task_mask);
	}
	return NULL;
}
#endif /* OSUBJ_THREADING */


/*
 *  System time.
 */
long
osubjClock(void)
{
#if defined(VXWORKS)
	struct timespec tspec;
	clock_gettime(CLOCK_REALTIME, &tspec);
	return (tspec.tv_sec * 1000L + (tspec.tv_nsec + 500000L) / 1000000L);
#else /* !VXWORKS */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000L + (tv.tv_usec + 500L) / 1000L);
#endif /* VXWORKS vs UNIX */
}


long
osubjSleep(long msec)
{
	struct timespec tobe, real;

	if (msec <= 0)
		return 0;
	tobe.tv_sec = msec / 1000L;
	tobe.tv_nsec = (msec % 1000L) * 1000000L;
	if (nanosleep(&tobe, &real) == OK)
		return msec;
	else
		return (real.tv_sec * 1000L + real.tv_nsec / 1000000L);
}


/*
 *  Memory allocation.
 *
 *  debug ids: 0 = unused
 *             1 = object / segment info
 *             2 = queue
 *             3 = queue net bufs
 *             4 = reget bufs
 *             5 = reput bufs
 *             6 = udp bufs
 *             7 = monitos descriptors
 *             8 = data read bufs
 */
int     osubj_alloc_qty = 0;
int     osubj_alloc_id_qty[20] = { 0 };

void   *
osubjAlloc(int size, int nelem, int id)
{
	int     n;
	void   *p;

	if (osubj_aborted)
		return NULL;
	if (size < 0 || nelem < 0)
		osubjAbort("internal error");
	n = size * nelem;
	if (n <= 0)
		return NULL;

#if defined(VXWORKS)
	if (osubj_part_id)
		p = memPartAlloc(osubj_part_id, n);
	else
#endif /* VXWORKS */
		p = malloc(n);
	if (NULL != p) {
		osubjZero(p, n);
		osubj_alloc_qty++;
		osubj_alloc_id_qty[id]++;
	}
	return p;
}


void
osubjFree(void *p, int id)
{
	if (osubj_aborted)
		return;
	if (!p)
		return;
#if defined(VXWORKS)
	if (osubj_part_id)
		memPartFree(osubj_part_id, p);
	else
#endif /* VXWORKS */
		free(p);
	osubj_alloc_qty--;
	osubj_alloc_id_qty[id]--;
}


void
osubjAbort(const char *msg)
{
	osubj_aborted = 1;
	osubjLog(msg);
#if defined(VXWORKS)
	while(1) osMsSleep(1000);
#else
	exit(1);
#endif /* VXWORKS */
}


/*
 *  Initialization is called once per agent.
 */
oret_t
osubjInit(int serv_port, int task_mask, ulong_t al_addr, uint_t al_size,
			OsubjCallback_t pcallback, OsubjMsgHandler_t pmsghandler)
{
	osubjExit();

#if defined(VXWORKS)
	if (al_size && !al_addr)
		al_addr = (ulong_t) malloc(al_size);
	if (al_size && !al_addr) {
		osubjLog("invalid arguments");
		return ERROR;
	}
	if (!osubj_part_id) {
		osubj_part_id = memPartCreate((char *) al_addr, al_size);
		if (!osubj_part_id) {
			osubjLog("cannot create partition");
			return ERROR;
		}
	}
#else  /* !VXWORKS */
	osubj_dl_handle = dlopen(NULL, RTLD_NOW);
	if (NULL == osubj_dl_handle) {
		osubjLog("dlopen failure");
		return ERROR;
	}
#endif /* VXWORKS vs UNIX */

	if (osubjInitNetwork(serv_port) != OK)
		goto FAIL;
	if (osubjInitMessages(pmsghandler) != OK)
		goto FAIL;

#if OSUBJ_THREADING == 1
	if (task_mask) {
#if defined(VXWORKS)
		osubj_task_mask = task_mask;
		osubj_task_id = osTaskCreate("tOsubj", osubj_task_prio,
									 (proc_t) osubjTask, 0, 0, 0, 0, 0);
#else /* !VXWORKS */
		int     rc;
		struct sched_param sched_param = { 0 };
		osubj_task_mask = task_mask;
		rc = pthread_create(&osubj_task_thread, NULL, &osubjTask, NULL);
		if (rc != 0) {
			osubjLog("cannot start osubj thread");
			osubj_task_thread = 0;
			goto FAIL;
		}
		sched_param.sched_priority = VXWORKS2POSIX(osubj_task_prio);
		pthread_setschedparam(osubj_task_thread, SCHED_FIFO, &sched_param);
		osubj_old_task_prio = osubj_task_prio;
		pthread_detach(osubj_task_thread);
#endif /* VXWORKS vs UNIX */
	}
#endif /* OSUBJ_THREADING */

	osubjEverWrite(&osubj_ban_write, TRUE);
	osubj_initialized = TRUE;
	return OK;
  FAIL:
	osubjExit();
	return ERROR;
}


/*
 *  Shut down the agent.
 */
oret_t
osubjExit(void)
{
	int     i, j;
	OsubjSubject_t *psubj;
	OsubjModule_t *pmod;

	osubj_task_mask = 0;
#if OSUBJ_THREADING == 1
#if defined(VXWORKS)
	if (osubj_task_id) {
		osTaskDelete(osubj_task_id);
		osubj_task_id = 0;
	}
#else /* !VXWORKS */
	if (osubj_task_thread != 0) {
		pthread_cancel(osubj_task_thread);
		osubj_task_thread = 0;
	}
#endif /* VXWORKS vs UNIX */
#endif /* OSUBJ_THREADING */
	osubjClearMonitoring();
	osubjClearMessages();
	if (NULL != osubj_subjects) {
		for (i = 0; i < osubj_subject_qty; i++) {
			psubj = &osubj_subjects[i];
			if (psubj->modules) {
				for (j = 0; j < psubj->module_qty; j++) {
					pmod = &psubj->modules[j];
					osubjFree(pmod->segments, 1);
					osubjRecZero(pmod);
				}
				osubjFree(psubj->modules, 1);
			}
			osubjRecZero(psubj);
		}
		osubj_subjects = NULL;
		osubj_subject_qty = 0;
	}
	osubjExitNetwork();
	osubjRemoteLog(FALSE);
	osubj_ban_write = FALSE;
	osubjArrZero(osubj_ever_write);
	osubj_alloc_qty = 0;
	osubj_initialized = FALSE;
	return OK;
}


/*
 *  Called once or more times per agent, once per subject.
 *  NOTE: Linux only uses absolute addressing.
 *  FIXME: more parameters needed
 */
oret_t
osubjAddSubject(const char *subject_name, const char *symtable_name)
{
	OsubjSubject_t *new_subjects, *psubj;
	OsubjModule_t *pmod;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (NULL == subject_name || '\0' == *subject_name
			|| NULL == symtable_name || '\0' == *symtable_name)
		return ERROR;

	/* grow the array */
	new_subjects = osubjNew(OsubjSubject_t, osubj_subject_qty + 1, 1);
	if (NULL == new_subjects)
		osubjAbort("not enough memory for subjects");
	if (osubj_subjects != NULL && osubj_subject_qty > 0)
		osubjCopy(osubj_subjects, new_subjects,
				  osubj_subject_qty * sizeof(OsubjSubject_t));
	psubj = &new_subjects[osubj_subject_qty];
	osubjRecZero(psubj);
	strcpy(psubj->subject_name, subject_name);
	strcpy(psubj->symtable_name, symtable_name);

	/* setup new subject */
#if defined(VXWORKS)
#define OSUBJ_MOD_LIST_SIZE  80
	MODULE_ID mids[OSUBJ_MOD_LIST_SIZE];
	MODULE_INFO minfo;
	int i;

	psubj->module_qty = moduleIdListGet(mids, OSUBJ_MOD_LIST_SIZE);
	if (psubj->module_qty < 1)
		return ERROR;
	psubj->modules = osubjNew(OsubjModule_t, psubj->module_qty, 1);
	for (i = 0; i < psubj->module_qty; i++) {
		pmod = &psubj->modules[i];
		if (moduleInfoGet(mids[i], &minfo) != OK)
			return ERROR;
		strcpy(pmod->module_name, minfo.name);
		pmod->segment_qty = 3;
		pmod->segments = osubjNew(OsubjSegment_t, pmod->segment_qty, 1);
		pmod->segments[0].segment_name = 'T';
		pmod->segments[1].segment_name = 'D';
		pmod->segments[2].segment_name = 'B';
		pmod->segments[0].start_addr = (long) minfo.segInfo.textAddr;
		pmod->segments[1].start_addr = (long) minfo.segInfo.dataAddr;
		pmod->segments[2].start_addr = (long) minfo.segInfo.bssAddr;
	}
#else /* !VXWORKS */
	psubj->module_qty = 1;
	psubj->modules = osubjNew(OsubjModule_t, psubj->module_qty, 1);
	pmod = &psubj->modules[0];
	strcpy(pmod->module_name, symtable_name);
	pmod->segment_qty = 1;
	pmod->segments = osubjNew(OsubjSegment_t, 1, 1);
	pmod->segments[0].segment_name = 'A';
	pmod->segments[0].start_addr = 0;
#endif /* VXWORKS vs UNIX */

	/* swap pointers */
	osubjFree(osubj_subjects, 1);
	osubj_subjects = new_subjects;
	osubj_subject_qty++;
	if (osubjIsConnected())
		osubjSendSegments(FALSE);
	return OK;
}


oret_t
osubjSendSegments(bool_t initial)
{
	OsubjSubject_t *psubj;
	OsubjModule_t *pmod;
	OsubjSegment_t *pseg;
	oret_t rc;
	char    buf[2048];		/* FIXME: magic size */
	char   *s;
	int     i, j, k, q, p = 0;
	ulong_t tmp_addr;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	buf[p++] = (char) (initial ? 1 : 0);
	if (osubj_subject_qty > 127) {
		osubjLogAt(2, "cannot handle more than 127 subjects");
		return ERROR;
	}
	buf[p++] = (char) osubj_subject_qty;
	for (i = 0, psubj = osubj_subjects; i < osubj_subject_qty; i++, psubj++) {
		s = psubj->subject_name;
		q = strlen(s);
		if (p + q + 14 > sizeof(buf))
			goto SMALL;
		buf[p++] = q;
		if (q > 0)
			memcpy(buf + p, s, q);
		p += q;
		if (psubj->module_qty > 127) {
			osubjLogAt(2, "cannot handle more than 127 modules per subject");
			return ERROR;
		}
		buf[p++] = (char) psubj->module_qty;
		for (j = 0, pmod = psubj->modules; j < psubj->module_qty; j++, pmod++) {
			s = pmod->module_name;
			q = strlen(s);
			if (p + q + 10 > sizeof(buf))
				goto SMALL;
			buf[p++] = q;
			if (q > 0)
				memcpy(buf + p, s, q);
			p += q;
			if (pmod->segment_qty > 127) {
				osubjLogAt(2,
						"cannot handle more than 127 segments per module");
				return ERROR;
			}
			buf[p++] = (char) pmod->segment_qty;
			for (k = 0, pseg = pmod->segments; k < pmod->segment_qty;
				 k++, pseg++) {
				if (p + 6 > sizeof(buf))
					goto SMALL;
				buf[p++] = pseg->segment_name;
				tmp_addr = OHTONL(pseg->start_addr);
				memcpy(buf + p, &tmp_addr, 4);
				p += 4;
			}
		}
	}
	buf[p++] = 0;
	rc = osubjSendPacket(OK, OLANG_DATA, OLANG_SEGMENTS, p, buf);
	return rc;
  SMALL:
	osubjLogAt(2, "segment buffer too small");
	return ERROR;
}


/*
 *  Return address of a symbol.
 */
long
osubjAddress(const char *subject_name, const char *symbol_name)
{
	int     i;
	long    a;

	if (!osubj_initialized || osubj_aborted)
		return 0;
	for (i = 0; i < osubj_subject_qty; i++)
		if (strcmp(subject_name, osubj_subjects[i].subject_name) == 0)
			break;
	if (i >= osubj_subject_qty)
		return 0;

#if defined(VXWORKS)
	a = (long) osGetSym(osubj_subjects[i].symtable_name, symbol_name,
						GETSYM_ANY);
#else /* !VXWORKS */
	a = (long) dlsym(osubj_dl_handle, symbol_name);
#endif /* VXWORKS vs UNIX */

	if (a == -1)
		a = 0;
	return a;
}


/*
 *  Main loop.
 */
oret_t
osubjWork(int timeout, int work_type)
{
	oret_t rc;
	long    end, now;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	if (work_type == 0)
		work_type = OSUBJ_WORK_ALL;
	if ((work_type & OSUBJ_WORK_ALL) == 0 || (work_type & ~OSUBJ_WORK_ALL) != 0)
		return ERROR;
	if (timeout == OSUBJ_NOWAIT) {
		rc = osubjInternalPoll(timeout, work_type);
		return OK;
	}
	if (timeout == OSUBJ_FOREVER) {
		rc = osubjInternalPoll(-1, work_type);
		return OK;
	}
	if (timeout < 0)
		return ERROR;
	now = osubjClock();
	end = now + timeout;
	while (end - now > 0) {
		osubjLogAt(14, "poll for %ldms", end - now);
		rc = osubjInternalPoll(end - now, work_type);
		now = osubjClock();
	}
	return OK;
}
