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

  Optikus fron-end client API.

*/

#ifndef OPTIKUS_WATCH_H
#define OPTIKUS_WATCH_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


typedef union
{
	char    v_char;			/* 'b' */
	uchar_t v_uchar;		/* 'B' */
	short   v_short;		/* 'h' */
	ushort_t v_ushort;		/* 'H' */
	int     v_int;			/* 'i' */
	uint_t  v_uint;			/* 'I' */
	long    v_long;			/* 'l' */
	ulong_t v_ulong;		/* 'L' */
	int     v_bool;			/* '?' */
	int     v_enum;			/* 'E' */
	float   v_float;		/* 'f' */
	double  v_double;		/* 'd' */
	char   *v_str;			/* 's' */
	void   *v_ptr;			/* '?' */
	ulong_t v_proc;			/* 'p' */
} ovariant_t;

typedef struct
{
	char    type;
	char    undef;
	short   len;
	long    time;
	ovariant_t v;
} oval_t;

#define OWATCH_MAX_PATH 140

typedef struct
{
	ooid_t  ooid;
	char    ptr;
	char    type;
	char    seg;
	char    spare1;
	int     len;
	int     bit_off;
	int     bit_len;
	char    desc[OWATCH_MAX_PATH];
} owquark_t;

typedef int owop_t;

#define OWOP_ERROR				(-1)
#define OWOP_OK					(0)
#define OWOP_ALL				(-2)

#define OWATCH_NOWAIT			(0)
#define OWATCH_FOREVER			(-1)

#define OWATCH_ERR_OK			(0)
#define OWATCH_ERR_PENDING		(1)
#define OWATCH_ERR_ERROR		(-1)
#define OWATCH_ERR_INTERNAL		(-10)
#define OWATCH_ERR_CANCELLED	(-11)
#define OWATCH_ERR_FREEOP		(-12)
#define OWATCH_ERR_NETWORK		(-13)
#define OWATCH_ERR_REFUSED		(-14)
#define OWATCH_ERR_SCREWED		(-15)
#define OWATCH_ERR_INVALID		(-16)
#define OWATCH_ERR_SMALL_DESC	(-17)
#define OWATCH_ERR_SMALL_DATA	(-18)
#define OWATCH_ERR_TOOLATE		(-19)
#define OWATCH_ERR_NOSPACE		(-20)
#define OWATCH_ERR_NODATA		(-21)
#define OWATCH_ERR_NOINFO		(-22)
#define OWATCH_ERR_NOTFOUND		(-23)
#define OWATCH_ERR_MSG_TRUNC	(-24)
#define OWATCH_ERR_RCVR_OFF		(-25)
#define OWATCH_ERR_RCVR_FULL	(-26)

typedef int (*OwatchDataHandler_t)(long param, ooid_t ooid,
									const oval_t * pvalue);
typedef int (*OwatchOpHandler_t)(long param, owop_t op, int err_code);
typedef int (*OwatchAliveHandler_t)(long param, const char *subject,
									const char *state);
typedef void (*OwatchIdleHandler_t)(void);

typedef int (*OwatchMsgHandler_t)(ooid_t id, const char *src, const char *dest,
						int klass, int type, void *data, int len, long param);

extern int owatch_verb;

typedef enum
{
	OWATCH_ROPT_NEVER = 0,
	OWATCH_ROPT_ALWAYS = 1,
	OWATCH_ROPT_AUTO = 2,
	OWATCH_ROPT_GET = 3,
	OWATCH_ROPT_ERROR = -1
} OwatchReadOpt_t;


oret_t  owatchInit(const char *client_name);
oret_t  owatchExit(void);
oret_t  owatchGetFileHandles(int *file_handle_buf, int *buf_size);
oret_t  owatchWork(int timeout_msec);
oret_t  owatchSetupConnection (const char *hub_host, const char *client_name,
								int conn_timeout, int *err_ptr);

int     owatchGetAsyncTimeout(void);
oret_t  owatchSetAsyncTimeout(int timeout_msec);
oret_t  owatchWaitOp(owop_t op, int timeout_msec, int *err_code);
oret_t  owatchCancelOp(owop_t op);
oret_t  owatchDetachOp(owop_t op);
oret_t  owatchAddOpHandler(const char *filter,
						   OwatchOpHandler_t phand, long param);
oret_t  owatchRemoveOpHandler(OwatchOpHandler_t phand);
oret_t  owatchLocalOpHandler(owop_t op, OwatchOpHandler_t phand, long param);
const char *owatchErrorString(int err_code);
oret_t  owatchRegisterIdleHandler(OwatchIdleHandler_t handler, bool_t enable);

owop_t  owatchConnect(const char *hub_url);
oret_t  owatchDisconnect(void);
oret_t  owatchGetSubjectList(char *list_buffer, int buf_len);
const char *owatchGetSubjectState(const char *subject_name);
bool_t  owatchGetAlive(const char *subject_name);
oret_t  owatchAddAliveHandler(OwatchAliveHandler_t phand, long param);
oret_t  owatchRemoveAliveHandler(OwatchAliveHandler_t phand);
owop_t  owatchTriggerSubject(const char *subject);

owop_t  owatchAddMonitorByDesc(const char *desc, ooid_t * ooid_buf,
							  bool_t dont_monitor);
owop_t  owatchAddMonitorByOoid(ooid_t ooid);
ooid_t  owatchFindMonitorOoidByDesc(const char *desc);
oret_t  owatchRemoveMonitor(ooid_t ooid);
oret_t  owatchAddDataHandler(const char *filter, OwatchDataHandler_t phand,
							 long param);
oret_t  owatchRemoveDataHandler(OwatchDataHandler_t phand);
oret_t  owatchSetMonitorData(ooid_t ooid, long user_data);
oret_t  owatchGetMonitorData(ooid_t ooid, long *user_data_p);

oret_t  owatchEnableMonitoring(bool_t enable);
oret_t  owatchBlockMonitoring(bool_t block, bool_t renew_if_unblock);
oret_t  owatchRemoveAllMonitors(void);
oret_t  owatchFlushMonitor(ooid_t ooid);
oret_t  owatchFlushAllMonitors(void);
owop_t  owatchRenewMonitor(ooid_t ooid);
oret_t  owatchRenewAllMonitors(void);

owop_t  owatchGetInfoByDesc(const char *desc, owquark_t * info_ptr);
owop_t  owatchGetInfoByOoid(ooid_t ooid, owquark_t * info_ptr);
oret_t  owatchGetValue(ooid_t ooid, oval_t * pvalue, char *data_buf,
					   int buf_len);

oret_t  owatchConvertValue(const oval_t * src_val, oval_t * dst_val,
						   char *data_buf, int data_buf_len);

owop_t  owatchRead(ooid_t ooid, oval_t * pvalue, char *data_buf,
				  int data_buf_len);
owop_t  owatchReadByName(const char *desc, oval_t * pvalue, char *data_buf,
						int data_buf_len);
OwatchReadOpt_t owatchOptimizeReading(const char *desc,
								OwatchReadOpt_t optimization, int timeout);

bool_t  owatchGetAutoCommit(void);
oret_t  owatchSetAutoCommit(bool_t enable_auto_commit);
owop_t  owatchCommit(void);
owop_t  owatchCommitAt(long vtu_time);
long    owatchGetTime(void);

owop_t  owatchWrite(ooid_t ooid, const oval_t * value);
owop_t  owatchWriteByName(const char *desc, const oval_t * pvalue);

long    owatchReadAsLong(const char *desc, int timeout, int *err_ptr);
float   owatchReadAsFloat(const char *desc, int timeout, int *err_ptr);
double  owatchReadAsDouble(const char *desc, int timeout, int *err_ptr);
char  * owatchReadAsString(const char *desc, int timeout, int *err_ptr);
oret_t  owatchWriteAsLong(const char *desc, long value,
							int timeout, int *err_ptr);
oret_t  owatchWriteAsFloat(const char *desc, float value,
							int timeout, int *err_ptr);
oret_t  owatchWriteAsDouble(const char *desc, double value,
							int timeout, int *err_ptr);
oret_t  owatchWriteAsString(const char *desc, const char *value,
							int timeout, int *err_ptr);

oret_t  owatchAddMsgHandler(int klass, OwatchMsgHandler_t phand, long param);
oret_t  owatchRemoveMsgHandler(int klass, OwatchMsgHandler_t phand);
owop_t  owatchSendMessage(ooid_t id, const char *dest, int klass, int type,
							void *data, int len);
owop_t  owatchReceiveMessage(ooid_t *id_p, char *src_p, char *dest_p,
						int *klass_p, int *type_p, void *data_buf, int *len_p);
oret_t  owatchSetMsgQueueLimits(int max_messages, int max_bytes);
owop_t  owatchSendCtlMsg(const char *dest, int klass, int type,
						const char *format, ...);
int     owatchGetAckCount(bool_t flush);
bool_t  owatchRemoteLog(bool_t enable);

OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_WATCH_H */
