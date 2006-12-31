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


OCWidget *
OQCanvasView::newBasicWidget(int rtti)
{
	qDebug(QString("newBasicWidget(0x%1)").arg(rtti, 0, 16));
	OQCanvas *c = getCanvas();
	switch (rtti) {
		case ORTTI_Label:		return new OCLabel(c);
		case ORTTI_String:		return new OCString(c);
		case ORTTI_Pixmap:		return new OCPixmap(c);
		case ORTTI_Bar:			return new OCBar(c);
		case ORTTI_Button:		return new OCButton(c);
		case ORTTI_Switch:		return new OCSwitch(c);
		case ORTTI_Anim:		return new OCAnim(c);
		case ORTTI_Ruler:		return new OCRuler(c);
		case ORTTI_Dial:		return new OCDial(c);
		case ORTTI_BigTank:		return new OCBigTank(c);
		case ORTTI_SmallTank:	return new OCSmallTank(c);
		case ORTTI_RoundTank:	return new OCRoundTank(c);
		case ORTTI_Thermometer:	return new OCThermometer(c);
		case ORTTI_Volume:		return new OCVolume(c);
		case ORTTI_Manometer:	return new OCManometer(c);
		case ORTTI_DigitalLED:	return new OCDigitalLED(c);
		case ORTTI_Curve:		return new OCCurve(c);
		case ORTTI_Runner:		return new OCRunner(c);
		case ORTTI_Offnom:		return new OCOffnom(c);
		case ORTTI_FormLink:	return new OCFormLink(c);
		default:
			qFatal("Invalid rtti");
			return 0;
	}
}



