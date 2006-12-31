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

  Watch library wrapper for Qt.

*/

#ifndef OQWATCH_H
#define OQWATCH_H

#include <qvariant.h>
#include <qobject.h>
#include <qmap.h>
#include <qpair.h>
#include <optikus/watch.h>
#include <optikus/conf-mem.h>


#define OQWATCH_POLL_PERIOD		100


class OQWatch : public QObject
{
	Q_OBJECT

public:
	OQWatch();
	OQWatch(OQWatch&);
	OQWatch(const char *client_name, int poll_period = OQWATCH_POLL_PERIOD);

	void init(const char *client_name, int poll_period = OQWATCH_POLL_PERIOD);
	void exit();
	static bool connect(const char *server = 0, int timeout = 0);
	static OQWatch *getInstance()	{ return instance; }


	static bool isAlive(const char *name)	{ return owatchGetAlive(name); }
	static bool isConnected()				{ return isAlive("*"); }
	static bool domainIsFull()				{ return isAlive("+"); }

	static void triggerSubject(const char *name)
	{
		owatchDetachOp(owatchTriggerSubject(name));
	}

	static void optimizeReading(const char *desc, bool enable = true)
	{
		owatchOptimizeReading(desc,
						enable ? OWATCH_ROPT_ALWAYS : OWATCH_ROPT_NEVER, 0);
	}

	static ooid_t getOoidByDesc(const char *desc, int timeout = 0);


	// getTYPE by description

	static int getInt(const char *desc, int timeout = 0, bool *failed_p = 0)
	{
		int err = 0;
		int val = (int) owatchReadAsLong(desc, timeout, &err);
		if (failed_p)  *failed_p = (err != 0);
		return val;
	}

	static uint_t getUInt(const char *desc, int timeout = 0, bool *failed_p = 0)
	{
		return (uint_t) getInt(desc, timeout, failed_p);
	}

	static long getLong(const char *desc, int timeout = 0, bool *failed_p = 0)
	{
		int err = 0;
		long val = owatchReadAsLong(desc, timeout, &err);
		if (failed_p)  *failed_p = (err != 0);
		return val;
	}

	static ulong_t getULong(const char *desc, int timeout = 0, bool *failed_p = 0)
	{
		return (ulong_t) getLong(desc, timeout, failed_p);
	}

	static float getFloat(const char *desc, int timeout = 0, bool *failed_p = 0)
	{
		int err = 0;
		float val = owatchReadAsFloat(desc, timeout, &err);
		if (failed_p)  *failed_p = (err != 0);
		return val;
	}

	static double getDouble(const char *desc, int timeout = 0, bool *failed_p = 0)
	{
		int err = 0;
		double val = owatchReadAsDouble(desc, timeout, &err);
		if (failed_p)  *failed_p = (err != 0);
		return val;
	}

	static QString getString(const char *desc, int timeout = 0, bool *failed_p = 0)
	{
		int err = 0;
		char *str = owatchReadAsString(desc, timeout, &err);
		if (failed_p)  *failed_p = (err != 0);
		if (str) {
			QString val(str);
			oxfree(str);
			return val;
		}
		return QString();
	}

	// Control messages

	static bool sendCMsgAsDoubleArray(const QString& target, int klass, int type,
									const QString& format, double *data)
	{
		QString xformat("$(D)");
		xformat += format;
		// FIXME: target is not used
		oret_t rc = owatchSendCtlMsg("sample", klass, type, xformat, data);
		return (OK == rc);
	}

	static int setVerbosity(int level)
	{
		int prev = owatch_verb;
		if (level != -1)
			owatch_verb = level;
		return prev;
	}

	static bool getInfo(ooid_t id, owquark_t& info, int timeout = 0)
	{
		return waitOp(owatchGetInfoByOoid(id, &info));
	}

	static bool getInfo(const char *desc, owquark_t& info, int timeout = 0)
	{
		return waitOp(owatchGetInfoByDesc(desc, &info));
	}

	static bool waitOp(owop_t op, int timeout = 0, int *err_out = 0,
						bool detach = false);

	static QVariant ovalToVariant(const oval_t *);

	int addMonitorBg(const char *desc, long param, int timeout);
	bool cancelMonitorBg(int id);

	static bool removeMonitor(ooid_t ooid)
	{
		return (owatchRemoveMonitor(ooid) == OK);
	}

	static void removeAllMonitors()			{ owatchRemoveAllMonitors(); }

	static void enableMonitoring(bool on)	{ owatchEnableMonitoring(on); }

	static QString errorString(int code)	{ return owatchErrorString(code); }

signals:
	void subjectsUpdated(const QString& all, const QString& sbj, const QString& st);
	void dataUpdated(const owquark_t&, const QVariant&, long);
	void monitorsUpdated(long param, const owquark_t&, int err);

protected:

	static int aliveHandler(long param, const char *subj, const char *state);
	static int dataHandler(long param, ooid_t id, const oval_t *pvalue);
	static int monitorOpHandler(long param, owop_t op, int err);
	void monitorHandler(owop_t op, int err_code);

	struct Triplet
	{
		QString desc;
		int timer;
		long param;
	};

	QMap<owop_t, Triplet> mon_ops;
	QMap<int, owop_t> mon_touts;

	void timerEvent(QTimerEvent *);
	int poll_period;

	static OQWatch *instance;
};

#endif // OQWATCH_H
