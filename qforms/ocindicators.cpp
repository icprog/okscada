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


#define qDebug(...)


/* ================ LED ================ */


OCLED::OCLED(OQCanvas *c)
	:	OCFormatted(c)
{
	glyphs.setAutoDelete(true);
}


void
OCLED::attrList(OCAttrList& atts) const
{
	OCFormatted::attrList(atts);
	atts.replace("subclass", "string");
}


bool
OCLED::setAttr(const QString& name, const QVariant& value)
{
	if (name == "subclass") {
		setSubclass(value.toString());
		return true;
	}
	return OCFormatted::setAttr(name, value);
}


QVariant
OCLED::getAttr(const QString& name) const
{
	if (name == "subclass")
		return getSubclass();
	return OCFormatted::getAttr(name);
}


void
OCLED::setSubclass(const QString& _subclass)
{
	glyphs.clear();
	subclass = _subclass;
	if (!(subclass == "d" || subclass == "b"
			|| subclass == "G" || subclass == "cntr")) {
		log("LED(%g,%g): subclass \"%s\" is invalid",
			x(), y(), subclass.ascii());
		subclass = "d";
	}
	for (int i = 0; i <= 11; i++) {
		QString gl, nm;
		switch (i) {
			case 10:
				gl = "."; nm = "dot";
				break;
			case 11:
				gl = "-"; nm = "dash";
				break;
			default:
				gl.sprintf("%d", i); nm = gl;
				break;
		}
		glyphs.insert(gl,
				new OQPixmap(QString("png/%1_%2.png").arg(subclass).arg(nm)));
	}
	updateText();
}


void
OCLED::polish()
{
	OCDataWidget::polish();
	if (subclass.isEmpty()) {
		log("LED(%g,%g): subclass not set", x(), y());
		setSubclass("d");
	}
}


void
OCLED::updateGeometry()
{
	int w = 0, h = 0;
	if (!glyphs.isEmpty()) {
		for (unsigned i = 0; i < text.length(); i++) {
			QPixmap *px = glyphs[QString(text[i])];
			if (!px)
				px = glyphs["-"];
			w += px->width();
			if (px->height() > h)
				h = px->height();
		}
	}
	updateSize(w, h);
}


void
OCLED::drawShape(QPainter& p)
{
	int cx = int(x());
	int cy = int(y());
	for (unsigned i = 0; i < text.length(); i++) {
		QPixmap *px = glyphs[QString(text[i])];
		if (!px)
			px = glyphs["-"];
		p.drawPixmap(cx, cy, *px);
		cx += px->width();
	}
}


void
OCLED::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = 0;
	maxv = 10;
	for (unsigned i = 1; i < text.length() && text[i] != '.'; i++)
		maxv *= 10;
	maxv -= 1;
	speed = 12;
}


/* ================ Curve ================ */


#define MARGIN	1


OCCurve::OCCurve(OQCanvas *c)
	:	OCDataWidget(c),
		ys(0), nx(0), ny(0),
		vmin(0), vmax(0),
		freq(0), when(0),
		timer_id(0)
		
{
	setBindLimit(1);
}


OCCurve::~OCCurve()
{
	timerUnbind();
	delete [] ys;
	ys = 0;
	plot.resize(0);
}


void
OCCurve::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("min", "double");
	atts.replace("max", "double");
	atts.replace("freq", "integer");
	atts.replace("when", "integer");
}


bool
OCCurve::setAttr(const QString& name, const QVariant& value)
{
	bool ok;
	if (name == "min") {
		double v = value.toDouble(&ok);
		if (ok)
			setMin(v);
		return ok;
	}
	if (name == "max") {
		double v = value.toDouble(&ok);
		if (ok)
			setMax(v);
		return ok;
	}
	if (name == "freq") {
		int v = value.toInt(&ok);
		if (ok)
			setFreq(v);
		return ok;
	}
	if (name == "when") {
		int v = value.toInt(&ok);
		if (ok)
			setWhen(v);
		return ok;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCCurve::getAttr(const QString& name) const
{
	if (name == "min")
		return getMin();
	if (name == "max")
		return getMax();
	if (name == "freq")
		return getFreq();
	if (name == "when")
		return getWhen();
	return OCDataWidget::getAttr(name);
}


void
OCCurve::setMin(double _min)
{
	vmin = _min;
	update();
}


void
OCCurve::setMax(double _max)
{
	vmax = _max;
	update();
}


void
OCCurve::timerBind(int _freq, int _when)
{
	timerUnbind();
	freq = _freq;
	when = _when;
	if (freq <= 0)
		return;
	int period = 1000 / freq;
	if (period < 10)
		period = 10;
	timer_id = startTimer(period);
}


void
OCCurve::timerUnbind()
{
	if (timer_id)
		killTimer(timer_id);
	timer_id = freq = when = 0;
}


void
OCCurve::timerEvent(QTimerEvent *e)
{
	if (e->timerId() == timer_id)
		updatePeriodic();
}


void
OCCurve::polish()
{
	OCDataWidget::polish();
	if (vmin > vmax) {
		double v = vmin;
		vmin = vmax;
		vmax = v;
	}
	if (freq <= 0)
		setFreq(10);
}


void
OCCurve::updateValue(int no, const QVariant& value)
{
}


void
OCCurve::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = vmin;
	maxv = vmax;
	speed = 5;
}


bool
OCCurve::resize(double w, double h)
{
	int nx0 = nx;
	OCDataWidget::resize(w, h);

	QRect br = boundingRect();
	nx = br.width() - MARGIN*2;
	if (nx < 0)
		nx = 0;
	ny = br.height() - MARGIN*2;
	if (ny < 0)
		ny = 0;

	if (nx != nx0) {
		plot.resize(nx);
		int *ys0 = ys;
		ys = nx ? new int[nx] : 0;
		int i;
		for (i = 0 ; i < nx0 && i < nx; i++)
			ys[i] = ys0[i];
		for ( ; i < nx; i++)
			ys[i] = 0;
		delete [] ys0;
	}
	return true;
}


void
OCCurve::updatePeriodic()
{
	if (!ys || isEdit())
		return;
	for (int i = 1; i < nx; i++)
		ys[i-1] = ys[i];
	if (nx > 0) {
		double v = readValue().toDouble();
		v = v < vmin ? vmin : v > vmax ? vmax : v;
		double div = vmax == vmin ? 1 : vmax - vmin;
		ys[nx-1] = int((v - vmin) / div * ny);
	}
	update();
}


void
OCCurve::drawShape(QPainter& p)
{
	const QPalette& pal = getParent()->palette();
	QRect br = boundingRect();
	int w = br.width();
	int h = br.height();
	p.save();
	p.translate(br.x(), br.y());
	QBrush cbg = pal.brush(QPalette::Active, QColorGroup::Button);
	p.setBrush(cbg);
	p.setPen(NoPen);
	p.drawRect(0, 0, w, h);
	QColor cgrid = pal.color(QPalette::Active, QColorGroup::Mid);
	p.setPen(QPen(cgrid, 2));
	p.setBrush(NoBrush);
	int i;
	int ng = 3;
	for (i = 0; i <= ng; i++) {
		int y = 1+i*(h-2)/ng;
		p.drawLine(1, y, w-1, y);
	}
	for (i = 0; i <= ng; i++) {
		int x = 1+i*(w-2)/ng;
		p.drawLine(x, 1, x, h-1);
	}
	if (nx > 0) {
		for (i = 0; i < nx; i++)
			plot.setPoint(i, i+MARGIN, h-ys[i]-MARGIN);
		QColor cplot = pal.color(QPalette::Active, QColorGroup::ButtonText);
		p.setPen(QPen(cplot, 1));
		p.drawPolyline(plot);
	}
	p.restore();
}
