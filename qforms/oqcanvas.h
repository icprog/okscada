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

  Visual canvas.

*/

#ifndef OQCANVAS_H
#define OQCANVAS_H

#include <optikus/watch.h>
#include <qcanvas.h>
#include <qstringlist.h>
#include <qmap.h>
#include <qmainwindow.h>
#include <qvariant.h>


#define ORTTI_RangeStart		0xb0000
#define ORTTI_RangeEnd			0xbffff

#define ORTTI_Widget			0xb0000
#define ORTTI_DataWidget		0xb0001
#define ORTTI_TimerWidget		0xb0002

#define OQCANVAS_FILE_EXT	".osx"


class OCWidget;

typedef QMapConstIterator<QString,QString> OCAttrListIterator;


class OCAttrList
{
public:
	OCAttrList()	{}
	OCAttrListIterator begin() const	{ return m.begin(); }
	OCAttrListIterator end() const		{ return m.end(); }
	int count()							{ return l.count(); }
	const QString& key(int i)			{ return l[i]; }
	const QString& type(int i) const	{ return m[l[i]]; }
	bool remove(const QString& key);
	void replace(const QString& key, const QString& type);
private:
	QMap<QString,QString> m;
	QStringList l;
};


inline bool
OCAttrList::remove(const QString& k)
	{ m.remove(k); return l.remove(k) > 0; }

inline void
OCAttrList::replace(const QString& k, const QString& t)
	{ m.replace(k, t); if (!l.contains(k)) l.append(k); }


class OQCanvas : public QCanvas
{
	Q_OBJECT

public:

	OQCanvas(QWidget *parent);

	void setEdit(bool on)		{ is_edit = on; }
	bool isEdit() const			{ return is_edit; }
	void setEditValue(const QVariant& val)	{ edit_mode_val = val; }
	QVariant getEditValue() const			{ return edit_mode_val; }

	void setTest(bool on);
	bool isTest() const				{ return is_test; }
	double currentTestSec() const	{ return test_sec; }

	void setBgImage(const QString& filename);
	QString getBgImage() const	{ return bg_image; }

	void setDefTextFont(const QString& font);
	void setDefTextColor(const QString& color);
	const QString& getDefTextFont() const		{ return def_text_font; }
	const QString& getDefTextColor() const		{ return def_text_color; }

	void setBaseSize(int w, int h, bool also_resize = true);
	QSize getBaseSize() const	{ return base_size; }
	void resize(int w, int h);
	QRect boundingRect(bool all_items = false);

	QWidget *getParent() const	{ return parent; }
	OCWidget *newBasicWidget(int rtti);
	int zRange(double& zmin, double& zmax);

	QCanvasItemList mouseTrackers();

	bool load(const QString& filename, QString *ermes_p);
	bool save(const QString& filename);
	void clear();

public slots:

	virtual void doOpenScreen(const QString& name)	{ emit openScreen(name); }
	virtual void updateDefaults();

signals:

	void openScreen(const QString& name);

protected:

	void timerEvent(QTimerEvent *e);

private:

	bool is_test;
	int test_timer_id;
	double test_sec;
	bool is_edit;
	QVariant edit_mode_val;
	QWidget *parent;
	QString bg_image;
	QSize base_size;
	QString def_text_font;
	QString def_text_color;

	static QMap<int,QString> rtti2class;
	static QMap<QString,int> class2rtti;
	static void setupClassMaps();

	// Mouse tracking
	friend class OCWidget;
	QCanvasItemList mtrackers;

public:

	static bool isMaybeOCWidget(QCanvasItem *item);
	static bool isValidOCWidget(QCanvasItem *item);
	static int classToRtti(const QString& klass);
	static const QString& rttiToClass(int rtti);
};


inline bool
OQCanvas::isMaybeOCWidget(QCanvasItem *ci)
	{ return ci->rtti() >= ORTTI_RangeStart && ci->rtti() <= ORTTI_RangeEnd; }

inline bool
OQCanvas::isValidOCWidget(QCanvasItem *ci)
	{ return !rttiToClass(ci->rtti()).isNull(); }

inline int
OQCanvas::classToRtti(const QString& klass)
	{ return class2rtti[klass]; }

inline const QString&
OQCanvas::rttiToClass(int rtti)
	{ return rtti2class[rtti]; }


class OQCanvasView : public QCanvasView
{
	Q_OBJECT

public:

	OQCanvasView(QWidget *parent);
	virtual ~OQCanvasView();
	QSize sizeHint() const;

	void setFileName(const QString& filename,
					bool polish = true, bool update_title = true);
	QString getFileName() const		{ return filename; }

	virtual bool load(QString *ermes_p = 0);
	virtual bool save();
	void adjustWindowSize();

	OQCanvas *getCanvas() const		{ return (OQCanvas *) canvas(); }
	OCWidget *newBasicWidget(int rtti);

public slots:

	virtual void clear();
	virtual bool canClose();

protected:

	void contentsMousePressEvent(QMouseEvent* e)	{ processMouseEvent(e); }
	void contentsMouseReleaseEvent(QMouseEvent* e)	{ processMouseEvent(e); }
	void contentsMouseMoveEvent(QMouseEvent* e)		{ processMouseEvent(e); }
	void contentsMouseDoubleClickEvent(QMouseEvent* e) { processMouseEvent(e); }

	virtual void processMouseEvent(QMouseEvent *e);
	virtual void processMouseEvent(QMouseEvent *e, OCWidget *w);

	void viewportResizeEvent(QResizeEvent * event);

private:

	QString filename;
};


inline OCWidget *
OQCanvasView::newBasicWidget(int rtti)
	{ return getCanvas()->newBasicWidget(rtti); }


class OQCanvasWindow : public QMainWindow
{
	Q_OBJECT

public:

	OQCanvasWindow(const QString& titleprefix);
	virtual ~OQCanvasWindow();
	void setCaption(const QString& name = 0);

	void setCanvasView(OQCanvasView *cview);
	OQCanvasView *canvasView() const		{ return cview; }

protected:

	void closeEvent(QCloseEvent *e);

private:

	OQCanvasView *cview;
	QString titleprefix;
};


class OCWidget : public QObject, public QCanvasRectangle
{
	Q_OBJECT

public:

	OCWidget(OQCanvas *canvas);
	virtual ~OCWidget();

	virtual void raise();
	virtual void lower();

	virtual void move(double x, double y);
	virtual bool resize(double w, double h);

	double width() const	{ return user_w; }
	double height() const	{ return user_h; }

	QSize usize() const		{ return QSize(int(user_w), int(user_h)); }
	QSize bsize() const		{ return boundingRect().size(); }

	QRect boundingRect() const;
	void draw(QPainter& p);
	void drawShape(QPainter& p)		{}
	void setMarkBounds(bool on);
	bool hasMarkBounds() const		{ return mark_bounds; }

	void setTrackMouse(bool on);
	bool tracksMouse() const	{ return track_mouse; }

	virtual void polish();
	virtual void attrList(OCAttrList&) const;
	virtual bool setAttr(const QString& name, const QVariant& value);
	virtual QVariant getAttr(const QString& name) const;

	void setTip(const QString& tip, bool only_visual = false);
	QString getTip() const;

	OQCanvas *getCanvas() const		{ return (OQCanvas *) canvas(); }
	QWidget *getParent() const		{ return getCanvas()->getParent(); }

	bool isEdit() const				{ return getCanvas()->isEdit(); }
	bool isTest() const				{ return getCanvas()->isTest(); }
	double currentTestSec() const	{ return getCanvas()->currentTestSec(); }

	int rtti() const	{ return ORTTI_Widget; }
	const QString& getClass() const   { return OQCanvas::rttiToClass(rtti()); }
	QString toString();

	static void log(const char * format, ...);
	static QFont stringToFont(const QString& desc, bool *ok = 0);
	static QString fontToString(const QFont& font);

	virtual void mouseAnyEvent(QMouseEvent* e)				{}
	virtual void mousePressEvent(QMouseEvent* e)			{}
	virtual void mouseReleaseEvent(QMouseEvent* e)			{}
	virtual void mouseMoveEvent(QMouseEvent* e)				{}
	virtual void mouseDoubleClickEvent(QMouseEvent* e)		{}

	bool hasChild() const			{ return (child != 0); }
	QWidget * childWidget() const	{ return child; }

protected:

	virtual void attachChild(QWidget *child);
	virtual void destroyChild();
	virtual bool eventFilter(QObject *watched, QEvent *e);
	virtual bool triageChildEvent(QEvent *e);
	QWidget * detachChild();

	void updateSize(int br_w, int br_h);
	void updateSize(const QSize& size);
	void updateSize(const QRect& brect);

protected slots:

	virtual void updateGeometry()		{}
	virtual void childDestroyed(QObject *);

public slots:

	virtual void redraw()		{ QCanvasRectangle::update(); }
	virtual void update()		{ redraw(); }
	virtual void updateValue(const QVariant& value)		{ update(); }
	virtual void updateValue(int no, const QVariant& value);
	virtual void updateDefaults()	{}

private:

	float user_w, user_h;
	int brect_w, brect_h;
	QWidget *child;
	QString tip;
	bool track_mouse : 1;
	bool mark_bounds : 1;
};

inline void
OCWidget::updateSize(const QSize& size)
	{ updateSize(size.width(), size.height()); }

inline void
OCWidget::updateSize(const QRect& rect)
	{ updateSize(rect.width(), rect.height()); }


class OCDataWidget : public OCWidget
{
	Q_OBJECT

public:

	OCDataWidget(OQCanvas *canvas);
	virtual ~OCDataWidget()		{ unbind(); }
	int rtti() const			{ return ORTTI_DataWidget; }

	virtual void polish();
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	virtual bool isReadOnly() const		{ return readonly; }
	virtual void setReadOnly(bool ro)	{ readonly = ro; }

	virtual bool bind(const QString& vars);
	virtual void unbind();
	void setBindLimit(int _maxnum)		{ maxnum = _maxnum; }

	int varNum() const			{ return num; }
	QString varList() const		{ return vars.join(" "); }
	QString varName(int no = 0) const;
	QVariant readValue(int no, bool *ok = 0);
	QVariant readValue(bool *ok = 0);
	bool writeValue(int no, const QVariant& value);
	bool writeValue(const QVariant& value)	{ return writeValue(0, value); }

	virtual QVariant testValue(int no);
	virtual double testValue(int no, double minv, double maxv,
							double speed, double offset, bool is_int);
	virtual void testValueProps(int no, double& minv, double& maxv,
								double& speed, double& offset, bool& is_int);

protected:

	bool triageChildEvent(QEvent *e);

private slots:

	void dataUpdated(const owquark_t& inf, const QVariant& val, long time);
	void monitorDone(long param, const owquark_t& inf, int err);

private:

	bool connected : 1;
	bool readonly : 1;
	int num, maxnum;
	QStringList vars;
	int  * bgids;
	unsigned  * ooids;
	QMap<unsigned,int> ooid2idx;
};


inline QString
OCDataWidget::varName(int no) const
	{ return (no >= 0 && no < num ? vars[no] : 0); }


#endif // OQCANVAS_H
