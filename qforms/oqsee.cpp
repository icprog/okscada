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

  Optikus Data Viewer.

*/

#include <qpopupmenu.h>
#include <qmenubar.h>
#include <qlayout.h>
#include <qvgroupbox.h>
#include <qhgroupbox.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qstatusbar.h>
#include <qscrollview.h>
#include <qhbox.h>
#include <qpainter.h>
#include <qstyle.h>

#include "oqcommon.h"
#include "oqsee.h"

#include <config.h>				/* for package requisites */
#include <optikus/conf-mem.h>	/* for oxbcopy */


//#define qDebug(x...)
#if 0
#define qDebugData(x...)	qDebug(x)
#else
#define qDebugData(x...)
#endif

const QFont VALUE_FONT("Courier New", 12, QFont::Bold);

#define ROW_GROW_CHUNK		40


void
OQSeeView::fileNew()
{
	qDebug("fileNew");
}


void
OQSeeView::fileOpen()
{
	qDebug("fileOpen");
}


void
OQSeeView::fileSave()
{
	qDebug("fileSave");
}


void
OQSeeView::fileSaveAs()
{
	qDebug("fileSaveAs");
}


bool
OQSeeView::canQuit()
{
	qDebug("canQuit: changed=%d numRows=%d", changed, numRows());
	return (!changed
			|| numRows() == 0
			|| canDelete(false, "You are trying to quit the viewer.\n\n"
								"All your modifications will be LOST !\n\n"
								"Are you sure ?"));
}


void
OQSeeView::editPaste()
{
	qDebug("editPaste");
}


void
OQSeeView::helpAbout()
{
	QMessageBox::about(this, "About OqSee",
		"OqSee " PACKAGE_VERSION ", the Optikus data viewer\n\n"
		PACKAGE_BUGREPORT "\n\n" PACKAGE_COPYRIGHT "\n\n" PACKAGE_WEBSITE);
}


void
OQSeeView::viewStateBox(bool on)
{
	if (on)
		stategroup->show();
	else
		stategroup->hide();
}


void
OQSeeView::viewHex(bool on)
{
	qDebug(QString("viewHex: %1").arg(on));
	Var *v = byrow[currentRow()];
	setHexData(v, on);
	viewHexAc->setOn(v->isHex());
	updateCell(currentRow(), 1);
}


void OQSeeView::setHexData(Var * v, bool on)
{
	if (!v->id)
		return;
	if (!on) {
		v->hex = 0;
		return;
	}
	owquark_t inf;
	if (!watch.getInfo(v->id, inf))
		return;
	static QString hexable("bBhHiIlL");
	int oldhex = v->hex;
	v->hex = hexable.contains(inf.type) ? inf.len * 2: 0;
	if (v->hex != oldhex) {
		qDebug("changed hex from %d to %d", oldhex, v->hex);
		changed = true;
	}
}


void
OQSeeView::viewLog(bool on)
{
	qDebug(QString("viewLog: %1").arg(on));
}


void
OQSeeView::playStart(bool on)
{
	playStopAc->setOn(!on);
	watch.enableMonitoring(on);
	qDebug(QString("play: %1").arg(on));
}


void
OQSeeView::playStop(bool on)
{
	playStartAc->setOn(!on);
}


void
OQSeeView::newCommand()
{
	QString cmd = inedit->text().stripWhiteSpace();
	qDebug(QString("newCommand: \"%1\"").arg(cmd));
	if (cmd.isEmpty())
		return;

	static int next_no = 12300000;

	Var *v = new Var;
	v->no = ++next_no;
	v->id = 0;
	v->desc = cmd;
	v->row = numRows();
	v->hex = viewDefHexAc->isOn() ? 1 : 0;
	qDebug("start adding row=%d no=%d desc=%s",
			v->row, v->no, (const char *) v->desc);

	if (v->row >= (int) byrow.size())
		byrow.resize(v->row + ROW_GROW_CHUNK);

	byno.insert(v->no, v);
	byrow[v->row] = v;
	v->bg_id = watch.addMonitorBg(cmd, v->no, -1);
	setNumRows(v->row + 1);
	changed = true;
}


void
OQSeeView::monitorsUpdated(long param, const owquark_t& inf, int err)
{
	setStatus(QString("monitor %1 : %2")
				.arg(inf.desc).arg(watch.errorString(err)));
	int no = (int) param;
	if (!byno.contains(no)) {
		qDebug(QString("monitorsUpdated: no=%1 not found").arg(no));
		return;
	}
	Var *v = byno[no];
	qDebug("update: id=%lu row=%d desc=%s no=%d err=%s", inf.ooid, v->row,
			(const char *) v->desc, no, (const char *) watch.errorString(err));
	v->bg_id = 0;
	if (err != OK) {
		v->hex = 0;
		v->ermes = watch.errorString(err);
	} else {
		v->id = inf.ooid;
		byid.replace(inf.ooid, v);
		setHexData(v, v->isHex());
	}
	updateCell(v->row, 2);
}


bool
OQSeeView::areYouSure(bool sure, const char *text)
{
	if (sure)
		return true;
	int ans = QMessageBox::warning(this, "Are you sure ?", text,
						QMessageBox::Yes | QMessageBox::Default,
						QMessageBox::No | QMessageBox::Escape);
	return (ans == QMessageBox::Yes);
}


void
OQSeeView::removeRow(int row, bool sure)
{
	int nrows = numRows();
	if (row < 0 || row >= nrows) {
		qDebug("cannot delete row %d", row);
		return;
	}

	if (!canDelete(sure, QString("Really remove row %1 ?").arg(row)))
		return;

	qDebug("deleting row %d ..", row);
	Var *v = byrow[row];

	if (v->bg_id)
		watch.cancelMonitorBg(v->bg_id);
	if (v->id)
		watch.removeMonitor(v->id);

	byid.remove(v->id);
	byno.remove(v->no);

	for (int r = row; r < nrows - 1; r++) {
		byrow[r] = byrow[r+1];
		byrow[r]->row --;
	}

	int crow = currentRow();
	QTable::removeRow(row);
	// Workaround for Qt jumping to the bottom row.
	setCurrentCell(crow == nrows - 1 ? nrows - 1 : crow, 1);

	delete v;
	changed = true;
	qDebug("row %d deleted", row);
}


void
OQSeeView::removeBroken()
{
	qDebug("removing broken items...");
	int count = 0;
	QMap<int,VarPtr>::iterator it = byno.begin();
	bool sure = false;
	int crow = currentRow();
	while (it != byno.end()) {
		Var *v = *it;
		++it;
		if (v->bg_id || !v->ermes.isEmpty()) {
			if (!canDelete(sure, "Really remove broken items ?"))
				return;
			sure = true;
			qDebug("broken: %s (row=%d)", (const char *) v->desc, v->row);
			removeRow(v->row);
			if (v->row < crow)
				crow--;
			count++;
		}
	}
	setCurrentCell(crow, 1);
	changed = true;
	qDebug("removed %d broken items", count);
}


void
OQSeeView::removeAll()
{
	if (numRows() == 0)
		return;
	if (!canDelete(false,
				"Are you ready to clear\n\nALL\n\nitems in the list ?"))
		return;

	qDebug("remove all items");
	for (QMap<int,VarPtr>::iterator it = byno.begin(); it != byno.end(); ++it) {
		Var *v = *it;
		if (v->bg_id)
			watch.cancelMonitorBg(v->bg_id);
		delete v;
	}
	watch.removeAllMonitors();
	setNumRows(0);
	byno.clear();
	byrow.truncate(0);
	byid.clear();
	changed = true;
}


void
OQSeeView::currentRowChanged(int row, int col)
{
	qDebug("current row = %d", row);
	viewHexAc->setOn(row < 0 ? false : byrow[row]->isHex() > 0);
}


void
OQSeeView::paintCell(QPainter *p, int row, int col, const QRect& cr,
					bool sel, const QColorGroup &cg)
{
	p->setClipRect(cellRect(row, col), QPainter::CoordPainter);

	int w = cr.width();
	int h = cr.height();

	Var *v = byrow[row];
	qDebugData("paint(%d,%d): v=%p desc=%s",
				row, col, v, v?(const char *)v->desc : "?");
	QString text;
	switch (col) {
		case 0:
			text = v->desc;
			break;
		case 1:
			if (!v->id) {
				text = "--";
			} else if (v->hex > 0) {
				text = "0x"
						+ QString("%1")
							.arg(watch.getLong(v->desc), 0, 16)
							.right(v->hex).rightJustify(v->hex, '0');
			} else {
				text = watch.getString(v->desc);
			}
			break;
		case 2:
			if (!v->ermes.isEmpty()) {
				text = v->ermes;
			} else if (!v->id) {
				text = "not available";
			} else {
				owquark_t info;
				if (watch.getInfo(v->id, info)) {
					text.sprintf("id=%03d type=%c ptr=%c seg=%c len=%-2d desc=%s",
								(int) info.ooid, info.type, info.ptr, info.seg,
								info.len, info.desc);
				} else {
					text = "no information";
				}
			}
			break;
		default:
			text = "?";
			break;
	}

	p->fillRect(0, 0, w, h, cg.brush(sel ? QColorGroup::Highlight
										: QColorGroup::Base));
	p->setPen(sel ? cg.highlightedText() : cg.text());
	p->setFont(VALUE_FONT);
    p->drawText(2, 0, w - 2 - 4, h, AlignLeft | AlignVCenter, text);

	int gridc = style().styleHint(QStyle::SH_Table_GridLineColor, this);
	const QPalette &pal = palette();
	if (gridc != -1 && (cg == colorGroup() || cg == pal.disabled()
						|| cg == pal.inactive())) {
		p->setPen((QRgb)gridc);
	} else {
		p->setPen(cg.mid());
	}
	w--;
	h--;
	p->drawLine(w, 0, w, h);
	p->drawLine(0, h, w, h);

	p->setClipping(false);
}


void
OQSeeView::dataUpdated(const owquark_t& inf, const QVariant& v, long time)
{
	int row = byid[inf.ooid]->row;
	qDebugData("data(row=%d id=%lu): %s = %s", row, inf.ooid,
			(const char *) inf.desc, (const char *) QVariant(v).asString());
	updateCell(row, 1);
}


void
OQSeeView::subjectsUpdated(const QString& all,
						const QString& subject, const QString& state)
{
	qDebug("subject [" + subject + "] : [" + state + "]");
	OQSubjectBox::updateGroupWidget(all, statebox);
	OQSubjectBox::updateGroupWidget(subject, state, statebox);
	if (subject == "*") {
		inbtn->setPixmap(OQPixmap(state.isEmpty()
								? "kpic/apply.png" : "kpic/button_ok.png"));
	}
}


void
OQSeeView::setStatus(const QString &msg)
{
	statusbar->message(msg);
}


OQSeeView::OQSeeView(QWidget *parent)
	: QTable(0, 3, parent)
{
	items.setAutoDelete(true);
	widgets.setAutoDelete(true);
	setReadOnly(true);
	setSelectionMode(SingleRow);
	QHeader *hh = horizontalHeader();
	hh->setLabel(0, "Name", 150);
	hh->setLabel(1, "Value", 120);
	hh->setLabel(2, "Information", 700);

	watch.init("oqsee");
	watch.connect();
	connect(&watch,
		SIGNAL(subjectsUpdated(const QString&, const QString&, const QString&)),
		SLOT(subjectsUpdated(const QString&, const QString&, const QString&)));
	connect(&watch,
			SIGNAL(monitorsUpdated(long, const owquark_t&, int)),
			SLOT(monitorsUpdated(long, const owquark_t&, int)));
	connect(&watch,
			SIGNAL(dataUpdated(const owquark_t&, const QVariant&, long)),
			SLOT(dataUpdated(const owquark_t&, const QVariant&, long)));
	connect(this, SIGNAL(currentChanged(int,int)),
			SLOT(currentRowChanged(int,int)));

	changed = false;
}


QAction *
OQSeeView::newAction(QPopupMenu *menu, const char *text, const char *icon,
					const char *slot, bool toggle, QKeySequence accel)
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
	if (menu) {
		action->addTo(menu);
	}
	return action;
}


void
OQSeeView::setupWidgets(QWidget *panel, QStatusBar *_statusbar)
{
	QBoxLayout *vbox = (QBoxLayout *) panel->layout();

	QHGroupBox *ingroup = new QHGroupBox("Commands", panel);
	vbox->addWidget(ingroup);
	inedit = new QLineEdit(ingroup);
	inbtn = new QPushButton(ingroup);
	int h = inedit->height();
	inbtn->setMinimumSize(h, h);
	inbtn->setPixmap(OQPixmap("kpic/apply.png"));

	QHGroupBox *stategroup = new QHGroupBox("Subjects", panel);
	vbox->addWidget(stategroup, 1, AlignCenter | AlignBottom);
	QScrollView *statescroll = new QScrollView(stategroup);
	this->stategroup = stategroup;
	statebox = new QHBox;
	OQSubjectBox *hubbox = new OQSubjectBox("*", statebox);
	statescroll->addChild(statebox);
	statescroll->setResizePolicy(QScrollView::AutoOneFit);
	h = hubbox->height() + statescroll->horizontalScrollBar()->height();
	statescroll->setMaximumHeight(h);
	statescroll->setVScrollBarMode(QScrollView::AlwaysOff);
	stategroup->hide();

	connect(inedit, SIGNAL(returnPressed()), SLOT(newCommand()));
	connect(inbtn, SIGNAL(clicked()), SLOT(newCommand()));

	statusbar = _statusbar;
	statusbar->clear();
	inedit->setFocus();
}


void
OQSeeView::setupActions(QMenuBar *menubar, QToolBar *toolbar)
{
	this->statusbar = statusbar;

	QPopupMenu *fileMenu = new QPopupMenu(this);
	menubar->insertItem("&File", fileMenu);
	QPopupMenu *editMenu = new QPopupMenu(this);
	menubar->insertItem("&Edit", editMenu);
	QPopupMenu *viewMenu = new QPopupMenu(this);
	menubar->insertItem("&View", viewMenu);
	QPopupMenu *helpMenu = new QPopupMenu(this);
	menubar->insertItem("&Help", helpMenu);

	QAction *fileOpenAc = newAction(fileMenu, "&Open", "kpic/fileopen.png",
									SLOT(fileOpen()));
	QAction *fileSaveAc = newAction(fileMenu, "&Save", "kpic/filesave.png",
									SLOT(fileSave()));
	newAction(fileMenu, "Save &as...", "kpic/filesaveas.png", SLOT(fileSaveAs()));
	fileMenu->insertSeparator();
	QAction *fileQuitAc = newAction(fileMenu, "&Quit", "kpic/exit.png",
									SLOT(fileQuit()));

	QAction *removeCurrentAc = newAction(editMenu,
									"Delete current item", "kpic/editcut.png",
									SLOT(removeCurrent()), false, Key_Delete);
	newAction(editMenu, "Paste", "kpic/editpaste.png", SLOT(editPaste()));
	QAction *removeBrokenAc = newAction(editMenu, "Remove broken items",
									"kpic/editdelete.png", SLOT(removeBroken()));
	QAction *removeAllAc = newAction(editMenu, "Clear list",
									"kpic/editclear.png", SLOT(removeAll()));
	editConfirmAc = newAction(editMenu, "confirm Actions", "", 0, true);

	playStartAc = newAction(viewMenu, "Play Start", "kpic/player_play.png",
							SLOT(playStart(bool)), true);
	playStopAc = newAction(viewMenu, "Play Stop", "kpic/stop.png",
							SLOT(playStop(bool)), true);
	viewLogAc = newAction(viewMenu, "Logging", "kpic/toggle_log.png",
							SLOT(viewLog(bool)), true);
	viewHexAc = newAction(viewMenu, "Hexadecimal", "kpic/math_brackets.png",
							SLOT(viewHex(bool)), true);
	viewDefHexAc = newAction(viewMenu, "by default Hexadecimal", "", 0, true);
	viewStateBoxAc = newAction(viewMenu, "Show State Box",
								"kpic/connect_creating.png",
								SLOT(viewStateBox(bool)), true);

	newAction(helpMenu, "About", "kpic/help.png", SLOT(helpAbout()));

	playStartAc->addTo(toolbar);
	viewLogAc->addTo(toolbar);
	viewHexAc->addTo(toolbar);
	toolbar->addSeparator();

	removeCurrentAc->addTo(toolbar);
	removeBrokenAc->addTo(toolbar);
	removeAllAc->addTo(toolbar);
	toolbar->addSeparator();

	fileOpenAc->addTo(toolbar);
	fileSaveAc->addTo(toolbar);
	toolbar->addSeparator();

	toolbar->setStretchableWidget(new QWidget(toolbar));	// spacer
	fileQuitAc->addTo(toolbar);

	editConfirmAc->setOn(true);
	playStartAc->setOn(true);
	viewDefHexAc->setOn(false);
}


OQSeeWindow::OQSeeWindow()
	: QMainWindow()
{
	QWidget *panel = new QWidget(this);
	setCentralWidget(panel);
	QVBoxLayout *vbox = new QVBoxLayout(panel);
	vbox->setMargin(2);
	vbox->setSpacing(2);

	QVGroupBox *dataframe = new QVGroupBox("Data", panel);
	vbox->addWidget(dataframe, 10);
	seeview = new OQSeeView(dataframe);
	seeview->setupWidgets(panel, statusBar());
	seeview->setupActions(menuBar(), new QToolBar(this));

	setCaption("Optikus Data Viewer");
	setIcon(OQPixmap("kpic/viewmag.png"));
	resize(400, 500);
}


void
OQSeeWindow::closeEvent(QCloseEvent *e)
{
	if (getView()->canQuit())
		QMainWindow::closeEvent(e);
}


int
main(int argc, char **argv)
{
	OQApplication app("oqsee", argc, argv);
	OQSeeWindow win;
	app.setMainWidget(&win);
	app.setQuitFilter(win.getView());
	win.show();
	return app.exec();
}

