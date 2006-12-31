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

#include <qcanvas.h>
#include <qstringlist.h>
#include <qmap.h>


class OCWidget;

#define ORTTI_Widget			0xb0000
#define ORTTI_ActiveWidget		0xb0001
#define ORTTI_PeriodicWidget	0xb0002

typedef QMap<QString, QString>  OCAttrMap;


class OQCanvas : public QCanvas
{
	Q_OBJECT

public:
	OQCanvas();

	void setDebug(bool on)		{ is_debug = on; }
	bool isDebug()	{ return is_debug; }

	void setTest(bool on)		{ is_test = on; }
	bool isTest()	{ return is_test; }

public slots:
	void refreshSlot(int param);

private:
	bool is_debug;
	bool is_test;
};


class OQCanvasView : public QCanvasView
{
public:
	OQCanvasView(QWidget *parent);
	OQCanvas *getCanvas()	{ return (OQCanvas *) canvas(); }
	OCWidget *newBasicWidget(int rtti);
};


class OCWidget : public QCanvasItem
{
public:
	OCWidget(OQCanvas *canvas);

	virtual void forget();
	virtual void destroy();
	virtual void resize(int w, int h, int x, int y);

	virtual OCAttrMap& getAttrMap(OCAttrMap&);

	virtual void testRefresh(QObject *obj, int param, int timeout = 200);
	virtual void refreshWidget();
	virtual void updateAppearance();

	void setTip(const char *);

	static void writeByName(const char *name, const QVariant& value);
	static void log(const QString& msg);

	OQCanvas *getCanvas()	{ return (OQCanvas *) canvas(); }
	bool isDebug()		{ return getCanvas()->isDebug(); }
	bool isTest()		{ return getCanvas()->isTest(); }

	virtual int rtti() const	{ return ORTTI_Widget; }

	void refreshSlot(int param);

public:
	virtual bool collidesWith(const QCanvasItem*) const
	{
		return false;
	}

	virtual void draw(QPainter&)
	{
	}

	virtual QRect boundingRect() const
	{
		return QRect();
	}

	virtual bool collidesWith(const QCanvasSprite*, const QCanvasPolygonalItem*,
							const QCanvasRectangle*, const QCanvasEllipse*,
							const QCanvasText*) const
	{
		return false;
	}
};


class OCActiveWidget : public OCWidget
{
public:
	OCActiveWidget(OQCanvas *canvas);

	virtual void forget()
	{
		OCWidget::forget();
		unbind();
	}

	virtual OCAttrMap& getAttrMap(OCAttrMap&);

	void bind(const QStringList& vars);
	void unbind();

	virtual int rtti() const	{ return ORTTI_ActiveWidget; }

};


class OCPeriodicWidget : public OCWidget
{
public:
	OCPeriodicWidget(OQCanvas *canvas);

	virtual void forget()
	{
		OCWidget::forget();
		unbindTime();
	}

	virtual OCAttrMap& getAttrMap(OCAttrMap&);

	void bindTime(int period = 100, int when = 0);
	void unbindTime();

	virtual int rtti() const	{ return ORTTI_PeriodicWidget; }

};


#endif // OQCANVAS_H
