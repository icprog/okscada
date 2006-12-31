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

  VxWorks specific wrappers and stubs.

  Note: To build for VxWorks, define the following feature flags on the
        compiler command line:
    VXWORKS
      If the macro is not set, UNIX is assumed.
    USPARC
      Define, if you intend to use the library on a 64-bit Ultra SPARC.

*/

#include <vxWorks.h>
#include <stdioLib.h>
#include <string.h>
#include <stdlib.h>
#include <symLib.h>
#include <intLib.h>
#include <iv.h>
#include <vmLib.h>
#include <sysSymTbl.h>
#include <a_out.h>
#include <taskLib.h>
#include <sysLib.h>
#include <vme.h>
#include <sockLib.h>
#include <selectLib.h>
#include <timers.h>
#include <ctype.h>
#include <semLib.h>
#include <msgQLib.h>
#include <vxLib.h>
#include <cacheLib.h>
#include <wdLib.h>
#include <shellLib.h>

#include <optikus/util.h>
#include <optikus/conf-os.h>


/* ------------------------ Locals -------------------------- */

/* VME control */

#define NO_SIZE_VAR  (ulong_t *)(long)-1

/* Address of the variable controlling the VME A24 size */
ulong_t *__os_vme_size_ptr = NO_SIZE_VAR;

/* The following arrays help to control the IRQ resources. */
#define MAX_OS_IRQ   64
int     __os_irq_count[MAX_OS_IRQ] = {0};
int     __os_send_intr_ok[MAX_OS_IRQ] = {0};
int     __os_send_intr_err[MAX_OS_IRQ] = {0};
char    __os_ban_irq[MAX_OS_IRQ] = {0};	/* disable IRQ by number. */
bool_t  __os_ban_all_irq = FALSE;		/* disables all IRQ */

bool_t  __os_addr_is_local = FALSE;

/* Task control */

int     os_stack_size = 16000;			/* Default task stack size */
int     __os_task_flags = VX_FP_TASK;	/* Default task flags */

/* MMU control */

typedef VM_CONTEXT_ID(*VmCurrentGet_t) (void);
typedef int (*VmPageSizeGet_t) (void);
typedef void (*VmStateGet_t) (VM_CONTEXT_ID, void *, unsigned *);
typedef void (*VmStateSet_t) (VM_CONTEXT_ID, void *, int, int, int);

VmCurrentGet_t __os_vmCurrentGet = 0;
VmPageSizeGet_t __os_vmPageSizeGet = 0;
VmStateGet_t __os_vmStateGet = 0;
VmStateSet_t __os_vmStateSet = 0;
int     __os_vm_inited = 0;


/* -------------------- Dynamic symbol tables ---------------------- */

/*
 * Look for a symbol in the VxWorks symbol table.
 */
void   *
osGetSym(const char *symtable, const char *name, int what)
{
	char    sym[80];
	SYM_TYPE type;
	void   *val;
	oret_t rc;
	int     off = 0;

	sym[0] = '_';
	strcpy(sym + 1, name);
	/* first without underscore, then with prepended underscore */
	for (off = 1; off >= 0; off--) {
		rc = symFindByName(symtable, sym + off, (char **) &val, &type);
		type &= N_DATA | N_BSS | N_TEXT;
		if (what == GETSYM_VAR && type != N_DATA && type != N_BSS)
			rc = ERROR;
		else if (what == GETSYM_FUN && type != N_TEXT)
			rc = ERROR;
		if (rc == OK)
			break;
	}
	return (rc == OK ? val : NULL);
}


/*
 * Initialize the virtual memory control
 * We simply dynamically link with the MMU control library,
 * if it is present in BSP.
 */
void
_osVmSetup(void)
{
	if (__os_vm_inited)
		return;
	__os_vmCurrentGet =
		(VmCurrentGet_t) osGetSym(".", "vmCurrentGet", GETSYM_FUN);
	__os_vmPageSizeGet =
		(VmPageSizeGet_t) osGetSym(".", "vmPageSizeGet", GETSYM_FUN);
	__os_vmStateGet = (VmStateGet_t) osGetSym(".", "vmStateGet", GETSYM_FUN);
	__os_vmStateSet = (VmStateSet_t) osGetSym(".", "vmStateSet", GETSYM_FUN);
	if (!__os_vmCurrentGet || !__os_vmPageSizeGet || !__os_vmStateGet
		|| !__os_vmStateSet)
		__os_vm_inited = -1;
	else
		__os_vm_inited = 1;
}


/*
 * Modify a function so that it jumps to another routine.
 */
oret_t
osSetJump(const char *symtable, const char *from, const char *to,
		  unsigned **saveptr)
{
	unsigned *pfrom = osGetSym(symtable, from, GETSYM_FUN);
	unsigned uto = (unsigned) osGetSym(symtable, to, GETSYM_FUN);
	unsigned *savebuf = 0;
	unsigned state;
	VM_CONTEXT_ID ctx = 0;
	int     len = 0;
	int     ll;
	void   *page = 0;

	_osVmSetup();
	if (!pfrom || !uto)
		return ERROR;
	if (__os_vm_inited > 0) {
		ctx = __os_vmCurrentGet();
		len = __os_vmPageSizeGet();
		page = (void *) ((unsigned) pfrom & ~(len - 1));
		__os_vmStateGet(ctx, page, &state);
		state &= VM_STATE_MASK_WRITABLE;
		if (state != VM_STATE_WRITABLE && __os_vm_inited > 0)
			__os_vmStateSet(ctx, page, len, VM_STATE_MASK_WRITABLE,
						   VM_STATE_WRITABLE);
	}
	if (saveptr == 0)
		savebuf = 0;
	else
		*saveptr = savebuf = malloc(sizeof(unsigned) * 3);
	if (savebuf != 0) {
		savebuf[0] = pfrom[0];
		savebuf[1] = pfrom[1];
		savebuf[2] = pfrom[2];
	}
	ll = intLock();
	pfrom[0] = 0x05000000 | (uto >> 10);   /*-  sethi %hi(to),%g2  -*/
	pfrom[1] = 0x81c0a000 | (uto & 0x3ff); /*-  jmp   %g2+%lo(to)  -*/
	pfrom[2] = 0x1000000;				   /*-  nop                -*/
	intUnlock(ll);
	if (__os_vm_inited > 0) {
		if (state != VM_STATE_WRITABLE)
			__os_vmStateSet(ctx, page, len, VM_STATE_MASK_WRITABLE, state);
	}
	return OK;
}


/*
 * Remove the jump instruction and restore the routine.
 */
oret_t
osUnsetJump(const char *symtable, const char *from, unsigned *savebuf)
{
	unsigned *pfrom = osGetSym(symtable, from, GETSYM_FUN);
	void   *page = 0;
	unsigned state = 0;
	VM_CONTEXT_ID ctx = 0;
	int     len = 0;
	int     ll;

	_osVmSetup();
	if (savebuf == NULL || pfrom == NULL)
		return ERROR;
	if (__os_vm_inited > 0) {
		ctx = __os_vmCurrentGet();
		len = __os_vmPageSizeGet();
		page = (void *) ((unsigned) pfrom & ~(len - 1));
		__os_vmStateGet(ctx, page, &state);
		state &= VM_STATE_MASK_WRITABLE;
		if (state != VM_STATE_WRITABLE)
			__os_vmStateSet(ctx, page, len, VM_STATE_MASK_WRITABLE,
						   VM_STATE_WRITABLE);
	}
	ll = intLock();
	pfrom[0] = savebuf[0];
	pfrom[1] = savebuf[1];
	pfrom[2] = savebuf[2];
	intUnlock(ll);
	if (__os_vm_inited > 0) {
		if (state != VM_STATE_WRITABLE)
			__os_vmStateSet(ctx, page, len, VM_STATE_MASK_WRITABLE, state);
	}
	return OK;
}


/* ------------------------- Task control -------------------------- */

/*
 * Create a task.
 */
int
osTaskCreate(const char *name, int prio, proc_t fun,
			 int p1, int p2, int p3, int p4, int p5)
{
	char    buf[32];
	static int no = 1;
	int     tid;

	if (name && *name)
		strcpy(buf, name);
	else
		sprintf(buf, "tTask%d", no++);
	if (!prio)
		prio = 100;
	tid = taskSpawn(buf, prio, __os_task_flags, os_stack_size,
					(FUNCPTR) fun, p1, p2, p3, p4, p5, 0, 0, 0, 0, 0);
	return (tid == ERROR ? 0 : tid);
}


/*
 * Kill a task.
 */
oret_t
osTaskDelete(int tid)
{
	if (tid == 0 || tid == ERROR)
		return ERROR;
	return taskDelete(tid);
}


/*
 * Return numeric task identifier given its name.
 */
int
osTaskId(const char *name)
{
	int     tid;

	if (!name || !*name)
		tid = taskIdSelf();
	else
		tid = taskNameToId((char *) name);
	if (tid == ERROR)
		tid = 0;
	return tid;
}


/*
 * Return task name given its numeric identifier.
 */
char   *
osTaskName(int tid, char *name_buf)
{
	static char my_name_buf[40];
	char   *s;

	if (!name_buf)
		name_buf = my_name_buf;
	name_buf[0] = 0;
	if (tid == 0 || tid == ERROR)
		return NULL;
	if (taskIdVerify(tid) != OK)
		return NULL;
	s = taskName(tid);
	if (s)
		strcpy(name_buf, s);
	return name_buf;
}


/*
 * .
 */
int
osTaskPrio(int tid, int new_prio)
{
	int     old_prio;

	if (tid == ERROR)
		return ERROR;
	if (taskPriorityGet(tid, &old_prio) != OK)
		return ERROR;
	if (new_prio >= 0 && new_prio != old_prio)
		taskPrioritySet(tid, new_prio);
	return old_prio;
}


/*
 * Suspend task.
 */
oret_t
osTaskSuspend(int tid)
{
	if (tid == ERROR)
		return ERROR;
	else
		return taskSuspend(tid);
}


/*
 * Resume task.
 */
oret_t
osTaskResume(int tid)
{
	if (tid == ERROR)
		return ERROR;
	else
		return taskResume(tid);
}


/*
 * Restart task.
 */
oret_t
osTaskRestart(int tid)
{
	if (tid == ERROR)
		return ERROR;
	else
		return taskRestart(tid);
}


/* --------------------- Scheduler Control ------------------------- */


/*
 * Disable (lock=true) or enable (lock=false) preemption.
 */
oret_t
osSchedLock(bool_t lock)
{
	if (lock)
		return taskLock();
	else
		return taskUnlock();
}


/*
 * Return true if called from ISR.
 */
bool_t
osIntrContext(void)
{
	return intContext();
}


/*
 * Disables interrupts.
 */
int
osIntrLock(bool_t just_emulate)
{
	return (just_emulate ? -43210 : intLock());
}


/*
 * Enables interrupts.
 */
oret_t
osIntrUnlock(int level)
{
	if (level != -43210)
		intUnlock(level);
	return OK;
}


/*
 * Reboots the current CPU/Board/System.
 */
void
osSysReboot(int wait_ms)
{
	extern void reboot(int);
	extern void sysReset(void);

	osMsSleep(wait_ms);
#if defined(USPARC)
	sysToMonitor(0);
#else
	sysReset();
#endif /* USPARC */
}


/*
 * Aborts the current script executed in the VxWorks shell.
 */
void
osShellStop(void)
{
	shellScriptAbort();
}


/* ---------------------- Critical Sections ------------------------- */


int
osMutexCreate(const char *name, bool_t check_prio, bool_t check_inversion)
{
	SEM_ID  id;
	int     options = check_prio ? SEM_Q_PRIORITY : SEM_Q_FIFO;

	if (check_inversion)
		options |= SEM_INVERSION_SAFE;
	id = semMCreate(options);
	return (int) id;
}


oret_t
osMutexDelete(int mutex)
{
	if (mutex == 0 || mutex == ERROR)
		return ERROR;
	else
		return semDelete((SEM_ID) mutex);
}


oret_t
osMutexGet(int mutex, int wait_tiks)
{
	if (mutex == 0 || mutex == ERROR)
		return ERROR;
	else
		return semTake((SEM_ID) mutex, wait_tiks);
}


oret_t
osMutexRelease(int mutex)
{
	if (mutex == 0 || mutex == ERROR)
		return ERROR;
	else
		return semGive((SEM_ID) mutex);
}


/* ---------------------------- Semaphores ----------------------------- */


int
osSemCreate(const char *name, bool_t check_prio, bool_t for_protection)
{
	SEM_ID  id = semBCreate(check_prio ? SEM_Q_PRIORITY : SEM_Q_FIFO,
							for_protection ? SEM_FULL : SEM_EMPTY);

	return (int) id;
}


oret_t
osSemDelete(int sema)
{
	if (sema == 0 || sema == ERROR)
		return ERROR;
	else
		return semDelete((SEM_ID) sema);
}


oret_t
osSemGet(int sema, int wait_tiks)
{
	if (sema == 0 || sema == ERROR)
		return ERROR;
	else
		return semTake((SEM_ID) sema, wait_tiks);
}


oret_t
osSemRelease(int sema)
{
	if (sema == 0 || sema == ERROR)
		return ERROR;
	else
		return semGive((SEM_ID) sema);
}


/* ---------------------- Counting Semaphores ------------------------- */


int
osCountCreate(const char *name, bool_t check_prio, int init_count)
{
	SEM_ID  id =
		semCCreate(check_prio ? SEM_Q_PRIORITY : SEM_Q_FIFO, init_count);
	return (int) id;
}


oret_t
osCountDelete(int count)
{
	if (count == 0 || count == ERROR)
		return ERROR;
	else
		return semDelete((SEM_ID) count);
}


oret_t
osCountGet(int count, int wait_tiks)
{
	if (count == 0 || count == ERROR)
		return ERROR;
	else
		return semTake((SEM_ID) count, wait_tiks);
}


oret_t
osCountRelease(int count)
{
	if (count == 0 || count == ERROR)
		return ERROR;
	else
		return semGive((SEM_ID) count);
}


/* -------------------------- Events ------------------------------ */


int
osEventCreate(const char *name, bool_t check_prio)
{
	SEM_ID  id =
		semBCreate(check_prio ? SEM_Q_PRIORITY : SEM_Q_FIFO, SEM_EMPTY);
	return (int) id;
}


oret_t
osEventDelete(int event)
{
	if (event == 0 || event == ERROR)
		return ERROR;
	else
		return semDelete((SEM_ID) event);
}


oret_t
osEventWait(int event, int wait_tiks)
{
	if (event == 0 || event == ERROR)
		return ERROR;
	else
		return semTake((SEM_ID) event, wait_tiks);
}


oret_t
osEventSend(int event)
{
	if (event == 0 || event == ERROR)
		return ERROR;
	else
		return semGive((SEM_ID) event);
}


oret_t
osEventPulse(int event)
{
	if (event == 0 || event == ERROR)
		return ERROR;
	else
		return semFlush((SEM_ID) event);
}


/* ------------------------- Message Queues ----------------------------- */


int
osCueCreate(const char *name, int num_elems, int elem_size, bool_t check_prio)
{
	MSG_Q_ID id = msgQCreate(num_elems, elem_size,
							 check_prio ? MSG_Q_PRIORITY : MSG_Q_FIFO);

	return (int) id;
}


oret_t
osCueDelete(int cue)
{
	if (cue == 0 || cue == ERROR)
		return ERROR;
	else
		return msgQDelete((MSG_Q_ID) cue);
}


oret_t
osCuePut(int cue, const void *elem, int elem_size, int wait_tiks)
{
	if (cue == 0 || cue == ERROR)
		return ERROR;
	else
		return msgQSend((MSG_Q_ID) cue, (char *) elem, elem_size, wait_tiks,
						MSG_PRI_NORMAL);
}


oret_t
osCueGet(int cue, void *elem, int elem_size, int wait_tiks)
{
	int     len;

	if (cue == 0 || cue == ERROR)
		return ERROR;
	len = msgQReceive((MSG_Q_ID) cue, (char *) elem, elem_size, wait_tiks);
	return (len == elem_size ? OK : ERROR);
}


int
osCueSize(int cue)
{
	if (cue == 0 || cue == ERROR)
		return ERROR;
	else
		return msgQNumMsgs((MSG_Q_ID) cue);
}


/* ----------------------- Watchdog Timers --------------------------- */


int
osWdogCreate(const char *name)
{
	return (int) wdCreate();
}


oret_t
osWdogDelete(int wd)
{
	if (wd == 0 || wd == ERROR)
		return ERROR;
	else
		return wdDelete((WDOG_ID) wd);
}


oret_t
osWdogStart(int wd, int tiks, proc_t proc, int param)
{
	if (wd == 0 || wd == ERROR || tiks <= 0 || proc == 0)
		return ERROR;
	else
		return wdStart((WDOG_ID) wd, tiks, (FUNCPTR) proc, param);
}


oret_t
osWdogStop(int wd)
{
	if (wd == 0 || wd == ERROR)
		return ERROR;
	else
		return wdCancel((WDOG_ID) wd);
}


/* ------------------------ System Information ------------------------- */

int
osGetBoardNo(void)
{
	return sysProcNumGet();
}


int
osGetClockRate(void)
{
	return sysClkRateGet();
}


int
osGetMemTop(void)
{
	return (int) sysMemTop();
}


/* ---------------------- VME related stuff ------------------------- */

/*
 * Initialize data and instruction caches for multiboard VME chassis.
 */
oret_t
osCacheSetup(void)
{
	if (!strcmp(sysModel(), "FORCE COMPUTERS SPARC CPU-5V")) {
		cacheDisable(DATA_CACHE);
		olog("cache disabled");
	} else {
		cacheEnable(DATA_CACHE);
		cacheEnable(INSTRUCTION_CACHE);
	}
	return OK;
}


/*
 * Flush (invalidate) data cache.
 */
oret_t
osMemCache(void *addr, int len, bool_t for_write)
{
	if (for_write)
		return cacheFlush(DATA_CACHE, addr, len);
	else
		return cacheInvalidate(DATA_CACHE, addr, len);
}


/*
 * Probes if memory can be accessed at given address.
 */
oret_t
osMemProbe(void *addr, int len, bool_t for_write)
{
	char    buf[16] = { 0 };
	oret_t rc;

	switch (len) {
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		return ERROR;
	}
	rc = vxMemProbe((char *) addr, (for_write ? VX_WRITE : VX_READ), len, buf);
	return rc;
}

bool_t  os_memprobe_implemented = TRUE;


/*
 * Translates local address into VME address, so that external
 * VME devices can access local memory.
 */

#define	TUNDRA_AM_IS_A16(am)	(((am) & 0xf0) == 0x20)
#define	TUNDRA_AM_IS_A24(am)	(((am) & 0xf0) == 0x30)
#define	TUNDRA_AM_IS_A32(am)	(((am) & 0xf0) == 0x00)

int
osMem2Vme(int space, int local)
{
	long    vme = -1;

	if (sysLocalToBusAdrs(space, (char *) (ulong_t) local, (char **) &vme))
		return -1;
	if (TUNDRA_AM_IS_A24(space))
		vme &= 0x00ffffff;
	else if (TUNDRA_AM_IS_A16(space))
		vme &= 0x0000ffff;
	return (int) vme;
}


/*
 * Translates VME address into local address, so that our CPU
 * can access external VME devices.
 */

int     __os_debug_vme2mem = 0;

int
osVme2Mem(int space, int vme, int size_kb)
{
	int     ret;
	char   *local;
	int     slave_addr;
	unsigned vme_addr = vme;
	unsigned local_addr, slave_local, slave_start, slave_end;
	unsigned size_limit, slave_size;

	/* First try to find the board slave window in this address space.
	 * Inofficial rule for RSC-made BSPs states that our SPARC boards
	 * can export memory start (address 0) or sysMemTop() to VME. */
	slave_addr = osMem2Vme(space, (slave_local = 0));
	if (slave_addr == ERROR)
		slave_addr = osMem2Vme(space, (slave_local = osGetMemTop()));
	if (slave_addr != ERROR) {
		/* OK, now find the range exported */
		vme_addr = vme;
		slave_start = slave_addr;
		slave_size = osGetMemTop();
		size_limit = 0x20000000;
		if (TUNDRA_AM_IS_A16(space)) {
			size_limit = 0x10000;
		} else if (TUNDRA_AM_IS_A24(space)) {
#if defined(USPARC)
			size_limit = 0x01000000;
#else /* !USPARC */
			/* On some SPARC boards, eg. Themis 8/64, sysLocalToBusAdrs is broken.
			 * It does not keep track of the real size of memory exported.
			 * So we try to limit probed sizes by some reasonable value.
			 */
			size_limit = 0x00100000;
#endif /* USPARC */
		}
		if (slave_size > size_limit)
			slave_size = size_limit;
		while (slave_size > 0) {
			slave_end = slave_local + slave_size;
			ret = osMem2Vme(space, slave_end);
			if (__os_debug_vme2mem > 3)
				printf
					("vme2mem: trial, spc=%02Xh start=0x%x local=0x%x size=0x%x ret=%d\n",
					 space, slave_local, slave_end, slave_size, ret);
			if (ret != ERROR)
				break;
			if (slave_size >= 0x10000000)
				slave_size /= 2;
			else {
				slave_size *= 3;
				slave_size /= 4;
			}
		}
		if (__os_debug_vme2mem > 1)
			printf
				("vme2mem: PCI->VME spc=%02Xh local=0x%x vme=0x%x size=0x%x\n",
				 space, slave_local, slave_start, slave_size);
		slave_end = slave_start + slave_size;
		if (slave_size > 0) {
			if (vme_addr >= slave_start && vme_addr < slave_end) {
				/* OK, our VME address lies within borders. translate. */
				local_addr = vme_addr - slave_start + slave_local;
				__os_addr_is_local = TRUE;
				return (int) local_addr;
			}
		}
	}
	/* now find the address amongst master VME windows */
	__os_addr_is_local = FALSE;
	if (__os_vme_size_ptr == NO_SIZE_VAR)
		__os_vme_size_ptr =
			(ulong_t *) osGetSym(".", "sysBusMapSize", GETSYM_VAR);
	if (__os_vme_size_ptr != 0) {
		taskLock();
		*__os_vme_size_ptr = size_kb * 1024ul;
	}
	ret = sysBusToLocalAdrs(space, (char *) vme, &local);
	if (__os_debug_vme2mem > 2)
		printf("vme2mem: not a local address 0x%x spc=%Xh ret=%d\n",
			   vme, space, ret);
	if (__os_vme_size_ptr != 0) {
		*__os_vme_size_ptr = 0;
		taskUnlock();
	}
	if (ret)
		return -1;
	else
		return (int) local;
}


/*
 * Connects interrupt service routine to the VME interrupt vector.
 */
oret_t
osSetVmeVect(int vect, proc_t proc, int param)
{
	return intConnect((VOIDFUNCPTR *) INUM_TO_IVEC(vect), (VOIDFUNCPTR) proc,
					  param);
}


/*
 * Enables or disables VME interrupts.
 */
int
osSetVmeIrq(int irq, bool_t enable)
{
	if (irq < 0 || irq >= MAX_OS_IRQ)
		return -3;
	if (enable) {
		/* Enable */
		if (__os_irq_count[irq] > 99)
			return -2;
		__os_irq_count[irq]++;
		if (__os_irq_count[irq] == 1) {
			if (__os_ban_all_irq || __os_ban_irq[irq])
				return OK;
			else
				return sysIntEnable(irq);
		}
	} else {
		/* Disable */
		if (__os_irq_count[irq] < 1)
			return -2;
		__os_irq_count[irq]--;
		if (__os_irq_count[irq] == 0) {
			if (__os_ban_all_irq || __os_ban_irq[irq])
				return OK;
			else
				return sysIntDisable(irq);
		}
	}
	return 1;
}


/*
 * Generate VME interrupts.
 */
oret_t
osSendVmeIntr(int irq, int vect)
{
	int     rc;

	rc = sysBusIntGen(irq, vect);
	if (rc == 0)
		__os_send_intr_ok[irq]++;
	else
		__os_send_intr_err[irq]++;
	return rc;
}
