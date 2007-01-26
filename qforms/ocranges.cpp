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
#include "oqpiclabel.h"

#include <qvariant.h>
#include <qpainter.h>
#include <qslider.h>

#include <math.h>


#define qDebug(...)

#define SLIDER_TEXT_FLAGS	(AlignHCenter | AlignTop)


/* ================ Dial ================ */


OCDial::OCDial(OQCanvas *c)
	:	OCDataWidget(c),
		vmin(0), vmax(0), vcur(0), vnew(0), angle(0),
		button(0)
{
	setBindLimit(1);
	face = OQPixmap("png/dial_bg.png");
	updateSize(face.size());
}


void
OCDial::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("min", "double");
	atts.replace("max", "double");
}


bool
OCDial::setAttr(const QString& name, const QVariant& value)
{
	if (name == "min") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMin(v);
		return ok;
	}
	if (name == "max") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMax(v);
		return ok;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCDial::getAttr(const QString& name) const
{
	if (name == "min")
		return getMin();
	if (name == "max")
		return getMax();
	return OCDataWidget::getAttr(name);
}


void
OCDial::polish()
{
	OCDataWidget::polish();
	if (vmin > vmax) {
		double v = vmin;
		vmin = vmax;
		vmax = v;
	}
	vcur = vnew = vnew < vmin ? vmin : vnew > vmax ? vmax : vnew;
}


bool
OCDial::resize(double w, double h)
{
	bool ok = OCDataWidget::resize(w, h);
	if (ok)
		updateSize(int(width()), int(height()));
	return ok;
}


void
OCDial::updateValue(const QVariant& value)
{
	vnew = value.toDouble();
	vnew = vnew < vmin ? vmin : vnew > vmax ? vmax : vnew;
	vcur = vnew;
	update();
}


void
OCDial::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = vmin;
	maxv = vmax;
	speed = 7;
}


void
OCDial::drawShape(QPainter& p)
{
	if (vnew != vcur) {
		vnew = vnew < vmin ? vmin : vnew > vmax ? vmax : vnew;
		writeValue(vnew);
		vcur = vnew;
	}

	QRect br = boundingRect();
	double w = br.width();
	double h = br.height();
	double xc = w / 2;
	double yc = h / 2;
	double rw = w * .45;
	double rh = h * .45;
	double ptr_w = rw * .2;
	double ptr_h = rh * .2;
	double increment = M_PI * 100 / (rw * rh);

	double inc = vmax > vmin ? vmax - vmin : vmin - vmax;
	while (inc > 0 && inc < 100)
		inc *= 10.;
	while (inc >= 1000)
		inc /= 10.;

	p.drawPixmap(br, face);
	p.save();
	p.translate(x(), y());

	QColor ctick1("#99ff99");
	QColor ctick2("#99cc99");
	QColor cpointer("#ffcc33");
	QColor ccontour("#660000");
	QPen pen(QColor("#000000"), 2, SolidLine, RoundCap, BevelJoin);

	double last = -1;
	for (int i = 0; i <= inc; i++) {
		double theta = i * M_PI / (18. * inc / 24.) - M_PI/6.;
		if (theta - last < increment)
			continue;
		last = theta;
		double tick_w = ptr_w;
		double tick_h = ptr_h;
		if (inc >= 10 && (i % int(inc / 10)) == 0) {
			pen.setColor(ctick1);
		} else {
			tick_w /= 2;
			tick_h /= 2;
			pen.setColor(ctick2);
    	}
		p.setPen(pen);
		double s = sin(theta);
		double c = cos(theta);
		p.drawLine(
			int(xc + c*(rw - tick_w)), int(yc - s*(rh - tick_h)),
			int(xc + c*rw), int(yc - s*rh));
	}

	if (vmax == vmin)
		angle = 2.e9;
	else
		angle = (M_PI*7./6.) - (vcur - vmin) * (M_PI*4./3.) / (vmax - vmin);

	double c, s;
	if (fabs(angle) < 1.e9) {
		s = sin(angle);
		c = cos(angle);
	} else {
		c = s = angle = 0;
	}

	p.translate(xc, yc);
	QPointArray points(5);
	points.setPoint(0, int(s*ptr_w/2), int(c*ptr_w/2));
	points.setPoint(1, int(c*rw), int(-s*rh));
	points.setPoint(2, int(-s*ptr_w/2), int(-c*ptr_h/2));
	points.setPoint(3, int(-c*rw/10), int(s*rh/10));
	points.setPoint(4, int(s*ptr_w/2), int(c*ptr_h/2));

	p.setBrush(cpointer);
	p.drawPolygon(points);
	pen.setColor(ccontour);
	p.setPen(pen);
	p.drawPolyline(points);

	p.restore();
}


void
OCDial::updateMouse(int x, int y)
{
	double xc = face.width() / 2;
	double yc = face.height() / 2;
	double a = atan2(yc - y, x - xc);
	if (a < -M_PI/2)
		a += M_PI*2;
	if (a < -M_PI/6)
		a = -M_PI/6;
	if (a > M_PI*7./6.)
		a = M_PI*7./6.;
	// value will be actually written when we are redrawn, throttled.
	vnew = vmin + (M_PI*7./6. - a) * (vmax - vmin) / (M_PI*4./3.);
	update();
}


void
OCDial::mousePressEvent(QMouseEvent* e)
{
	if (e->button() != LeftButton || isTest() || isEdit() || isReadOnly())
		return;
	double w = face.width();
	double h = face.height();
	double radius = (w < h ? w : h) * 0.45;
	double ptr_w = radius / 5;
	double dx = e->x() - w / 2;
	double dy = h / 2 - e->y();
	double s = sin(angle);
	double c = cos(angle);
	double d_par = s * dy + c * dx;
	double d_per = fabs(s * dx - c * dy);
	if (!button && d_per < ptr_w/2 && d_par > -ptr_w) {
		button = e->button();
		setTrackMouse(true);
		updateMouse(e->x(), e->y());
	}
}


void
OCDial::mouseReleaseEvent(QMouseEvent* e)
{
	if (button && e->button() == button) {
		updateMouse(e->x(), e->y());
		button = 0;
		setTrackMouse(false);
	}
}


void
OCDial::mouseMoveEvent(QMouseEvent* e)
{
	if (button && e->state() == button)
		updateMouse(e->x(), e->y());
}


/* ================ Ruler ================ */


OCRuler::OCRuler(OQCanvas *c)
	:	OCDataWidget(c),
		vmin(0), vmax(0), vcur(0), vslide(0),
		off_x(0), off_y(0)
{
	setBindLimit(1);
	attachChild(new QSlider(Horizontal, getParent()));
	slider()->setTracking(false);
	connect(slider(), SIGNAL(valueChanged(int)), SLOT(valueChanged(int)));
	connect(slider(), SIGNAL(sliderMoved(int)), SLOT(sliderMoved(int)));
}


void
OCRuler::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("min", "double");
	atts.replace("max", "double");
	atts.replace("vertical", "boolean");
}


bool
OCRuler::setAttr(const QString& name, const QVariant& value)
{
	if (name == "min") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMin(v);
		return ok;
	}
	if (name == "max") {
		bool ok;
		double v = value.toDouble(&ok);
		if (ok)
			setMax(v);
		return ok;
	}
	if (name == "vertical") {
		setVertical(value.toBool());
		return true;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCRuler::getAttr(const QString& name) const
{
	if (name == "min")
		return getMin();
	if (name == "max")
		return getMax();
	if (name == "vertical")
		return isVertical();
	return OCDataWidget::getAttr(name);
}


void
OCRuler::setVertical(bool vert)
{
	slider()->setOrientation(vert ? Vertical : Horizontal);
	updateGeometry();
}


bool
OCRuler::isVertical() const
{
	return (slider()->orientation() == Vertical);
}


void
OCRuler::polish()
{
	OCDataWidget::polish();
	if (vmin > vmax) {
		double v = vmin;
		vmin = vmax;
		vmax = v;
	}
	vcur = vcur < vmin ? vmin : vcur > vmax ? vmax : vcur;

	slider()->setMinValue(int(vmin));
	slider()->setMaxValue(int(vmax-0.1));
	slider()->setValue(int(vcur));

	if (width() <= 0 || height() <= 0) {
		if (slider()->orientation() == Vertical)
			updateSize(40, 80);
		else
			updateSize(80, 40);
		updateGeometry();
	}
}


bool
OCRuler::resize(double w, double h)
{
	// Temporarily detach the slider and resize it appropriately.
	QWidget *child = detachChild();
	OCWidget::resize(w, h);
	updateSize(int(w), int(h));
	attachChild(child);
	updateGeometry();
	return true;
}


void
OCRuler::move(double x, double y)
{
	// Temporarily detach the slider and move it appropriately.
	QWidget *child = detachChild();
	OCDataWidget::move(x, y);
	child->move(int(x) + off_x, int(y) + off_y);
	attachChild(child);
}


void
OCRuler::updateGeometry()
{
	QSize rsz = QFontMetrics(slider()->font()).size(SLIDER_TEXT_FLAGS, "09.,");
	int gap = 2;
	int min_sld = 2;
	int sld = 16;
	off_y = rsz.height() + gap;

	QRect br = boundingRect();
	if (br.height() < off_y + min_sld)
		br.setHeight(off_y + min_sld);
	if (br.width() < rsz.width())
		br.setWidth(rsz.width());

	if (slider()->orientation() == Vertical) {
		if (sld > br.width())
			sld = br.width();
		off_x = (br.width() - sld) / 2;
		slider()->setGeometry(br.x() + off_x, br.y() + off_y,
								sld, br.height() - off_y);
	} else {
		if (sld > br.height() - off_y)
			sld = br.height() - off_y;
		off_x = 0;
		slider()->setGeometry(br.x(), br.y() + off_y, br.width(), sld);
	}

	updateSize(br);
	update();
}


void
OCRuler::updateValue(const QVariant& value)
{
	vcur = value.toDouble();
	vcur = vcur < vmin ? vmin : vcur > vmax ? vmax : vcur;
	update();
}


void
OCRuler::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = vmin;
	maxv = vmax;
	speed = 5;
}


void
OCRuler::update()
{
	if (slider() && slider()->value() != int(vcur))
		slider()->setValue(int(vcur));
	vslide = vcur;
	OCDataWidget::update();
}


void
OCRuler::drawShape(QPainter& p)
{
	QString vs;
	vs.sprintf("%.1f", vslide);
	if (slider()) {
		p.setFont(slider()->font());
		p.setPen(slider()->paletteForegroundColor());
	}
	p.drawText(boundingRect(), SLIDER_TEXT_FLAGS, vs);
}


void
OCRuler::sliderMoved(int val)
{
	vslide = val < vmin ? vmin : val > vmax ? vmax : val;
	OCDataWidget::update();	// ask to redraw the value text
}


void
OCRuler::valueChanged(int val)
{
	double vnew = val;
	if (vcur != vnew) {
		qDebug("value changed %d (%g) (was %g)", val, vnew, vcur);
		writeValue(vnew);
		vcur = vslide = vnew;
	}
}
