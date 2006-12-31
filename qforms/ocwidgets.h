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

#ifndef OCWIDGETS_H
#define OCWIDGETS_H

#include "oqcanvas.h"


#define ORTTI_Label				0xb0100
#define ORTTI_String			0xb0101
#define ORTTI_Pixmap			0xb0102
#define ORTTI_Bar				0xb0103
#define ORTTI_Button			0xb0200
#define ORTTI_Switch			0xb0201
#define ORTTI_Anim				0xb0202
#define ORTTI_Range				0xb0300
#define ORTTI_Ruler				0xb0301
#define ORTTI_Dial				0xb0302
#define ORTTI_Meter				0xb0400
#define ORTTI_BigTank			0xb0401
#define ORTTI_SmallTank			0xb0402
#define ORTTI_RoundTank			0xb0403
#define ORTTI_Thermometer		0xb0404
#define ORTTI_Volume			0xb0405
#define ORTTI_Manometer			0xb0406
#define ORTTI_DigitalLED		0xb0407
#define ORTTI_Curve				0xb0500
#define ORTTI_Control			0xb0600
#define ORTTI_Runner			0xb0601
#define ORTTI_Offnom			0xb0602
#define ORTTI_FormLink			0xb0603


class OCLabel : public OCWidget
{
public:
	OCLabel(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Label; }
};


class OCString : public OCWidget
{
public:
	OCString(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_String; }
};


class OCPixmap : public OCWidget
{
public:
	OCPixmap(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Pixmap; }
};


class OCBar : public OCWidget
{
public:
	OCBar(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Bar; }
};


class OCButton : public OCWidget
{
public:
	OCButton(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Button; }
};


class OCSwitch : public OCWidget
{
public:
	OCSwitch(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Switch; }
};


class OCAnim : public OCWidget
{
public:
	OCAnim(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Anim; }
};


class OCRuler : public OCWidget
{
public:
	OCRuler(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Ruler; }
};


class OCDial : public OCWidget
{
public:
	OCDial(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Dial; }
};


class OCBigTank : public OCWidget
{
public:
	OCBigTank(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_BigTank; }
};


class OCSmallTank : public OCWidget
{
public:
	OCSmallTank(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_SmallTank; }
};


class OCRoundTank : public OCWidget
{
public:
	OCRoundTank(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_RoundTank; }
};


class OCThermometer : public OCWidget
{
public:
	OCThermometer(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Thermometer; }
};


class OCVolume : public OCWidget
{
public:
	OCVolume(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Volume; }
};


class OCManometer : public OCWidget
{
public:
	OCManometer(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Manometer; }
};


class OCDigitalLED : public OCWidget
{
public:
	OCDigitalLED(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_DigitalLED; }
};


class OCCurve : public OCWidget
{
public:
	OCCurve(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Curve; }
};


class OCRunner : public OCWidget
{
public:
	OCRunner(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Runner; }
};


class OCOffnom : public OCWidget
{
public:
	OCOffnom(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_Offnom; }
};


class OCFormLink : public OCWidget
{
public:
	OCFormLink(OQCanvas *c) : OCWidget(c)	{}
	virtual int rtti() const	{ return ORTTI_FormLink; }
};


#endif // OCWIDGETS_H
