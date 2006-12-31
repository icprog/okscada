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

  Linux specific wrappers and stubs.

*/

#define _GNU_SOURCE
#include <pthread.h>
#include <dlfcn.h>

#include <optikus/conf-os.h>	/* includes config.h */
#include <optikus/conf-mem.h>	/* for oxnew,oxfree,... */
#include <optikus/log.h>		/* for ologAbort */

#include <stdio.h>			/* for printf,sprintf */
#include <stdlib.h>			/* for atexit */
#include <unistd.h>			/* for geteuid */

/* FIXME: clocks should move to a separate source */
#include <optikus/util.h>	/* for osMsSleep,... */
#include <sys/time.h>		/* for gettimeofday */

#if defined(HAVE_TUNDRA_H)
#include <tundra.h>
#endif /* HAVE_TUNDRA_H */


void	_osTaskSchedPoint(void);

/* FIXME: SCHEDPOINT s dangerous. It at least breaks Perl GTK. */
#define SCHEDPOINT   _osTaskSchedPoint()


int     os_stack_size = 16000;			/* Default task stack size */


/* ---------------- Dynamic Symbol Tables ------------------- */

/*
 * Look for a symbol in the VxWorks symbol table.
 */

void   *_os_dl_handle = (void *) -1;

void
_osSymExit(void)
{
	if (_os_dl_handle != (void *) 0 && _os_dl_handle != (void *) -1)
		dlclose(_os_dl_handle);
	_os_dl_handle = (void *) -1;
}

void   *
osGetSym(const char *stab, const char *name, int what)
{
	if (_os_dl_handle == (void *) -1) {
		_os_dl_handle = dlopen(NULL, RTLD_NOW);
		if (_os_dl_handle == (void *) 0)
			ologAbort("dlopen failure");
		atexit(_osSymExit);
	}
	if (_os_dl_handle == (void *) 0)
		return (void *) 0;
	return dlsym(_os_dl_handle, name);
}


/*
 * Modify a function so that it jumps to another routine.
 */
oret_t
osSetJump(const char *stab, const char *from, const char *to,
		  unsigned **saveptr)
{
	ologAbort("osSetJump unimplemented");
	return ERROR;
}


/*
 * Remove the jump instruction and restore the routine.
 */
oret_t
osUnsetJump(const char *stab, const char *from, unsigned *savebuf)
{
	ologAbort("osUnsetJump unimplemented");
	return ERROR;
}


/* ------------------------- Task control -------------------------- */

int     _os_quiet = 1;

bool_t  os_dummy_tasks = FALSE;

#define DUMMY_TID  (-3333)

#define OS_MAX_TASK    64

#define OS_TASK_SCHED  SCHED_RR

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
	/* FIXME: posix: 1..99, vxworks: 0..255, order differs */
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
	pthread_exit((void *) 0);
	return (void *) 0;
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
	if (_os_broken_sched)
		pthread_yield();
	return;
}

/*
 * Create a task.
 */
int
_osTaskCreate(const char *name, int prio, proc_t func,
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
 * Kill a task.
 */
oret_t
_osTaskDelete(int tid)
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
 * Return numeric task identifier given its name.
 */
int
_osTaskId(const char *name)
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
 * user-level wrappers which may act as dummy
 */
int
osTaskCreate(const char *name, int prio, proc_t func,
			 int p1, int p2, int p3, int p4, int p5)
{
	if (os_dummy_tasks)
		return DUMMY_TID;
	else
		return _osTaskCreate(name, prio, func, p1, p2, p3, p4, p5);
}

oret_t
osTaskDelete(int tid)
{
	if (tid == DUMMY_TID)
		return OK;
	else
		return _osTaskDelete(tid);
}

int
osTaskId(const char *name)
{
	if (os_dummy_tasks)
		return DUMMY_TID;
	else
		return _osTaskId(name);
}


/*
 * Return task name given its numeric identifier.
 */
char   *
osTaskName(int tid, char *name_buf)
{
	static char my_name_buf[40];
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	if (tid == DUMMY_TID) {
		strcpy(name_buf, "tDUMMY");
		return OK;
	}
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
_osTaskPrio(int tid, int new_prio)
{
	int     old_prio;
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	if (tid == DUMMY_TID)
		return OK;
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


int
osTaskPrio(int tid, int new_prio)
{
	if (os_dummy_tasks)
		return 1;
	else
		return _osTaskPrio(tid, new_prio);
}


/*
 * Suspend task.
 */
oret_t
_osTaskSuspend(int tid)
{
	oret_t rc = ERROR;
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	if (tid == DUMMY_TID)
		return OK;
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


oret_t
osTaskSuspend(int tid)
{
	if (os_dummy_tasks)
		return OK;
	else
		return _osTaskSuspend(tid);
}


/*
 * Resume task.
 */
oret_t
osTaskResume(int tid)
{
	oret_t rc = ERROR;
	OsTask_t *ptask = (OsTask_t *) tid, *pself;

	if (tid == DUMMY_TID)
		return OK;
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
	if (tid == DUMMY_TID)
		return OK;
	SCHEDPOINT;
	return OK;
}


/* ------------------ Scheduler Control (STUBS) --------------------- */


/*
 * Disable (lock=true) or enable (lock=false) preemption.
 */
oret_t
osSchedLock(bool_t lock)
{
	return ERROR;
}


/*
 * Return true if called from ISR.
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
 * Reboots the current CPU/Board/System.
 */
void
osSysReboot(int wait_ms)
{
	ologAbort("osSysReboot unimplemented");
}


/*
 * Aborts the current script executed in the VxWorks shell.
 */
void
osShellStop(void)
{
	ologAbort("osShellStop unimplemented");
}


/* ---------------------- Critical Sections ------------------------- */

#define OS_MUTEX_MAGIC  0x1e01e8a

typedef struct
{
	int     magic;
	pthread_mutexattr_t attr;
	pthread_mutex_t mutex;
} OsMutex_t;

int
osMutexCreate(const char *name, bool_t check_prio, bool_t check_inversion)
{
	OsMutex_t *pmutex = oxnew(OsMutex_t, 1);

	OS_INIT;
	pmutex->magic = OS_MUTEX_MAGIC;
	pthread_mutexattr_init(&pmutex->attr);
	pthread_mutexattr_settype(&pmutex->attr, PTHREAD_MUTEX_RECURSIVE_NP);
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
	struct timespec ts;
	OsMutex_t *pmutex = (OsMutex_t *) mutex;

	OS_INIT;
	if (mutex <= 0)
		return ERROR;
	if (pmutex->magic != OS_MUTEX_MAGIC)
		ologAbort("os: corrupt mutex");
	if (wait_tiks < 0) {
		rc = pthread_mutex_lock(&pmutex->mutex);
		SCHEDPOINT;
		return rc ? ERROR : OK;
	}
	if (wait_tiks == 0) {
		rc = pthread_mutex_trylock(&pmutex->mutex);
		SCHEDPOINT;
		return rc ? ERROR : OK;
	}
	msec = _osTicks2Msec(wait_tiks);
	ts.tv_sec = msec / 1000L;
	ts.tv_nsec = (msec % 1000L) * 1000000L;
	rc = pthread_mutex_timedlock(&pmutex->mutex, &ts);
	SCHEDPOINT;
	return rc ? ERROR : OK;
}

oret_t
osMutexRelease(int mutex)
{
	int     rc;
	OsMutex_t *pmutex = (OsMutex_t *) mutex;

	OS_INIT;
	if (mutex <= 0)
		return ERROR;
	if (pmutex->magic != OS_MUTEX_MAGIC)
		ologAbort("os: corrupt mutex");
	rc = pthread_mutex_unlock(&pmutex->mutex);
	SCHEDPOINT;
	return rc ? ERROR : OK;
}


/* ---------------------------- Semaphores ----------------------------- */

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


/* ---------------------- Counting Semaphores ------------------------- */

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


/* ------------------------- Message Queues ----------------------------- */

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


/* ----------------------- Watchdog Timers --------------------------- */

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
	pwdog->tid = _osTaskCreate(pwdog->name, 1,
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
		_osTaskDelete(pwdog->tid);
	}
	pwdog->tid = 0;
	return OK;
}


/* ------------------------ System Information ------------------------- */

int
osGetClockRate(void)
{
	ologAbort("osGetClockRate unimplemented");
	return 0;
}


int
osGetMemTop(void)
{
	return 0;
}


/* ---------------------- VME related stuff ------------------------- */

/*
 * Initialize data and instruction caches for multiboard VME chassis.
 */
oret_t
osCacheSetup(void)
{
	return OK;
}


/*
 * Flush (invalidate) data cache.
 */
oret_t
osMemCache(void *addr, int len, bool_t for_write)
{
	return OK;
}


/*
 * Probes if memory can be accessed at given address.
 */
oret_t
osMemProbe(void *addr, int len, bool_t for_write)
{
	return OK;
}

bool_t  os_memprobe_implemented = FALSE;


#if defined(HAVE_TUNDRA_H)

/* ---------------------------------------------------------------- */
/* -------  API to the Linux' Tundra Universe II VME driver ------- */
/* ------------------- see tundra/tundra.c ------------------------ */
/* ---------------------------------------------------------------- */

typedef struct
{
	bool_t  is_master;
	unsigned vme_addr;
	unsigned size;
	short   am;
	short   space;
	unsigned ctl;
	char    path[120];
	int     img_no;
	int     handle;
	char   *map_addr;
} TundraImage_t;

typedef struct
{
	int     vect;
	proc_t  proc;
	int     param;
	int     handle;
	int     taskid;
	int     seq_no;
} TundraVector_t;

#define MAX_VME_IMAGE  32
#define MAX_VME_IRQ    8
#define MAX_VME_VECTOR 256

static bool_t tundra_error = FALSE;
static char tundra_root[80] = "";
static TundraImage_t *tundra_images = 0;
static TundraVector_t *tundra_vectors = 0;
static int tundra_genint_handle = 0;
static int tundra_seq_no = 0;
static int tundra_board_no = -1;

int     tundra_interrupt_task_prio = 1;
int     tundra_verb = 1;

/* The following arrays help to control the IRQ resources. */

int     __os_irq_count[8] = {0};
bool_t  __os_ban_irq[8] = {0};		/* disable IRQ by number. */
bool_t  __os_ban_all_irq = FALSE;	/* disables all IRQ */
bool_t  __os_addr_is_local = FALSE;


static int
am_to_space(int am)
{
	if (TUNDRA_AM_IS_A16(am))
		return TUNDRA_SPACE_A16D32;
	else if (TUNDRA_AM_IS_A24(am))
		return TUNDRA_SPACE_A24D32;
	else if (TUNDRA_AM_IS_A32(am))
		return TUNDRA_SPACE_A32D32;
	else
		return -1;
}


static oret_t
tundraClearVector(int vect)
{
	TundraVector_t *pvec;

	if (vect < 0 || vect > 255)
		return ERROR;
	pvec = &tundra_vectors[vect];
	pvec->seq_no = 0;
	pvec->vect = vect;
	pvec->proc = 0;
	pvec->param = 0;
	if (pvec->taskid)
		_osTaskDelete(pvec->taskid);
	pvec->taskid = 0;
	if (pvec->handle > 0)
		close(pvec->handle);
	pvec->handle = 0;
	return OK;
}


static void
tundraExit(void)
{
	TundraImage_t *img;
	int     i;

	if (tundra_genint_handle > 0)
		close(tundra_genint_handle);
	tundra_genint_handle = 0;
	if (tundra_images) {
		for (i = 0; i < MAX_VME_IMAGE; i++) {
			img = &tundra_images[i];
			if (!img->size)
				continue;
			if (img->map_addr != 0 && img->map_addr != (char *) -1)
				munmap(img->map_addr, img->size);
			img->map_addr = 0;
			if (img->handle > 0)
				close(img->handle);
			img->handle = 0;
			img->size = 0;
			oxvzero(img);
		}
		oxfree(tundra_images);
		tundra_images = 0;
	}
	if (tundra_vectors) {
		for (i = 0; i < MAX_VME_VECTOR; i++)
			tundraClearVector(i);
		oxfree(tundra_vectors);
		tundra_vectors = 0;
	}
	for (i = 0; i < MAX_VME_IRQ; i++)
		__os_irq_count[i] = 0;
	tundra_board_no = -1;
	tundra_root[0] = 0;
	tundra_error = FALSE;
}


static oret_t
tundraSetupImages(void)
{
	FILE   *f;
	char    name[80], line[120], tmp_path[120];
	char    str_ms[12], temp_s1[12], temp_s2[12];
	int     n, qty;
	TundraImage_t image;

	if (tundra_images)
		return OK;

	tundra_images = oxnew(TundraImage_t, MAX_VME_IMAGE);

	sprintf(name, "%s/images", tundra_root);
	f = fopen(name, "r");
	if (!f)
		return ERROR;
	qty = 0;
	while (!feof(f)) {
		line[0] = 0;
		fgets(line, sizeof line, f);
		if (line[0] == 0 || line[0] == '#')
			continue;
		/* ..."MST 0 0x00000000 0x00010000 A16 D32 0x2d 0x80805000 driver/tundra/0/master/0" */
		oxvzero(&image);
		tmp_path[0] = 0;
		n = sscanf(line, " %s %d 0x%x 0x%x %s %s 0x%hx 0x%x %s",
				   str_ms, &image.img_no, &image.vme_addr, &image.size,
				   temp_s1, temp_s2, &image.am, &image.ctl, tmp_path);
		if (n != 9)
			goto FAIL;
		if (!strcmp(str_ms, "MST"))
			image.is_master = TRUE;
		else if (!strcmp(str_ms, "SLV"))
			image.is_master = FALSE;
		else
			goto FAIL;
		image.space = am_to_space(image.am);
		if (image.space < 0)
			goto FAIL;
		sprintf(image.path, "/proc/%s", tmp_path);
		oxvcopy(&image, &tundra_images[qty++]);
	}
	fclose(f);
	return OK;
  FAIL:
	olog("tundra image file has wrong format line=[%s]", line);
	oxfree(tundra_images);
	tundra_images = 0;
	__os_addr_is_local = FALSE;
	return ERROR;
}


static oret_t
tundraInit(void)
{
	FILE   *f;
	char    name[120], temp_root[80];
	int     card_num, card_no, board_no, irq_state;
	int     i;

	if (tundra_error)
		return ERROR;
	if (tundra_root[0])
		return OK;
	tundraExit();
	f = fopen("/proc/driver/tundra/cardnum", "r");
	if (!f) {
		olog("Tundra Universe driver not installed");
		tundra_error = TRUE;
		return ERROR;
	}
	card_num = -1;
	fscanf(f, " %d", &card_num);
	fclose(f);
	if (card_num < 1) {
		olog("Tundra Universe cards not found");
		tundra_error = TRUE;
		return ERROR;
	}
	card_no = 0;			/* We can use only the 1st card */
	sprintf(temp_root, "/proc/driver/tundra/%d", card_no);
	sprintf(name, "%s/boardno", temp_root);
	f = fopen(name, "r");
	i = f ? fscanf(f, " %d", &board_no) : 0;
	fclose(f);
	if (!f || i != 1) {
		olog("Tundra Universe card 0 not responding");
		tundra_error = TRUE;
		return ERROR;
	}
	tundra_board_no = board_no;
	strcpy(tundra_root, temp_root);
	if (tundraSetupImages()) {
		tundra_root[0] = 0;
		olog("Tundra Universe memory mapping setup error");
		tundra_error = TRUE;
		tundraExit();
		return ERROR;
	}
	tundra_vectors = oxnew(TundraVector_t, MAX_VME_VECTOR);
	for (i = 0; i < MAX_VME_VECTOR; i++)
		tundraClearVector(i);
	for (i = 0; i < MAX_VME_IRQ; i++)
		__os_irq_count[i] = 0;
	/*
		Some interrupt levels may have already be enabled by external means.
		We want that these levels not be disabled upon program exit.
		For this we mark them with positive IRQ count.
	*/
	for (i = 0; i < MAX_VME_IRQ; i++) {
		irq_state = 0;
		sprintf(name, "%s/irq/%d", tundra_root, i);
		f = fopen(name, "r");
		if (f) {
			if (fscanf(f, " %d", &irq_state) == 1) {
				if (irq_state)
					__os_irq_count[i] = 1;
			}
			fclose(f);
		}
	}
	atexit(tundraExit);
	return OK;
}


int
osGetBoardNo(void)
{
	tundraInit();
	return tundra_board_no;
}


/*
 * Translates local address into VME address, so that external
 * VME devices can access local memory.
 */

int     __os_open_errno = 0, __os_mmap_errno = 0;
bool_t  __os_dummy_vme = FALSE;

static oret_t
tundraMapImage(TundraImage_t * img)
{
	if (img->map_addr)
		return OK;
	if (__os_dummy_vme) {
		img->map_addr = malloc(img->size);
		if (!img->map_addr)
			return ERROR;
		memset(img->map_addr, 0, img->size);
		return OK;
	}
	if (img->handle < 0)
		return ERROR;
	if (img->handle == 0) {
		img->handle = open(img->path, O_RDWR, 0);
		if (img->handle < 0) {
			__os_open_errno = errno;
			return ERROR;
		}
	}
	img->map_addr = mmap(NULL, img->size,
						 PROT_READ | PROT_WRITE, MAP_SHARED, img->handle, 0);
	if (img->map_addr == (char *) -1)
		img->map_addr = 0;
	if (img->map_addr == 0) {
		__os_mmap_errno = errno;
		close(img->handle);
		img->handle = -1;
		return ERROR;
	}
	__os_open_errno = __os_mmap_errno = 0;
	return OK;
}


static int
tundraVmeToMem(int am, int vme, bool_t is_master)
{
	TundraImage_t *img;
	int     space, i;
	unsigned uvme = vme;
	char   *addr;

	if (tundraInit())
		return ERROR;
	space = am_to_space(am);
	if (space < 0)
		return ERROR;
	for (i = 0; i < MAX_VME_IMAGE; i++) {
		img = &tundra_images[i];
		if (!img->size
			|| img->space != space
			|| img->is_master != is_master
			|| uvme < img->vme_addr || uvme >= img->vme_addr + img->size)
			continue;
		if (tundraMapImage(img))
			return ERROR;
		addr = img->map_addr;
		addr += uvme - img->vme_addr;
		ologIf(tundra_verb > 2, "vme(%02Xh) 0x%08x -> local(%s) 0x%08x",
			  am, vme, is_master ? "MST" : "SLV", (int) addr);
		return (int) addr;
	}
	return ERROR;
}


int
osMem2Vme(int am, int local)
{
	TundraImage_t *img;
	int     space, i;
	unsigned addr;

	if (tundraInit())
		return ERROR;
	space = am_to_space(am);
	if (space < 0)
		return ERROR;
	for (i = 0; i < MAX_VME_IMAGE; i++) {
		img = &tundra_images[i];
		if (!img->size || img->space != space || img->is_master)
			continue;
		if (local == 0) {
			if (tundraMapImage(img))
				return ERROR;
			addr = img->vme_addr + local;
			ologIf(tundra_verb > 2, "local 0 -> vme(%02Xh) 0x%08x", am, addr);
			return (int) addr;
		}
		if ((char *) local == img->map_addr) {
			addr = (unsigned) img->map_addr;
			ologIf(tundra_verb > 2, "local 0x%08x -> vme(%02Xh) 0x%08x", local,
				  am, addr);
			return (int) addr;
		}
	}
	return ERROR;
}


/*
 * Translates VME address into local address, so that our CPU
 * can access external VME devices.
 */

int
osVme2Mem(int am, int vme, int size_kb)
{
	int     addr;

	__os_addr_is_local = FALSE;
	addr = tundraVmeToMem(am, vme, FALSE);
	if (addr != ERROR) {
		__os_addr_is_local = TRUE;
		return addr;
	}
	addr = tundraVmeToMem(am, vme, TRUE);
	return addr;
}


/*
 * Connects interrupt service routine to the VME interrupt vector.
 */

static int
tundraInterruptTask(int vect, int seq_no)
{
	TundraVector_t *pvec = &tundra_vectors[vect];
	char    buf[1];
	int     ret;
	void    (*handler) (int) = (void *) pvec->proc;
	int     param = pvec->param;

	for (;;) {
		ret = read(pvec->handle, buf, 1);
		if (!tundra_vectors)
			break;
		if (pvec->seq_no != seq_no)
			break;
		if (ret != 1)
			continue;
		handler(param);
	}
	return 0;
}


oret_t
osSetVmeVect(int vect, proc_t proc, int param)
{
	TundraVector_t *pvec;
	char    name[120];

	if (tundraInit())
		return ERROR;
	if (vect < 0 || vect >= MAX_VME_VECTOR)
		return ERROR;
	if (tundraClearVector(vect))
		return ERROR;
	if (proc == 0)
		return OK;
	pvec = &tundra_vectors[vect];
	pvec->seq_no = 0;
	pvec->proc = proc;
	pvec->param = param;
	sprintf(name, "%s/vector/%03d", tundra_root, vect);
	pvec->handle = open(name, O_RDONLY, 0);
	if (pvec->handle <= 0)
		goto FAIL;
	ioctl(pvec->handle, TUNDRA_OP_INFINITE, 1);
	sprintf(name, "tundv%03d", vect);
	if (!++tundra_seq_no)
		++tundra_seq_no;
	pvec->seq_no = tundra_seq_no;
	pvec->taskid = _osTaskCreate(name, tundra_interrupt_task_prio,
								 (proc_t) tundraInterruptTask,
								 vect, pvec->seq_no, 0, 0, 0);
	if (!pvec->taskid)
		goto FAIL;
	return OK;
  FAIL:
	tundraClearVector(vect);
	return ERROR;
}


/*
 * Enables or disables VME interrupts.
 */

int
osSetVmeIrq(int irq, bool_t enable)
{
	char    name[120], rbuf[1], wbuf[1];
	int     handle;

	if (tundraInit())
		return ERROR;
	if (irq < 0 || irq >= MAX_VME_IRQ)
		return ERROR;
	if (enable) {
		/* Enable */
		if (__os_irq_count[irq] > 99)
			return ERROR;
		__os_irq_count[irq]++;
		if (__os_irq_count[irq] != 1)
			return OK;
		if (__os_ban_all_irq || __os_ban_irq[irq])
			return OK;
	} else {
		/* Disable */
		if (__os_irq_count[irq] < 1)
			return ERROR;
		__os_irq_count[irq]--;
		if (__os_irq_count[irq] != 0)
			return OK;
		if (__os_ban_all_irq || __os_ban_irq[irq])
			return OK;
	}
	rbuf[0] = 0;
	wbuf[0] = enable ? '1' : '0';
	sprintf(name, "%s/irq/%d", tundra_root, irq);
	handle = open(name, O_RDWR, 0);
	if (handle <= 0)
		return ERROR;
	read(handle, rbuf, 1);
	write(handle, wbuf, 1);
	close(handle);
	switch (rbuf[0]) {
	case '0':
		return OK;
	case '1':
		return OK;
	default:
		return ERROR;
	}
}


/*
 * Generate VME interrupts.
 */
oret_t
osSendVmeIntr(int irq, int vect)
{
	char    name[120];
	int     ret, param;

	if (irq < 0 || irq >= MAX_VME_IRQ
			|| vect < 0 || vect >= MAX_VME_VECTOR)
		return ERROR;
	if (tundraInit())
		return ERROR;
	if (tundra_genint_handle < 0)
		return ERROR;
	if (tundra_genint_handle == 0) {
		sprintf(name, "%s/genint", tundra_root);
		tundra_genint_handle = open(name, O_WRONLY, 0);
		if (tundra_genint_handle <= 0) {
			tundra_genint_handle = -1;
			return ERROR;
		}
	}
	param = (irq << 8) | vect;
	ret = ioctl(tundra_genint_handle, TUNDRA_OP_GENINT, param);
	return ret;
}

#else /* !HAVE_TUNDRA_H */

/* ------------------- VME related stuff (STUBS) ----------------------- */

int
osGetBoardNo(void)
{
	return 0;
}

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

#endif /* HAVE_TUNDRA_H */
