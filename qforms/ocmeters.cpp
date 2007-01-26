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

  Library of standard widgets for visual canvas.

*/

#include "ocwidgets.h"
#include "oqcommon.h"

#include <qvariant.h>
#include <qpainter.h>

#include <math.h>


#define qDebug(...)


/* ================ Manometer ================ */


OCManometer::OCManometer(OQCanvas *c)
	:	OCDataWidget(c),
		vmin(0), vmax(0), vcur(0), subclass("")
{
}


void
OCManometer::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("MIN", "double");
	atts.replace("MAX", "double");
	atts.replace("subclass", "string");
}


bool
OCManometer::setAttr(const QString& name, const QVariant& value)
{
	if (name == "MIN") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMin(v);
		return ok;
	}
	if (name == "MAX") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMax(v);
		return ok;
	}
	if (name == "subclass") {
		setSubclass(value.toString());
		return true;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCManometer::getAttr(const QString& name) const
{
	if (name == "MIN")
		return getMin();
	if (name == "MAX")
		return getMax();
	if (name == "subclass")
		return getSubclass();
	return OCDataWidget::getAttr(name);
}


void
OCManometer::setMin(double _min)
{
	vmin = _min;
	update();
}


void
OCManometer::setMax(double _max)
{
	vmax = _max;
	update();
}


void
OCManometer::setSubclass(const QString& _subclass)
{
	subclass = _subclass;
	bg = OQPixmap("png/mnmtr_" + subclass + ".png");
	updateSize(bg.size());
	update();
}


void
OCManometer::polish()
{
	OCDataWidget::polish();
	if (vmin > vmax) {
		double v = vmin;
		vmin = vmax;
		vmax = v;
	}
	if (vmin == vmax)
		vmax *= 1.01;
}


void
OCManometer::updateValue(const QVariant& value)
{
	double v = value.toDouble();
	vcur = vmax != vmin ? (v - vmin) / (vmax - vmin) : 0;
	update();
}


void
OCManometer::testValueProps(int no, double& minv, double& maxv,
							double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = vmin;
	maxv = vmax;
}


void
OCManometer::drawShape(QPainter& p)
{
	double angle = vcur * M_PI*3/2 - M_PI*5/4;
	double sz = bg.width() < bg.height() ? bg.width() : bg.height();
	double outer = sz / 2;
	double inner = sz / 14.3;
	p.drawPixmap(int(x()), int(y()), bg);
	p.setPen(QPen(QColor("#003366"), 3, SolidLine, FlatCap, MiterJoin));
	p.drawLine(int(x() + outer + cos(angle) * (outer - inner)),
				int(y() + outer + sin(angle) * (outer - inner)),
				int(x() + outer - 1 + cos(angle) * inner),
				int(y() + outer - 1 + sin(angle) * inner));
}


/* ================ Meter ================ */


OCMeter::OCMeter(OQCanvas *c, int _ymin, int _ymax,
				const QString& _subclass, const QString& _colorclass)
	:	OCDataWidget(c),
		subclass(_subclass), ymin(_ymin), ymax(_ymax),
		vmin(0), vmax(0), vcur(0)
{
	pxempty = OQPixmap(QString("png/%1_empty.png").arg(subclass));
	updateSize(pxempty.size());
	setColorClass(_colorclass);
}


void
OCMeter::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("MIN", "double");
	atts.replace("MAX", "double");
	atts.replace("color", "string");
}


bool
OCMeter::setAttr(const QString& name, const QVariant& value)
{
	if (name == "MIN") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMin(v);
		return ok;
	}
	if (name == "MAX") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMax(v);
		return ok;
	}
	if (name == "color") {
		setColorClass(value.toString());
		return true;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCMeter::getAttr(const QString& name) const
{
	if (name == "MIN")
		return getMin();
	if (name == "MAX")
		return getMax();
	if (name == "color")
		return getColorClass();
	return OCDataWidget::getAttr(name);
}


void
OCMeter::setMin(double _min)
{
	vmin = _min;
	update();
}


void
OCMeter::setMax(double _max)
{
	vmax = _max;
	update();
}


void
OCMeter::setColorClass(const QString& _colorclass)
{
	colorclass = _colorclass;
	pxfill = OQPixmap(QString("png/%1_%2.png").arg(subclass).arg(colorclass));
	update();
}


void
OCMeter::polish()
{
	OCDataWidget::polish();
	vmax = vmax > vmin ? vmax : vmin + 2;
}


void
OCMeter::updateValue(const QVariant& value)
{
	vcur = value.toDouble();
	update();
}


void
OCMeter::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = vmin;
	maxv = vmax;
}


void
OCMeter::drawShape(QPainter& p)
{
	int x0 = int(x());
	int y0 = int(y());
	int w = pxempty.width();
	int h = pxempty.height();
	int y = vcur < vmin ? ymin : vcur > vmax ? ymax
			: ymin + int((vcur - vmin) * (ymax - ymin) / (vmax - vmin));
	int dy = h < y ? 0 : h - y;
	p.drawPixmap(x0, y0, pxempty, 0, 0, w, h);
	p.drawPixmap(x0, y0+y, pxfill, 0, y, w, dy);
}
