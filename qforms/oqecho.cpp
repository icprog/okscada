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

  Optikus Message Viewer.

*/

#include <qlayout.h>
#include <qframe.h>

#include "oqcommon.h"
#include "oqecho.h"


#define POLL_INTERVAL		200


static const QFont TEXT_FONT("Courier New", 13, QFont::Bold);


OQEcho::OQEcho()
	: QWidget()
{
	QVBoxLayout *vbox = new QVBoxLayout(this);
	vbox->setMargin(4);
	vbox->setSpacing(4);

	scroll = new QMultiLineEdit(this);
	vbox->addWidget(scroll);
	scroll->setTextFormat(scroll->PlainText);
	scroll->setWordWrap(scroll->NoWrap);
	scroll->setFont(TEXT_FONT);
	scroll->clear();

	QFrame *sep = new QFrame(this);
	sep->setFrameStyle(QFrame::HLine | QFrame::Sunken);
	vbox->addWidget(sep);

	QHBoxLayout *cmdlay = new QHBoxLayout(vbox);
	QButton *cmdbtn = new OQPushButton("Command", "kpic/configure.png", this);
	cmdlay->addWidget(cmdbtn);
	line = new QLineEdit(this);
	cmdlay->addWidget(line);
	connect(cmdbtn, SIGNAL(clicked()), SLOT(commandEntered()));
	connect(line, SIGNAL(returnPressed()), SLOT(commandEntered()));

	QHBoxLayout *btns = new QHBoxLayout(vbox);
	btns->addStretch(1);
	QButton *copyb = new OQPushButton("Co&py", "kpic/editcopy.png", this);
	btns->addWidget(copyb);

	QButton *clearb = new OQPushButton("&Clear", "kpic/editclear.png", this);
	btns->addWidget(clearb);
	connect(clearb, SIGNAL(clicked()), scroll, SLOT(clear()));

	QButton *helpb = new OQPushButton("&Help", "kpic/help.png", this);
	btns->addWidget(helpb);
	connect(helpb, SIGNAL(clicked()), SLOT(helpClicked()));

	QButton *quitb = new OQPushButton("&Quit", "kpic/toggle_log.png", this);
	btns->addWidget(quitb);
	connect(quitb, SIGNAL(clicked()), qApp, SLOT(quit()));

	proc = 0;
	restart();
	startTimer(POLL_INTERVAL);
}


OQEcho::~OQEcho()
{
	kill();
}


void
OQEcho::kill()
{
	if (proc && proc->isRunning()) {
		proc->tryTerminate();
		if (proc->isRunning())
			proc->kill();
	}
	delete proc;
	proc = 0;
}


void
OQEcho::restart()
{
	kill();
	QString cmd = OQApp::bin.filePath("sample-echo");
	proc = new QProcess(cmd);
	proc->setWorkingDirectory(OQApp::root);
	proc->start();
	scroll->setUnderline(true);
	output("Starting...");
	scroll->setUnderline(false);
}


void
OQEcho::commandEntered()
{
	command(line->text());
}


void
OQEcho::helpClicked()
{
	command("help");
}


void
OQEcho::command(const QString& _cmd)
{
	QString cmd = _cmd.stripWhiteSpace().simplifyWhiteSpace();
	if (!cmd.isEmpty()) {
		proc->writeToStdin(cmd + "\n");
		scroll->setItalic(true);
		output(cmd);
		scroll->setItalic(false);
	}
}


void
OQEcho::output(const QString& _str)
{
	QString str = _str;
	str = str.stripWhiteSpace();
	if (str.startsWith("echo>"))
		str = str.remove(0, 5);
	if (str.startsWith("ECHO:"))
		str = str.remove(0, 5);
	str = str.stripWhiteSpace();
	if (!str.isEmpty())
		scroll->append(str);
}


void
OQEcho::timerEvent(QTimerEvent *e)
{
	while (proc->canReadLineStderr()) {
		QString str = proc->readLineStderr();
		output(str);
	}
	while (proc->canReadLineStdout()) {
		QString str = proc->readLineStdout();
		output(str);
	}
	if (!proc->isRunning())
		restart();
}


int
main(int argc, char **argv)
{
	OQApp app("oqecho", argc, argv);
	OQEcho win;
	app.setMainWidget(&win);
	win.resize(800, 400);
	win.setCaption("Optikus Message Viewer");
	win.setIcon(OQPixmap("tools/book.png"));
	win.show();
	return app.exec();
}

