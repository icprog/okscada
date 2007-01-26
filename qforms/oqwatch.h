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
	static void idle(int timeout = 0);

	static OQWatch *getInstance()	{ return instance; }
	static int setVerbosity(int level);

	static bool isAlive(const char *name)	{ return owatchGetAlive(name); }
	static bool isConnected()				{ return isAlive("*"); }
	static bool domainIsFull()				{ return isAlive("+"); }

	static void triggerSubject(const char *name);
	static void optimizeReading(const char *desc, bool enable = true);

	static ooid_t getOoidByDesc(const char *desc, int timeout = 0);
	static bool getInfo(ooid_t id, owquark_t& inf, int timeout = 0);
	static bool getInfo(const char *desc, owquark_t& inf, int timeout = 0);
	static bool waitOp(owop_t op, int timeout = 0,
						int *err = 0, bool detach = false);

	static bool sendCMsgAsDoubleArray(const QString& target, int klass, int type,
									const QString& format, double *data);

	// typed reading

	static QVariant ovalToVariant(const oval_t *);
	static bool variantToOval(const QVariant& var, oval_t& val, char* &buf);

	static int readInt(const char *desc, int timeout = 0, bool *ok = 0);
	static uint_t readUInt(const char *desc, int timeout = 0, bool *ok = 0);
	static long readLong(const char *desc, int timeout = 0, bool *ok = 0);
	static ulong_t readULong(const char *desc, int timeout = 0, bool *ok = 0);
	static float readFloat(const char *desc, int timeout = 0, bool *ok = 0);
	static double readDouble(const char *desc, int timeout = 0, bool *ok = 0);
	static QString readString(const char *desc, int timeout = 0, bool *ok = 0);
	static QVariant readValue(const char *desc, int timeout = 0, bool *ok = 0);

	static bool removeMonitor(ooid_t ooid);
	static void removeAllMonitors();
	static void renewAllMonitors();
	static void enableMonitoring(bool on);
	static void blockMonitoring(bool block, bool flush = false);

	static QString errorString(int code)	{ return owatchErrorString(code); }

	int monitorBg(const char *desc, long param = 0, int timeout = -1);
	int readBg(const char *desc, long param = 0, int timeout = -1);
	int writeBg(const char *desc, const QVariant& val, long param = 0,
				int timeout = -1);

	bool cancelBg(int id);
	void cancelAllBg();

signals:
	void beginUpdates();
	void endUpdates();
	void subjectsUpdated(const QString& all,
						const QString& sbj, const QString& st);
	void dataUpdated(const owquark_t& inf, const QVariant& val, long time);

	void monitorDone(long param, const owquark_t& inf, int err);
	void readDone(long param, const owquark_t& inf,
					const QVariant& val, int err);
	void writeDone(long param, const owquark_t& inf, int err);

protected:

	enum OpType
	{
		OP_NONE = 0,
		OP_MONITOR = 1,
		OP_READ = 2,
		OP_WRITE = 3,
		OP_WRITE_NOINFO = 4
	};

	class Bg
	{
	public:

		OpType type;
		int id;
		QString desc;
		long param;
		int timer;
		oval_t *val;
		char *buf;
		owop_t op;

		Bg(OpType _type, int _id, const QString& _desc, long _param)
			: type(_type), id(_id), desc(_desc), param(_param),
				timer(0), val(0), buf(0), op(0)
		{
		}

		~Bg()	{ dispose(); }

		void dispose()
		{
			delete val;
			val = 0;
			delete buf;
			buf = 0;
		}
	};

	QMap<int, Bg*> ids;
	QMap<owop_t, Bg*> ops;
	QMap<int, Bg*> timers;

	int poll_period;
	static int next_id;
	static OQWatch *instance;

	static int aliveHandler(long param, const char *subj, const char *state);
	static int dataHandler(long param, ooid_t id, const oval_t *pvalue);
	static int libraryOpHandler(long param, owop_t op, int err);

	void bgHandler(owop_t op, int err);
	void finishBg(OpType type, const char *desc, long param, int err,
					const QVariant *val_p = 0);
	bool writeBgGotInfo(Bg * bg);
	void timerEvent(QTimerEvent *);
};


inline void OQWatch::triggerSubject(const char *name)
	{ owatchDetachOp(owatchTriggerSubject(name)); }

inline void OQWatch::optimizeReading(const char *desc, bool enable)
	{ owatchOptimizeReading(desc, enable ? OWATCH_ROPT_ALWAYS
										: OWATCH_ROPT_NEVER, 0); }

inline bool OQWatch::getInfo(ooid_t id, owquark_t& inf, int timeout)
	{ return waitOp(owatchGetInfoByOoid(id, &inf)); }

inline bool OQWatch::getInfo(const char *desc, owquark_t& inf, int timeout)
	{ return waitOp(owatchGetInfoByDesc(desc, &inf)); }

inline bool OQWatch::removeMonitor(ooid_t ooid)
	{ return (owatchRemoveMonitor(ooid) == OK); }

inline void OQWatch::removeAllMonitors()
	{ owatchRemoveAllMonitors(); }

inline void OQWatch::renewAllMonitors()
	{ owatchRenewAllMonitors(); }

inline void OQWatch::enableMonitoring(bool on)
	{ owatchEnableMonitoring(on); }

inline void OQWatch::blockMonitoring(bool block, bool flush)
	{ owatchBlockMonitoring(block, flush); }


#endif // OQWATCH_H
