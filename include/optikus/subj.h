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

  API of the optikus back-end agent.

*/

#ifndef OPTIKUS_SUBJ_H
#define OPTIKUS_SUBJ_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


extern int osubj_verb;

typedef int (*OsubjCallback_t) (long param);
typedef int (*OsubjMsgHandler_t)(ooid_t id, const char *src, const char *dest,
								int klass, int type, void *data, int len);

#define OSUBJ_WORK_DATA			0x01
#define OSUBJ_WORK_COMMAND		0x02
#define OSUBJ_WORK_CONTROL		0x04
#define OSUBJ_WORK_MONITOR		0x08
#define OSUBJ_WORK_MESSAGE		0x10
#define OSUBJ_WORK_HIGH_PRIO	(OSUBJ_WORK_DATA|OSUBJ_WORK_COMMAND|OSUBJ_WORK_MESSAGE)
#define OSUBJ_WORK_LOW_PRIO		(OSUBJ_WORK_CONTROL)
#define OSUBJ_WORK_NETWORK		(OSUBJ_WORK_HIGH_PRIO|OSUBJ_WORK_LOW_PRIO)
#define OSUBJ_WORK_ALL			(OSUBJ_WORK_NETWORK|OSUBJ_WORK_MONITOR)

#define OSUBJ_NOWAIT     (0)
#define OSUBJ_FOREVER    (-1)

oret_t  osubjInit(int serv_port, int task_mask,
					ulong_t al_addr, uint_t al_size,
					OsubjCallback_t callback_p, OsubjMsgHandler_t phand);
oret_t  osubjExit(void);
oret_t  osubjAddSubject(const char *subject_name, const char *symtable_name);
long     osubjAddress(const char *subject, const char *global);
bool_t   osubjIsConnected(void);
oret_t  osubjWork(int timeout, int work_type);
oret_t  osubjMonitoring(void);
long     osubjSleep(long msec);
oret_t  osubjEverWrite(void *adr, bool_t enable);
oret_t  osubjEnableMsgClass(int klass, bool_t enable);
oret_t  osubjSendMessage(ooid_t id, const char *dest, int klass, int type,
							void *data, int len);
int      osubjLog(const char *fmt, ...);
bool_t   osubjRemoteLog(bool_t enable);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_SUBJ_H */
