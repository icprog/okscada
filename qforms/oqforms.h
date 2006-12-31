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

#include <qkeysequence.h>
#include <qmainwindow.h>

#include "oqcanvas.h"
#include "ocwidgets.h"


class QAction;
class QWidget;
class QToolBar;
class QStatusBar;


class OQFormView : public OQCanvasView
{
	Q_OBJECT

public:

	OQFormView(QWidget *parent);
	void setupWidgets(QWidget *panel, QStatusBar *statusbar);
	void setupActions(QMenuBar *menubar, QToolBar *toolbar);
	void setStatus(const QString &);

public slots:

	void showList(bool);
	void hideList(bool);
	void closeWindow();
	void openRoot();
	void quitAll();

private:

	QAction *newAction(QToolBar *toolbar, const char *text, const char *icon,
						const char *slot, bool toggle = false,
						QKeySequence accel = 0);
	QAction *showListAc;
	QAction *hideListAc;

	QStatusBar *statusbar;
};


class OQFormWindow : public QMainWindow
{
public:
	OQFormWindow(const QString& name);
};
