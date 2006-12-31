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
#include <qframe.h>
#include <qlabel.h>
#include <qtooltip.h>
#include <qdir.h>
#include <qdatetime.h>
#include <qobjectlist.h>
#include <qstringlist.h>
#include <qmessagebox.h>

#include "oqdock.h"

#include <stdlib.h>
#include <iostream>
#include <optikus/log.h>


/*				OQDockBar				*/

#define TIME_SUBJECT		"sample"
#define TIME_VAR			TIME_SUBJECT "@gm_time.tm_"
#define PROCLIST_PERIOD		3000
#define WALLTIME_PERIOD		500


//
// .
//
OQDockBar::OQDockBar(OQWatch *_watch)
{
	QVBoxLayout *vert = new QVBoxLayout(this);

	QHBoxLayout *upper = new QHBoxLayout;
	upper->setMargin(4);
	upper->setSpacing(10);
	vert->addLayout(upper);

	QHBoxLayout *lower = new QHBoxLayout;
	lower->setMargin(4);
	lower->setSpacing(10);
	vert->addLayout(lower);

	progbar = new QHBoxLayout;
	progbar->setSpacing(4);
	timebar = new QVBoxLayout;
	upper->addLayout(progbar);
	upper->addStretch(1);
	QFrame *sep1 = new QFrame(this);
	sep1->setFrameStyle(QFrame::VLine | QFrame::Sunken);
	upper->addWidget(sep1, 0);
	upper->addLayout(timebar);

	subjectbar = new QHBoxLayout;
	statebar = new QHBoxLayout;
	statebar->setSpacing(4);
	lower->addLayout(subjectbar);
	lower->addStretch(1);
	QFrame *sep2 = new QFrame(this);
	sep2->setFrameStyle(QFrame::VLine | QFrame::Sunken);
	lower->addWidget(sep2, 0);
	lower->addLayout(statebar);

	createProgramButtons();
	createOtherWidgets();
	proclist_timer = startTimer(PROCLIST_PERIOD);
	walltime_timer = startTimer(WALLTIME_PERIOD);

	simtime_sec_id = 0;

	watch = _watch ?: watch->getInstance();

	connect(watch,
		SIGNAL(subjectsUpdated(const QString&, const QString&, const QString&)),
		SLOT(updateSubjects(const QString&, const QString&, const QString&)));

	connect(watch,
		SIGNAL(dataUpdated(const owquark_t&, const QVariant&, long)),
		SLOT(updateData(const owquark_t&, const QVariant&, long)));
}


//
// .
//
void
OQDockBar::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == proclist_timer) {
		refreshProcessList();
	}
	else if (event->timerId() == walltime_timer) {
		QDateTime dt(QDateTime::currentDateTime());
		QDate d(dt.date());
		QTime t(dt.time());
		walltime->set(d.year(), d.month(), d.day(),
						t.hour(), t.minute(), t.second());
	}
}


//
// .
//
void
OQDockBar::updateSubjects(const QString& all_subjects,
							const QString& subject, const QString& state)
{
	if (all_subjects != subject_list) {
		subject_list = all_subjects;
		OQSubjectBox::updateGroupWidget(all_subjects, this, getSubjectBar());
	}
	OQSubjectBox::updateGroupWidget(subject, state, this, getSubjectBar());

	if (subject == TIME_SUBJECT) {
		if (state.isEmpty()) {
			simtime->set();
			simtime_sec_id = 0;
		} else {
			watch->optimizeReading(TIME_VAR "year");
			watch->optimizeReading(TIME_VAR "mon");
			watch->optimizeReading(TIME_VAR "mday");
			watch->optimizeReading(TIME_VAR "hour");
			watch->optimizeReading(TIME_VAR "min");
			watch->optimizeReading(TIME_VAR "sec");
		}
	}
}


//
// .
//
void
OQDockBar::updateData(const owquark_t& info, const QVariant& v, long time)
{
	if (simtime_sec_id == 0) {
		simtime_sec_id = watch->getOoidByDesc(TIME_VAR "sec");
	}
	if (info.ooid == simtime_sec_id) {
		simtime->set(
			watch->getInt(TIME_VAR "year"), watch->getInt(TIME_VAR "mon"),
			watch->getInt(TIME_VAR "mday"), watch->getInt(TIME_VAR "hour"),
			watch->getInt(TIME_VAR "min"), watch->getInt(TIME_VAR "sec"));
	}
}


//
// .
//
void
OQDockBar::refreshProcessList()
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


//
// .
//
bool
OQDockBar::processIsAlive(const char *program)
{
	QString lookfor = QString(" ") + program + ":";
	return (proclist.find(lookfor) >= 0);
}


//
// .
//
void
OQDockBar::createOtherWidgets()
{
	simtime = new OQTimeLabel("SIM", this);
	walltime = new OQTimeLabel("MSK", this);

	serverbox = new OQServerBox(this, statebar);
	new OQServerStatus(this, statebar);
	new OQScriptBox(this, statebar);
}


//
// .
//
void
OQDockBar::enableHub(bool on)
{
	OQApplication *app = OQApplication::oqApp;
	QString cmd = app->bin_home + "/";
	cmd.append(on ? "oscan -E" : "ocontrol stop");
	if (!app->log_file.isNull())
		cmd.append(" >> " + app->log_file + " 2>&1");
	cmd.append(" &");
	//std::cout << "execute: " << cmd << "\n";
	system(cmd);
}


//
// .
//
void
OQDockBar::checkinHub()
{
	refreshProcessList();
	if (!serverbox->isOn())
		enableHub(true);
}


//
// .
//
void
OQDockBar::createProgramButtons()
{
	new OQProgramButton(this, "oqecho", "",
						"tools/book.png", "Message Viewer");
	new OQProgramButton(this, "oqmsg", "",
						"tools/sb_uvo.png", "Message Center");
	new OQProgramButton(this, "oqmap", "",
						"kpic/fork.png", "Map View");
	new OQProgramButton(this, "oqforms", "root",
						"tools/tree.png", "Display Viewer");
	new OQProgramButton(this, "oqlogs", "",
						"kpic/toggle_log.png", "Log Viewer");
	new OQProgramButton(this, "oqsee", "",
						"kpic/viewmag.png", "Data Viewer");
	new OQProgramButton(this, "oqedit", "",
						"kpic/colorize.png", "Display Editor");
}


/*				OQDockButton				*/


//
// .
//
OQDockButton::OQDockButton(OQDockBar *parent, QBoxLayout *layout,
					const char *_icon_on, const char *_icon_off,
					const char *_tooltip_on, const char *_tooltip_off,
					bool toggle, int size)
	: OQPushButton(0, 0, parent)
{
	setFocusPolicy(NoFocus);
	setFlat(true);
	setToggleButton(toggle);
	if (size > 0) {
		setMinimumSize(size, size);
		setMaximumSize(size, size);
	}
	icon_on = _icon_on;
	icon_off = _icon_off ?: _icon_on;
	tooltip_on = _tooltip_on;
	tooltip_off = _tooltip_off ?: _tooltip_on;
	setOn(false);
	layout->addWidget(this);
}


//
// .
//
void
OQDockButton::setOn(bool on)
{
	setPixmap(OQPixmap(on ? icon_on : icon_off));
	const char *tooltip = on ? tooltip_on : tooltip_off;
	if (tooltip)
		QToolTip::add(this, tooltip);
	else
		QToolTip::remove(this);
	OQPushButton::setOn(on);
}


/*				OQServerBox				*/

//
// .
//
OQServerBox::OQServerBox(OQDockBar *dockbar, QBoxLayout *layout)
	: OQDockButton(dockbar, layout,
			"kpic/viewmag.png", "kpic/viewmagoff.png",
			"Server: RUNS", "Server: STOPPED", true, 36)
{
	connect(dockbar, SIGNAL(processesUpdated(const QString&)), SLOT(updateState()));
	connect(this, SIGNAL(clicked()), SLOT(boxClicked()));
}


//
// .
//
void
OQServerBox::updateState()
{
	setOn(getDockBar()->processIsAlive("ohubd"));
}


//
// .
//
void
OQServerBox::boxClicked()
{
	getDockBar()->enableHub(isOn());
}


/*				OQServerStatus				*/

//
// .
//
OQServerStatus::OQServerStatus(OQDockBar *dockbar, QBoxLayout *layout)
	: OQDockButton(dockbar, layout,
			"kpic/reload.png", "kpic/player_stop.png",
			"Current Mode: RUNNING", "Current Mode: STOP", false, 36)
{
	connect(OQWatch::getInstance(),
		SIGNAL(subjectsUpdated(const QString&, const QString&, const QString&)),
		SLOT(updateState(const QString&, const QString&, const QString&)));
}


//
// .
//
void
OQServerStatus::updateState(const QString& list,
							const QString& subject, const QString& state)
{
	if (subject == "*")
		setOn(!state.isEmpty());
}


/*				OQScriptBox				*/

//
// .
//
OQScriptBox::OQScriptBox(OQDockBar *dockbar, QBoxLayout *layout)
	: OQDockButton(dockbar, layout,
			"kpic/exec.png", "kpic/editclear.png",
			"scenarios are running.\nPress here to terminate them.",
			"scenarios not found.\nYou can press here to be sure...",
			false, 36)
{
	connect(dockbar, SIGNAL(processesUpdated(const QString&)), SLOT(updateState()));
	connect(this, SIGNAL(clicked()), SLOT(boxClicked()));
}


//
// .
//
void
OQScriptBox::updateState()
{
	setOn(getDockBar()->processIsAlive("otsp"));
}


//
// .
//
void
OQScriptBox::boxClicked()
{
	int bno = QMessageBox::question(this,
					tr("Kill Scripts Confirmation"),
					tr("Are you sure to stop\nrunning scenarios ?"),
					tr("&Yes"), tr("&No"), QString::null, 0, 1);
	if (bno == 0) {
		QString cmd = OQApplication::bin_home + "/" + "ocontrol cleanup";
		olog("running scripts killed");
	}
}


/*				OQProgramButton				*/

//
// .
//
OQProgramButton::OQProgramButton(OQDockBar *dockbar,
								const char *_program, const char *_param,
								const char *icon, const char *tooltip)
	: OQDockButton(dockbar, dockbar->getProgBar(), icon, 0, tooltip, 0)
{
	program = _program;
	param = _param;
	connect(this, SIGNAL(clicked()), SLOT(buttonClicked()));
	connect(dockbar, SIGNAL(processesUpdated(const QString&)),
			SLOT(updateState()));
}


//
// .
//
void
OQProgramButton::updateState()
{
	setOn(getDockBar()->processIsAlive(program));
}


//
// .
//
void
OQProgramButton::buttonClicked()
{
	if (isOn()) {
		QString cmd = OQApplication::bin_home + "/" + program;
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
	getDockBar()->refreshProcessList();
}


/*				OQTimeLabel				*/

//
// .
//
OQTimeLabel::OQTimeLabel(const char *_region, OQDockBar *dockbar)
	: QLabel(dockbar)
{
	setTextFormat(PlainText);
	setFont(QFont("Courier New", 13, QFont::Bold));
	dockbar->getTimeBar()->addWidget(this);
	region = _region;
	set();
}


//
// .
//
void
OQTimeLabel::set(int yy, int mm, int dd, int hh, int mi, int ss)
{
	QString text;
	if (yy <= 0 || mm <= 0 || dd <= 0 || hh < 0 || mi < 0 || ss < 0) {
		text.sprintf("%3s ----/--/-- --:--:--", region);
	} else {
		text.sprintf("%3s %04d/%02d/%02d %03d:%02d:%02d",
					region, yy, mm, dd, hh, mi, ss);
	}
	setText(text);
}


/*					main()					*/


int
main(int argc, char **argv)
{
	OQApplication app("oqdock", argc, argv);
	OQWatch watch("oqdock");
	OQDockBar dockbar;
	watch.connect();
	app.setMainWidget(&dockbar);
	dockbar.setCaption("Optikus Dock Bar");
	dockbar.setIcon(OQPixmap("kpic/blend.png"));
	dockbar.checkinHub();
	dockbar.show();
	return app.exec();
}

