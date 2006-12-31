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

  Internal declarations for the front-end client API.

*/

#ifndef OPTIKUS_WATCH_PRIV_H
#define OPTIKUS_WATCH_PRIV_H

#include <optikus/watch.h>
#include <optikus/log.h>
#include <optikus/lang.h>

OPTIKUS_BEGIN_C_DECLS


#define OWATCH_MAX_NAME 40

extern int     owatch_verb;
extern bool_t  owatch_initialized;

#define owatchLog(verb,args...)		(owatch_verb >= (verb) ? olog(args) : -1)


/* ======== operations ======== */

#define OWATCH_MAX_OP_HANDLERS   16		/* FIXME: use lists and avoid this limit */
#define OWATCH_MAX_OPS           8000	/* FIXME: use lists and avoid this limit */
#define OWATCH_POLL_INTERVAL_MS  50
#define OWATCH_MAX_IDLE_HANDLERS 8		/* ditto */

#define OWATCH_OPS_FREE			0
#define OWATCH_OPS_PENDING		1
#define OWATCH_OPS_DONE			2

#define OWATCH_OPT_CONNECT		1
#define OWATCH_OPT_DESC_INFO	2
#define OWATCH_OPT_OOID_INFO	3
#define OWATCH_OPT_DESC_MON		4
#define OWATCH_OPT_OOID_MON		5
#define OWATCH_OPT_WAIT_MON		6
#define OWATCH_OPT_SEND			7
#define OWATCH_OPT_WRITE		8
#define OWATCH_OPT_READ			9
#define OWATCH_OPT_TRIGGER		10
#define OWATCH_OPT_COMBO_MON	11
#define OWATCH_OPT_MSG_SEND		12
#define OWATCH_OPT_MSG_RECV		13
#define OWATCH_OPT_MSG_CLASS	14

#define OWOP_NULL  (-3)
#define OWOP_SELF  (-4)


typedef int (*OwatchOpCanceller_t) (owop_t op, long data1, long data2);
typedef int (*OwatchOpChainer_t) (owop_t mop, owop_t sop, int err_code,
								long param1, long param2);

typedef struct
{
	owop_t  op;					/* FIXME: fields are not explained */
	int     op_state;			/* OWATCH_OPS_* */
	int     err_code;
	long    data1;
	long    data2;
	OwatchOpCanceller_t canceller;
	/* for chaining */
	owop_t  m_op;
	owop_t  s_op;
	OwatchOpChainer_t chainer;
	long    ch_param1;
	long    ch_param2;
	OwatchOpHandler_t loc_handler;
	long    loc_param;
} OwatchOpRecord_t;


oret_t  owatchInitOperations(void);
oret_t  owatchExitOperations(void);
owop_t  owatchInternalWaitOp(owop_t op);
owop_t  owatchAllocateOp(OwatchOpCanceller_t pcanc, long data1, long data2);
owop_t  owatchReuseSlaveOp(owop_t op, OwatchOpCanceller_t pcanc,
							long data1, long data2);
oret_t  owatchDeallocateOp(owop_t op);
oret_t  owatchSetOpData(owop_t op, long data1, long data2);
oret_t  owatchGetOpData(owop_t op, long *pdata1, long *pdata2);
owop_t  owatchFinalizeOp(owop_t op, int err_code);
bool_t  owatchIsValidOp(owop_t op);
OwatchOpRecord_t *owatchGetOpRecord(owop_t op);
oret_t  owatchChainOps(owop_t m_op, owop_t s_op, OwatchOpChainer_t chainer,
						long param1, long param2);
oret_t  owatchUnchainOp(owop_t m_op);
int     owatchFreeOpCount(void);
oret_t  owatchCallIdleHandler(void);

extern bool_t owatch_handlers_are_lazy;

oret_t  owatchRecordLazyCall(int type, long param);
int     owatchPerformLazyCalls(void);

/*					network						*/

oret_t  owatchHubHandleIncoming(int kind, int type, int len, char *buf);
int     owatchInternalPoll(long timeout);
oret_t  owatchSendPacket(int kind, int type, int len, void *data);
owop_t  owatchSecureSend(int kind, int type, int len, void *data);
bool_t  owatchIsConnected(void);

/*					Node state					*/

#define OWATCH_MAX_ALIVE_HANDLERS  16

oret_t  owatchInitSubjects(void);
oret_t  owatchExitSubjects(void);
oret_t  owatchHandleSubjects(int kind, int type, int len, char *buf);
oret_t  owatchSetSubjectState(const char *name, const char *state,
							  char availability);
oret_t  owatchClearTriggers(void);
oret_t  owatchCancelAllTriggers(void);
oret_t  owatchHandleTriggerReply(int kind, int type, int len, char *buf);
oret_t  owatchHandleTriggerTimeouts(void);
void    owatchCallAliveHandlers(long p);

/*					Quarks						*/

oret_t  owatchFlushInfoCache(void);
oret_t  owatchUpdateInfoCache(const char *desc, owquark_t * quark_p);
oret_t  owatchHandleGetInfoReply(int kind, int type, int len, char *buf);
owop_t  owatchNowaitGetInfoByDesc(const char *desc, owquark_t * quark_p);
owop_t  owatchNowaitGetInfoByOoid(ooid_t ooid, owquark_t * quark_p);
oret_t  owatchFindCachedInfoByDesc(const char *desc,
									owquark_t * quark_p);
oret_t  owatchFindCachedInfoByOoid(ulong_t ooid, owquark_t * quark_p);
oret_t  owatchUnpackInfoReply(char *buf, int *p_ptr, owquark_t * quark_p,
								char *ermes, int ermes_len);

/*					Monitoring						*/

#define OWATCH_MAX_DATA_HANDLERS  8
#define OWATCH_MAX_OP_PER_MONITOR 4

struct OwatchMonitorRecord_st;

oret_t  owatchInitMonitoring(void);
oret_t  owatchExitMonitoring(void);
oret_t  owatchHandleMonitoring(int kind, int type, int len, char *buf);
oret_t  owatchHandleRegistration(int kind, int type, int len, char *buf);
oret_t  owatchHandleMonComboReply(int kind, int type, int len, char *buf);
int     owatchMonSecureStart(void);
int     owatchMonSecureMore(void);
struct OwatchMonitorRecord_st *owatchFindMonitorByOoid(ooid_t ooid);
void    owatchCallDataHandlers(long p);

/*					Data R/W						*/

oret_t  owatchHandleWriteReply(int kind, int type, int len, char *buf);
owop_t  owatchNowaitWriteByAny(const char *a_desc, ooid_t an_ooid,
								const oval_t * pval);
oret_t  owatchHandleReadReply(int kind, int type, int len, char *buf);
owop_t  owatchNowaitReadByAny(const char *a_desc, ooid_t an_ooid,
								oval_t * pvalue, char *data_buf, int buf_len);
oret_t  owatchFastFormsWrite(const char *desc, ooid_t ooid, char type,
							int timeout, char *data, int size);

/*					Conversions						*/

oret_t  owatchCopyValue(const oval_t * src, oval_t * dst,
						char *buf, int buf_len);
int     owatchGetValueLength(const oval_t * pval);
int     owatchLazyDataHandlers(void);

/*					Proper						*/

extern char owatch_watcher_desc[];

oret_t  owatchHandleIncoming(int kind, int type, int len, char *buf);

/*					Messaging					*/

oret_t  owatchSetupMessages(void);
oret_t  owatchClearMessages(void);
oret_t  owatchHandleMsgRecv(int kind, int type, int len, char *buf);
oret_t  owatchHandleMsgSendReply(int kind, int type, int len, char *buf);
oret_t  owatchMsgClassRegistration(bool_t enable);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_WATCH_PRIV_H */
