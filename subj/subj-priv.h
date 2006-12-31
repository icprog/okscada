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

  Internal declarations for the agent API.

*/

#ifndef OPTIKUS_SUBJ_PRIV_H
#define OPTIKUS_SUBJ_PRIV_H

#include <optikus/subj.h>
#include <optikus/log.h>
#include <optikus/lang.h>

OPTIKUS_BEGIN_C_DECLS


#define OSUBJ_MAX_CLASS			64
#define OSUBJ_MAX_NAME			40
#define OSUBJ_MAX_EVER_WRITE	32


typedef struct
{
	char    segment_name;
	long    start_addr;
} OsubjSegment_t;


typedef struct
{
	char    module_name[OSUBJ_MAX_NAME];
	int     segment_qty;
	OsubjSegment_t *segments;
} OsubjModule_t;


typedef struct
{
	char    subject_name[OSUBJ_MAX_NAME];
	char    symtable_name[OSUBJ_MAX_NAME];
	OsubjModule_t *modules;
	int     module_qty;
} OsubjSubject_t;


extern OsubjSubject_t *osubj_subjects;
extern int osubj_subject_qty;

extern int osubj_spin;
extern bool_t osubj_aborted;
extern bool_t osubj_initialized;

extern void *osubj_ever_write[OSUBJ_MAX_EVER_WRITE];
extern bool_t osubj_ban_write;


#define OSUBJ_THREADING	1
#define OSUBJ_LOGGING	1

#if   OSUBJ_LOGGING == 0
#define osubjLog(...)
#elif OSUBJ_LOGGING == 1
#include <optikus/log.h>
#define osubjLog           olog
#elif OSUBJ_LOGGING == 2
#define osubjLog           printf
#elif OSUBJ_LOGGING == 3
/* use the osubjLog() routine */
#else
#error unknown logging type
#endif

#define osubjLogAt(verb,args...)   (osubj_verb >= (verb) ? osubjLog(args) : -1)

#define osubjCopy(s,d,n)  ((void)((n)>0?memcpy((void*)(d),(void*)(s),(n)):0))
#define osubjZero(b,n)    ((void)((n)>0?memset((void*)(b),0,(n)):0))

#define osubjRecCopy(s,d) osubjCopy((s),(d),sizeof(*(s)))
#define osubjRecZero(p)   osubjZero((p),sizeof(*(p)))
#define osubjArrZero(p)   osubjZero((p),sizeof(p))

#define osubjNew(type,num,id)    ((type *)osubjAlloc(sizeof(type),(num),(id)))


void *  osubjAlloc(int size, int nelem, int id);
void    osubjFree(void *ptr, int id);
void    osubjAbort(const char *msg);
long    osubjClock(void);
oret_t  osubjInitNetwork(int serv_port);
oret_t  osubjExitNetwork(void);
int     osubjInternalPoll(long timeout, int work_type);
oret_t  osubjHandleIncoming(int kind, int type, int len, char *buf);
oret_t  osubjSendPacket(int id, int kind, int type, int len, void *data);
oret_t  osubjSendSegments(bool_t initial);
oret_t  osubjHandleDataReg(int kind, int type, int len, char *buf);
oret_t  osubjHandleDataUnreg(int kind, int type, int len, char *buf);
oret_t  osubjHandleDataGroupUnreg(int kind, int type, int len, char *buf);
oret_t  osubjClearMonitoring(void);
oret_t  osubjHandleDataWrite(int kind, int type, int len, char *buf);
oret_t  osubjHandleDataRead(int kind, int type, int len, char *buf);
oret_t  osubjAddDataMonReply(int type, ulong_t ooid);
oret_t  osubjHandleMsgRecv(int kind, int type, int len, char *buf);
oret_t  osubjInitMessages(OsubjMsgHandler_t phandler);
oret_t  osubjClearMessages(void);
oret_t  osubjHandleMsgRecv(int kind, int type, int len, char *buf);
oret_t  osubjSendClasses(void);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_SUBJ_PRIV_H */
