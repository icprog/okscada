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
#include <qimage.h>

#include <math.h>


#define qDebug(...)

#define PXWIDTH		128
#define PXHEIGHT	128


/* ================ SolArray ================ */


OCSolArray::OCSolArray(OQCanvas *c)
	:	OCDataWidget(c),
		no(-1)
{
	sun = OQPixmap("png/sb_sun.png");
}


void
OCSolArray::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("number", "integer");
}


bool
OCSolArray::setAttr(const QString& name, const QVariant& value)
{
	if (name == "number") {
		bool ok;
		int n = value.toInt(&ok);
		if (ok)
			setNumber(n);
		update();
		return ok;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCSolArray::getAttr(const QString& name) const
{
	if (name == "number")
		return getNumber();
	return OCWidget::getAttr(name);
}


bool
OCSolArray::setNumber(int _no)
{
	bool ok = true;
	if (!(_no == 2 || _no == 4)) {
		log("solar(%g,%g): number %d must be 2 or 4", x(), y(), _no);
		_no = 0;
		ok = false;
	}
	no = _no;
	QString file = QString("png/sb_%1.png").arg(no);
	bg = OQPixmap(file);
	if (!(bg.width() == PXWIDTH && bg.height() == PXHEIGHT)) {
		if (ok)
			log("solar(%g,%g): background %s (%dx%d) must be %dx%d", x(), y(),
				file.ascii(), bg.width(), bg.height(), PXWIDTH, PXHEIGHT);
		bg.convertFromImage(bg.convertToImage().smoothScale(PXWIDTH, PXHEIGHT));
		ok = false;
	}
	updateSize(PXWIDTH, PXHEIGHT);
	QString prefix(QString("sa%1_").arg(no));
	const char *p = prefix.ascii();
	QString vars;
	vars.sprintf("%sangle %ssunx %ssuny %snight %ssupp %szone", p,p,p,p,p,p);
	bind(vars);
	return ok;
}


void
OCSolArray::polish()
{
	OCDataWidget::polish();
	if (no < 0) {
		log("solar(%g,%g): number not set", x(), y());
		setNumber(0);
	}
}


void
OCSolArray::drawShape(QPainter& p)
{
	double angle = readValue(0).toDouble();
	double sunx = readValue(1).toDouble();
	double suny = readValue(2).toDouble();
	bool night = readValue(3).toBool();
	bool supp = readValue(4).toBool();
	bool zone = readValue(5).toBool();

	qDebug("solar(%d): angle=%g s1=%g s2=%g night=%d supp=%d zone=%d",
			no, angle, s1, s2, night, supp, zone);

	angle = M_PI/2 - angle * M_PI/180;
	while (angle < 0)
		angle += M_PI*2;
	while (angle > M_PI*2)
		angle -= M_PI*2;
	int nz = int((angle + M_PI/16) / M_PI*8);
	double z1 = nz/16. * M_PI*2 - M_PI/16;
	double z2 = nz/16. * M_PI*2 + M_PI/16;

	QPen pen(QColor("#000000"), 3, SolidLine, RoundCap, BevelJoin);
	QColor red("#d64e4e");
	QColor aqua("#009cea");
	QColor blue("#004e9c");
	QColor grass("#00fd00");
	QColor yellow("#ffff00");

	p.save();
	p.translate(int(x()), int(y()));
	p.drawPixmap(0, 0, bg);

	p.translate(PXWIDTH/2-1, PXHEIGHT/2-1);
	QPointArray pazone(3);
	pazone.setPoint(0, 0, 0);
	pazone.setPoint(1, int(cos(z2)*43), int(sin(z2)*43));
	pazone.setPoint(2, int(cos(z1)*43), int(sin(z1)*43));
	p.setBrush(zone ? yellow : aqua);
	p.drawPolygon(pazone);

	pen.setColor(blue);
	p.setPen(pen);
	p.drawLine(int(cos(z1)*62), int(sin(z1)*62), 0, 0);
	p.drawLine(int(cos(z2)*62), int(sin(z2)*62), 0, 0);

	if (!night) {
		if (no == 4)
			sunx = -sunx;
		p.drawPixmap(int(1-8+sunx*40), int(1-8-suny*40), sun);
	}

	pen.setColor(grass);
	p.setPen(pen);
	p.drawLine(int(cos(angle)*62), int(sin(angle)*62), 0, 0);

	if (supp) {
		QPointArray pasupp(3);
		pasupp.setPoint(0, 0, 0);
		pasupp.setPoint(1, -10, 62);
		pasupp.setPoint(2, -21, 58);
		p.setBrush(red);
		p.setPen(NoPen);
		p.drawPolygon(pasupp);
	}

	p.restore();
}


void
OCSolArray::testValueProps(int no, double& minv, double& maxv,
							double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = 0;
	offset = this->no == 4 ? 30 : 0;
	is_int = false;
	switch (no) {
		case 0:		// angle
			maxv = 360;
			speed = 31;
			break;
		case 1:		// sunx
			maxv = 1;
			speed = 37;
			break;
		case 2:		// suny
			maxv = 1;
			speed = 39;
			break;
		case 3:		// night
			maxv = 1.1;
			speed = 14;
			break;
		case 4:		// supp
			maxv = 1.99;
			speed = 35;
			break;
		case 5:		// zone
			maxv = 2;
			speed = 36;
			break;
	}
}
