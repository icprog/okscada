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


#define qDebug(...)
#define BUTTON_PIC_MARGIN	8


/* ================ Anim ================ */


OCAnim::OCAnim(OQCanvas *c)
	:	OCDataWidget(c),
		cur(0), pics(0)
{
	setBindLimit(1);
}


void
OCAnim::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("Image", "images");
}


bool
OCAnim::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Image") {
		setImages(QStringList::split(",", value.toString()));
		return true;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCAnim::getAttr(const QString& name) const
{
	if (name == "Image")
		return picnames.join(",");
	return OCDataWidget::getAttr(name);
}


bool
OCAnim::setImages(const QStringList& _picnames)
{
	clear();
	picnames = _picnames;
	int n = picnames.count();
	if (n > 0) {
		pics = new QPixmap[n];
		for (int i = 0; i < n; i++)
			pics[i] = OQPixmap(picnames[i]);
	}
	updateGeometry();
	update();
	return true;
}


void
OCAnim::clear()
{
	picnames.clear();
	delete [] pics;
	pics = 0;
	cur = 0;
	updateGeometry();
}


void
OCAnim::updateValue(const QVariant& value)
{
	cur = value.toUInt() % picnames.count();
	updateGeometry();
	update();
}


void
OCAnim::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = 0;
	maxv = picnames.count() + 0.8;
	is_int = true;
}


void
OCAnim::drawShape(QPainter& p)
{
	if (picnames.count())
		p.drawPixmap(int(x()), int(y()), pics[cur]);
}


void
OCAnim::updateGeometry()
{
	if (picnames.count())
		updateSize(pics[cur].size());
	else
		updateSize(1, 1);
}


/* ================ Button ================ */


OCButton::OCButton(OQCanvas *c)
	:	OCDataWidget(c),
		updating(false), down(false)
{
	setBindLimit(1);
	attachChild(new OQPicButton(getParent(), true));
	connect(button(), SIGNAL(toggled(bool)), SLOT(toggled(bool)));
}


void
OCButton::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("Label", "glabel");
	atts.replace("no_border", "boolean");
}


bool
OCButton::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Label") {
		setLabel(value.toString());
		return true;
	}
	if (name == "no_border") {
		button()->setFlat(value.toBool());
		return true;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCButton::getAttr(const QString& name) const
{
	if (name == "Label")
		return getLabel();
	if (name == "no_border")
		return QVariant(button()->isFlat(), 01);
	return OCDataWidget::getAttr(name);
}


void
OCButton::setLabel(const QString& label)
{
	button()->setLabel(label);
}


void
OCButton::toggled(bool on)
{
	// Note: in read-only mode the event will be filtered out by OCWidget.
	if (updating || !button())
		return;
	int newval = on ? 1 : 0;
	qDebug("button: set %s = %d", varName(0).ascii(), newval);
	writeValue(newval);
}


void
OCButton::updateValue(const QVariant& value)
{
	if (!button())
		return;
	updating = true;
	down = (value.toInt() != 0);
	qDebug("button: update %s as %d", varName(0).ascii(), down);
	button()->setOn(down);
	updating = false;
}


void
OCButton::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = 0;
	maxv = 1.5;
	is_int = true;
}


void
OCButton::drawShape(QPainter& p)
{
	// button will draw itself
}


/* ================ Switch ================ */


OCSwitch::OCSwitch(OQCanvas *c)
	:	OCDataWidget(c),
		margin(BUTTON_PIC_MARGIN),
		updating(false), down(false), pics(0)
{
	setBindLimit(1);
	attachChild(new QPushButton(getParent()));
	button()->setToggleButton(true);
	button()->setFocusPolicy(QWidget::NoFocus);
	connect(button(), SIGNAL(toggled(bool)), SLOT(toggled(bool)));
}


void
OCSwitch::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("no_border", "boolean");
	atts.replace("Image", "images");
}


bool
OCSwitch::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Image") {
		setImages(QStringList::split(",", value.toString()));
		return true;
	}
	if (name == "no_border") {
		button()->setFlat(value.toBool());
		return true;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCSwitch::getAttr(const QString& name) const
{
	if (name == "Image")
		return picnames.join(",");
	if (name == "no_border")
		return QVariant(button()->isFlat(), 01);
	return OCDataWidget::getAttr(name);
}


bool
OCSwitch::setImages(const QStringList& _picnames)
{
	clear();
	picnames = _picnames;
	pics = new QPixmap[2];
	int n = picnames.count();
	for (int i = 0; i < n; i++)
		pics[i] = OQPixmap(picnames[i]);
	if (n < 2)
		pics[0] = OQPixmap("tools/red_ex.png");
	if (n == 1)
		pics[1] = OQPixmap("tools/red_ex.png");
	updatePixmap();
	return true;
}


void
OCSwitch::clear()
{
	picnames.clear();
	delete [] pics;
	pics = 0;
}


void
OCSwitch::toggled(bool on)
{
	// Note: in read-only mode the event will be filtered out by OCWidget.
	if (updating || !button())
		return;
	int newval = on ? 1 : 0;
	qDebug("switch: set %s = %d", varName(0).ascii(), newval);
	writeValue(newval);
}


void
OCSwitch::updateValue(const QVariant& value)
{
	if (!button())
		return;
	updating = true;
	down = (value.toInt() > 0);
	qDebug("switch: update %s as %d", varName(0).ascii(), down);
	button()->setOn(down);
	updatePixmap();
	updating = false;
}


void
OCSwitch::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = 0;
	maxv = 1.5;
	is_int = true;
}


void
OCSwitch::updatePixmap()
{
	if (button() && pics) {
		QPushButton& btn = *button();
		QPixmap& pic = pics[int(down)];

		btn.setPixmap(pic);
		if (btn.width() != pic.width() + margin
				|| btn.height() != pic.height() + margin) {
			btn.resize(pic.width() + margin, pic.height() + margin);
		}
		updateSize(button()->size());
	} else {
		updateSize(1, 1);
	}
}


void
OCSwitch::drawShape(QPainter& p)
{
	// button will draw itself
}
