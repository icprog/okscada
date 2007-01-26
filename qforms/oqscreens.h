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

  Optikus Screen Viewer.

*/

#include "oqcanvas.h"
#include "ocwidgets.h"
#include "oqcommon.h"

#include <qkeysequence.h>
#include <qmainwindow.h>
#include <qdict.h>
#include <qpushbutton.h>
#include <qdialog.h>


class QAction;
class QWidget;
class QToolBar;
class QStatusBar;
class QSignalMapper;
class QLineEdit;
class OQScreenList;


class OQInputPopup : public QDialog
{
	Q_OBJECT

public:

	OQInputPopup(QWidget *parent, const char *name,
				OCDataWidget *dw, const QPoint& gpos);
	virtual ~OQInputPopup();

protected slots:

	void write();
	void click(int btn);
	void valueChanged()		{ changed = true; }
	void handleCancel()		{ click(0); }
	void handleOK()			{ click(1); }
	void handleApply()		{ click(2); }

protected:

	bool changed;
	QLineEdit *edit;
	QPushButton *apply;
	OCDataWidget *dw;
};


class OQScreenView : public OQCanvasView
{
	Q_OBJECT

public:

	OQScreenView(QWidget *parent, QToolBar *tb, QStatusBar *sb);
	void setupWidgets(QWidget *panel, QStatusBar *statusbar);
	void setupActions(QToolBar *toolbar);
	void setStatus(const QString &);
	void connectTo(OQScreenList *list);

public slots:

	bool canClose();
	void toggleList(bool);
	void touchList();
	void closePopups();

signals:

	void screenClosing(const QString& name);
	void listToggled(bool);
	void openRoot();

protected:

	void processMouseEvent(QMouseEvent *event, OCWidget *widget);
	void createPopup(OCWidget *widget, const QPoint& mouse_ptr);

private:

	QAction *toggleListAc;
	QStatusBar *statusbar;
	bool connected;
};


class OQScreenWindow : public OQCanvasWindow
{
public:
	OQScreenWindow();
	OQScreenView *screenView()		{ return (OQScreenView *) canvasView(); }
};


class OQScreenList : public QMainWindow, public OQuitFilter
{
	Q_OBJECT

public:

	OQScreenList();
	void addScreen(const QString& screen, const QString& title = 0);
	void setRoot(const QString& root);
	bool exists(const QString& screen);
	bool alone();
	bool canQuit();
	bool hasTestMode() const;

	static bool log_open_close;

public slots:

	void openScreen(const QString& screen);
	void closeScreen(const QString& screen);
	void openRoot(bool force = true);
	void showList()		{ toggleList(true); }
	void hideList()		{ toggleList(false); }
	void toggleList(bool on);
	void setTestMode(bool on);

signals:
	void listToggled(bool);

protected slots:

	void toggled(const QString &screen);
	void closeEvent(QCloseEvent *e);

private:

	QString toScreen(const QString& name);
	void setScreenButton(const QString& screen, bool on);
	void populateList(QString screen_dir, QString list_file);

	QWidget *blist;
	QDict<QPushButton> bdict;
	QDict<OQScreenWindow> sdict;
	QSignalMapper *sigmap;
	QString root;
	QString first;
	QAction *testAc;
};
