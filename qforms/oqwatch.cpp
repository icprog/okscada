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

  Watch library wrapper.

*/

#include "oqwatch.h"

#include <stdlib.h>
#include <string.h>
#include <iostream>


#define SUBJECT_LIST_SIZE	1024

OQWatch *OQWatch::instance;


//
// .
//
void
OQWatch::init(const char *client_name, int _poll_period)
{
	owatchInit(client_name);
	owatchAddAliveHandler(&aliveHandler, 0);
	owatchAddDataHandler("", &dataHandler, 0);
	poll_period = _poll_period;
	killTimers();
	startTimer(poll_period);
}


void
OQWatch::exit()
{
	owatchExit();
	killTimers();
	poll_period = 0;
}


OQWatch::OQWatch()
{
	poll_period = 0;
	instance = this;
}


OQWatch::OQWatch(const char *client_name, int poll_period)
{
	init(client_name, poll_period);
	instance = this;
}


OQWatch::OQWatch(OQWatch& copy)
{
	poll_period = copy.poll_period;
	if (poll_period)
		startTimer(poll_period);
	copy.killTimers();
	copy.poll_period = 0;
	instance = this;
}


//
// .
//
int
OQWatch::aliveHandler(long param, const char *subject, const char *state)
{
	char subject_list[SUBJECT_LIST_SIZE];
	owatchGetSubjectList(subject_list, sizeof subject_list);
	emit instance->subjectsUpdated(subject_list, subject, state);
	return 0;
}


//
// .
//
bool
OQWatch::waitOp(owop_t op, int timeout, int *err_out, bool detach)
{
	if (op == OWOP_OK) {
		if (err_out)
			*err_out = OK;
		return true;
	}
	if (op == OWOP_ERROR) {
		if (err_out)
			*err_out = ERROR;
		return false;
	}
	int local_err;
	if (!err_out)
		err_out = &local_err;
	*err_out = 0;
	oret_t ret = owatchWaitOp(op, timeout, err_out);
	if (ret == OWOP_OK)
		return true;
	if (ret == OWOP_ERROR)
		return false;
	if (detach)
		owatchDetachOp(op);
	else
		owatchCancelOp(op);
	return false;
}


//
// .
//
QVariant
OQWatch::ovalToVariant(const oval_t *pval)
{
	QVariant v;
	if (!pval || pval->undef)
		return v;
	switch (pval->type) {
		case 'b':	v = QVariant((int)pval->v.v_char);		break;
		case 'B':	v = QVariant((uint)pval->v.v_uchar);	break;
		case 'h':	v = QVariant((int)pval->v.v_short);		break;
		case 'H':	v = QVariant((uint)pval->v.v_ushort);	break;
		case 'i':	v = QVariant(pval->v.v_int);			break;
		case 'I':	v = QVariant(pval->v.v_uint);			break;
		case 'l':	v = QVariant((Q_LLONG)pval->v.v_long);	break;
		case 'L':	v = QVariant((Q_ULLONG)pval->v.v_ulong);break;
		case 'E':	v = QVariant((int)pval->v.v_enum);		break;
		case 'f':	v = QVariant((double)pval->v.v_float);	break;
		case 'd':	v = QVariant(pval->v.v_double);			break;
		case 's':	v = QVariant(pval->v.v_str);			break;
		case 'p':	v = QVariant((Q_LLONG)pval->v.v_proc);	break;
		default:	break;
	}
	return v;
}


//
// .
//
int
OQWatch::dataHandler(long param, ooid_t id, const oval_t *pval)
{
	owquark_t info;
	if (instance && getInfo(id, info))
		emit instance->dataUpdated(info, ovalToVariant(pval), pval->time);
	return 0;
}


//
// .
//
bool
OQWatch::connect(const char *server, int timeout)
{
	if (!server || !*server)
		server = getenv("OPTIKUS_SERVER");
	if (!server || !*server)
		server = "localhost";
	int err;
	bool ret = waitOp(owatchConnect(server), timeout, &err, !timeout);
	return (ret || (!timeout && err == OWATCH_ERR_PENDING));
}


//
// .
//
ooid_t
OQWatch::getOoidByDesc(const char *desc, int timeout)
{
	owquark_t info;
	owop_t op = owatchGetInfoByDesc(desc, &info);
	oret_t ret = owatchWaitOp(op, timeout, 0);
	if (ret == OK)
		return info.ooid;
	if (timeout == 0)
		owatchDetachOp(op);
	else
		owatchCancelOp(op);
	return 0;
}


//
// .
//
int
OQWatch::addMonitorBg(const char *desc, long param, int timeout)
{
	static ooid_t ooid_buf;
	owop_t op = owatchAddMonitorByDesc(desc, &ooid_buf, FALSE);
	if (op == OWOP_OK) {
		owquark_t info;
		if (getInfo(desc, info, 0)) {
			emit monitorsUpdated(param, info, OK);
			return 0;
		}
		op = OWOP_ERROR;
	}
	if (op == OWOP_ERROR) {
		owquark_t info = {};
		strcpy(info.desc, desc);
		emit monitorsUpdated(param, info, ERROR);
		return 0;
	}

	Triplet t;
	t.desc = desc;
	t.param = param;
	t.timer = 0;
	
	if (timeout > 0) {
		if ((t.timer = startTimer(timeout)) != 0)
			mon_touts.replace(t.timer, op);
	}
	mon_ops.replace(op, t);

	owatchLocalOpHandler(op, monitorOpHandler, op);
	return op;
}


bool
OQWatch::cancelMonitorBg(int op_id)
{
	owop_t op = op_id;
	if (!mon_ops.contains(op))
		return false;
	Triplet& t = mon_ops[op];
	if (t.timer) {
		killTimer(t.timer);
		mon_touts.remove(t.timer);
	}
	mon_ops.remove(op);
	return true;
}


//
// .
//
void
OQWatch::timerEvent(QTimerEvent *e)
{
	owatchWork(0);
	int id = e->timerId();
	if (mon_touts.contains(id)) {
		owop_t op = mon_touts[id];
		mon_touts.remove(id);
		owatchCancelOp(op);
		if (mon_ops.contains(op)) {
			Triplet& t = mon_ops[op];
			owquark_t info = {};
			strcpy(info.desc, t.desc);
			long param = t.param;
			mon_ops.remove(op);
			emit monitorsUpdated(param, info, OWATCH_ERR_PENDING);
		}
	}
}


//
// .
//
int
OQWatch::monitorOpHandler(long param, owop_t op, int err_code)
{
	instance->monitorHandler(op, err_code);
	return 0;
}


void
OQWatch::monitorHandler(owop_t op, int err_code)
{
	if (!mon_ops.contains(op))
		return;
	Triplet& t = mon_ops[op];
	long param = t.param;
	if (t.timer) {
		killTimer(t.timer);
		mon_touts.remove(t.timer);
	}
	if (err_code == OK) {
		owquark_t info;
		if (getInfo(t.desc, info, 0)) {
			mon_ops.remove(op);
			emit monitorsUpdated(param, info, OK);
			return;
		}
		err_code = ERROR;
	}
	owquark_t info = {};
	strcpy(info.desc, t.desc);
	mon_ops.remove(op);
	emit monitorsUpdated(param, info, err_code);
}
