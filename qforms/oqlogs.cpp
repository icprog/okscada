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

  Dynamic log display.

*/

#include <qapplication.h>
#include <qwidget.h>
#include <qframe.h>
#include <qvbox.h>
#include <qlayout.h>
#include <qscrollview.h>
#include <qvgroupbox.h>
#include <qpushbutton.h>

#include "oqcommon.h"
#include "oqlogs.h"

#include <stdio.h>


static const QFont TEXT_FONT("Courier New", 13, QFont::Bold);
static const int MAX_LINE = 1000;
static const int MAX_CHUNK = 10000;
static const int MINOR_PERIOD = 100;
static const int MAJOR_PERIOD = 5;


OQLogView::OQLogView(const QString& title, QWidget *parent)
	: QMultiLineEdit(parent)
{
	setTextFormat(LogText);
	setWordWrap(NoWrap);
	setCurrentFont(TEXT_FONT);
	setFont(TEXT_FONT);
	setMaxLogLines(MAX_CHUNK);
	clear();
	file.setName(OQApp::logs.filePath(title + ".log"));
	notifier = 0;
	idle = 0;
	recharge();
	scrollToBottom();
	startTimer(MINOR_PERIOD);
}


void
OQLogView::recharge()
{
	if (file.handle() != -1)
		return;
	struct stat st;
	if (stat( file.name(), &st ) < 0)
		return;
	file.open(IO_ReadOnly);
	if (file.handle() == -1)
		return;
	inode = st.st_ino;
	put(">>> file opened <<<\n");
	idle = 0;
	if (notifier != 0)
		delete notifier;
	notifier = new QSocketNotifier(file.handle(), QSocketNotifier::Read);
	connect(notifier, SIGNAL(activated(int)), SLOT(rescan()));
	notifier->setEnabled(false);
	rescan();
}


void
OQLogView::rescan()
{
	QString line;
	if (file.handle() == -1)
		return;
	while (file.readLine(line, MAX_LINE) > 0) {
		put(line);
		idle = 0;
	}
}


void
OQLogView::timerEvent(QTimerEvent *e)
{
	rescan();
	if (idle++ < MAJOR_PERIOD || idle % MAJOR_PERIOD != 0)
		return;
	struct stat st;
	if (stat(file.name(), &st) < 0)
		return;
	if (st.st_ino == inode)
		return;
	file.close();
	recharge();
}


void
OQLogView::put(const QString& msg)
{
	int para_from, index_from, para_to, index_to;
	getSelection(&para_from, &index_from, &para_to, &index_to);
	setCurrentFont(TEXT_FONT);
	setFont(TEXT_FONT);
	QString str = msg;
	if (str.endsWith("\n"))
		str.setLength(msg.length() - 1);
	int pos;
	while ((pos = str.find('<')) >= 0)
		str = str.replace(pos, 1, "&lt;");
	while ((pos = str.find('>')) >= 0)
		str = str.replace(pos, 1, "&gt;");
	append(str);
	setSelection(para_from, index_from, para_to, index_to);
}


OQLogTab::OQLogTab(const QString& title, QTabWidget *tabs)
	: QWidget(tabs)
{
	QVBoxLayout *vbox = new QVBoxLayout(this);
	vbox->setMargin(4);
	vbox->setSpacing(4);

	QVGroupBox *group = new QVGroupBox(title, this);
	vbox->addWidget(group);

	OQLogView *log = new OQLogView(title, group);

	QHBoxLayout *bar = new QHBoxLayout(vbox);
	bar->setMargin(4);
	bar->setSpacing(4);
	bar->addStretch(1);

	QButton *clearbtn = new OQPushButton("&Clear", "kpic/editclear.png", this);
	bar->addWidget(clearbtn);
	connect(clearbtn, SIGNAL(clicked()), log, SLOT(clear()));

	QButton *quitbtn = new OQPushButton("&Quit", "kpic/button_cancel.png", this);
	bar->addWidget(quitbtn);
	connect(quitbtn, SIGNAL(clicked()), qApp, SLOT(quit()));

	tabs->addTab(this, title);
}


int
main(int argc, char **argv)
{
	OQApp app("oqlogs", argc, argv);
	QVBox win;
	win.setMinimumSize(400, 200);
	win.resize(800, 400);
	QTabWidget *tabs = new QTabWidget(&win);

	new OQLogTab("scripts", tabs);
	new OQLogTab("remote",  tabs);
	new OQLogTab("ohub",    tabs);
	new OQLogTab("pforms",  tabs);
	new OQLogTab("qforms",  tabs);

	app.setMainWidget(&win);
	win.setCaption("Optikus Log Viewer");
	win.setIcon(OQPixmap("kpic/toggle_log.png"));
	win.show();
	return app.exec();
}

