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

#include "oqcommon.h"
#include "oqwatch.h"

#include <qtoolbar.h>
#include <qmainwindow.h>
#include <qaction.h>
#include <qlabel.h>


class OQDockBar;


class OQDockButton : public QAction
{
	Q_OBJECT

public:
	OQDockButton(OQDockBar *parent, QToolBar *toolbar,
				const QString& icon_on, const QString& icon_off,
				const QString& tip_on, const QString& tip_off,
				bool toggle = true);

	OQDockBar *getDockBar() { return (OQDockBar *) parent(); }

public slots:
	void setOn(bool on);

private:

	QString icon_on, icon_off, tip_on, tip_off;
};


class OQProgramButton : public OQDockButton
{
	Q_OBJECT

public:
	OQProgramButton(OQDockBar *dockbar, QToolBar *toolbar,
					const QString& program, const QString& param,
					const QString& icon, const QString& tip);
public slots:
	void buttonClicked();
	void updateState();

protected:
	QString program, param;
};


class OQServerBox : public OQDockButton
{
	Q_OBJECT

public:
	OQServerBox(OQDockBar *dockbar, QToolBar *toolbar);

public slots:
	void boxClicked();
	void updateState();
};


class OQServerStatus : public OQDockButton
{
	Q_OBJECT

public:
	OQServerStatus(OQDockBar *dockbar, QToolBar *toolbar);

public slots:
	void updateState(const QString&, const QString&, const QString&);
};


class OQTimeLabel : public QLabel
{
public:
	OQTimeLabel(const QString& region, QWidget *parent);
	void set(int yy, int mm, int dd, int hh, int mi, int ss);
	void set() { set(0,0,0, 0,0,0); }

protected:
	const QString region;
};


class OQScriptBox : public OQDockButton
{
	Q_OBJECT

public:
	OQScriptBox(OQDockBar *dockbar, QToolBar *toolbar);

public slots:
	void boxClicked();
	void updateState();
};


class OQDockBar : public QMainWindow
{
	Q_OBJECT

public:

	OQDockBar(OQWatch *watch);

	void updateProcessList();
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

private:

	void createProgramButtons(QToolBar *top);
	void timerEvent(QTimerEvent *);

	OQWatch *watch;
	QString subject_list;

	QString proclist;
	int proclist_timer;

	QWidget *subjectbox;
	OQServerBox *serverbox;

	OQTimeLabel *simtime;
	ooid_t simtime_sec_id;
	OQTimeLabel *walltime;
	int walltime_timer;
};

