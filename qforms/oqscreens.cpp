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

  Optikus Screen Viewer.

*/

#include "oqcommon.h"
#include "oqscreens.h"
#include "oqwatch.h"

#include <qstatusbar.h>
#include <qaction.h>
#include <qpopupmenu.h>
#include <qtoolbar.h>
#include <qvgroupbox.h>
#include <qvbox.h>
#include <qsignalmapper.h>
#include <qregexp.h>
#include <qmessagebox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>


#define qDebug(...)

bool OQScreenList::log_open_close = false;


/* ================ Input Popup ================ */


OQInputPopup::OQInputPopup(QWidget *parent, const char *name,
							OCDataWidget *_dw, const QPoint& gpos)
	:	QDialog(parent, name),
		changed(false), edit(0), apply(0), dw(_dw)
{
	setWFlags(WDestructiveClose);
	setCaption("Value: " + dw->varName());
	QBoxLayout *vbox = new QVBoxLayout(this, 4, 4);
	QBoxLayout *top = new QHBoxLayout(vbox, 4);
	QBoxLayout *bot = new QHBoxLayout(vbox, 4);
	QLabel *label = new QLabel(dw->varName(), this);
	top->addWidget(label);
	edit = new QLineEdit(this);
	edit->setReadOnly(dw->isReadOnly());
	top->addWidget(edit);
	label->setBuddy(edit);
	edit->setText(dw->readValue().toString());
	connect(edit, SIGNAL(textChanged(const QString&)), SLOT(valueChanged()));
	adjustSize();
	move(mapToParent(mapFromGlobal(gpos)) - QPoint(width()/2, height()/2));
	QPushButton *ok, *cancel;
	ok = new OQPushButton("O&K", "kpic/button_ok.png", this);
	if (dw->isReadOnly()) {
		connect(ok, SIGNAL(clicked()), SLOT(handleCancel()));
		bot->addStretch(2);
		bot->addWidget(ok, 1);
		bot->addStretch(2);
	} else {
		bot->addStretch(1);
		connect(ok, SIGNAL(clicked()), SLOT(handleOK()));
		bot->addWidget(ok);
		apply = new OQPushButton("&Apply", "kpic/apply.png", this);
		bot->addWidget(apply);
		connect(apply, SIGNAL(clicked()), SLOT(handleApply()));
		cancel = new OQPushButton("&Cancel", "kpic/button_cancel.png", this);
		bot->addWidget(cancel);
		connect(cancel, SIGNAL(clicked()), SLOT(handleCancel()));
		bot->addStretch(1);
	}
	connect(dw, SIGNAL(destroyed()), SLOT(handleCancel()));
	show();
}


OQInputPopup::~OQInputPopup()
{
	qDebug("input popup destroyed");
}


void
OQInputPopup::write()
{
	if (!dw->isReadOnly() && changed) {
		QString str = edit->text().stripWhiteSpace();
		if (!str.isEmpty()) {
			qDebug("write " + dw->varName() + " := " + str);
			dw->writeValue(QVariant(str));
		}
	}
}


void
OQInputPopup::click(int btn)
{
	switch (btn) {
		case 0:		// cancel
			done(0);
			break;
		case 1:		// OK
			write();
			done(1);
			break;
		case 2:		// Apply
			changed = true;
			write();
			apply->setDefault(true);
			break;
	}
	changed = false;
}


/* ================ View ================ */


void
OQScreenView::toggleList(bool on)
{
	toggleListAc->setOn(on);
	emit listToggled(on);
}


void
OQScreenView::touchList()
{
	emit listToggled(toggleListAc->isOn());
}


bool
OQScreenView::canClose()
{
	emit screenClosing(name());
	// when connected to list, the list decides whether we can close or not.
	return !connected;
}


void
OQScreenView::setStatus(const QString &msg)
{
	statusbar->message(msg);
}


OQScreenView::OQScreenView(QWidget *parent, QToolBar *tb, QStatusBar *sb)
	:	OQCanvasView(parent),
		statusbar(sb),
		connected(false)
{
	statusbar->clear();
	QAction *a;

	a = new QAction(OQPixmap("kpic/stop.png"), "Close screen", 0, this);
	connect(a, SIGNAL(activated()), SLOT(canClose()));
	a->addTo(tb);

	a = new QAction(OQPixmap("kpic/view_tree.png"), "Show/Hide list", 0, this);
	connect(a, SIGNAL(toggled(bool)), this, SLOT(toggleList(bool)));
	a->setToggleAction(true);
	a->addTo(tb);
	toggleListAc = a;
	toggleListAc->setOn(false);

	a = new QAction(OQPixmap("kpic/view_remove.png"),
					"Raise list (if shown)", 0, this);
	connect(a, SIGNAL(activated()), SLOT(touchList()));
	a->addTo(tb);

	a = new QAction(OQPixmap("tools/tree.png"), "Open root screen", 0, this);
	connect(a, SIGNAL(activated()), SIGNAL(openRoot()));
	a->addTo(tb);

	tb->setStretchableWidget(new QWidget(tb));	// spacer

	a = new QAction(OQPixmap("kpic/exit.png"), "Quit all screens", 0, this);
	connect(a, SIGNAL(activated()), OQApp::oqApp, SLOT(quit()));
	a->addTo(tb);
}


void
OQScreenView::connectTo(OQScreenList *list)
{
	connect(getCanvas(), SIGNAL(openScreen(const QString&)),
			list, SLOT(openScreen(const QString&)));
	connect(this, SIGNAL(screenClosing(const QString&)),
			list, SLOT(closeScreen(const QString&)));
	connected = true;
	connect(this, SIGNAL(openRoot()), list, SLOT(openRoot()));
	connect(this, SIGNAL(listToggled(bool)), list, SLOT(toggleList(bool)));
	toggleListAc->setOn(list->isVisible());
	connect(list, SIGNAL(listToggled(bool)), SLOT(toggleList(bool)));
}


void
OQScreenView::createPopup(OCWidget *w, const QPoint& gpos)
{
	closePopups();
	if (!w->inherits("OCDataWidget")) {
		qDebug("not a data widget %s (%g,%g)",
				w->getClass().ascii(), w->x(), w->y());
		return;
	}
	OCDataWidget *dw = (OCDataWidget *) w;
	const QString& var = dw->varName();
	if (var.isEmpty()) {
		qDebug("no variable to change");
		return;
	}
	new OQInputPopup(this, "input_popup", dw, gpos);
}


void
OQScreenView::processMouseEvent(QMouseEvent *e, OCWidget *w)
{
	OQCanvasView::processMouseEvent(e, w);
	if (e->type() == QEvent::MouseButtonPress) {
		closePopups();
		if (e->button() == RightButton && w != 0)
			createPopup(w, mapToGlobal(contentsToViewport(e->pos())));
	}
}


void
OQScreenView::closePopups()
{
	OQInputPopup *ip = (OQInputPopup *) child(0, "OQInputPopup", false);
	delete ip;
	qDebug("close popups: %p", ip);
}


/* ================ Window ================ */


OQScreenWindow::OQScreenWindow()
	: OQCanvasWindow("Screen")
{
	setDockWindowsMovable(false);
	setCanvasView(new OQScreenView(this, new QToolBar(this), statusBar()));
	setIcon(OQPixmap("kpic/frame_image.png"));
}


/* ================ List ================ */


OQScreenList::OQScreenList()
{
	setDockWindowsMovable(false);
	QToolBar *toolbar = new QToolBar(this);
	moveDockWindow(toolbar, DockBottom);
	QAction *a;

	a = new QAction(OQPixmap("kpic/view_remove.png"), "Hide list", 0, this);
	connect(a, SIGNAL(activated()), SLOT(hideList()));
	a->addTo(toolbar);

	a = new QAction(OQPixmap("tools/tree.png"), "Open root screen", 0, this);
	connect(a, SIGNAL(activated()), SLOT(openRoot()));
	a->addTo(toolbar);

	a = new QAction(OQPixmap("kpic/fork.png"), "Test mode On/Off", 0, this);
	connect(a, SIGNAL(toggled(bool)), this, SLOT(setTestMode(bool)));
	a->setToggleAction(true);
	a->addTo(toolbar);
	testAc = a;
	testAc->setOn(false);

	toolbar->addSeparator();
	toolbar->setStretchableWidget(new QWidget(toolbar));	// spacer

	a = new QAction(OQPixmap("kpic/exit.png"), "Quit all screens", 0, this);
	connect(a, SIGNAL(activated()), OQApp::oqApp, SLOT(quit()));
	a->addTo(toolbar);

	QVGroupBox *frame = new QVGroupBox(this);
	QScrollView *scroll = new QScrollView(frame);
	setCentralWidget(frame);
	blist = new QVBox;
	scroll->setResizePolicy(QScrollView::AutoOneFit);
	scroll->addChild(blist);
	sigmap = new QSignalMapper(this);
	connect(sigmap, SIGNAL(mapped(const QString&)),
			SLOT(toggled(const QString&)));

	populateList(OQApp::screens.path(), OQApp::etc.filePath("screens.lst"));

	setIcon(OQPixmap("kpic/frame_spreadsheet.png"));
	setCaption("Optikus Screen List");
	adjustSize();
	resize(width() + 16, height() + 16);
}


void
OQScreenList::populateList(QString scr_dir, QString list_file)
{
	QObject *bottom_span = blist->child("bottom_span");
	delete bottom_span;
	QStringList sfiles;
	sfiles = QDir(scr_dir).entryList("*" OQCANVAS_FILE_EXT,
									QDir::Files, QDir::Name);
	//qDebug("sfiles=[" + sfiles.join(",") + "]");
	QFile file(list_file);
	file.open(IO_ReadOnly);
	QString line;
	QRegExp rx("\\s*(\\S+)\\s+(\\S.*)");
	while (file.readLine(line, 1024) >= 0) {
		if (!rx.exactMatch(line))
			continue;
		QString sfile = rx.cap(1);
		QString stitle = rx.cap(2).stripWhiteSpace();
		if (sfile.startsWith("#") || stitle.isEmpty())
			continue;
		//qDebug("sfile=[" + sfile + "] stitle=[" + stitle + "]");
		sfile = toScreen(sfile);
		if (sfiles.find(sfile) == sfiles.end())
			continue;
		if (first.isNull())
			first = sfile;
		addScreen(sfile, stitle);
		sfiles.remove(sfile);
	}
	for (QStringList::Iterator it = sfiles.begin(); it != sfiles.end(); ++it)
		addScreen(*it);
	new QWidget(blist, "bottom_span");
}


void
OQScreenList::addScreen(const QString& file, const QString& title)
{
	QPushButton *sb = new QPushButton(title.isNull() ? file : title, blist);
	sb->setFocusPolicy(NoFocus);
	sb->setToggleButton(true);
	bdict.insert(file, sb);
	setScreenButton(file, false);
	sigmap->setMapping(sb, file);
	connect(sb, SIGNAL(toggled(bool)), sigmap, SLOT(map()));
}


bool
OQScreenList::exists(const QString& screen)
{
	if (screen.isNull())
		return false;
	if (bdict.find(screen) != 0)
		return true;
	qWarning("screen \"" + screen + "\" not found");
	return false;
}


void
OQScreenList::setScreenButton(const QString& screen, bool on)
{
	QPushButton *sb = bdict[screen];
	if (!sb)
		return;
	sb->setIconSet(OQPixmap(on ? "tools/bulb.png" : "tools/ar_r.png"));
	if (sb->isOn() != on)
		sb->setOn(on);
}


void
OQScreenList::toggled(const QString& screen)
{
	if (bdict[screen]->isOn())
		openScreen(screen);
	else
		closeScreen(screen);
}


void
OQScreenList::toggleList(bool on)
{
	if (on == isVisible()) {
		if (on)
			raise();
	} else {
		if (on)
			show();
		else if (!alone())
			hide();
		emit listToggled(on);
	}
}


bool
OQScreenList::hasTestMode() const
{
	return testAc->isOn();
}


void
OQScreenList::setTestMode(bool on)
{
	if (on != testAc->isOn()) {
		testAc->setOn(on);
		return;
	}
	qDebug("test mode=%d", on);
	QDictIterator<OQScreenWindow> it(sdict);
	for( ; *it; ++it)
		(*it)->screenView()->getCanvas()->setTest(on);
}


QString
OQScreenList::toScreen(const QString& name)
{
	QString screen;
	if (!name.isEmpty()) {
		QFileInfo fi(name.stripWhiteSpace());
		screen = fi.fileName();
		if (screen.isEmpty())
			screen = QString();
		else if (fi.extension().isEmpty())
			screen.append(OQCANVAS_FILE_EXT);
	}
	return screen;
}


void
OQScreenList::setRoot(const QString& name)
{
	QString screen = toScreen(name);
	if (exists(screen))
		root = screen;
}


void
OQScreenList::openRoot(bool force)
{
	QString screen = root;
	if (root.isNull() && force)
		screen = first;
	openScreen(screen);
	alone();
}


bool
OQScreenList::alone()
{
	bool is_alone = sdict.isEmpty();
	if (is_alone)
		showList();
	return is_alone;
}


void
OQScreenList::closeEvent(QCloseEvent *e)
{
	if (sdict.isEmpty())
		OQApp::oqApp->quit();
	else
		hideList();
}


void
OQScreenList::openScreen(const QString& name)
{
	QString screen = toScreen(name);
	if (!exists(screen))
		return;
	setScreenButton(screen, true);
	OQScreenWindow *win = sdict[screen];
	if (win) {
		win->raise();
		return;
	}
	if (log_open_close)
		OCWidget::log("open screen [%s]", screen.ascii());
	win = new OQScreenWindow;
	OQScreenView *view = win->screenView();
	view->getCanvas()->setTest(hasTestMode());
	view->setFileName(screen);
	view->load();
	view->adjustWindowSize();
	view->connectTo(this);
	win->show();
	sdict.replace(screen, win);
}


void
OQScreenList::closeScreen(const QString& name)
{
	QString screen = toScreen(name);
	if (!exists(screen))
		return;
	setScreenButton(screen, false);
	OQScreenWindow *w = sdict[screen];
	sdict.remove(screen);
	alone();
	if (w) {
		if (log_open_close)
			OCWidget::log("close screen [%s]", screen.ascii());
		w->hide();
		w->deleteLater();
	}
}


bool
OQScreenList::canQuit()
{
	if (sdict.isEmpty()) {
		qDebug("no open screens. ok to quit.");
		return true;
	}
	int ans = QMessageBox::warning(this,
						"Are you sure ?", "Quit all screens ?",
						QMessageBox::Yes | QMessageBox::Default,
						QMessageBox::No | QMessageBox::Escape);
	return (ans == QMessageBox::Yes);
}


/* ================ main ================ */


int
main(int argc, char **argv)
{
	OQApp app("oqscreens", argc, argv);
	OQWatch watch("oqscreens");
	watch.connect();
	OQScreenList list;
	app.setMainWidget(&list);
	app.setQuitFilter(&list);
	QString root;
	bool test = false;
	for (int i = 1; i < argc; i++) {
		if (!argv[i] || !argv[i][0])
			continue;
		if (argv[i][0] == '-') {
			if (QString(argv[i]) == "-test")
				test = true;
			continue;
		}
		if (root.isEmpty())
			root = argv[i];
	}
	if (!QString(getenv("OPTIKUS_DATATEST")).isEmpty())
		test = true;
	if (root.isEmpty())
		root = "root";	// mainter mode
	list.setRoot(root);
	list.setTestMode(test);
	list.openRoot(false);
	return app.exec();
}
