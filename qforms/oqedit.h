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

  Optikus Display Editor.

*/

#include "oqcanvas.h"
#include "ocwidgets.h"
#include "oqcommon.h"

#include <qkeysequence.h>
#include <qdialog.h>


class QAction;
class QWidget;
class QPopupMenu;
class QMenuBar;
class QToolBar;
class QStatusBar;
class QSignalMapper;
class QPushButton;
class QLineEdit;
class OQEditView;
class OQEditWindow;


class OQGridCanvasItem : public QCanvasRectangle
{
public:

	OQGridCanvasItem(QCanvas *c);
	void setSnap(int dx, int dy);
	void getSnap(int& dx, int& dy);

protected:

	void drawShape(QPainter& p);

private:

	int dx, dy;
};


class OQPropEdit : public QDialog
{
	Q_OBJECT

public:

	OQPropEdit(OQEditView *parent, const char *name, const QString& caption,
				OCWidget *w, bool existing);
	virtual ~OQPropEdit();

protected slots:

	void loadProps();
	bool saveProps();
	void valueChanged();
	void clearChanged();
	void click(int btn);
	void handleCancel()		{ click(0); }
	void handleOK()			{ click(1); }
	void handleApply()	 	{ click(2); }
	void handleReload()		{ click(3); }
	void editSpecial(int row);
	void widgetGeometryChanged(OCWidget *);

signals:

	void widgetChanged();

protected:

	void buildProps();
	QLineEdit *getEdit(int i);
	OQEditView *editView()		{ return (OQEditView *) parentWidget(); }

	QStatusBar *statusbar;
	QSignalMapper *specmap;
	QWidget *box;
	QPushButton *apply;
	OCWidget *w;
	OCAttrList atts;
	bool changed : 1;
	bool existing : 1;
};


class OQEditView : public OQCanvasView, public OQuitFilter
{
	Q_OBJECT

public:

	OQEditView(OQEditWindow *parent);

	void setGrid(int step_x, int step_y);
	void setGrid(int step)		{ setGrid(step, step); }
	void setStatus(const QString& msg);
	void setTempStatus(const QString& msg);
	void setFileName(const QString& filename);

	bool load();
	bool save();
	bool canQuit();

	OQEditWindow * editWindow() const
		{ return (OQEditWindow *) parentWidget(); }

public slots:

	bool canClose();
	void clear();
	void canvasResized();
	void widgetChanged();
	void selectWidget(OCWidget *w);

	void fileNew();
	void fileOpen();
	void fileSave();
	void fileSaveAs();
	void fileQuit();

	void editDefFont();
	void editDefColor();
	void editBgImage();
	void editProps();
	void editCopy();
	void editDelete();
	void editSnapLess();
	void editSnapMore();
	void editRaise();
	void editLower();
	void setResizeMode(bool on);

	void setGrid1()		{ setGrid(1); }
	void setGrid4()		{ setGrid(4); }
	void setGrid8()		{ setGrid(8); }
	void setGrid16()	{ setGrid(16); }

	void helpAbout();

	void newWidget(int rtti);

signals:
	void widgetGeometryChanged(OCWidget *);

protected:

	QPoint align(QPoint pos);
	void setBaseSize();
	void viewportResizeEvent(QResizeEvent * event);

	void processMouseEvent(QMouseEvent *e, OCWidget *w);
	void leaveEvent(QEvent *e);
	void updateStatus();
	void closePropEdit();

	bool canDo(bool sure, const QString& text);
	bool areYouSure(bool sure, const QString& text);

	void timerEvent(QTimerEvent *e);
	void keyPressEvent(QKeyEvent *e);
	void keyReleaseEvent(QKeyEvent *e);

private:

	QAction *newAction(const QString& text, const QString& icon,
						const char *slot, bool toggle = false,
						QKeySequence accel = 0);
	QAction *newCreator(QPopupMenu *pm, QToolBar *tb,
						const QString& text, const QString& icon,
						int rtti, QKeySequence accel = 0);
	QAction *newCreator(QPopupMenu *pm, const QString& text, int rtti,
						QKeySequence accel = 0)
		{ return newCreator(pm, 0, text, 0, rtti, accel); }

	QSignalMapper *creator_map;

	int mouse_x, mouse_y;
	OCWidget *hover_widget;
	QString status_msg;
	QStatusBar *statusbar;
	int status_timer_id;
	QColor saved_status_bg_color;

	OQGridCanvasItem *grid_ci;
	QPopupMenu *widget_menu, *canvas_menu;

	OCWidget *sel_widget;
	QPoint rel_pos;
	QAction *resize_mode;
	bool saved_resize_mode;
	bool left_button_pressed;
	int size_timer_id;

	bool changed : 1;
	bool has_name : 1;
};


inline bool
OQEditView::canDo(bool sure, const QString& text)
	{ return (sure || areYouSure(false, text)); }


class OQEditWindow : public OQCanvasWindow
{
public:
	OQEditWindow();
	OQEditView *editView()		{ return (OQEditView *) canvasView(); }
};
