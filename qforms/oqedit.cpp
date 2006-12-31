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

#include <qstatusbar.h>
#include <qaction.h>
#include <qpopupmenu.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qmainwindow.h>
#include <qmessagebox.h>

#include <config.h>		/* for package requisites */

#include "oqcommon.h"
#include "oqedit.h"


#define qDebug(...)


void
OQEditView::newWidget(OCWidget *)
{
	qDebug("newWidget");
}


void
OQEditView::setGrid(int step_x, int step_y)
{
	qDebug(QString("setGrid(%1 x %2)").arg(step_x).arg(step_y));
}


void
OQEditView::setBgImage()
{
	qDebug("setBgImage");
}


void
OQEditView::fileNew()
{
	qDebug("fileNew");
}


void
OQEditView::fileOpen()
{
	qDebug("fileOpen");
}


void
OQEditView::fileSave()
{
	qDebug("fileSave");
}


void
OQEditView::fileSaveAs()
{
	qDebug("fileSaveAs");
}


void
OQEditView::fileQuit()
{
	qDebug("fileQuit");
	qApp->quit();
}


void
OQEditView::editProps()
{
	qDebug("editProps");
}


void
OQEditView::editCopy()
{
	qDebug("editCopy");
}


void
OQEditView::editDelete()
{
	qDebug("editDelete");
}


void
OQEditView::editSnapDown()
{
	qDebug("editSnapDown");
}


void
OQEditView::editSnapUp()
{
	qDebug("editSnapUp");
}


void
OQEditView::editBringBack()
{
	qDebug("editBringBack");
}


void
OQEditView::editBringFront()
{
	qDebug("editBringFront");
}


void
OQEditView::editResize()
{
	qDebug("editResize");
}


void
OQEditView::helpAbout()
{
	QMessageBox::about(this, "About OqEdit",
		"OqEdit " PACKAGE_VERSION ", the Optikus display editor\n\n"
		PACKAGE_BUGREPORT "\n\n" PACKAGE_COPYRIGHT "\n\n" PACKAGE_WEBSITE);
}


void
OQEditView::setStatus(const QString &msg)
{
	statusbar->message(msg);
}


QAction *
OQEditView::newAction(const char *text, const char *icon, const char *slot,
						bool toggle, QKeySequence accel)
{
	QAction *action;
	if (icon && *icon)
		action = new QAction(OQPixmap(icon), text, accel, this);
	else
		action = new QAction(text, accel, this);
	if (toggle) {
		action->setToggleAction(true);
		if (slot)
			connect(action, SIGNAL(toggled(bool)), slot);
	} else {
		if (slot)
			connect(action, SIGNAL(activated()), slot);
	}
	return action;
}


QAction *
OQEditView::newCreator(QPopupMenu *pm, QToolBar *tb,
						const char *text, const char *icon,
						const char *slot, QKeySequence accel)
{
	QAction *action = newAction(text, icon, slot, false, accel);
	if (pm)
		action->addTo(pm);
	if (tb)
		action->addTo(tb);
	return action;
}


OQEditView::OQEditView(QWidget *parent)
	: OQCanvasView(parent)
{
}


void
OQEditView::setupWidgets(QWidget *panel, QStatusBar *_statusbar)
{
	statusbar = _statusbar;
	statusbar->clear();
}


void
OQEditView::setupActions(QMenuBar *menubar, QToolBar *toolbar)
{
	QPopupMenu *fileMenu = new QPopupMenu(this);
	menubar->insertItem("&File", fileMenu);
	QPopupMenu *newMenu = new QPopupMenu(this);
	menubar->insertItem("&Create", newMenu);
	QPopupMenu *optMenu = new QPopupMenu(this);
	menubar->insertItem("&Options", optMenu);

	menubar->insertSeparator();		// never worked
	QPopupMenu *helpMenu = new QPopupMenu(this);
	menubar->insertItem("&Help", helpMenu);

	QAction *fileNewAc = newAction("&New", "kpic/filenew.png", SLOT(fileNew()));
	fileNewAc->addTo(fileMenu);
	QAction *fileOpenAc = newAction("&Open", "kpic/fileopen.png", SLOT(fileOpen()));
	fileOpenAc->addTo(fileMenu);
	QAction *fileSaveAc = newAction("&Save", "kpic/filesave.png", SLOT(fileSave()));
	fileSaveAc->addTo(fileMenu);
	QAction *fileSaveAsAc = newAction("Save &as...", "kpic/filesaveas.png", SLOT(fileSaveAs()));
	fileSaveAsAc->addTo(fileMenu);
	QAction *fileQuitAc = newAction("&Quit", "kpic/exit.png", SLOT(fileQuit()));
	fileQuitAc->addTo(fileMenu);

	fileNewAc->addTo(toolbar);
	fileOpenAc->addTo(toolbar);
	fileSaveAc->addTo(toolbar);
	fileSaveAsAc->addTo(toolbar);
	toolbar->addSeparator();

	QAction *editPropsAc = newAction("Change item properties",
									"kpic/edit.png", SLOT(editProps()));
	QAction *editCopyAc = newAction("Copy item",
									"kpic/editcopy.png", SLOT(editCopy()));
	QAction *editDeleteAc = newAction("Delete item",
									"kpic/editdelete.png", SLOT(editDelete()));
	QAction *editSnapDownAc = newAction("Snap down",
									"kpic/fontsizedown.png", SLOT(editSnapDown()));
	QAction *editSnapUpAc = newAction("Snap up",
									"kpic/fontsizeup.png", SLOT(editSnapUp()));
	QAction *editBringBackAc = newAction("Bring to back",
									"kpic/2downarrow.png", SLOT(editBringBack()));
	QAction *editBringFrontAc = newAction("Bring to front",
									"kpic/2uparrow.png", SLOT(editBringFront()));
	QAction *editResizeAc = newAction("Resize item",
									"kpic/move.png", SLOT(editResize()));

	editPropsAc->addTo(toolbar);
	editCopyAc->addTo(toolbar);
	editDeleteAc->addTo(toolbar);
	toolbar->addSeparator();

	QAction *optBgImageAc = newAction("&Background image", "", SLOT(setBgImage()));
	optBgImageAc->addTo(optMenu);
	QAction *optGrid1_Ac = newAction("Grid: &1", "", SLOT(setGrid1()));
	optGrid1_Ac->addTo(optMenu);
	QAction *optGrid4_Ac = newAction("Grid: &4", "", SLOT(setGrid4()));
	optGrid4_Ac->addTo(optMenu);
	QAction *optGrid8_Ac = newAction("Grid: &8", "", SLOT(setGrid8()));
	optGrid8_Ac->addTo(optMenu);
	QAction *optGrid16_Ac = newAction("Grid: &16", "", SLOT(setGrid16()));
	optGrid16_Ac->addTo(optMenu);

	QAction *helpAboutAc = newAction("About", "kpic/help.png", SLOT(helpAbout()));
	helpAboutAc->addTo(helpMenu);

	QPopupMenu *pmLabel = new QPopupMenu(this);
	newMenu->insertItem("&Label", pmLabel);
	QPopupMenu *pmButton = new QPopupMenu(this);
	newMenu->insertItem("&Button", pmButton);
	QPopupMenu *pmRange = new QPopupMenu(this);
	newMenu->insertItem("&Range", pmRange);
	QPopupMenu *pmMeter = new QPopupMenu(this);
	newMenu->insertItem("&Meter", pmMeter);
	QPopupMenu *pmIndic = new QPopupMenu(this);
	newMenu->insertItem("&Indicator", pmIndic);
	QPopupMenu *pmControl = new QPopupMenu(this);
	newMenu->insertItem("C&ontrol", pmControl);

	newCreator(pmLabel, toolbar, "Label", "kpic/frame_text.png", SLOT(newLabel()));
	newCreator(pmLabel, toolbar, "String", "kpic/frame_formula.png", SLOT(newString()));
	newCreator(pmLabel, toolbar, "Pixmap", "kpic/frame_image.png", SLOT(newPixmap()));
	newCreator(pmLabel, "Bar", SLOT(newBar()));
	newCreator(pmButton, "Button", SLOT(newButton()));
	newCreator(pmButton, toolbar, "Switch", "kpic/frame_chart.png", SLOT(newSwitch()));
	newCreator(pmButton, "Animation", SLOT(newAnim()));
	newCreator(pmRange, "Ruler", SLOT(newRuler()));
	newCreator(pmRange, "Dial", SLOT(newDial()));
	newCreator(pmMeter, "Big Tank", SLOT(newBigTank()));
	newCreator(pmMeter, "Small Tank", SLOT(newSmallTank()));
	newCreator(pmMeter, "Round Tank", SLOT(newRoundTank()));
	newCreator(pmMeter, "Thermometer", SLOT(newThermometer()));
	newCreator(pmMeter, "Volume", SLOT(newVolume()));
	newCreator(pmMeter, "Manometer", SLOT(newManometer()));
	newCreator(pmIndic, "Digital LED", SLOT(newDigitalLED()));
	newCreator(pmIndic, "Curve", SLOT(newCurve()));
	newCreator(pmControl, "Runner", SLOT(newRunner()));
	newCreator(pmControl, "Offnominal", SLOT(newOffnom()));
	newCreator(pmControl, "Form Link", SLOT(newFormLink()));
	toolbar->addSeparator();

	editSnapDownAc->addTo(toolbar);
	editSnapUpAc->addTo(toolbar);
	editBringBackAc->addTo(toolbar);
	editBringFrontAc->addTo(toolbar);
	editResizeAc->addTo(toolbar);
	toolbar->addSeparator();

	toolbar->setStretchableWidget(new QWidget(toolbar));	// spacer
	fileQuitAc->addTo(toolbar);
}


int
main(int argc, char **argv)
{
	OQApplication app("oqedit", argc, argv);
	QMainWindow win;

	OQEditView *editview = new OQEditView(&win);
	editview->setupWidgets(0, win.statusBar());
	editview->setupActions(win.menuBar(), new QToolBar(&win));
	win.setCentralWidget(editview);

	app.setMainWidget(&win);
	win.setCaption("Optikus Display Editor");
	win.setIcon(OQPixmap("kpic/colorize.png"));
	win.resize(600, 500);
	win.show();
	return app.exec();
}

