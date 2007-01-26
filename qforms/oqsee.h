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
class OQSeeRepeater;


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
	bool saveTo(const QString& name);

public slots:

	void fileOpen();
	void fileSave();
	void fileSaveAs();
	void fileQuit()		{ OQApp::oqApp->quit(); }

	void viewHex(bool);
	void setLogging(bool);
	void viewStateBox(bool);
	void moveUp()		{ swapRows(currentRow(), currentRow() - 1); }
	void moveDown()		{ swapRows(currentRow(), currentRow() + 1); }
	void selectUp()		{ selectRow(currentRow() - 1); }
	void selectDown()	{ selectRow(currentRow() + 1); }
	void swapRows(int, int);
	void selectRow(int);

	void playStart(bool);
	void playStop(bool);

	void helpAbout();

	void newCommand();

	int appendItem(const QString& name, bool hex, int *bg_p = 0);
	void monitorDone(long, const owquark_t&, int);

	void dataUpdated(const owquark_t&, const QVariant&, long time);
	void removeCurrent()		{ removeRow(currentRow(), false); }
	void removeBroken();
	void removeAll(bool sure = false);

	void subjectsUpdated(const QString&, const QString&, const QString&);

	int readValue(const QString& name, int *bg_p = 0);
	void readDone(long param, const owquark_t& inf,
					const QVariant& val, int err);
	int writeValue(const QString& name, const QString& val, int *bg_p = 0);
	void writeDone(long param, const owquark_t& inf, int err);

	void removeRow(int row, bool sure = true);
	void currentRowChanged(int row, int col);

	void beginUpdates();
	void endUpdates();

public:

	void resizeData(int)	{}
	QTableItem *item(int r, int c) const
	{
		return items.find(indexOf(r, c));
	}
	void setItem(int r, int c, QTableItem *i)
	{
		items.replace(indexOf(r, c), i);
	}
	void clearCell( int r, int c)
	{
		items.remove(indexOf(r, c));
	}
	void takeItem(QTableItem *item)
	{
		items.setAutoDelete(false);
		items.remove(indexOf(item->row(), item->col()));
		items.setAutoDelete(true);
	}
	void insertWidget(int r, int c, QWidget *w)
	{
		widgets.replace(indexOf(r, c), w);
	}
	QWidget *cellWidget(int r, int c) const
	{
		return widgets.find(indexOf(r, c));
	}
	void clearCellWidget(int r, int c)
	{
		QWidget *w = widgets.take(indexOf(r, c));
		if (w) {
			w->deleteLater();
		}
    }

private:

	struct Var
	{
		int no;
		ooid_t id;
		Var *next;
		QString desc;
		int row;
		int bg_id;
		QString ermes;
		int hex;
		long stamp;

		bool isHex()	{ return (hex > 0); }

		QString toString(const QVariant * val = 0)
		{
			if (hex <= 0)
				return (val ? val->toString() : OQWatch::readString(desc));
			return ("0x"
					+ QString("%1")
					.arg(val ? val->toUInt() : OQWatch::readLong(desc), 0, 16)
					.right(hex).rightJustify(hex, '0'));
		}
	};

	typedef Var * VarPtr;

	QAction *newAction(QPopupMenu *menu, const char *text, const char *icon,
						const char *slot, QKeySequence accel = 0,
						bool toggle = false);

	bool canDo(bool sure, const QString& text);
	bool areYouSure(bool sure, const QString& text);

	void setHexData(Var * v, bool on);
	void logAllValues();
	void logOneValue(Var *v, const QVariant *val = 0);

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
	OQSeeRepeater *mon_repeater;
	OQSeeRepeater *write_repeater;

	bool changed;
	QString filename;
	bool logging;

	QIntDict<QTableItem> items;
	QIntDict<QWidget> widgets;

	QMap<int,VarPtr> byno;
	QMemArray<VarPtr> byrow;
	QIntDict<Var> byid;

	static int next_no;
};


inline bool
OQSeeView::canDo(bool sure, const QString& text)
	{ return (sure || !editConfirmAc->isOn() || areYouSure(false, text)); }


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


#define OQSEE_REP_MAX_IDX	4

class OQSeeRepeater : public QObject
{
	Q_OBJECT

public:
	OQSeeRepeater(OQSeeView *view, OQWatch *watch);
	bool isRepeater(const QString& desc)	{ return parse(desc, true); }
	bool initMonitor(const QString& name, bool hex);
	bool initWrite(const QString& name, const QString& val);
	bool isActive()		{ return active; }
	void cancel();

public slots:
	void monitorDone(long p, const owquark_t& inf, int err)	{ done(p, err); }
	void writeDone(long p, const owquark_t& inf, int err)	{ done(p, err); }

private:
	OQSeeView *view;
	OQWatch *watch;
	bool active;
	QString format;
	QString value;
	bool hex;
	bool is_write;
	int cur_no;
	int cur_bg;
	int last_no;
	int last_err;
	int idx_num;
	int idx_cur[OQSEE_REP_MAX_IDX];
	int idx_min[OQSEE_REP_MAX_IDX];
	int idx_max[OQSEE_REP_MAX_IDX];

	bool next();
	void done(long param, int err);
	bool parse(const QString& desc, bool onlycheck = false);
	void setStatus(const QString& msg)	{ view->setStatus(msg); }
};

