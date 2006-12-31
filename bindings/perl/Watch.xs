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

  Perl binding for Optikus.

*/

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#if PERL_IS_5_8_PLUS
#include "ppport.h"
#endif

#include "optikus-perl.h"
#include <optikus/util.h>
#include <optikus/log.h>
#include <optikus/watch.h>
#include <optikus/conf-mem.h>	/* for oxnew,oxstrdup,oxvcopy,... */

#include <pthread.h>			/* for pthread_mutex* */


#define WATCH_DEF_TIMEOUT	1000
#define WATCH_DEF_MSG_DEST	"sample"


#define SET_SUB(to,from)		\
		if (!from)				\
			to = 0;				\
		else if (to == (SV*)0)	\
			to = newSVsv(from);	\
		else					\
			SvSetSV (to, from);


typedef struct WatchEvent_st
{
	struct WatchEvent_st *next;
	int type; /* 1 = alive, 2 = monitor */
	union {
		/* for alive */
		struct {
			long param;
			char *name;
			char *state;
		} alive;
		/* for data */
		struct {
			long    param;
			ooid_t ooid;
			oval_t val;
			char   *buf;
		} data;
	} v;
} WatchEvent_t;


#define _watchLog(verb,args...)		(_watch_verb >= (verb) ? olog(args) : -1)

static int _watch_verb;

extern bool_t owatch_handlers_are_lazy;

static WatchEvent_t *_watch_first_event;
static WatchEvent_t *_watch_last_event;

static bool_t  _watch_handlers_are_lazy;

static SV *_watch_alive_sub;
static SV *_watch_data_sub;
static SV *_watch_idle_sub;
static bool_t _watch_idle_enable;

static int _watch_def_timeout;
static char *_watch_def_msg_dest;

static bool_t _watch_inited;
static pthread_mutex_t _watch_mutex;


/*
 * .
 */
static void
_callAliveHandler (const char *name, const char *state)
{
	if (_watch_alive_sub) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP) ;
		XPUSHs(sv_2mortal(newSVpv(name, 0)));
		XPUSHs(sv_2mortal(newSVpv(state, 0)));
		PUTBACK;
		call_sv(_watch_alive_sub, G_DISCARD);
		FREETMPS;
		LEAVE;
	}
}


static void
_callDataHandler (ooid_t ooid, const oval_t *pval)
{
	long mon_data;
	char type_str[2];
	owquark_t info;

	if (_watch_data_sub) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		owatchGetMonitorData(ooid, &mon_data);
		XPUSHs(sv_2mortal(newSViv(mon_data)));
		PUSH_WATCH_VAL (pval);
		XPUSHs(sv_2mortal(newSViv(pval->undef)));
		XPUSHs(sv_2mortal(newSViv(ooid)));
		owatchGetInfoByOoid(ooid, &info);
		XPUSHs(sv_2mortal(newSVpv(info.desc, 0)));
		type_str[0] = pval->type;
		type_str[1] = 0;
		XPUSHs(sv_2mortal(newSVpv(type_str, 0)));
		XPUSHs(sv_2mortal(newSViv(pval->len)));
		PUTBACK;
		call_sv(_watch_data_sub, G_DISCARD);
		FREETMPS;
		LEAVE;
	}
}


int
_watchLazyHandleEvents (bool_t really_work)
{
	WatchEvent_t *event;
	int count = 0;
	while (_watch_first_event) {
		event = _watch_first_event;
		if (event->type == 1) {
			/* alive */
			if (_watch_alive_sub && really_work)  {
				_watchLog(6, "calling alive sub %p", _watch_alive_sub);
				_callAliveHandler(event->v.alive.name, event->v.alive.state);
			}
			oxfree(event->v.alive.name);
			oxfree(event->v.alive.state);
		}
		if (event->type == 2) {
			/* data */
			if (_watch_data_sub && really_work)  {
				_watchLog(6, "calling data sub %p", _watch_data_sub);
				_callDataHandler(event->v.data.ooid, &event->v.data.val);
			}
			oxfree(event->v.data.buf);
		}
		_watch_first_event = event->next;
		oxvzero(event);
		oxfree(event);
		count++;
	}
	_watch_first_event = _watch_last_event = 0;
	return count;
}


static int
_watchAliveHandler (long param, const char *name, const char *state)
{
	static const char *me = "watchAliveHandler";
	WatchEvent_t *event;

	_watchLog(4, "%s: subject=[%s] state=[%s]", me, name, state);
	if (!_watch_handlers_are_lazy) {
		_callAliveHandler(name, state);
		return 0;
	}
	event = oxnew(WatchEvent_t, 1);
	event->type = 1;
	event->v.alive.param = param;
	event->v.alive.name = oxstrdup(name);
	event->v.alive.state = oxstrdup(state);
	/* add to the end of linked list */
	if (_watch_last_event)
		_watch_last_event->next = event;
	_watch_last_event = event;
	if (!_watch_first_event)
		_watch_first_event = event;
	return 0;
}


static int
_watchDataHandler (long param, ooid_t ooid, const oval_t *pval)
{
	static const char *me = "watchDataHandler";
	WatchEvent_t *event;
	_watchLog(4, "%s: ooid=%ld type='%c' len=%d ",
				me, ooid, pval->type, pval->len);
	if (!_watch_handlers_are_lazy) {
		_callDataHandler(ooid, pval);
		return 0;
	}
	event = oxnew(WatchEvent_t, 1);
	event->type = 2;
	event->v.data.param = param;
	event->v.data.ooid = ooid;
	oxvcopy(pval, &event->v.data.val);
	if (pval->type == 's') {
		event->v.data.buf = oxnew(char, pval->len + 1);
		event->v.data.val.v.v_str = event->v.data.buf;
	}
	/* add to the end of linked list */
	if (_watch_last_event)
		_watch_last_event->next = event;
	_watch_last_event = event;
	if (!_watch_first_event)
		_watch_first_event = event;
	return 0;
}


static void
_watchIdleHandler (void)
{
	if (_watch_idle_sub && _watch_idle_enable) {
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP) ;
		PUTBACK;
		call_sv(_watch_idle_sub, G_DISCARD);
		FREETMPS;
		LEAVE;
	}
}


static int
_watchGetInfo (const char *name, owquark_t *info_p, int timeout)
{
	ooid_t ooid;
	owop_t op;
	oret_t rc;
	int err_code;
	if (!name || !*name)
		return -1;
	if (*name < '0' || *name > '9') {
		op = owatchGetInfoByDesc(name, info_p);
	} else {
		ooid = atol(name);
		if (ooid <= 0)
			return -1;
		op = owatchGetInfoByOoid(ooid, info_p);
	}
	rc = owatchWaitOp(op, timeout, &err_code);
	if (rc != OK || info_p->ooid==0) {
		_watchLog(3, "watch: cannot get info \"%s\" because %s",
					name, owatchErrorString(err_code));
		return err_code;
	}
	return 0;
}


static inline
SV *
_hvGetVal (HV *hv, const char *key)
{
	SV **psv = hv_fetch(hv, key, strlen(key), 0);
	return (psv ? *psv : NULL);
}


static inline
char *
_hvGetStr (HV *hv, const char *key)
{
	SV *sv = _hvGetVal(hv, key);
	return (sv ? SvPV_nolen(sv) : NULL);
}


static int
_watchInit (const char *client_name, const char *server, const char *msg_dest,
			int def_timeout, int conn_timeout, int verbosity,
			SV *alive_sub, SV *data_sub, SV *idle_sub)
{
	pthread_mutexattr_t mattr;
	oret_t rc;
	owop_t op;
	int err;
	const char *s;

	if (verbosity < 0) {
		verbosity = -verbosity;
		owatch_verb = _watch_verb = verbosity;
	}

	pthread_mutexattr_init(&mattr);
	pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&_watch_mutex, &mattr);
	pthread_mutexattr_destroy(&mattr);

	if (!_watch_handlers_are_lazy)
		owatch_handlers_are_lazy = TRUE;
	if (owatchInit(client_name) == ERROR)
		return 0;

	_watch_inited = TRUE;

	if (def_timeout < 0)
		def_timeout = -1;
	if (def_timeout == 0)
		def_timeout = WATCH_DEF_TIMEOUT;
	_watch_def_timeout = def_timeout;

	if (NULL == msg_dest || !*msg_dest)
		s = WATCH_DEF_MSG_DEST;
	else
		s = msg_dest;
	_watch_def_msg_dest = oxstrdup(s);

	owatchRegisterIdleHandler(_watchIdleHandler, TRUE);
	owatchAddAliveHandler(_watchAliveHandler, 0);

	SET_SUB(_watch_alive_sub, alive_sub);
	SET_SUB(_watch_data_sub, data_sub);
	SET_SUB(_watch_idle_sub, idle_sub);
	_watch_idle_enable = FALSE;

	rc = OK;
	op = owatchConnect(server);
	if (0 == conn_timeout) {
		/* lazy connection */
		owatchDetachOp(op);
	} else {
		if (owatchWaitOp(op, conn_timeout, &err) == ERROR) {
			_watchLog(1, "watchInit: cannot connect to \"%s\" error=%d",
						server, err);
			owatchDetachOp(op);
		}
	}

	owatchAddDataHandler("", _watchDataHandler, 0);
	owatchEnableMonitoring(TRUE);

	return (rc == OK);
}


/* =================== MODULE: Watch ====================== */

MODULE = Optikus::Watch		PACKAGE = Optikus::Watch


int
watchHashInit (args)
	HV *args
  PREINIT:
	SV *sv = NULL;
	const char *client_name = NULL;
	const char *server = NULL;
	const char *msg_dest = NULL;
	int def_timeout = 0;
	int conn_timeout = 0;
	int verbosity = 0;
	SV *alive_sub = NULL;
	SV *data_sub = NULL;
	SV *idle_sub = NULL;
  CODE:
	client_name = _hvGetStr(args, "client_name");
	server = _hvGetStr(args, "server");
	msg_dest = _hvGetStr(args, "msg_dest");
	if (NULL != (sv = _hvGetVal(args, "def_timeout")))
		def_timeout = (int) SvIV(sv);
	if (NULL != (sv = _hvGetVal(args, "conn_timeout")))
		conn_timeout = (int) SvIV(sv);
	if (NULL != (sv = _hvGetVal(args, "verbosity")))
		verbosity = (int) SvIV(sv);
	alive_sub = _hvGetVal(args, "alive_handler");
	data_sub = _hvGetVal(args, "data_handler");
	idle_sub = _hvGetVal(args, "idle_handler");
	RETVAL = _watchInit(client_name, server, msg_dest,
						def_timeout, conn_timeout, verbosity,
						alive_sub, data_sub, idle_sub);
  OUTPUT:
	RETVAL


int
watchExit ()
  CODE:
	_watch_alive_sub = (SV*) NULL;
	_watch_data_sub = (SV*) NULL;
	_watch_idle_sub = (SV*) NULL;
	_watch_idle_enable = FALSE;
	owatchEnableMonitoring(FALSE);
	_watchLazyHandleEvents(FALSE);
	RETVAL = (owatchExit() == OK);
	pthread_mutex_destroy(&_watch_mutex);
	oxfree(_watch_def_msg_dest);
	_watch_def_msg_dest = NULL;
	_watch_inited = FALSE;
  OUTPUT:
	RETVAL


int
watchLock ()
  CODE:
	RETVAL = (_watch_inited ? pthread_mutex_lock(&_watch_mutex) == 0 : 0);
  OUTPUT:
	RETVAL


int
watchUnlock ()
  CODE:
	RETVAL = (_watch_inited ? pthread_mutex_unlock(&_watch_mutex) == 0 : 0);
  OUTPUT:
	RETVAL


void
watchGetInfo (name, timeout)
	const char *name
	int timeout
  INIT:
	int err;
	owquark_t info;
	char type_str[4];
  PPCODE:
	err = _watchGetInfo(name, &info, timeout);
	if (err == 0) {
		XPUSHs(sv_2mortal(newSViv(info.ooid)));
		XPUSHs(sv_2mortal(newSVpv(info.desc, 0)));
		type_str[0] = info.type;
		type_str[1] = 0;
		XPUSHs(sv_2mortal(newSVpv(type_str, 0)));
		XPUSHs(sv_2mortal(newSViv(info.len)));
	}


const char *
watchGetSubjectInfo (subject)
	const char *subject
  CODE:
	if (NULL == subject || '\0' == *subject) {
		char buf[100];		/* FIXME: magic size, stack buffer ! */
		buf[0] = 0;
		owatchGetSubjectList(buf, sizeof(buf));
		RETVAL = buf;
	} else {
		RETVAL = owatchGetSubjectState(subject);
	}
  OUTPUT:
	RETVAL
	

int
watchWriteAsString (name, value, timeout)
	const char *name
	const char *value
	int timeout
  INIT:
	static const char *me = "watchWriteAsString";
	owop_t op;
	oret_t rc;
	owquark_t info;
	oval_t real_val, temp_val;
	char tmp_str[400];
	int len = sizeof(tmp_str);
	char* ptr = tmp_str;
	int err;
  CODE:
	if (NULL == name || '\0' == *name) {
		rc = ERROR;
	} else {
		if (*name >= '0' && *name <= '9') {
			ulong_t ooid = atol(name);
			if (ooid > 0)
				op = owatchGetInfoByOoid(ooid, &info);
			else
				op = OWOP_ERROR;
		} else {
			op = owatchGetInfoByDesc(name, &info);
		}
		rc = owatchWaitOp(op, timeout, &err);
		if (rc != OK || info.ooid == 0) {
			owatchCancelOp(op);
			_watchLog(2, "%s: cannot get \"%s\" info because %s",
						me, name, owatchErrorString(err));
		} else {
			temp_val.v.v_str = (char*) value;
			temp_val.len = strlen(value);
			temp_val.type = 's';
			real_val.time = 0;
			real_val.undef = 0;
			real_val.type = info.type;
			rc = owatchConvertValue(&temp_val, &real_val, ptr, len);
			if (rc != OK) {
				_watchLog(2, "%s: cannot convert value \"%s\" for \"%s\"",
							me, value, name);
			} else {
				op = owatchWrite(info.ooid, &real_val);
				rc = owatchWaitOp(op, timeout, &err);
				if (rc != OK) {
					owatchCancelOp(op);
					_watchLog(2, "%s: owatchWaitOp(%s) failure %s",
								me, name, owatchErrorString(err));
				}
			}
		}
	}
	RETVAL = (rc == OK);
  OUTPUT:
	RETVAL


void
watchReadByName (name, timeout)
	const char *name
	int timeout
  INIT:
	static const char *me = "watchReadByName";
	char buffer[400];		/* FIXME: magic size */
	int buflen = sizeof(buffer);
	oval_t val, *pval = &val;
	owop_t op;
	oret_t rc;
	owquark_t info;
	int err;
PPCODE:
	if (NULL == name || '\0' == *name) {
		rc = ERROR;
	} else {
		if (*name >= '0' && *name <= '9') {
			ulong_t ooid = atol(name);
			if (ooid > 0)
				op = owatchGetInfoByOoid(ooid, &info);
			else
				op = OWOP_ERROR;
		} else {
			op = owatchGetInfoByDesc(name, &info);
		}
		rc = owatchWaitOp(op, timeout, &err);
		if (rc != OK || info.ooid == 0) {
			owatchCancelOp(op);
			_watchLog(2, "%s: cannot get \"%s\" info because %s",
						me, name, owatchErrorString(err));
		} else {
			op = owatchRead(info.ooid, pval, buffer, buflen);
			rc = owatchWaitOp(op, timeout, &err);
			if (rc != OK)  {
				owatchCancelOp(op);
				_watchLog(2, "%s: cannot read \"%s\" because %s",
							me, name, owatchErrorString(err));
			} else {
				PUSH_WATCH_VAL(&val);
			}
		}
	}


int
watchEnableMonitoring (enable)
	int enable
  CODE:
	RETVAL = (owatchEnableMonitoring(enable != 0) == OK);
  OUTPUT:
	RETVAL


int
watchMonitor (ooid, data, timeout)
	unsigned long ooid
	long data
	int timeout
  INIT:
	owop_t op;
	oret_t rc;
	int err;
  CODE:
	RETVAL = 0;
	op = owatchAddMonitorByOoid(ooid);
	if (op != OWOP_ERROR) {
		rc = owatchWaitOp(op, timeout, &err);
		if (rc == OK) {
			if (data)
				owatchSetMonitorData(ooid, data);
			RETVAL = 1;
		}
	}
  OUTPUT:
	RETVAL


int
watchUnmonitor (ooid)
	unsigned long ooid
  CODE:
	if (ooid == 0)
		RETVAL = owatchRemoveAllMonitors();
	else
		RETVAL = owatchRemoveMonitor(ooid);
  OUTPUT:
	RETVAL


int
watchRenewMonitor (ooid, timeout)
	unsigned long ooid
	int timeout
  INIT:
	owop_t op;
	oret_t rc;
	int err;
  CODE:
	RETVAL = 0;
	op = owatchRenewMonitor(ooid);
	if (op != OWOP_ERROR) {
		rc = owatchWaitOp(op, timeout, &err);
		if (rc == OK) {
			RETVAL = 1;
		}
	}
  OUTPUT:
	RETVAL


int
watchSendCtlMsgAsArray (dest, klass, type, format, ...)
	const char *dest
	int klass
	int type
	const char *format
  INIT:
	int gap = 4;
	int i, narg;
	double dargs[128];		/* FIXME: magic buffer size */
	char xformat[256];
	const char *s;
	owop_t op;
	int err;
	oret_t rc;
	bool_t ok;
  CODE:
	if (NULL == format)
		format = "";
	for (s = format, narg = 0; *s; s++) {
		if (*s == '%')
			narg++;
	}
	ok = TRUE;
	if (items - gap < narg) {
		olog("not all arguments supplied, need %d arguments", narg);
		ok = FALSE;
	}
	for (i = 0; i < narg && ok; i++) {
		dargs[i] = SvNV(ST(i+gap));
		ok = SvOK(ST(i+gap));
	}
	strcpy(xformat, "$(D)");
	strcat(xformat, format ? format : "");
	if (ok) {
		/* FIXME: the dest argument is ignored */
		dest = _watch_def_msg_dest;
		op = owatchSendCtlMsg(dest, klass, type, xformat, dargs);
		rc = owatchWaitOp(op, _watch_def_timeout, &err);
		if (rc == ERROR && err == OWATCH_ERR_PENDING)
			owatchCancelOp(op);
		ok = (rc == OK);
	}
	RETVAL = ok;
  OUTPUT:
	RETVAL


int
watchSendCtlMsgAsString (dest, klass, type, format, all_args)
	const char *dest
	int klass
	int type
	const char *format
	const char *all_args
  INIT:
	char xformat[256];		/* FIXME: magic buffer size */
	oret_t rc;
	owop_t op;
	int err;
  CODE:
	if (NULL == format)
		format = "";
	if (NULL == all_args)
		all_args = "";
	strcpy(xformat, "$(S)");
	strcat(xformat, format);
	/* FIXME: the dest argument is ignored */
	dest = _watch_def_msg_dest;
	op = owatchSendCtlMsg(dest, klass, type, xformat, all_args);
	rc = owatchWaitOp(op, _watch_def_timeout, &err);
	if (rc == ERROR && err == OWATCH_ERR_PENDING)
		owatchCancelOp(op);
	RETVAL = (rc == OK);
  OUTPUT:
	RETVAL


int
watchIdleEnable (enable)
	int enable
  CODE:
	RETVAL = _watch_idle_enable;
	_watch_idle_enable = enable;
  OUTPUT:
	RETVAL


int
owatchGetAckCount(flush)
	int flush


void
watchSetDebugging (level)
	int level
  INIT:
	extern int owatch_verb;
  CODE:
	_watch_verb = owatch_verb = level;


int
watchLazyHandleEvents (really_work)
	int really_work;
  CODE:
	RETVAL = _watchLazyHandleEvents(really_work);
  OUTPUT:
	RETVAL


int
watchIdle (timeout)
	int timeout
  CODE:
	RETVAL = (owatchWork(timeout) == OK);
  OUTPUT:
	RETVAL


int
watchWaitSec (seconds)
	float seconds
  CODE:
	RETVAL = (owatchWork((int) (seconds * 1000.0)) == OK);
  OUTPUT:
	RETVAL

