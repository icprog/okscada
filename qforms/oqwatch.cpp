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


#define qDebug(...)


#define SUBJECT_LIST_SIZE	1024

OQWatch *OQWatch::instance;
int OQWatch::next_id = 780000;


// ======== Initialization ========


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


int
OQWatch::setVerbosity(int level)
{
	int prev = owatch_verb;
	if (level != -1)
		owatch_verb = level;
	return prev;
}


// ======== Static handlers ========

int
OQWatch::aliveHandler(long param, const char *subject, const char *state)
{
	char subject_list[SUBJECT_LIST_SIZE];
	owatchGetSubjectList(subject_list, sizeof subject_list);
	emit instance->subjectsUpdated(subject_list, subject, state);
	return 0;
}


int
OQWatch::dataHandler(long param, ooid_t id, const oval_t *pval)
{
	owquark_t inf;
	if (instance && getInfo(id, inf))
		emit instance->dataUpdated(inf, ovalToVariant(pval), pval->time);
	return 0;
}


int
OQWatch::libraryOpHandler(long param, owop_t op, int err_code)
{
	instance->bgHandler(op, err_code);
	return 0;
}


void
OQWatch::idle(int timeout)
{
	OQWatch *w = instance;
	if (w)
		emit w->beginUpdates();
	owatchWork(timeout);
	if (w)
		emit w->endUpdates();
}


// ======== Operations ========


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


// ======== Conversions ========


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


bool
OQWatch::variantToOval(const QVariant& var, oval_t& oval, char * & buf)
{
	buf = 0;
	oval.time = 0;
	oval.undef = 0;

	switch (var.type()) {
		case QVariant::Bool:
		case QVariant::Int:
			oval.v.v_int = var.toInt();
			oval.type = 'i';
			oval.len = sizeof(oval.v.v_int);
			break;
		case QVariant::UInt:
			oval.v.v_uint = var.toUInt();
			oval.type = 'd';
			oval.len = sizeof(oval.v.v_uint);
			break;
		case QVariant::Double:
			oval.v.v_double = var.toDouble();
			oval.type = 'd';
			oval.len = sizeof(oval.v.v_double);
			break;
		case QVariant::LongLong:
		case QVariant::ULongLong:
			oval.v.v_double = var.toDouble();
			oval.type = 'd';
			oval.len = sizeof(oval.v.v_double);
			break;
		case QVariant::String:
			{
				oval.type = 's';
				const QString& qs = var.toCString();
				buf = new char[(oval.len = qs.length()) + 1];
				strcpy(oval.v.v_str = buf, (const char *) qs);
			}
			break;
		case QVariant::CString:
			{
				oval.type = 's';
				const QCString& qcs = var.toCString();
				buf = new char[(oval.len = qcs.length()) + 1];
				strcpy(oval.v.v_str = buf, (const char *) qcs);
			}
			break;
		default:
			oval.type = 0;
			oval.len = 0;
			oval.undef = 1;
			return false;
	}

	return true;
}


// ======== Typed reading ========


int
OQWatch::readInt(const char *desc, int timeout, bool *ok)
{
	int err = 0;
	int val = (int) owatchReadAsLong(desc, timeout, &err);
	if (ok)
		*ok = !err;
	return val;
}

uint_t
OQWatch::readUInt(const char *desc, int timeout, bool *ok)
{
	return (uint_t) readInt(desc, timeout, ok);
}

long
OQWatch::readLong(const char *desc, int timeout, bool *ok)
{
	int err = 0;
	long val = owatchReadAsLong(desc, timeout, &err);
	if (ok)
		*ok = !err;
	return val;
}

ulong_t
OQWatch::readULong(const char *desc, int timeout, bool *ok)
{
	return (ulong_t) readLong(desc, timeout, ok);
}

float
OQWatch::readFloat(const char *desc, int timeout, bool *ok)
{
	int err = 0;
	float val = owatchReadAsFloat(desc, timeout, &err);
	if (ok)
		*ok = !err;
	return val;
}

double
OQWatch::readDouble(const char *desc, int timeout, bool *ok)
{
	int err = 0;
	double val = owatchReadAsDouble(desc, timeout, &err);
	if (ok)
		*ok = !err;
	return val;
}

QString
OQWatch::readString(const char *desc, int timeout, bool *ok)
{
	int err = 0;
	char *str = owatchReadAsString(desc, timeout, &err);
	if (ok)
		*ok = !err;
	if (str) {
		QString val(str);
		oxfree(str);
		return val;
	}
	return QString();
}


QVariant
OQWatch::readValue(const char *desc, int timeout, bool *ok)
{
	owquark_t inf;
	if (getInfo(desc, inf, timeout)) {
		char buf[128];
		char *buf_ptr;
		int buf_len;
		oval_t val;
		if (inf.len >= (int) sizeof(buf)) {
			buf_len = inf.len + 2;
			buf_ptr = new char[buf_len];
		} else {
			buf_ptr = buf;
			buf_len = sizeof(buf);
		}
		bool op_ok = waitOp(owatchRead(inf.ooid, &val, buf_ptr, buf_len));
		if (op_ok) {
			QVariant v = ovalToVariant(&val);
			if (buf_ptr != buf)
				delete buf_ptr;
			if (ok)
				*ok = true;
			return v;
		}
	}
	if (ok)
		*ok = false;
	return QVariant();
}


// ======== Control messages ========


bool
OQWatch::sendCMsgAsDoubleArray(const QString& target, int klass, int type,
								const QString& format, double *data)
{
	QString xformat("$(D)");
	xformat += format;
	// FIXME: target is not used
	oret_t rc = owatchSendCtlMsg("sample", klass, type, xformat, data);
	return (OK == rc);
}


// ======== Background operations: API ========


bool
OQWatch::cancelBg(int id)
{
	if (!ids.contains(id)) {
		qDebug("cancelBg id %d not found", id);
		return false;
	}
	Bg * bg = ids[id];
	qDebug("cancelBg %p id=%d", bg, id);
	if (bg->timer) {
		qDebug("cancelBg: kill timer %d", bg->timer);
		killTimer(bg->timer);
		timers.remove(bg->timer);
	}
	if (bg->op) {
		qDebug("cancelBg: cancel op %xh", bg->op);
		owatchLocalOpHandler(bg->op, 0, 0);
		owatchCancelOp(bg->op);
		ops.remove(bg->op);
	}
	ids.remove(id);
	delete bg;
	return true;
}


void
OQWatch::cancelAllBg()
{
	QMap<int,Bg*>::Iterator it;
	for (it = ids.begin(); it != ids.end(); ++it) {
		Bg *bg = it.data();
		if (bg->op) {
			owatchLocalOpHandler(bg->op, 0, 0);
			owatchCancelOp(bg->op);
		}
		delete bg;
	}
	killTimers();
	ids.clear();
	ops.clear();
	timers.clear();
}


int
OQWatch::monitorBg(const char *desc, long param, int timeout)
{
	static ooid_t ooid_buf;
	owop_t op = owatchAddMonitorByDesc(desc, &ooid_buf, FALSE);

	if (op == OWOP_OK || op == OWOP_ERROR) {
		finishBg(OP_MONITOR, desc, param, op == OWOP_OK ? OK : ERROR);
		return 0;
	}

	Bg *bg = new Bg(OP_MONITOR, ++next_id, desc, param);

	if (timeout > 0 && (bg->timer = startTimer(timeout)) != 0)
		timers.replace(bg->timer, bg);

	ids.replace(bg->id, bg);
	ops.replace(bg->op = op, bg);
	owatchLocalOpHandler(op, libraryOpHandler, op);
	return bg->id;
}


int
OQWatch::readBg(const char *desc, long param, int timeout)
{
	int read_buf_len = 512;		// FIXME: magic size
	char *read_buf = new char[read_buf_len];
	oval_t *read_val = new oval_t;

	owop_t op = owatchReadByName(desc, read_val, read_buf, read_buf_len);
	if (op == OWOP_ERROR || op == OWOP_OK) {
		QVariant var;
		if (op == OWOP_OK)
			var = ovalToVariant(read_val);
		finishBg(OP_READ, desc, param, OK, &var);
		delete read_val;
		delete read_buf;
		return 0;
	}

	Bg *bg = new Bg(OP_READ, ++next_id, desc, param);
	bg->val = read_val;
	bg->buf = read_buf;

	if (timeout > 0 && (bg->timer = startTimer(timeout)) != 0)
		timers.replace(bg->timer, bg);

	ids.replace(bg->id, bg);
	ops.replace(bg->op = op, bg);
	owatchLocalOpHandler(op, libraryOpHandler, op);
	return bg->id;
}


int
OQWatch::writeBg(const char *desc, const QVariant& v, long param, int timeout)
{
	oval_t tval;
	char *tbuf;
	if (!variantToOval(v, tval, tbuf)) {
		qDebug("writeBg cannot convert desc=%s param=%ld", desc, param);
		// FIXME: better error needed
		finishBg(OP_WRITE, desc, param, ERROR);
		return 0;
	}

	int id = ++next_id;
	Bg *bg = new Bg(OP_WRITE_NOINFO, id, desc, param);
	bg->val = new oval_t(tval);
	bg->buf = tbuf;
	qDebug("writeBg start desc=%s par=%ld bg=%p id=%d", desc, param, bg, bg->id);

	static owquark_t dummy_inf;
	owop_t op = owatchGetInfoByDesc(desc, &dummy_inf);

	int err = 0;
	owatchWaitOp(op, 0, &err);

	switch (err) {
	case OK:
		qDebug("writeBg info ready");
		if (!writeBgGotInfo(bg)) {
			qDebug("writeBg info ready but cannot write");
			cancelBg(id);
			return 0;
		}
		qDebug("writeBg write pending");
		break;
	case OWATCH_ERR_PENDING:
		qDebug("writeBg info pending");
		bg->op = op;
		ops.replace(op, bg);
		owatchLocalOpHandler(op, libraryOpHandler, op);
		break;
	default:
		qDebug("writeBg info error %d", err);
		finishBg(OP_WRITE, desc, param, err);
		cancelBg(id);
		return 0;
	}

	// info or write is pending...
	if (timeout > 0 && (bg->timer = startTimer(timeout)) != 0)
		timers.replace(bg->timer, bg);
	ids.replace(bg->id, bg);
	return bg->id;
}


// ========= Background operations: internals ========


bool
OQWatch::writeBgGotInfo(Bg * bg)
{
	owquark_t inf;
	getInfo(bg->desc, inf);	// must be OK, not verifying.
	qDebug("WBGI bg=%p id=%d", bg, bg->id);

	oval_t rval = {};
	char rbuf[512];		// FIXME: magic size
	rval.type = inf.type;
	rval.len = inf.len;
	oret_t ret = owatchConvertValue(bg->val, &rval, rbuf, sizeof(rbuf));
	bg->dispose();
	if (ret != OK) {
		// FIXME: better error needed
		qDebug("WBGI cannot convert");
		finishBg(OP_WRITE, bg->desc, bg->param, ERROR);
		return false;
	}

	owop_t op = owatchWriteByName(bg->desc, &rval);
	if (op == OWOP_OK || op == OWOP_ERROR) {
		qDebug("WBGI cannot start writing");
		finishBg(OP_WRITE, bg->desc, bg->param, op == OWOP_OK ? OK : ERROR);
		return false;
	}

	qDebug("WBGI bound to op=%xh", op);
	bg->type = OP_WRITE;
	bg->op = op;
	ops.replace(op, bg);
	owatchLocalOpHandler(op, libraryOpHandler, op);
	return true;
}


void
OQWatch::bgHandler(owop_t op, int err)
{
	if (!ops.contains(op)) {
		qWarning("op 0x%x too late", op);
		return;
	}
	Bg * bg = ops[op];
	int id = bg->id;
	bg->op = 0;
	qDebug("bgHandler op=%xh err=%d bg=%p", op, err, bg);
	switch (bg->type) {
	case OP_WRITE_NOINFO:
		if (err == OK) {
			qDebug("dehash op %xh", op);
			ops.remove(op);
			if (writeBgGotInfo(bg)) {
				qDebug("continue with op=%xh bg=%p", op, bg);
				return;
			}
		}
		finishBg(OP_WRITE, bg->desc, bg->param, err);
		break;
	case OP_READ:
		{
			const QVariant& var = ovalToVariant(bg->val);
			finishBg(bg->type, bg->desc, bg->param, err, &var);
		}
		break;
	case OP_WRITE:
	case OP_MONITOR:
		finishBg(bg->type, bg->desc, bg->param, err);
		break;
	default:
		qFatal("invalid operation code %d", bg->type);
		return;
	}
	cancelBg(id);
}


void
OQWatch::finishBg(OpType type, const char *desc, long param, int err,
					const QVariant *val_p)
{
	qDebug("finishBg type=%d desc=%s param=%ld err=%d", type, desc, param, err);
	owquark_t inf = {};
	if (err == OK && !getInfo(desc, inf)) {
		err = ERROR;
	}
	if (err != OK) {
		strcpy(inf.desc, desc);
	}
	switch (type) {
		case OP_MONITOR:
			emit monitorDone(param, inf, err);
			break;
		case OP_WRITE:
			emit writeDone(param, inf, err);
			break;
		case OP_READ:
			emit readDone(param, inf, *val_p, err);
			break;
		default:
			qFatal("unexpected operation type");
			break;
	}
}


void
OQWatch::timerEvent(QTimerEvent *e)
{
	idle();
	int timer_id = e->timerId();
	if (!timers.contains(timer_id))
		return;
	Bg * bg = timers[timer_id];
	owatchLocalOpHandler(bg->op, 0, 0);
	bgHandler(bg->op, OWATCH_ERR_PENDING);
}
