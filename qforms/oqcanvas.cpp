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

#include "oqcanvas.h"


/*				Canvas				*/


OQCanvas::OQCanvas()
	: QCanvas()
{
}


void
OQCanvas::refreshSlot(int param)
{
}


/*				Canvas View				*/


OQCanvasView::OQCanvasView(QWidget *parent)
	: QCanvasView(parent)
{
	setCanvas(new OQCanvas());
	getCanvas()->resize(100, 100);
}


/*				Widget				*/


OCWidget::OCWidget(OQCanvas *c)
	: QCanvasItem(c)
{
}


void
OCWidget::forget()
{
}


void
OCWidget::destroy()
{
}


void
OCWidget::resize(int w, int h, int x, int y)
{
}


OCAttrMap&
OCWidget::getAttrMap(OCAttrMap& atts)
{
	return atts;
}


void
OCWidget::testRefresh(QObject *obj, int param, int timeout)
{
}


void
OCWidget::refreshWidget()
{
}


void
OCWidget::updateAppearance()
{
}


void
OCWidget::setTip(const char *tip)
{
}


void
OCWidget::writeByName(const char *name, const QVariant& value)
{
}


void
OCWidget::log(const QString& msg)
{
}


void
OCWidget::refreshSlot(int param)
{
}


/*				Active Widget				*/

OCActiveWidget::OCActiveWidget(OQCanvas *c)
	: OCWidget(c)
{
}


OCAttrMap&
OCActiveWidget::getAttrMap(OCAttrMap& atts)
{
	OCWidget::getAttrMap(atts);
	return atts;
}


void
OCActiveWidget::bind(const QStringList& vars)
{
}


void
OCActiveWidget::unbind()
{
}


/*				Periodic Widget				*/


OCPeriodicWidget::OCPeriodicWidget(OQCanvas *c)
	: OCWidget(c)
{
}


OCAttrMap&
OCPeriodicWidget::getAttrMap(OCAttrMap& atts)
{
	OCWidget::getAttrMap(atts);
	return atts;
}


void
OCPeriodicWidget::bindTime(int period, int when)
{
}


void
OCPeriodicWidget::unbindTime()
{
}

