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

  Optikus Data Viewer.

*/

#include <qmainwindow.h>
#include <qaction.h>
#include <qtable.h>
#include <qintdict.h>
#include <qmemarray.h>
#include <qlineedit.h>
#include <qpushbutton.h>

#include "oqwatch.h"
#include "oqcommon.h"


class QPopupMenu;


class OQSeeView : public QTable, public OQuitFilter
{
	Q_OBJECT

public:

	OQSeeView(QWidget *parent);
	void setupWidgets(QWidget *panel, QStatusBar *statusbar);
	void setupActions(QMenuBar *menubar, QToolBar *toolbar);
	void paintCell(QPainter *p, int row, int col, const QRect& cr,
					bool selected, const QColorGroup &cg);
	void setStatus(const QString &);
	bool canQuit();

public slots:

	void fileNew();
	void fileOpen();
	void fileSave();
	void fileSaveAs();
	void fileQuit()		{ OQApplication::oqApp->quit(); }

	void editPaste();

	void viewHex(bool);
	void viewLog(bool);
	void viewStateBox(bool);

	void playStart(bool);
	void playStop(bool);

	void helpAbout();

	void newCommand();

	void monitorsUpdated(long, const owquark_t&, int);
	void dataUpdated(const owquark_t&, const QVariant&, long time);
	void subjectsUpdated(const QString&, const QString&, const QString&);

	void removeCurrent()		{ removeRow(currentRow(), false); }
	void removeBroken();
	void removeAll();

public slots:

	void removeRow(int row, bool sure = true);
	void currentRowChanged(int row, int col);

public:

	void resizeData(int)	{}
	QTableItem *item(int r, int c) const { return items.find(indexOf(r, c)); }
	void setItem(int r, int c, QTableItem *i) { items.replace(indexOf(r, c), i); }
	void clearCell( int r, int c) { items.remove(indexOf(r, c)); }
	void takeItem(QTableItem *item)
	{
		items.setAutoDelete(false);
		items.remove(indexOf(item->row(), item->col()));
		items.setAutoDelete(true);
	}
	void insertWidget(int r, int c, QWidget *w) { widgets.replace(indexOf(r, c), w); }
	QWidget *cellWidget(int r, int c) const { return widgets.find(indexOf(r, c)); }
	void clearCellWidget(int r, int c)
	{
		QWidget *w = widgets.take(indexOf(r, c));
		if (w) {
			w->deleteLater();
		}
    }

private:

	QAction *newAction(QPopupMenu *menu,
						const char *text, const char *icon,
						const char *slot, bool toggle = false,
						QKeySequence accel = 0);

	bool canDelete(bool sure, const char *text)
	{
		return (sure || !editConfirmAc->isOn() || areYouSure(false, text));
	}
	bool areYouSure(bool sure, const char *text);

	struct Var
	{
		int no;
		ooid_t id;
		QString desc;
		int row;
		int bg_id;
		QString ermes;
		int hex;
		bool isHex()	{ return (hex > 0); }
	};

	typedef Var * VarPtr;

	void setHexData(Var * v, bool on);

	QAction *playStartAc;
	QAction *playStopAc;
	QAction *editConfirmAc;
	QAction *viewLogAc;
	QAction *viewDefHexAc;
	QAction *viewHexAc;
	QAction *viewStateBoxAc;

	QLineEdit *inedit;
	QPushButton *inbtn;
	QWidget *statebox, *stategroup;
	QStatusBar *statusbar;

	OQWatch watch;

	bool changed;

	QIntDict<QTableItem> items;
	QIntDict<QWidget> widgets;

	QMap<int,VarPtr> byno;
	QMemArray<VarPtr> byrow;
	QIntDict<Var> byid;
};


class OQSeeWindow : public QMainWindow
{
public:
	OQSeeWindow();
	OQSeeView *getView()	{ return seeview; }
protected:
	void closeEvent(QCloseEvent * e);
private:
	OQSeeView *seeview;
};
