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

#include "oqdock.h"

#include <qlayout.h>
#include <qgroupbox.h>
#include <qdatetime.h>
#include <qobjectlist.h>
#include <qstringlist.h>
#include <qmessagebox.h>
#include <qdockarea.h>
#include <qstatusbar.h>

#include <optikus/log.h>


#define qDebug(...)

#define PROCLIST_PERIOD		2000


/* ======== Dock Bar ======== */


OQDockBar::OQDockBar(OQWatch *_watch)
	:	QMainWindow(),
		watch(_watch)
{
	setUsesBigPixmaps(true);
	setDockWindowsMovable(false);

	QToolBar *top = new QToolBar(this);
	createProgramButtons(top);
	top->setStretchableWidget(new QWidget(top));

	(new QFrame(top))->setFrameStyle(QFrame::VLine | QFrame::Sunken);
	serverbox = new OQServerBox(this, top);
	new OQServerStatus(this, top);
	new OQScriptBox(this, top);

	subjectbox = new QGroupBox(this);
	QBoxLayout *lo = new QHBoxLayout(subjectbox, 5, 2);
	lo->setAutoAdd(true);
	lo->addStretch(1);
	setCentralWidget(subjectbox);

	proclist_timer = startTimer(PROCLIST_PERIOD);

	connect(watch,
		SIGNAL(subjectsUpdated(const QString&, const QString&, const QString&)),
		SLOT(updateSubjects(const QString&, const QString&, const QString&)));

	top->adjustSize();
	resize(top->width() + 4, 1);
}


void
OQDockBar::createProgramButtons(QToolBar *top)
{
	new OQProgramButton(this, top, "oqecho", "",
						"tools/book.png", "Message Viewer");
	new OQProgramButton(this, top, "oqmsg", "",
						"tools/nav.png", "Message Center");
	new OQProgramButton(this, top, "oqmap", "",
						"tools/earth.png", "Map View");
	new OQProgramButton(this, top, "oqscreens", "root",
						"tools/tree.png", "Screen Viewer");
	new OQProgramButton(this, top, "oqlogs", "",
						"kpic/toggle_log.png", "Log Viewer");
	new OQProgramButton(this, top, "oqsee", "",
						"kpic/viewmag.png", "Data Viewer");
	new OQProgramButton(this, top, "oqedit", "",
						"kpic/colorize.png", "Screen Editor");
}


void
OQDockBar::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == proclist_timer) {
		updateProcessList();
	}
}


void
OQDockBar::updateSubjects(const QString& all_subjects,
							const QString& subject, const QString& state)
{
	if (all_subjects != subject_list) {
		subject_list = all_subjects;
		OQSubjectBox::updateGroupWidget(all_subjects, subjectbox, 0);
	}
	OQSubjectBox::updateGroupWidget(subject, state, subjectbox, 0);
}


void
OQDockBar::updateProcessList()
{
	QString newlist;
	QDir proc("/proc");
	if (proc.exists()) {
		proc.setFilter(QDir::Dirs);
		proc.setSorting(QDir::Unsorted);
		unsigned count = proc.count();
		for (unsigned i = 0; i < count; i++) {
			QString subdir = proc[i];
			if (!subdir[0].isDigit()) {
				continue;
			}
			QFile spec(proc.filePath(subdir) + "/cmdline");
			if (!spec.open(IO_ReadOnly)) {
				continue;
			}
			char buf[256];
			if (spec.readBlock(buf, sizeof buf) > 0) {
				QString name(buf);
				int pos = name.find(' ');
				if (pos >= 0)
					name.truncate(pos);
				pos = name.findRev('/');
				if (pos >= 0)
					name.remove(0, pos+1);
				if (name.length() > 0) {
					newlist.append(" ");
					newlist.append(name);
					newlist.append(":");
					newlist.append(subdir);
				}
			}
			spec.close();
		}
		newlist.append(" ");
	}
	if (newlist != proclist) {
		proclist = newlist;
		emit processesUpdated(newlist);
		serverbox->setOn(processIsAlive("ohubd"));
	}
}


bool
OQDockBar::processIsAlive(const char *program)
{
	QString lookfor = QString(" ") + program + ":";
	return (proclist.find(lookfor) >= 0);
}


void
OQDockBar::enableHub(bool on)
{
	QString cmd;
	if (on)
		cmd = OQApp::bin.filePath("oscan") + " -E";
	else
		cmd = OQApp::bin.filePath("ocontrol") + " stop";
	if (!OQApp::log.name().isNull())
		cmd.append(" >> " + OQApp::log.name() + " 2>&1");
	cmd.append(" &");
	qDebug("execute: " + cmd);
	system(cmd);
}


void
OQDockBar::checkinHub()
{
	updateProcessList();
	if (!serverbox->isOn())
		enableHub(true);
}


/* ======== OQDockButton ======== */


OQDockButton::OQDockButton(OQDockBar *parent, QToolBar *toolbar,
					const QString& _icon_on, const QString& _icon_off,
					const QString& _tip_on, const QString& _tip_off,
					bool toggle)
	:	QAction(parent),
		icon_on(_icon_on),
		icon_off(_icon_off.isNull() ? _icon_on : _icon_off),
		tip_on(_tip_on),
		tip_off(_tip_off.isNull() ? _tip_on : _tip_off)
{
	setToggleAction(toggle);
	setOn(false);
	addTo(toolbar);
}


void
OQDockButton::setOn(bool on)
{
	setIconSet(OQPixmap(on ? icon_on : icon_off));
	setToolTip(on ? tip_on : tip_off);
	if (isToggleAction())
		QAction::setOn(on);
}


/* ======== OQServerBox ======== */


OQServerBox::OQServerBox(OQDockBar *dockbar, QToolBar *toolbar)
	: OQDockButton(dockbar, toolbar,
			"kpic/exec.png", "kpic/stop.png",
			"Server: RUNS", "Server: STOPPED", true)
{
	connect(dockbar, SIGNAL(processesUpdated(const QString&)), SLOT(updateState()));
	connect(this, SIGNAL(activated()), SLOT(boxClicked()));
}


void
OQServerBox::updateState()
{
	setOn(getDockBar()->processIsAlive("ohubd"));
}


void
OQServerBox::boxClicked()
{
	getDockBar()->enableHub(isOn());
}


/* ======== OQServerStatus ======== */


OQServerStatus::OQServerStatus(OQDockBar *dockbar, QToolBar *toolbar)
	: OQDockButton(dockbar, toolbar,
			"kpic/player_play.png", "kpic/player_stop.png",
			"Current Mode: RUNNING", "Current Mode: STOP", false)
{
	connect(OQWatch::getInstance(),
		SIGNAL(subjectsUpdated(const QString&, const QString&, const QString&)),
		SLOT(updateState(const QString&, const QString&, const QString&)));
}


void
OQServerStatus::updateState(const QString& list,
							const QString& subject, const QString& state)
{
	if (subject == "*")
		setOn(!state.isEmpty());
}


/* ======== OQScriptBox ======== */


OQScriptBox::OQScriptBox(OQDockBar *dockbar, QToolBar *toolbar)
	: OQDockButton(dockbar, toolbar,
			"kpic/exec.png", "kpic/editclear.png",
			"scenarios are running.\nPress here to terminate them.",
			"scenarios not found.\nYou can press here to be sure...",
			false)
{
	connect(dockbar, SIGNAL(processesUpdated(const QString&)), SLOT(updateState()));
	connect(this, SIGNAL(activated()), SLOT(boxClicked()));
}


void
OQScriptBox::updateState()
{
	setOn(getDockBar()->processIsAlive("otsp"));
}


void
OQScriptBox::boxClicked()
{
	int ans = QMessageBox::warning(
					getDockBar(),
					"Kill Scripts Confirmation",
					"Are you sure to stop\nrunning scenarios ?",
					QMessageBox::Yes | QMessageBox::Default,
					QMessageBox::No | QMessageBox::Escape);
	if (ans == QMessageBox::Yes) {
		QString cmd = OQApp::bin.filePath("ocontrol") + " cleanup";
		olog("running scripts killed");
	}
}


/* ======== OQProgramButton ======== */


OQProgramButton::OQProgramButton(OQDockBar *dockbar, QToolBar *toolbar,
								const QString& _program, const QString& _param,
								const QString& icon, const QString& tooltip)
	:	OQDockButton(dockbar, toolbar, icon, 0, tooltip, 0),
		program(_program), param(_param)
{
	connect(this, SIGNAL(activated()), SLOT(buttonClicked()));
	connect(dockbar, SIGNAL(processesUpdated(const QString&)),
			SLOT(updateState()));
}


void
OQProgramButton::updateState()
{
	setOn(getDockBar()->processIsAlive(program));
}


void
OQProgramButton::buttonClicked()
{
	if (isOn()) {
		QString cmd = OQApp::bin.filePath(program);
		if (NULL != param && *param) {
			cmd.append(" ");
			cmd.append(param);
		}
		cmd.append(" &");
		system(cmd);
	} else {
		QString cmd = "pkill ";
		cmd.append(program);
		system(cmd);
	}
	getDockBar()->updateProcessList();
}


/* ======== main() ======== */


int
main(int argc, char **argv)
{
	OQApp app("oqdock", argc, argv);
	OQWatch watch("oqdock");
	OQDockBar dockbar(&watch);
	watch.connect();
	app.setMainWidget(&dockbar);
	dockbar.setCaption("Optikus Dock Bar");
	dockbar.setIcon(OQPixmap("kpic/blend.png"));
	dockbar.checkinHub();
	dockbar.show();
	return app.exec();
}
