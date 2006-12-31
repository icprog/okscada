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

  OS compatibility layer.

*/

#ifndef OPTIKUS_CONF_OS_H
#define OPTIKUS_CONF_OS_H

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


/*				object symbol manipulation				*/

#define GETSYM_ANY   0
#define GETSYM_VAR   1
#define GETSYM_FUN   2

void   *osGetSym(const char *symname, const char *name, int what);
oret_t  osSetJump(const char *symname, const char *from, const char *to,
				   unsigned **saveptr);
oret_t  osUnsetJump(const char *symname, const char *from, unsigned *savebuf);

/*			task/thread/process control					*/

int     osTaskCreate(const char *name, int prio, proc_t fun,
					 int p1, int p2, int p3, int p4, int p5);
int     osTaskDelete(int tid);
int     osTaskId(const char *name);
char   *osTaskName(int tid, char *name_buf);
int     osTaskPrio(int tid, int new_prio);
oret_t  osTaskSuspend(int tid);
oret_t  osTaskResume(int tid);
oret_t  osTaskRestart(int tid);

/*					scheduler control					*/

oret_t  osSchedLock(bool_t lock);
bool_t  osIntrContext(void);
int     osIntrLock(bool_t just_emulate);
oret_t  osIntrUnlock(int level);
void    osSysReboot(int wait_ms);
void    osShellStop(void);

/*						Mutexes							*/

int     osMutexCreate(const char *name, bool_t check_prio,
					  bool_t check_inversion);
oret_t  osMutexDelete(int mutex);
oret_t  osMutexGet(int mutex, int wait_tiks);
oret_t  osMutexRelease(int mutex);

/*					Binary Semaphores					*/

int     osSemCreate(const char *name, bool_t check_prio, bool_t for_protection);
oret_t  osSemDelete(int sema);
oret_t  osSemGet(int sema, int wait_tiks);
oret_t  osSemRelease(int sema);

/*					Counting Semaphores					*/

int     osCountCreate(const char *name, bool_t check_prio, int init_count);
oret_t  osCountDelete(int count);
oret_t  osCountGet(int count, int wait_tiks);
oret_t  osCountRelease(int count);

/*						Events							*/

int     osEventCreate(const char *name, bool_t check_prio);
oret_t  osEventDelete(int event);
oret_t  osEventWait(int event, int wait_tiks);
oret_t  osEventSend(int event);
oret_t  osEventPulse(int event);

/*						OS queues						*/

int     osCueCreate(const char *name, int num_elems, int elem_size,
					bool_t check_prio);
oret_t  osCueDelete(int cue);
oret_t  osCuePut(int cue, const void *elem, int elem_size, int wait_tiks);
oret_t  osCueGet(int cue, void *elem, int elem_size, int wait_tiks);
int     osCueSize(int cue);

#define osTCueCreate(name,type,len,prio)  osCueCreate((name),(len),sizeof(type),(prio))
#define osTCueDelete(cue)                 osCueDelete(cue)
#define osTCuePut(cue,elem,tiks)          osCuePut((cue),(const void*)(&(elem)),sizeof(elem),(tiks))
#define osTCueGet(cue,elem,tiks)          osCueGet((cue),(void*)(&(elem)),sizeof(elem),(tiks))
#define osTCueSize(cue)                   osCueSize(cue)

/*						Watchdogs						*/

int     osWdogCreate(const char *name);
oret_t  osWdogDelete(int wd);
oret_t  osWdogStart(int wd, int tiks, proc_t proc, int param);
oret_t  osWdogStop(int wd);

/*						OS state info					*/

int     osGetBoardNo(void);
int     osGetClockRate(void);
int     osGetMemTop(void);
int     osKeyHit(void);

/*						VME API							*/

#define VME_AM_SUP_A24   0x3d
#define VME_AM_SUP_A32   0x0d
#define VME_AM_SUP_A16   0x2d

oret_t  osCacheSetup(void);
oret_t  osMemProbe(void *addr, int len, bool_t for_write);
oret_t  osMemCache(void *addr, int len, bool_t for_write);

int     osMem2Vme(int space, int local);
int     osVme2Mem(int space, int vme, int size_kb);
oret_t  osSetVmeVect(int vect, proc_t proc, int param);
int     osSetVmeIrq(int irq, bool_t enable);
oret_t  osSendVmeIntr(int irq, int vec);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_CONF_OS_H */
