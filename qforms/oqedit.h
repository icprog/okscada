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

  Optikus Display Editor.

*/

#include <qkeysequence.h>

#include "oqcanvas.h"
#include "ocwidgets.h"


class QAction;
class QWidget;
class QPopupMenu;
class QMenuBar;
class QToolBar;
class QStatusBar;


class OQEditView : public OQCanvasView
{
	Q_OBJECT

public:

	OQEditView(QWidget *parent);
	void setupWidgets(QWidget *panel, QStatusBar *statusbar);
	void setupActions(QMenuBar *menubar, QToolBar *toolbar);
	void setStatus(const QString &);
	void newWidget(OCWidget *);
	void setGrid(int step_x, int step_y);
	void setGrid(int step)		{ setGrid(step, step); }

public slots:

	void fileNew();
	void fileOpen();
	void fileSave();
	void fileSaveAs();
	void fileQuit();

	void editProps();
	void editCopy();
	void editDelete();
	void editSnapDown();
	void editSnapUp();
	void editBringBack();
	void editBringFront();
	void editResize();

	void setBgImage();
	void setGrid1()		{ setGrid(1); }
	void setGrid4()		{ setGrid(4); }
	void setGrid8()		{ setGrid(8); }
	void setGrid16()	{ setGrid(16); }

	void helpAbout();

	void newLabel()			{ newBasicWidget(ORTTI_Label); }
	void newString()		{ newBasicWidget(ORTTI_String); }
	void newPixmap()		{ newBasicWidget(ORTTI_Pixmap); }
	void newBar()			{ newBasicWidget(ORTTI_Bar); }

	void newButton()		{ newBasicWidget(ORTTI_Button); }
	void newSwitch()		{ newBasicWidget(ORTTI_Switch); }
	void newAnim()			{ newBasicWidget(ORTTI_Anim); }

	void newRuler()			{ newBasicWidget(ORTTI_Ruler); }
	void newDial()			{ newBasicWidget(ORTTI_Dial); }

	void newBigTank()		{ newBasicWidget(ORTTI_BigTank); }
	void newSmallTank()		{ newBasicWidget(ORTTI_SmallTank); }
	void newRoundTank()		{ newBasicWidget(ORTTI_RoundTank); }
	void newThermometer()	{ newBasicWidget(ORTTI_Thermometer); }
	void newVolume()		{ newBasicWidget(ORTTI_Volume); }
	void newManometer()		{ newBasicWidget(ORTTI_Manometer); }
	void newDigitalLED()	{ newBasicWidget(ORTTI_DigitalLED); }

	void newCurve()			{ newBasicWidget(ORTTI_Curve); }
	void newRunner()		{ newBasicWidget(ORTTI_Runner); }
	void newOffnom()		{ newBasicWidget(ORTTI_Offnom); }
	void newFormLink()		{ newBasicWidget(ORTTI_FormLink); }

private:

	QAction *newAction(const char *text, const char *icon,
						const char *slot, bool toggle = false,
						QKeySequence accel = 0);
	QAction *newCreator(QPopupMenu *pm, QToolBar *tb,
						const char *text, const char *icon,
						const char *slot, QKeySequence accel = 0);
	QAction *newCreator(QPopupMenu *pm, const char *text,
						const char *slot, QKeySequence accel = 0)
	{
		return newCreator(pm, 0, text, 0, slot, accel);
	}

	QStatusBar *statusbar;
};
