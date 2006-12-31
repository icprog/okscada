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

  Optikus Display Viewer.

*/

#include <qstatusbar.h>
#include <qaction.h>
#include <qpopupmenu.h>
#include <qtoolbar.h>


#include "oqcommon.h"
#include "oqforms.h"


#define qDebug(...)


void
OQFormView::closeWindow()
{
	qDebug("closeWindow");
}


void
OQFormView::showList(bool on)
{
	qDebug(QString("showList: %1").arg(on));
	hideListAc->setOn(!on);
}


void
OQFormView::hideList(bool off)
{
	showListAc->setOn(!off);
	qDebug("fileSave");
}


void
OQFormView::openRoot()
{
	qDebug("openRoot");
}


void
OQFormView::quitAll()
{
	qDebug("quitAll");
	qApp->quit();
}


void
OQFormView::setStatus(const QString &msg)
{
	statusbar->message(msg);
}


QAction *
OQFormView::newAction(QToolBar *toolbar, const char *text, const char *icon,
						const char *slot, bool toggle, QKeySequence accel)
{
	QAction *action;
	action = new QAction(OQPixmap(icon), text, accel, this);
	if (toggle) {
		action->setToggleAction(true);
		if (slot)
			connect(action, SIGNAL(toggled(bool)), slot);
	} else {
		if (slot)
			connect(action, SIGNAL(activated()), slot);
	}
	action->addTo(toolbar);
	return action;
}


OQFormView::OQFormView(QWidget *parent)
	: OQCanvasView(parent)
{
}


void
OQFormView::setupWidgets(QWidget *panel, QStatusBar *_statusbar)
{
	statusbar = _statusbar;
	statusbar->clear();
}


void
OQFormView::setupActions(QMenuBar *menubar, QToolBar *toolbar)
{
	newAction(toolbar, "Close Window", "kpic/stop.png", SLOT(closeWindow()));
	showListAc = newAction(toolbar, "Show List", "kpic/view_tree.png",
							SLOT(showList(bool)), true);
	hideListAc = newAction(toolbar, "Hide List", "kpic/view_remove.png",
							SLOT(hideList(bool)), true);
	hideListAc->setOn(true);
	newAction(toolbar, "Open Root Display", "tools/tree.png", SLOT(openRoot()));
	toolbar->setStretchableWidget(new QWidget(toolbar));	// spacer
	newAction(toolbar, "Quit all Forms", "kpic/exit.png", SLOT(quitAll()));
}


OQFormWindow::OQFormWindow(const QString& name)
	: QMainWindow()
{
	OQFormView *formview = new OQFormView(this);
	formview->setupWidgets(0, statusBar());
	formview->setupActions(0, new QToolBar(this));
	setCentralWidget(formview);
	setCaption("Display Viewer: " + name);
	setIcon(OQPixmap("kpic/frame_image.png"));
	resize(600, 500);
}


int
main(int argc, char **argv)
{
	OQApplication app("oqforms", argc, argv);
	OQFormWindow win("Root");
	app.setMainWidget(&win);
	win.show();
	return app.exec();
}
