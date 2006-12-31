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

  Solaris specific wrappers and stubs.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#define __EXTENSIONS__
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <ctype.h>

#include <optikus/util.h>
#include <optikus/conf-os.h>

#define __USE_GNU
#undef  __USE_UNIX98
#undef  __USE_XOPEN2K
#include <pthread.h>
#include <dlfcn.h>

void    _osTaskSchedPoint(void);

#define SCHEDPOINT   _osTaskSchedPoint()


int     os_stack_size = 16000;		/* Default task stack size */


/* ---------------- Dynamic Symbol Tables ------------------- */

/*
 * Find symbol in the symbol table.
 */

void   *_os_dl_handle = (void *) -1;

void
_osSymExit(void)
{
	if (_os_dl_handle != NULL && _os_dl_handle != (void *) -1) {
		dlclose(_os_dl_handle);
	}
	_os_dl_handle = (void *) -1;
}

void   *
osGetSym(const char *stab, const char *name, int what)
{
	if (_os_dl_handle == (void *) -1) {
		_os_dl_handle = dlopen(NULL, RTLD_NOW);
		if (NULL == _os_dl_handle)
			ologAbort("dlopen failure");
		atexit(_osSymExit);
	}
	if (NULL == _os_dl_handle)
		return NULL;
	return dlsym(_os_dl_handle, name);
}


/*
 * Modify the function code to jump to another function (STUB).
 */
oret_t
osSetJump(const char *stab, const char *from, const char *to,
		  unsigned **saveptr)
{
	ologAbort("osSetJump unimplemented");
	return ERROR;
}


/*
 * Restore original function code (STUB).
 */
oret_t
osUnsetJump(const char *stab, const char *from, unsigned *savebuf)
{
	ologAbort("osUnsetJump unimplemented");
	return ERROR;
}


/* --------------------- Task Control ------------------------ */

int     _os_quiet = 1;

#define OS_MAX_TASK    64

#define OS_TASK_SCHED  SCHED_FIFO

#define OSTS_STOP      0
#define OSTS_RUN       1
#define OSTS_SUSPEND   2

#define OS_TASK_MAGIC  0x1e00e8a

typedef struct
{
	int     magic;
	int     state;
	char    name[32];
	int     prio;
	proc_t  func;
	int     p1, p2, p3, p4, p5;
	bool_t  thread_ok;
	pthread_t thread;
	pthread_attr_t attr;
	struct sched_param sched_param;
	pthread_cond_t resume_cond;
	pthread_mutex_t mutex;
} OsTask_t;

typedef int (*OsTaskFunc_t) (int p1, int p2, int p3, int p4, int p5);

pthread_mutex_t _os_task_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t _os_task_key;
bool_t  _os_tasks_inited = FALSE;
bool_t  _os_broken_sched = FALSE;
bool_t  _os_trace = 0;
OsTask_t *_os_task[OS_MAX_TASK] = { 0 };

#define OS_MAIN_THREAD_PRIO  100

#define OS_INIT   if(!_os_tasks_inited)_osInitTasks()
#define OS_TRACE  if(_os_trace)printf

int
_osPrio_vxworks2posix(int vxp)
{
	int     pop;

	// FIXME: posix: 1..99, vxworks: 0..255, order differs
	pop = vxp;
	if (pop < 1)
		pop = 1;
	if (pop > 99)
		pop = 99;
	pop = 100 - pop;
	return pop;
}

int
_osTicks2Msec(int tiks)
{
	return tiks;
}

OsTask_t *
_osTaskAllocate(const char *name, int prio)
{
	OsTask_t *ptask;
	int     no;

	for (no = 0; no < OS_MAX_TASK; no++)
		if (_os_task[no] == 0)
			break;
	if (no >= OS_MAX_TASK)
		ologAbort("os: out of task descriptors");

	ptask = oxnew(OsTask_t, 1);
	ptask->magic = OS_TASK_MAGIC;

	strcpy(ptask->name, name);
	ptask->prio = prio;
	ptask->func = (proc_t) 0;
	ptask->p1 = ptask->p2 = ptask->p3 = ptask->p4 = ptask->p5 = 0;

	pthread_mutex_init(&ptask->mutex, NULL);
	pthread_cond_init(&ptask->resume_cond, NULL);
	pthread_attr_init(&ptask->attr);

	pthread_attr_setdetachstate(&ptask->attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setscope(&ptask->attr, PTHREAD_SCOPE_PROCESS);
	pthread_attr_setschedpolicy(&ptask->attr, OS_TASK_SCHED);
	ptask->sched_param.sched_priority = _osPrio_vxworks2posix(ptask->prio);
	pthread_attr_setschedparam(&ptask->attr, &ptask->sched_param);

	ptask->state = OSTS_STOP;
	ptask->thread_ok = FALSE;

	_os_task[no] = ptask;
	return ptask;
}

oret_t
_osTaskDeallocate(OsTask_t * ptask)
{
	int     no, rc;

	if (ptask->state == OSTS_RUN || ptask->state == OSTS_SUSPEND) {
		if (ptask->thread_ok)
			rc = pthread_cancel(ptask->thread);
	}
	for (no = 0; no < OS_MAX_TASK; no++) {
		if (_os_task[no] == ptask)
			_os_task[no] = (OsTask_t *) 0;
	}
	pthread_mutex_destroy(&ptask->mutex);
	pthread_cond_destroy(&ptask->resume_cond);
	pthread_attr_destroy(&ptask->attr);
	oxvzero(ptask);
	oxfree(ptask);
	return OK;
}

void
_osInitTasks(void)
{
	int     rc;
	OsTask_t *ptask;

	if (_os_tasks_inited)
		return;
	OS_TRACE("osi:lock\n");
	pthread_mutex_lock(&_os_task_mutex);
	if (!_os_tasks_inited) {
		_os_broken_sched = geteuid() != 0;
		if (_os_broken_sched && !_os_quiet)
			fprintf(stderr, "warning: incomplete priority scheduler\n");
		rc = pthread_key_create(&_os_task_key, NULL);
		ptask = _osTaskAllocate("[MAIN]", OS_MAIN_THREAD_PRIO);
		if (ptask == 0)
			ologAbort("os: cannot allocate main thread");
		ptask->thread = pthread_self();
		pthread_setspecific(_os_task_key, ptask);
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
		pthread_setschedparam(ptask->thread, OS_TASK_SCHED,
							  &ptask->sched_param);
		_os_tasks_inited = TRUE;
	}
	pthread_mutex_unlock(&_os_task_mutex);
	OS_TRACE("osi:unlock\n");
}

void   *
_osTaskWrapper(void *arg)
{
	OsTask_t *ptask = (OsTask_t *) arg;
	OsTaskFunc_t pfunc = (OsTaskFunc_t) ptask->func;

	OS_TRACE("otw(%p):lock\n", ptask);
	pthread_mutex_lock(&_os_task_mutex);
	pthread_setspecific(_os_task_key, ptask);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_mutex_unlock(&_os_task_mutex);
	OS_TRACE("otw(%p):unlock\n", ptask);
	(*pfunc) (ptask->p1, ptask->p2, ptask->p3, ptask->p4, ptask->p5);
	ptask->state = OSTS_STOP;
	pthread_exit(NULL);
	return NULL;
}

void
_osTaskSchedPoint()
{
	OsTask_t *ptask;

	OS_INIT;
	ptask = pthread_getspecific(_os_task_key);
	if (ptask == 0 || ptask->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	if (ptask->state == OSTS_SUSPEND) {
		OS_TRACE("sp(%p): p1\n", ptask);
		pthread_mutex_lock(&ptask->mutex);
		OS_TRACE("sp(%p): p2\n", ptask);
		while (ptask->state == OSTS_SUSPEND) {
			OS_TRACE("sp(%p): p3\n", ptask);
			pthread_cond_wait(&ptask->resume_cond, &ptask->mutex);
			OS_TRACE("sp(%p): p4\n", ptask);
		}
		OS_TRACE("sp(%p): p5\n", ptask);
		pthread_mutex_unlock(&ptask->mutex);
		OS_TRACE("sp(%p): p6\n", ptask);
	}
	pthread_testcancel();
#if 0
	if (_os_broken_sched)
		pthread_yield();
#endif
	return;
}

/*
 * Creates task.
 */
int
osTaskCreate(const char *name, int prio, proc_t func,
			 int p1, int p2, int p3, int p4, int p5)
{
	int     rc, tid = 0;
	OsTask_t *ptask = oxnew(OsTask_t, 1);

	OS_INIT;
	OS_TRACE("otc(%s/%p): lock\n", name, ptask);

	pthread_mutex_lock(&_os_task_mutex);
	ptask = _osTaskAllocate(name, prio);
	if (ptask == 0)
		rc = -1;
	else {
		ptask->func = func;
		ptask->p1 = p1;
		ptask->p2 = p2;
		ptask->p3 = p3;
		ptask->p4 = p4;
		ptask->p5 = p5;
		rc = pthread_create(&ptask->thread, NULL, &_osTaskWrapper, ptask);
	}
	if (rc == 0) {
		pthread_setschedparam(ptask->thread, OS_TASK_SCHED,
							  &ptask->sched_param);
		pthread_detach(ptask->thread);
		ptask->thread_ok = TRUE;
		ptask->state = OSTS_RUN;
		tid = (int) ptask;
	} else {
		_osTaskDeallocate(ptask);
		tid = 0;
	}
	pthread_mutex_unlock(&_os_task_mutex);

	OS_TRACE("otc(%s/%p):unlock\n", name, ptask);
	SCHEDPOINT;
	OS_TRACE("tc(%s/%p/%x): done\n", name, ptask, tid);
	return tid;
}

/*
 * Kills task.
 */
oret_t
osTaskDelete(int tid)
{
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	OS_INIT;
	if (tid < 0)
		return ERROR;
	pself = pthread_getspecific(_os_task_key);
	ptask = tid ? (OsTask_t *) tid : pself;
	if (ptask == 0 || ptask->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	if (ptask == pself)
		return ERROR;
	OS_TRACE("otd:lock\n");
	pthread_mutex_lock(&_os_task_mutex);
	_osTaskDeallocate(ptask);
	pthread_mutex_unlock(&_os_task_mutex);
	OS_TRACE("otd:unlock\n");
	return OK;
}


/*
 * Return numeric task identifier by its name.
 */
int
osTaskId(const char *name)
{
	OsTask_t *ptask, *pself;
	int     i, tid = 0;

	OS_INIT;
	pself = pthread_getspecific(_os_task_key);
	if (pself == 0 || pself->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	if (name == 0) {
		tid = (int) pself;
		return tid;
	}
	pthread_mutex_lock(&_os_task_mutex);
	for (i = 0; i < OS_MAX_TASK; i++) {
		ptask = _os_task[i];
		if (ptask == 0)
			continue;
		if (ptask->magic != OS_TASK_MAGIC)
			ologAbort("os: corrupt task");
		if (strcmp(ptask->name, name) == 0) {
			tid = (int) ptask;
			break;
		}
	}
	pthread_mutex_unlock(&_os_task_mutex);
	return tid;
}


/*
 * Return task name by its numeric identifier.
 */
char   *
osTaskName(int tid, char *name_buf)
{
	static char my_name_buf[40];
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	OS_INIT;
	if (tid < 0)
		return NULL;
	pself = pthread_getspecific(_os_task_key);
	ptask = tid ? (OsTask_t *) tid : pself;
	if (ptask == 0 || ptask->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	if (name_buf == 0)
		name_buf = my_name_buf;
	*name_buf = 0;
	if (ptask->state != OSTS_RUN && ptask->state != OSTS_SUSPEND)
		return NULL;
	strcpy(name_buf, ptask->name);
	return name_buf;
}


/*
 * .
 */
int
osTaskPrio(int tid, int new_prio)
{
	int     old_prio;
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	OS_INIT;
	if (tid < 0)
		return ERROR;
	pself = pthread_getspecific(_os_task_key);
	ptask = tid ? (OsTask_t *) tid : pself;
	if (ptask == 0 || ptask->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	old_prio = ptask->prio;
	if (new_prio > 255)
		new_prio = 255;
	if (new_prio >= 0 && new_prio != old_prio) {
		ptask->prio = new_prio;
		ptask->sched_param.sched_priority = _osPrio_vxworks2posix(new_prio);
		pthread_setschedparam(ptask->thread, OS_TASK_SCHED,
							  &ptask->sched_param);
	}
	SCHEDPOINT;
	return old_prio;
}


/*
 * Suspend task.
 */
oret_t
osTaskSuspend(int tid)
{
	oret_t rc = ERROR;
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	OS_INIT;
	if (tid < 0)
		return ERROR;
	pself = pthread_getspecific(_os_task_key);
	ptask = tid ? (OsTask_t *) tid : pself;
	if (ptask == 0 || ptask->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	OS_TRACE("suspend%p/%p: p1\n", pself, ptask);
	pthread_mutex_lock(&ptask->mutex);
	OS_TRACE("suspend%p/%p: p2\n", pself, ptask);
	if (ptask->state == OSTS_RUN) {
		ptask->state = OSTS_SUSPEND;
		rc = OK;
	}
	OS_TRACE("suspend%p/%p: p3\n", pself, ptask);
	pthread_mutex_unlock(&ptask->mutex);
	OS_TRACE("suspend%p/%p: p4\n", pself, ptask);
	SCHEDPOINT;
	OS_TRACE("suspend%p/%p: p5\n", pself, ptask);
	return rc;
}


/*
 * Resume task.
 */
oret_t
osTaskResume(int tid)
{
	oret_t rc = ERROR;
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	OS_INIT;
	if (tid < 0)
		return ERROR;
	pself = pthread_getspecific(_os_task_key);
	ptask = tid ? (OsTask_t *) tid : pself;
	if (ptask == 0 || ptask->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	if (ptask == pself)
		return ERROR;
	pthread_mutex_lock(&ptask->mutex);
	if (ptask->state == OSTS_SUSPEND) {
		ptask->state = OSTS_RUN;
		pthread_cond_signal(&ptask->resume_cond);
		rc = OK;
	}
	pthread_mutex_unlock(&ptask->mutex);
	SCHEDPOINT;
	return rc;
}


/*
 * Restart task.
 */
oret_t
osTaskRestart(int tid)
{
	SCHEDPOINT;
	ologAbort("osTaskRestart unimplemented");
	return ERROR;
}


/* ---------------- OS Scheduler Control (STUBS) ----------------------- */


/*
 * Enable or disable preemption.
 */
oret_t
osSchedLock(bool_t lock)
{
	return ERROR;
}


/*
 * Returns true if called from the ISR context.
 */
bool_t
osIntrContext(void)
{
	return FALSE;
}


/*
 * Disables interrupts.
 */
int
osIntrLock(bool_t just_emulate)
{
	return ERROR;
}


/*
 * Enables interrupts.
 */
oret_t
osIntrUnlock(int level)
{
	return ERROR;
}


/*
 * Reboots the computer.
 */
void
osSysReboot(int wait_ms)
{
	ologAbort("osSysReboot unimplemented");
}


/*
 * Aborts execution of the startup script.
 */
void
osShellStop(void)
{
	ologAbort("osShellStop unimplemented");
}


/* ------------------------ Critical Sections -------------------------- */

#define OS_MUTEX_MAGIC  0x1e01e8a

typedef struct
{
	int     magic;
	pthread_mutexattr_t attr;
	pthread_mutex_t mutex;
	OsTask_t *ptask;
	int     count;
} OsMutex_t;

int
osMutexCreate(const char *name, bool_t check_prio, bool_t check_inversion)
{
	OsMutex_t *pmutex = oxnew(OsMutex_t, 1);

	OS_INIT;
	pmutex->magic = OS_MUTEX_MAGIC;
	pthread_mutexattr_init(&pmutex->attr);
	/* Unfortunately recursive mutexes are not implemented in Solaris 6. */
	/* pthread_mutexattr_settype (&pmutex->attr, PTHREAD_MUTEX_RECURSIVE); */
	pmutex->ptask = 0;
	pmutex->count = 0;
	pthread_mutex_init(&pmutex->mutex, &pmutex->attr);
	return (int) pmutex;
}

oret_t
osMutexDelete(int mutex)
{
	OsMutex_t *pmutex = (OsMutex_t *) mutex;

	OS_INIT;
	if (mutex <= 0)
		return ERROR;
	if (pmutex->magic != OS_MUTEX_MAGIC)
		ologAbort("os: corrupt mutex");
	pthread_mutex_destroy(&pmutex->mutex);
	pthread_mutexattr_destroy(&pmutex->attr);
	oxvzero(pmutex);
	oxfree(pmutex);
	return OK;
}

oret_t
osMutexGet(int mutex, int wait_tiks)
{
	int     msec, rc;
	long    end_ms, cur_ms;
	OsMutex_t *pmutex = (OsMutex_t *) mutex;
	OsTask_t *pself;

	OS_INIT;
	if (mutex <= 0)
		return ERROR;
	if (pmutex->magic != OS_MUTEX_MAGIC)
		ologAbort("os: corrupt mutex");
	pself = pthread_getspecific(_os_task_key);
	if (pself == 0 || pself->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	if (pmutex->ptask == pself) {
		pmutex->count++;
		return OK;
	}
	if (wait_tiks < 0) {
		rc = pthread_mutex_lock(&pmutex->mutex);
		if (rc == OK) {
			pmutex->ptask = pself;
			pmutex->count = 1;
		}
		SCHEDPOINT;
		return rc ? ERROR : OK;
	}
	if (wait_tiks == 0) {
		rc = pthread_mutex_trylock(&pmutex->mutex);
		if (rc == OK) {
			pmutex->ptask = pself;
			pmutex->count = 1;
		}
		SCHEDPOINT;
		return rc ? ERROR : OK;
	}
	msec = _osTicks2Msec(wait_tiks);
	cur_ms = osMsClock();
	end_ms = cur_ms + msec;
	while (1) {
		rc = pthread_mutex_trylock(&pmutex->mutex);
		if (rc == OK) {
			pmutex->ptask = pself;
			pmutex->count = 1;
			break;
		}
		SCHEDPOINT;
		cur_ms = osMsClock();
		if (cur_ms - end_ms >= 0)
			return ERROR;
		osMsSleep(10);
	}
	return OK;
}

oret_t
osMutexRelease(int mutex)
{
	int     rc;
	OsMutex_t *pmutex = (OsMutex_t *) mutex;
	OsTask_t *pself;

	OS_INIT;
	if (mutex <= 0)
		return ERROR;
	if (pmutex->magic != OS_MUTEX_MAGIC)
		ologAbort("os: corrupt mutex");
	pself = pthread_getspecific(_os_task_key);
	if (pself == 0 || pself->magic != OS_TASK_MAGIC)
		ologAbort("os: corrupt task");
	if (pmutex->ptask != pself)
		return ERROR;
	if (--pmutex->count > 0)
		return OK;
	rc = pthread_mutex_unlock(&pmutex->mutex);
	SCHEDPOINT;
	return rc ? ERROR : OK;
}


/* -------------------------- Semaphores --------------------------- */

oret_t
_osCondWait(pthread_cond_t * pcond, pthread_mutex_t * pmutex, int wait_tiks)
{
	int     rc, msec, usec;
	struct timeval tv;
	struct timezone tz;
	struct timespec ts;

	OS_INIT;
	if (wait_tiks < 0) {
		rc = pthread_cond_wait(pcond, pmutex);
		return rc ? ERROR : OK;
	}
	msec = _osTicks2Msec(wait_tiks);
	gettimeofday(&tv, &tz);
	usec = msec * 1000;
	tv.tv_sec += usec / 1000000L;
	tv.tv_usec += usec % 1000000L;
	while (tv.tv_usec > 1000000L) {
		tv.tv_usec -= 1000000L;
		tv.tv_sec += 1;
	}
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	rc = pthread_cond_timedwait(pcond, pmutex, &ts);
	return rc ? ERROR : OK;
}

#define OS_SEM_MAGIC  0x1e02e8a

typedef struct
{
	int     magic;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int     state;
} OsSem_t;

int
osSemCreate(const char *name, bool_t check_prio, bool_t for_protection)
{
	OsSem_t *psem = oxnew(OsSem_t, 1);

	OS_INIT;
	psem->magic = OS_SEM_MAGIC;
	pthread_mutex_init(&psem->mutex, NULL);
	pthread_cond_init(&psem->cond, NULL);
	psem->state = for_protection ? 0 : 1;
	return (int) psem;
}

oret_t
osSemDelete(int sema)
{
	OsSem_t *psem = (OsSem_t *) sema;

	OS_INIT;
	if (sema <= 0)
		return ERROR;
	if (psem->magic != OS_SEM_MAGIC)
		ologAbort("os: corrupt sem");
	pthread_mutex_unlock(&psem->mutex);
	pthread_mutex_destroy(&psem->mutex);
	pthread_cond_destroy(&psem->cond);
	oxvzero(psem);
	oxfree(psem);
	return OK;
}

oret_t
osSemGet(int sema, int wait_tiks)
{
	oret_t rc = ERROR;
	OsSem_t *psem = (OsSem_t *) sema;

	OS_INIT;
	if (sema <= 0)
		return ERROR;
	if (psem->magic != OS_SEM_MAGIC)
		ologAbort("os: corrupt sem");
	pthread_mutex_lock(&psem->mutex);
	if (psem->state == 0)
		_osCondWait(&psem->cond, &psem->mutex, wait_tiks);
	if (psem->state) {
		psem->state = 0;
		rc = OK;
	}
	pthread_mutex_unlock(&psem->mutex);
	SCHEDPOINT;
	return rc;
}

oret_t
osSemRelease(int sema)
{
	OsSem_t *psem = (OsSem_t *) sema;

	OS_INIT;
	if (sema <= 0)
		return ERROR;
	if (psem->magic != OS_SEM_MAGIC)
		ologAbort("os: corrupt sem");
	if (psem->state)
		return ERROR;
	pthread_mutex_lock(&psem->mutex);
	psem->state = 1;
	pthread_cond_signal(&psem->cond);
	pthread_mutex_unlock(&psem->mutex);
	SCHEDPOINT;
	return OK;
}


/* ------------------------ Counting Semaphores --------------------------- */

#define OS_COUNT_MAGIC  0x1e03e8a

typedef struct
{
	int     magic;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int     count;
} OsCount_t;

int
osCountCreate(const char *name, bool_t check_prio, int init_count)
{
	OsCount_t *pcount = oxnew(OsCount_t, 1);

	OS_INIT;
	pcount->magic = OS_COUNT_MAGIC;
	pthread_mutex_init(&pcount->mutex, NULL);
	pthread_cond_init(&pcount->cond, NULL);
	pcount->count = init_count;
	return (int) pcount;
}

oret_t
osCountDelete(int count)
{
	OsCount_t *pcount = (OsCount_t *) count;

	OS_INIT;
	if (count <= 0)
		return ERROR;
	if (pcount->magic != OS_COUNT_MAGIC)
		ologAbort("os: corrupt count");
	pthread_mutex_unlock(&pcount->mutex);
	pthread_mutex_destroy(&pcount->mutex);
	pthread_cond_destroy(&pcount->cond);
	oxvzero(pcount);
	oxfree(pcount);
	return OK;
}

oret_t
osCountGet(int count, int wait_tiks)
{
	oret_t rc = ERROR;
	OsCount_t *pcount = (OsCount_t *) count;

	OS_INIT;
	if (count <= 0)
		return ERROR;
	if (pcount->magic != OS_COUNT_MAGIC)
		ologAbort("os: corrupt count");
	pthread_mutex_lock(&pcount->mutex);
	if (pcount->count == 0)
		_osCondWait(&pcount->cond, &pcount->mutex, wait_tiks);
	if (pcount->count > 0) {
		pcount->count--;
		rc = OK;
	}
	pthread_mutex_unlock(&pcount->mutex);
	SCHEDPOINT;
	return rc;
}

oret_t
osCountRelease(int count)
{
	OsCount_t *pcount = (OsCount_t *) count;

	OS_INIT;
	if (count <= 0)
		return ERROR;
	if (pcount->magic != OS_COUNT_MAGIC)
		ologAbort("os: corrupt count");
	pthread_mutex_lock(&pcount->mutex);
	pcount->count++;
	pthread_cond_signal(&pcount->cond);
	pthread_mutex_unlock(&pcount->mutex);
	SCHEDPOINT;
	return OK;
}


/* -------------------------- Events ------------------------------ */

#define OS_EVENT_MAGIC  0x1e04e8a

typedef struct
{
	int     magic;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
} OsEvent_t;

int
osEventCreate(const char *name, bool_t check_prio)
{
	OsEvent_t *pevent = oxnew(OsEvent_t, 1);

	OS_INIT;
	pevent->magic = OS_EVENT_MAGIC;
	pthread_mutex_init(&pevent->mutex, NULL);
	pthread_cond_init(&pevent->cond, NULL);
	pthread_mutex_lock(&pevent->mutex);
	return (int) pevent;
}

oret_t
osEventDelete(int event)
{
	OsEvent_t *pevent = (OsEvent_t *) event;

	OS_INIT;
	if (event <= 0)
		return ERROR;
	if (pevent->magic != OS_EVENT_MAGIC)
		ologAbort("os: corrupt event");
	pthread_cond_destroy(&pevent->cond);
	pthread_mutex_unlock(&pevent->mutex);
	pthread_mutex_destroy(&pevent->mutex);
	oxvzero(pevent);
	oxfree(pevent);
	return OK;
}

oret_t
osEventWait(int event, int wait_tiks)
{
	oret_t rc;
	OsEvent_t *pevent = (OsEvent_t *) event;

	OS_INIT;
	if (event <= 0)
		return ERROR;
	if (pevent->magic != OS_EVENT_MAGIC)
		ologAbort("os: corrupt event");
	rc = _osCondWait(&pevent->cond, &pevent->mutex, wait_tiks);
	SCHEDPOINT;
	return rc;
}

oret_t
osEventSend(int event)
{
	int     rc;
	OsEvent_t *pevent = (OsEvent_t *) event;

	OS_INIT;
	if (event <= 0)
		return ERROR;
	if (pevent->magic != OS_EVENT_MAGIC)
		ologAbort("os: corrupt event");
	rc = pthread_cond_signal(&pevent->cond);
	SCHEDPOINT;
	return rc ? ERROR : OK;
}

oret_t
osEventPulse(int event)
{
	int     rc;
	OsEvent_t *pevent = (OsEvent_t *) event;

	OS_INIT;
	if (event <= 0)
		return ERROR;
	if (pevent->magic != OS_EVENT_MAGIC)
		ologAbort("os: corrupt event");
	rc = pthread_cond_broadcast(&pevent->cond);
	SCHEDPOINT;
	return rc ? ERROR : OK;
}


/* ----------------------- Message Queues ------------------------- */

#define OS_CUE_MAGIC  0x1e05e8a

typedef struct
{
	int     magic;
	pthread_mutex_t mutex;
	pthread_cond_t rd_cond;
	pthread_cond_t wr_cond;
	int     count;
	int     head;
	int     tail;
	int     num;
	int     size;
	char   *data;
} OsCue_t;

int
osCueCreate(const char *name, int num_elems, int elem_size, bool_t check_prio)
{
	OsCue_t *pcue = oxnew(OsCue_t, 1);

	OS_INIT;
	pcue->magic = OS_CUE_MAGIC;
	pthread_mutex_init(&pcue->mutex, NULL);
	pthread_cond_init(&pcue->rd_cond, NULL);
	pthread_cond_init(&pcue->wr_cond, NULL);
	pcue->count = pcue->head = pcue->tail = 0;
	pcue->num = num_elems;
	pcue->size = elem_size;
	pcue->data = oxnew(char, num_elems * elem_size);

	return (int) pcue;
}

oret_t
osCueDelete(int cue)
{
	OsCue_t *pcue = (OsCue_t *) cue;

	OS_INIT;
	if (cue <= 0)
		return ERROR;
	if (pcue->magic != OS_CUE_MAGIC)
		ologAbort("os: corrupt cue");
	pthread_mutex_unlock(&pcue->mutex);
	pthread_mutex_destroy(&pcue->mutex);
	pthread_cond_destroy(&pcue->rd_cond);
	pthread_cond_destroy(&pcue->wr_cond);
	oxfree(pcue->data);
	oxvzero(pcue);
	oxfree(pcue);
	return OK;
}

oret_t
osCuePut(int cue, const void *elem, int elem_size, int wait_tiks)
{
	oret_t rc = ERROR;
	OsCue_t *pcue = (OsCue_t *) cue;

	OS_INIT;
	if (cue <= 0 || elem_size <= 0)
		return ERROR;
	if (pcue->magic != OS_CUE_MAGIC)
		ologAbort("os: corrupt cue");
	pthread_mutex_lock(&pcue->mutex);
	if (pcue->count >= pcue->num)
		_osCondWait(&pcue->wr_cond, &pcue->mutex, wait_tiks);
	if (pcue->count < pcue->num) {
		if (elem_size > pcue->size)
			elem_size = pcue->size;
		bcopy(elem, pcue->data + pcue->head * pcue->size, elem_size);
		if (++pcue->head >= pcue->num)
			pcue->head = 0;
		pcue->count++;
		pthread_cond_signal(&pcue->rd_cond);
		rc = OK;
	}
	pthread_mutex_unlock(&pcue->mutex);
	SCHEDPOINT;
	return rc;
}

oret_t
osCueGet(int cue, void *elem, int elem_size, int wait_tiks)
{
	oret_t rc = ERROR;
	OsCue_t *pcue = (OsCue_t *) cue;

	OS_INIT;
	if (cue <= 0 || elem_size <= 0)
		return ERROR;
	if (pcue->magic != OS_CUE_MAGIC)
		ologAbort("os: corrupt cue");
	pthread_mutex_lock(&pcue->mutex);
	if (pcue->count == 0)
		_osCondWait(&pcue->rd_cond, &pcue->mutex, wait_tiks);
	if (pcue->count > 0) {
		if (elem_size > pcue->size)
			elem_size = pcue->size;
		bcopy(pcue->data + pcue->tail * pcue->size, elem, elem_size);
		if (++pcue->tail >= pcue->num)
			pcue->tail = 0;
		pcue->count--;
		pthread_cond_signal(&pcue->wr_cond);
		rc = OK;
	}
	pthread_mutex_unlock(&pcue->mutex);
	SCHEDPOINT;
	return rc;
}

int
osCueSize(int cue)
{
	OsCue_t *pcue = (OsCue_t *) cue;

	OS_INIT;
	if (cue <= 0)
		return -1;
	if (pcue->magic != OS_CUE_MAGIC)
		ologAbort("os: corrupt cue");
	return pcue->count;
}


/* --------------------- Watchdog Timers -------------------------- */

#define OS_WDOG_MAGIC  0x1e06e8a

typedef struct
{
	int     magic;
	int     round;
	int     tid;
	char    name[40];
} OsWdog_t;

typedef void (*OsWdogFunc_t) (int param);

void
_osWdogWrapper(int wd, int tiks, int proc, int param, int round)
{
	OsWdog_t *pwdog = (OsWdog_t *) wd;
	OsWdogFunc_t func = (OsWdogFunc_t) proc;
	int     msec;

	OS_INIT;
	if (wd <= 0 || pwdog->magic != OS_WDOG_MAGIC)
		return;
	if (proc == 0 || tiks < 0 || round != pwdog->round)
		return;
	msec = _osTicks2Msec(tiks);
	osMsSleep(msec);
	if (round != pwdog->round)
		return;
	(*func) (param);
}

int
osWdogCreate(const char *name)
{
	static int wdno = 0;
	OsWdog_t *pwdog = oxnew(OsWdog_t, 1);

	OS_INIT;
	pwdog->magic = OS_WDOG_MAGIC;
	sprintf(pwdog->name, "wd/%d", ++wdno);
	pwdog->round = 0;
	pwdog->tid = 0;
	return (int) pwdog;
}

oret_t
osWdogDelete(int wd)
{
	OsWdog_t *pwdog = (OsWdog_t *) wd;

	OS_INIT;
	if (wd <= 0)
		return ERROR;
	if (pwdog->magic != OS_WDOG_MAGIC)
		return ERROR;
	osWdogStop(wd);
	oxvzero(pwdog);
	oxfree(pwdog);
	return OK;
}


oret_t
osWdogStart(int wd, int tiks, proc_t proc, int param)
{
	OsWdog_t *pwdog = (OsWdog_t *) wd;

	OS_INIT;
	if (wd <= 0)
		return ERROR;
	if (pwdog->magic != OS_WDOG_MAGIC)
		return ERROR;
	osWdogStop(wd);
	if (tiks < 0 || proc == 0)
		return ERROR;
	pwdog->round++;
	pwdog->tid = osTaskCreate(pwdog->name, 1,
							  (proc_t) _osWdogWrapper, (int) pwdog,
							  tiks, (int) proc, param, pwdog->round);
	return OK;
}


oret_t
osWdogStop(int wd)
{
	OsWdog_t *pwdog = (OsWdog_t *) wd;

	OS_INIT;
	if (wd <= 0)
		return ERROR;
	if (pwdog->magic != OS_WDOG_MAGIC)
		return ERROR;
	if (pwdog->tid != 0) {
		pwdog->round++;
		osTaskDelete(pwdog->tid);
	}
	pwdog->tid = 0;
	return OK;
}


/* ---------------------- System Information ------------------------ */

int
osGetBoardNo(void)
{
	return 0;
}


int
osGetClockRate(void)
{
	ologAbort("osGetClockRate unimplemented");
	return 0;
}


int
osGetMemTop(void)
{
	ologAbort("osGetMemTop unimplemented");
	return 0;
}


/* ------------------- VME related stuff (STUBS) ----------------------- */


oret_t
osCacheSetup(void)
{
	ologAbort("osCacheSetup unimplemented");
	return ERROR;
}


oret_t
osMemCache(void *addr, int len, bool_t for_write)
{
	ologAbort("osMemCache unimplemented");
	return ERROR;
}


oret_t
osMemProbe(void *addr, int len, bool_t for_write)
{
	ologAbort("osMemProbe unimplemented");
	return ERROR;
}

bool_t  os_memprobe_implemented = FALSE;


int
osMem2Vme(int space, int local)
{
	ologAbort("osMem2Vme unimplemented");
	return -1;
}


int
osVme2Mem(int space, int vme, int size_kb)
{
	ologAbort("osVme2Mem unimplemented");
	return -1;
}


oret_t
osSetVmeVect(int vect, proc_t proc, int param)
{
	ologAbort("osSetVmeVect unimplemented");
	return ERROR;
}


int
osSetVmeIrq(int irq, bool_t enable)
{
	ologAbort("osSetVmeIrq unimplemented");
	return ERROR;
}


oret_t
osSendVmeIntr(int irq, int vect)
{
	ologAbort("osSendVmeIntr unimplemented");
	return ERROR;
}
