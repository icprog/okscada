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


#define qDebug(...)


QMap<int,QString> OQCanvas::rtti2class;
QMap<QString,int> OQCanvas::class2rtti;



void
OQCanvas::setupClassMaps()
{
	if (!rtti2class.empty())
		return;

#	define ADDCLASS(x,y)	rtti2class.insert(ORTTI_##x, #y)
#	define ADDCLASS1(x)		ADDCLASS(x,x)
	ADDCLASS1(Label);
	ADDCLASS1(String);
	ADDCLASS1(Pixmap);
	ADDCLASS1(Bar);
	ADDCLASS(Button, Toggle::Button);
	ADDCLASS(Switch, Toggle::Switch);
	ADDCLASS1(Anim);
	ADDCLASS(Ruler, Range::Ruler);
	ADDCLASS(Dial, Range::Dial);
	ADDCLASS(BigTank, Meter::BigTank);
	ADDCLASS(SmallTank, Meter::SmallTank);
	ADDCLASS(RoundTank, Meter::RoundTank);
	ADDCLASS(Thermometer, Meter::ThermoMeter);
	ADDCLASS(Volume, Meter::Volume);
	ADDCLASS(Manometer, ManoMeter);
	ADDCLASS1(LED);
	ADDCLASS1(Curve);
	ADDCLASS(Runner, Control::Runner);
	ADDCLASS(Offnom, Control::Offnominal);
	ADDCLASS(Link, Control::Link);
	ADDCLASS(SolArray, Custom::SolArray);

	QMap<int,QString>::Iterator it;
	for (it = rtti2class.begin(); it != rtti2class.end(); ++it)
		class2rtti.insert(it.data(), it.key());
}


OCWidget *
OQCanvas::newBasicWidget(int rtti)
{
	qDebug(QString("newBasicWidget(0x%1)").arg(rtti, 0, 16));
	switch (rtti) {
		case ORTTI_Label:		return new OCLabel(this);
		case ORTTI_String:		return new OCString(this);
		case ORTTI_Pixmap:		return new OCPixmap(this);
		case ORTTI_Bar:			return new OCBar(this);
		case ORTTI_Button:		return new OCButton(this);
		case ORTTI_Switch:		return new OCSwitch(this);
		case ORTTI_Anim:		return new OCAnim(this);
		case ORTTI_Ruler:		return new OCRuler(this);
		case ORTTI_Dial:		return new OCDial(this);
		case ORTTI_BigTank:		return new OCBigTank(this);
		case ORTTI_SmallTank:	return new OCSmallTank(this);
		case ORTTI_RoundTank:	return new OCRoundTank(this);
		case ORTTI_Thermometer:	return new OCThermometer(this);
		case ORTTI_Volume:		return new OCVolume(this);
		case ORTTI_Manometer:	return new OCManometer(this);
		case ORTTI_LED:			return new OCLED(this);
		case ORTTI_Curve:		return new OCCurve(this);
		case ORTTI_Runner:		return new OCRunner(this);
		case ORTTI_Offnom:		return new OCOffnom(this);
		case ORTTI_Link:		return new OCLink(this);
		case ORTTI_SolArray:	return new OCSolArray(this);
		default:
			qWarning("Invalid rtti 0x%x", rtti);
			return 0;
	}
}
