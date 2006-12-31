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

  Dock bar for easy access to key applications.

*/

#include <qlayout.h>
#include <qlabel.h>
#include <qvariant.h>

#include "oqcommon.h"
#include "oqwatch.h"


#define BUTTON_SIZE		44

class OQDockBar;


class OQDockButton : public OQPushButton
{
	Q_OBJECT
public:
	OQDockButton(OQDockBar *parent, QBoxLayout *layout,
				const char *icon_on, const char *icon_off,
				const char *tooltip_on, const char *tooltip_off,
				bool toggle = true, int size = BUTTON_SIZE);

	OQDockBar *getDockBar() { return (OQDockBar *) parentWidget(); }

public slots:
	void setOn(bool on);

private:

	const char *icon_on, *icon_off;
	const char *tooltip_on, *tooltip_off;
};


class OQProgramButton : public OQDockButton
{
	Q_OBJECT
public:
	OQProgramButton(OQDockBar *dockbar,
					const char *program, const char *param,
					const char *icon, const char *tooltip);
public slots:
	void buttonClicked();
	void updateState();

protected:
	const char *program;
	const char *param;
};


class OQServerBox : public OQDockButton
{
	Q_OBJECT
public:
	OQServerBox(OQDockBar *dockbar, QBoxLayout *layout);
public slots:
	void boxClicked();
	void updateState();
};


class OQServerStatus : public OQDockButton
{
	Q_OBJECT
public:
	OQServerStatus(OQDockBar *dockbar, QBoxLayout *layout);
public slots:
	void updateState(const QString&, const QString&, const QString&);
};


class OQTimeLabel : public QLabel
{
public:
	OQTimeLabel(const char *region, OQDockBar *parent);
	void set(int yy, int mm, int dd, int hh, int mi, int ss);
	void set() { set(0,0,0, 0,0,0); }
protected:
	const char *region;
};


class OQScriptBox : public OQDockButton
{
	Q_OBJECT
public:
	OQScriptBox(OQDockBar *dockbar, QBoxLayout *layout);
public slots:
	void boxClicked();
	void updateState();
};


class OQDockBar : public QWidget
{
	Q_OBJECT

public:
	OQDockBar(OQWatch *watch = 0);

	QBoxLayout *getProgBar() { return progbar; }
	QBoxLayout *getSubjectBar() { return subjectbar; }
	QBoxLayout *getTimeBar() { return timebar; }
	void refreshProcessList();
	bool processIsAlive(const char *process);
	const QString &getProcList() { return proclist; }
	void enableHub(bool on);
	void checkinHub();

protected slots:
	void updateSubjects(const QString& list,
						const QString& subject, const QString& state);
	void updateData(const owquark_t& , const QVariant& value, long time);

signals:
	void processesUpdated(const QString& processes);

protected:
	void createProgramButtons();
	void createOtherWidgets();

	void timerEvent(QTimerEvent *);

	OQWatch *watch;
	QString subject_list;

	QString proclist;
	int proclist_timer;

	QBoxLayout *progbar;
	QBoxLayout *subjectbar;
	QBoxLayout *timebar;
	QBoxLayout *statebar;

	OQTimeLabel *simtime;
	ooid_t simtime_sec_id;
	OQTimeLabel *walltime;
	int walltime_timer;

	OQServerBox *serverbox;
};

