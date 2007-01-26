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

  Common utilities.

*/

#ifndef OQCOMMON_H
#define OQCOMMON_H

#include <qpixmap.h>
#include <qstring.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qdir.h>


class QSignalMapper;


class OQPixmap : public QPixmap
{
public:
	OQPixmap(const QString& file, bool substitute = true);
};


class OQPushButton : public QPushButton
{
public:
	OQPushButton(const QString& text, const QString& icon = 0,
				QWidget *parent = 0);
};


class OQSubjectBox : public OQPushButton
{
	Q_OBJECT

public:
	OQSubjectBox(const char *name, QWidget *parent, QBoxLayout *layout = 0);
	void setState(bool online);
	void setState(const QString& state)		{ setState(!state.isEmpty()); }
	static void updateGroupWidget(const QString& all_subjects,
								QWidget *group, QBoxLayout *layout = 0);
	static void updateGroupWidget(const QString& subject, const QString& state,
								QWidget *group, QBoxLayout *layout = 0);

protected slots:
	void subjectClicked();
};


class OQuitFilter
{
public:
	virtual bool canQuit()		{ return true; }
};


class OQApp : public QApplication
{
	Q_OBJECT

public:
	OQApp(const QString& client_name, int &argc, char **argv,
			const QString& theme = "simple");
	virtual ~OQApp();

	void quitForce();
	void setQuitFilter(OQuitFilter *filter)		{ quit_filter = filter; }

	int runScript(const QString& script, const QString& lang);
	bool killScript(int pid);
	void killAllScripts();

	static void setTheme(const QString& theme);
	static void msleep(int msec, bool process_events = true);

signals:
	void scriptFinished(int pid, int exit_status);

public slots:
	virtual void quit();

protected slots:
	void scriptStartSlot(const QString& name);
	void scriptExitSlot(const QString& name);

protected:
	OQuitFilter *quit_filter;
	QSignalMapper *sm_start;
	QSignalMapper *sm_exit;

public:
	static OQApp * oqApp;

	static QDir  root;
	static QDir  share;
	static QDir  pics;
	static QDir  screens;
	static QDir  etc;
	static QDir  bin;
	static QDir  var;
	static QDir  logs;
	static QFile log;
	static bool  log_to_stdout;
};

#endif // OQCOMMON_H
