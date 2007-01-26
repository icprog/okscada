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

#include <config.h>		/* for package requisites */

#include "oqedit.h"
#include "oqcommon.h"

#include <qstatusbar.h>
#include <qaction.h>
#include <qpopupmenu.h>
#include <qmenubar.h>
#include <qtoolbar.h>
#include <qmessagebox.h>
#include <qsignalmapper.h>
#include <qfiledialog.h>
#include <qfontdialog.h>
#include <qcolordialog.h>
#include <qpainter.h>
#include <qvgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qregexp.h>


#define qDebug(...)

#define OQEDIT_FILE_FILTER		\
		("screen files (*" OQCANVAS_FILE_EXT ");;all files (*)")
#define OQEDIT_IMAGE_FILTER		\
		("images (*.png *.gif *.jpg);;all files(*)")
#define OQEDIT_CONSTANT_VALUE	0
#define TEMP_STATUS_TIMEOUT		1500
#define PROPS_CHANGED_MARK		" [*]"
#define MOUSE_BUTTONS(state)	((state) & (LeftButton|RightButton|MidButton))
#define INITIAL_WIDGET_SIZE		48
#define RESIZE_TIMER_PERIOD		100
#define CANVAS_SIZE_MARGIN		10


/* ================ Property Editor ================ */


OQPropEdit::OQPropEdit(OQEditView *parent, const char *name,
						const QString& _caption, OCWidget *_w, bool _ex)
	:	QDialog(parent, name),
		w(_w), changed(false), existing(_ex)
{
	setWFlags(WDestructiveClose);
	specmap = new QSignalMapper(this);
	connect(specmap, SIGNAL(mapped(int)), SLOT(editSpecial(int)));
	setCaption(_caption + ": " + w->getClass());

	QBoxLayout *vbox = new QVBoxLayout(this, 4, 4);
	QVGroupBox *group = new QVGroupBox(w->getClass(), this, "groupbox");
	vbox->addWidget(group);
	box = new QWidget(group);

	statusbar = new QStatusBar(this);
	vbox->addWidget(statusbar);
	QBoxLayout *bot = new QHBoxLayout(vbox, 4);

	bot->addStretch(1);
	QPushButton *ok, *cancel, *reload;
	ok = new OQPushButton("O&K", "kpic/button_ok.png", this);
	bot->addWidget(ok);
	connect(ok, SIGNAL(clicked()), SLOT(handleOK()));

	apply = new OQPushButton("&Apply", "kpic/apply.png", this);
	bot->addWidget(apply);
	connect(apply, SIGNAL(clicked()), SLOT(handleApply()));

	reload = new OQPushButton("&Reload", "kpic/editclear.png", this);
	bot->addWidget(reload);
	connect(reload, SIGNAL(clicked()), SLOT(handleReload()));

	cancel = new OQPushButton("&Cancel", "kpic/button_cancel.png", this);
	bot->addWidget(cancel);
	connect(cancel, SIGNAL(clicked()), SLOT(handleCancel()));

	bot->addStretch(1);
	buildProps();
	loadProps();
	clearChanged();
	if (atts.count())
		getEdit(0)->setFocus();
	ok->setDefault(true);
	connect(w, SIGNAL(destroyed()), SLOT(handleCancel()));
	connect(editView(), SIGNAL(widgetGeometryChanged(OCWidget *)),
			SLOT(widgetGeometryChanged(OCWidget *)));
	show();
}


OQPropEdit::~OQPropEdit()
{
	if (!existing)
		delete w;
	qDebug("prop. editor destroyed: existing=%d widget=%p", existing, w);
}


void
OQPropEdit::valueChanged()
{
	if (!changed)
		setCaption(caption() + PROPS_CHANGED_MARK);
	changed = true;
}


void
OQPropEdit::widgetGeometryChanged(OCWidget *_w)
{
	if (_w != w || changed)
		return;
	int key_count = 0;
	for (int i = 0; i < atts.count(); i++) {
		QString k = atts.key(i);
		if (k == "posx" || k == "posy"
				|| k == "width" || k == "height") {
			getEdit(i)->setText(w->getAttr(k).toString());
			if (++key_count == 4)
				break;
		}
	}
	clearChanged();
}


void
OQPropEdit::clearChanged()
{
	if (changed) {
		int pos = caption().findRev(PROPS_CHANGED_MARK);
		if (pos != -1)
			setCaption(caption().left(pos));
		changed = false;
	}
}


void
OQPropEdit::click(int btn)
{
	switch (btn) {
		case 0:		// cancel
			done(0);
			break;
		case 1:		// OK
			if (!saveProps())
				return;
			done(1);
			break;
		case 2:		// Apply
			apply->setDefault(true);
			if (!saveProps())
				return;
			break;
		case 3:		// Reload
			loadProps();
			break;
	}
	clearChanged();
	statusbar->clear();
}


QLineEdit *
OQPropEdit::getEdit(int i)
{
	return (QLineEdit *) box->child(QString("edit_%1").arg(i), 0, false);
}


void
OQPropEdit::buildProps()
{
	w->attrList(atts);
	int n = atts.count();
	QGridLayout *grid = new QGridLayout(box, n, 2, 4, 4);
	for (int i = 0; i < n; i++) {
		QHBoxLayout *lbox = new QHBoxLayout(4);
		grid->addLayout(lbox, i, 0);
		QLabel *label = new QLabel(atts.key(i)+" ("+atts.type(i)+")", box);
		lbox->addWidget(label, 0, AlignLeft|AlignVCenter);
		QLineEdit *edit = new QLineEdit(box, QString("edit_%1").arg(i));
		grid->addWidget(edit, i, 1);
		label->setBuddy(edit);
		connect(edit, SIGNAL(textChanged(const QString&)),
				SLOT(valueChanged()));
		QString type = atts.type(i);
		if (type == "image" || type == "images" || type == "glabel"
				|| type == "font" || type == "color") {
			QPushButton *btn = new OQPushButton(0, "kpic/folder.png", box);
			btn->setFlat(true);
			btn->setFocusPolicy(NoFocus);
			lbox->addWidget(btn, 0, AlignRight|AlignVCenter);
			specmap->setMapping(btn, i);
			connect(btn, SIGNAL(clicked()), specmap, SLOT(map()));
		}
	}
}


void
OQPropEdit::editSpecial(int i)
{
	QLineEdit *edit = getEdit(i);
	QString val = edit->text().stripWhiteSpace();
	QString type = atts.type(i);
	if (type == "image" || type == "images" || type == "glabel") {
		edit->setFocus();
		QString file0;
		if (type == "image") {
			file0 = val;
		} else if (type == "images") {
			int pos = val.findRev(',');
			file0 = (pos < 0 ? val : val.mid(pos+1)).stripWhiteSpace();
		} else { // type == "glabel"
			QRegExp rx("\\bpic\\(([^\\)]+)\\)");
			int pos = rx.searchRev(val);
			file0 = pos < 0 ? "" : rx.cap(1).stripWhiteSpace();
		}
		qDebug("type="+type+" file0="+file0);
		file0 = OQApp::pics.filePath(file0);
		QString file;
		file = QFileDialog::getOpenFileName(file0, OQEDIT_IMAGE_FILTER,
											this, 0, "Choose Image");
		qDebug("file="+file);
		if (!file.isEmpty() && file != file0) {
			QString path = OQApp::pics.filePath("");
			if (file.startsWith(path))
				file = file.mid(path.length());
			qDebug("file="+file+" path="+path);
			if (type == "image")
				val = file;
			else if (type == "images")
				val += (val.length() ? "," : "") + file;
			else // type == "glabel"
				val += "\\n pic(" + file + ") \\n";
			edit->setText(val);
			valueChanged();
		}
	}
	if (type == "font") {
		edit->setFocus();
		QFont font0(OCWidget::stringToFont(val));
		bool ok = false;
		QFont font = QFontDialog::getFont(&ok, font0, this);
		if (ok && font != font0) {
			edit->setText(OCWidget::fontToString(font));
			valueChanged();
		}
	}
	if (type == "color") {
		edit->setFocus();
		QColor color0(val);
		QColor color = QColorDialog::getColor(color0, this);
		if (color.isValid() && color != color0) {
			edit->setText(color.name());
			valueChanged();
		}
	}
}


void
OQPropEdit::loadProps()
{
	for (int i = 0; i < atts.count(); i++) {
		QString v = w->getAttr(atts.key(i)).toString();
		if (atts.type(i) == "boolean") {
			v = v.simplifyWhiteSpace().lower();
			bool flag = (v[0]=='y' || v[0]=='t' || v=="on" || v.toInt() > 0);
			v = flag ? "yes" : "no";
		}
		getEdit(i)->setText(v);
	}
}


bool
OQPropEdit::saveProps()
{
	if (existing && !changed) {
		qDebug("values unchanged");
		return true;
	}
	int i;
	int n = atts.count();
	QString ermes;

	// validate changed properties
	for (i = 0; i < n; i++) {
		QString v = getEdit(i)->text().stripWhiteSpace();
		if (v.isEmpty())
			continue;
		QString type = atts.type(i);
		bool ok = true;
		if (type == "boolean") {
			v = v.lower();
			ok = (v=="1" || v=="0" || v=="on" || v=="off"
					|| v=="y" || v=="n" || v=="yes" || v=="no"
					|| v=="t" || v=="f" || v=="true" || v=="false");
			ermes = "invalid boolean";
		}
		if (type == "integer" || type == "int") {
			v.toInt(&ok);
			ermes = "invalid integer";
		}
		if (type == "float" || type == "double") {
			v.toDouble(&ok);
			ermes = "invalid floating number";
		}
		if (!ok)
			break;
	}
	if (i < n) {
		statusbar->message(atts.key(i)+": "+ermes, 2000);
		getEdit(i)->setFocus();
		return false;
	}

	// apply properties to widget
	bool glabel_changed = false;
	for (i = 0; i < n; i++) {
		QString v = getEdit(i)->text().stripWhiteSpace();
		QString type = atts.type(i);
		if (type == "boolean") {
			v = v.simplifyWhiteSpace().lower();
			bool flag = (v[0]=='y' || v[0]=='t' || v=="on" || v.toInt() > 0);
			v = flag ? "1" : "0";
		}
		if (!v.isEmpty()
				|| type == "string" || type == "font" || type == "color") {
			if (type == "glabel")
				glabel_changed = true;
			w->setAttr(atts.key(i), v);
		}
	}
	if (!existing) {
		w->polish();
		if (w->bsize().width() <= 1 || w->bsize().height() <= 1)
			w->resize(INITIAL_WIDGET_SIZE, INITIAL_WIDGET_SIZE);
		w->raise();
		loadProps();
		w->show();
		editView()->setTempStatus(w->getClass() + ": instantiated");
		editView()->selectWidget(w);
		existing = true;
	}
	w->update();
	emit widgetChanged();
	if (glabel_changed) {
		// FIXME: This is a hack !!!
		//        Need to find a better way of updating QPicButton`s.
		w->getCanvas()->getParent()->hide();
		w->getCanvas()->getParent()->show();
	}
	return true;
}


void
OQEditView::closePropEdit()
{
	OQPropEdit *pe = (OQPropEdit *) child(0, "OQPropEdit", false);
	delete pe;
	//qDebug("closePropEdit: %p", pe);
}


void
OQEditView::editProps()
{
	if (sel_widget) {
		OQPropEdit *pe;
		closePropEdit();
		pe = new OQPropEdit(this, "property_editor", "Edit",
							sel_widget, true);
		connect(pe, SIGNAL(widgetChanged()), SLOT(widgetChanged()));
	}
}


void
OQEditView::newWidget(int rtti)
{
	qDebug("new %s (0x%x)", OQCanvas::rttiToClass(rtti).ascii(), rtti);
	OCWidget *w = newBasicWidget(rtti);
	if (!w) {
		setTempStatus("widget creation failure");
		return;
	}
	w->move((contentsWidth() - w->boundingRect().width()) / 2,
			(contentsHeight() - w->boundingRect().height()) / 2);
	ensureVisible(contentsWidth() / 2, contentsHeight() / 2);
	closePropEdit();
	OQPropEdit *pe = new OQPropEdit(this, "property_editor", "Create",
									w, false);
	connect(pe, SIGNAL(widgetChanged()), SLOT(widgetChanged()));
}


void
OQEditView::editCopy()
{
	if (!sel_widget)
		return;
	OCWidget *w = newBasicWidget(sel_widget->rtti());
	if (!w) {
		setTempStatus("widget copying failure");
		return;
	}
	OCAttrList atts;
	sel_widget->attrList(atts);
	for (int i = 0; i < atts.count(); i++)
		w->setAttr(atts.key(i), sel_widget->getAttr(atts.key(i)));
	w->move(w->x() + 10, w->y() + 10);
	w->resize(sel_widget->width(), sel_widget->height());
	ensureVisible(int(w->x() + w->boundingRect().width() / 2),
					int(w->y() + w->boundingRect().height() / 2));
	OQPropEdit *pe;
	closePropEdit();
	pe = new OQPropEdit(this, "property_editor", "Copy",
						w, false);
	connect(pe, SIGNAL(widgetChanged()), SLOT(widgetChanged()));
}


/* ================ Snap Grid ================ */


OQGridCanvasItem::OQGridCanvasItem(QCanvas *c)
	:	QCanvasRectangle(c),
		dx(1), dy(1)
{
	move(0, 0);
	setZ(-4096);
}


void
OQGridCanvasItem::setSnap(int _dx, int _dy)
{
	_dx = _dx < 1 ? 1 : _dx;
	_dy = _dy < 1 ? 1 : _dx;
	if (dx == _dx && dy == _dy)
		return;
	dx = _dx;
	dy = _dy;
	if (dx < 4 || dy < 4) {
		hide();
	} else {
		show();
		update();
	}
}


void
OQGridCanvasItem::getSnap(int& _dx, int& _dy)
{
	_dx = dx;
	_dy = dy;
}


void
OQGridCanvasItem::drawShape(QPainter& p)
{
	int nx = dx/4;
	int ny = dy/4;
	nx = nx < 5 ?: 5;
	nx = ny < 5 ?: 5;
	if (nx <= 0 || ny <= 0)
		return;
	for (int off = 0; off <= 1; off++) {
		p.setPen(QPen(QColor(off < 1 ? "#ffffff" : "#333333"), 0));
		for (int x = off; x <= width(); x += dx) {
			for (int y = off; y <= height(); y += dy) {
				p.drawLine(x-nx, y, x+nx, y);
				p.drawLine(x, y-ny, x, y+ny);
			}
		}
	}
}


void
OQEditView::canvasResized()
{
	qDebug("grid(%d,%d)", canvas()->width(), canvas()->height());
	grid_ci->setSize(canvas()->width(), canvas()->height());
}


void
OQEditView::clear()
{
	if (size_timer_id) {
		killTimer(size_timer_id);
		size_timer_id = 0;
	}
	selectWidget(0);
	closePropEdit();
	OQCanvasView::clear();
	grid_ci = new OQGridCanvasItem(getCanvas());
	connect(getCanvas(), SIGNAL(resized()), SLOT(canvasResized()));
	canvasResized();
	changed = false;
}


void
OQEditView::setGrid(int step_x, int step_y)
{
	qDebug(QString("setGrid(%1 x %2)").arg(step_x).arg(step_y));
	grid_ci->setSnap(step_x, step_y);
}


void
OQEditView::editSnapLess()
{
	qDebug("editSnapLess");
	int sx, sy;
	grid_ci->getSnap(sx, sy);
	sx = sx < 2 ? 1 : sx/2;
	sy = sy < 2 ? 1 : sy/2;
	grid_ci->setSnap(sx, sy);
}


void
OQEditView::editSnapMore()
{
	qDebug("editSnapMore");
	int sx, sy;
	grid_ci->getSnap(sx, sy);
	sx = sx > 8 ? 16 : sx*2;
	sy = sy > 8 ? 16 : sy*2;
	grid_ci->setSnap(sx, sy);
}


QPoint
OQEditView::align(QPoint pos)
{
	int sx, sy;
	grid_ci->getSnap(sx, sy);
	return QPoint((pos.x() + sx/2)/sx * sx, (pos.y() + sy/2)/sy * sy);
}


/* ================ Edit View: Mouse ================ */


void
OQEditView::processMouseEvent(QMouseEvent *e, OCWidget *w)
{
	OQCanvasView::processMouseEvent(e, w);
	mouse_x = e->pos().x();
	mouse_y = e->pos().y();
	hover_widget = w;

	// popup
	if (e->type() == QEvent::MouseButtonPress
			&& e->button() == RightButton) {
		if (w)
			selectWidget(w);
		QPoint gp = mapToGlobal(contentsToViewport(e->pos()));
		(w && w->inherits("OCWidget") ? widget_menu : canvas_menu)->popup(gp);
	}

	if (w && e->type() == QEvent::MouseButtonDblClick
			&& e->button() == LeftButton) {
		selectWidget(w);
		editProps();
	}

	// left press
	if (e->type() == QEvent::MouseButtonPress
			&& e->button() == LeftButton) {
		selectWidget(w);
		left_button_pressed = true;
		saved_resize_mode = resize_mode->isOn();
		setResizeMode((e->state() & ShiftButton) || resize_mode->isOn());
	}

	// left release
	if (e->type() == QEvent::MouseButtonRelease
			&& e->button() == LeftButton) {
		left_button_pressed = false;
		setResizeMode(saved_resize_mode);
	}

	// left move
	if (e->type() == QEvent::MouseMove
			&& MOUSE_BUTTONS(e->state()) == LeftButton
			&& sel_widget) {
		QPoint pos = align(e->pos() - rel_pos);
		if (resize_mode->isOn()) {
			if (pos.x() > 0 && pos.y() > 0) {
				if (sel_widget->resize(pos.x(), pos.y())) {
					emit widgetGeometryChanged(sel_widget);
					changed = true;
				} else {
					setTempStatus("resize request rejected !");
					selectWidget(0);
					setResizeMode(saved_resize_mode);
				}
			}
		} else {
			sel_widget->move(pos.x(), pos.y());
			emit widgetGeometryChanged(sel_widget);
			changed = true;
		}
		if (!size_timer_id)
			size_timer_id = startTimer(RESIZE_TIMER_PERIOD);
	}

	updateStatus();
}


void
OQEditView::selectWidget(OCWidget *w)
{
	if (sel_widget && sel_widget != w) {
		sel_widget->setMarkBounds(false);
		if (sel_widget->hasChild())
			sel_widget->childWidget()->clearFocus();
	}
	sel_widget = w;
	if (sel_widget) {
		sel_widget->setMarkBounds(true);
		if (sel_widget->hasChild())
			sel_widget->childWidget()->setFocus();
	}
}


void
OQEditView::keyPressEvent(QKeyEvent *e)
{
	qDebug("key pressed=%d (mouse_left=%d, sel=%p)",
			e->key(), left_button_pressed, sel_widget);
	if (e->key() == Key_Shift && left_button_pressed && sel_widget)
		setResizeMode(true);
}


void
OQEditView::keyReleaseEvent(QKeyEvent *e)
{
	qDebug("key released=%d (mouse_left=%d, sel=%p)",
			e->key(), left_button_pressed, sel_widget);
	if (e->key() == Key_Shift && left_button_pressed && sel_widget)
		setResizeMode(false);
}


void
OQEditView::setResizeMode(bool on)
{
	if (resize_mode->isOn() != on) {
		resize_mode->setOn(on);
		return;
	}
	if (sel_widget) {
		rel_pos.setX(mouse_x - int(on ? sel_widget->width() : sel_widget->x()));
		rel_pos.setY(mouse_y - int(on ? sel_widget->height() : sel_widget->y()));
	}
}


void
OQEditView::setBaseSize()
{
	OQCanvas *c = getCanvas();
	QRect br = c->boundingRect();
	if (br.size() == c->getBaseSize())
		return;
	c->setBaseSize(br.width(), br.height(), false);
	br.addCoords(0, 0, CANVAS_SIZE_MARGIN, CANVAS_SIZE_MARGIN);
	int cw, ch;
	viewportToContents(viewport()->width(), viewport()->height(), cw, ch);
	br |= QRect(0, 0, cw, ch);
	if (br != QRect(0, 0, c->width(), c->height()))
		c->resize(br.width(), br.height());
	updateStatus();
}


void
OQEditView::viewportResizeEvent(QResizeEvent * e)
{
	OQCanvasView::viewportResizeEvent(e);
	updateStatus();
}


/* ================ Edit View: Status Bar ================ */


void
OQEditView::setStatus(const QString& msg)
{
	status_msg = msg;
	updateStatus();
}


void
OQEditView::leaveEvent(QEvent *e)
{
	mouse_x = mouse_y = -1;
	hover_widget = 0;
	updateStatus();
}


void
OQEditView::widgetChanged()
{
	changed = true;
	updateStatus();
}


void
OQEditView::updateStatus()
{
	if (status_timer_id)
		return;
	QString msg;
	if (mouse_x < 0 || mouse_x > width() || mouse_y < 0 || mouse_y > height()) {
		OQCanvas *c = getCanvas();
		msg.sprintf("size: base(%dx%d), current(%dx%d)",
					c->getBaseSize().width(), c->getBaseSize().height(),
					c->width(), c->height());
	} else {
		msg.sprintf("%d x %d | ", mouse_x, mouse_y);
		if (hover_widget || sel_widget) {
			OCWidget *widget = hover_widget ?: sel_widget;
			msg.append(hover_widget == sel_widget ? "+"
												: hover_widget ? "-" : "*");
			msg.append(QString(" %1(%2,%3 %4x%5)")
					.arg(widget->getClass())
					.arg(widget->x()).arg(widget->y())
					.arg(widget->width()).arg(widget->height()));
		}
	}
	QString marker = changed ? "[*] " : "... ";
	statusbar->message(marker + msg + " | " + status_msg);
}


void
OQEditView::setTempStatus(const QString& msg)
{
	if (statusbar) {
		const QPalette& pal = statusbar->palette();
		if (status_timer_id) {
			killTimer(status_timer_id);
		} else {
			saved_status_bg_color = statusbar->backgroundColor();
			statusbar->setBackgroundColor(
						pal.color(QPalette::Active, QColorGroup::Highlight));
		}
		statusbar->message(msg);
		status_timer_id = startTimer(TEMP_STATUS_TIMEOUT);
	}
}


void
OQEditView::timerEvent(QTimerEvent *e)
{
	if (status_timer_id && e->timerId() == status_timer_id) {
		killTimer(status_timer_id);
		status_timer_id = 0;
		statusbar->setBackgroundColor(saved_status_bg_color);
		updateStatus();
	}
	if (size_timer_id && e->timerId() == size_timer_id) {
		killTimer(size_timer_id);
		size_timer_id = 0;
		setBaseSize();
	}
}


/* ================ Edit View: Load / Save ================ */


void
OQEditView::setFileName(const QString& filename)
{
	OQCanvasView::setFileName(filename);
	has_name = true;
}


void
OQEditView::fileNew()
{
	if (!canDo(!changed,
			"ALL contents will be removed !\n\nAre you sure ?"))
		return;
	clear();
	setFileName("noname");
	has_name = false;
	getCanvas()->setEdit(true);
	getCanvas()->setEditValue(OQEDIT_CONSTANT_VALUE);
	setTempStatus("Cleared");
}


bool
OQEditView::load()
{
	bool ok = OQCanvasView::load();
	if (ok) {
		QRect br = getCanvas()->boundingRect();
		getCanvas()->setBaseSize(br.width(), br.height(), false);
		adjustWindowSize();
	}
	changed = false;
	setTempStatus("opened: " + getFileName());
	return ok;
}


bool
OQEditView::save()
{
	// Temporarily assign actual size to base size so that it is saved in file.
	OQCanvas *c = getCanvas();
	QSize bsz = c->getBaseSize();
	QSize csz = c->size();
	c->setBaseSize(csz.width(), csz.height(), false);
	bool ok = OQCanvasView::save();
	c->setBaseSize(bsz.width(), bsz.height(), false);
	return ok;
}


void
OQEditView::fileOpen()
{
	if (changed) {
		if (!canDo(false,
				"Your changes will be lost !\n\nAre you sure ?"))
			return;
	}
	QString file = QFileDialog::getOpenFileName(getFileName(),
							OQEDIT_FILE_FILTER, this, "Open", "Load Screen");
	if (file.isEmpty())
		return;
	if (!QFile(file).exists()) {
		setStatus(file + ": not found");
		return;
	}
	setFileName(file);
	load();
}


void
OQEditView::fileSave()
{
	if (!has_name) {
		fileSaveAs();
		return;
	}
	if (save())
		changed = false;
	setTempStatus("saved " + getFileName());
}


void
OQEditView::fileSaveAs()
{
	QString file = QFileDialog::getSaveFileName(getFileName(),
							OQEDIT_FILE_FILTER, this, "Save", "Save Screen");
	if (file.isEmpty())
		return;
	setFileName(file);
	if (save())
		changed = false;
	setTempStatus("saved as " + file);
}


void
OQEditView::fileQuit()
{
	OQApp::oqApp->quit();
}


bool
OQEditView::areYouSure(bool sure, const QString& text)
{
	if (sure)
		return true;
	int ans = QMessageBox::warning(this, "Are you sure ?", text,
						QMessageBox::Yes | QMessageBox::Default,
						QMessageBox::No | QMessageBox::Escape);
	return (ans == QMessageBox::Yes);
}


bool
OQEditView::canClose()
{
	return canQuit();
}


bool
OQEditView::canQuit()
{
	qDebug("canQuit: changed=%d", changed);
	if (!changed)
		return true;
	bool can = canDo(false, "You are trying to quit the viewer.\n\n"
					"All your modifications will be LOST !\n\n"
					"Are you sure ?");
	if (can)
		clear();
	return can;
}


/* ================ Edit View: Actions ================ */


void
OQEditView::editDelete()
{
	qDebug("editDelete");
	if (!sel_widget)
		return;
	OCWidget *w = sel_widget;
	selectWidget(0);
	setTempStatus(w->getClass() + ": destroyed");
	// FIXME: trying to workaround OQPicLabel crashes !
	w->hide();
	w->deleteLater();
}


void
OQEditView::editDefFont()
{
	QString def_font = getCanvas()->getDefTextFont().stripWhiteSpace();
	QFont font0;
	if (!def_font.isEmpty())
		font0 = OCWidget::stringToFont(def_font);
	bool ok = false;
	QFont font = QFontDialog::getFont(&ok, font0, this);
	if (ok && font != font0) {
		getCanvas()->setDefTextFont(OCWidget::fontToString(font));
		changed = true;
	}
}


void
OQEditView::editDefColor()
{
	QString def_color = getCanvas()->getDefTextColor().stripWhiteSpace();
	QColor color0;
	if (!def_color.isEmpty())
		color0.setNamedColor(def_color);
	QColor color = QColorDialog::getColor(color0, this);
	if (color.isValid() && color != color0) {
		getCanvas()->setDefTextColor(color.name());
		changed = true;
	}
}


void
OQEditView::editBgImage()
{
	qDebug("editBgImage");
	QString file0 = OQApp::pics.filePath(
								getCanvas()->getBgImage().stripWhiteSpace());

	QFileInfo fi(file0);
	QString dirname = fi.dirPath();
	QString filename = fi.fileName();

	if (!QDir(dirname).exists())
		dirname = QDir::currentDirPath();

	QFileDialog *dlg = new QFileDialog(dirname, QString::null, this, 0, true);
	dlg->setCaption("Choose Background Image");
	dlg->setFilters(QStringList::split(
							";;", "Images (*.png *.jpg);;All files (*)"));
	dlg->setMode(QFileDialog::AnyFile);
	QPushButton *okb = (QPushButton *) dlg->child("OK", "QPushButton", true);
	if (okb)
		okb->setText("&Select");
	if (!filename.isEmpty())
		dlg->setSelection(filename);

	QString file;
	while (dlg->exec() == QDialog::Accepted) {
		file = dlg->selectedFile().stripWhiteSpace();
		if (QFile(file).exists())
			break;
		file = QString::null;
	}
	delete dlg;
	if (!file.isNull() && file != file0) {
		QString path = OQApp::pics.filePath("");
		if (file.startsWith(path))
			file = file.mid(path.length());
		getCanvas()->setBgImage(file);
	}
}


void
OQEditView::editRaise()
{
	qDebug("editRaise");
	if (sel_widget)
		sel_widget->raise();
}


void
OQEditView::editLower()
{
	qDebug("editLower");
	if (sel_widget)
		sel_widget->lower();
}


void
OQEditView::helpAbout()
{
	QMessageBox::about(this, "About OqEdit",
		"OqEdit " PACKAGE_VERSION ", the Optikus display editor\n\n"
		PACKAGE_BUGREPORT "\n\n" PACKAGE_COPYRIGHT "\n\n" PACKAGE_WEBSITE);
}


/* ================ Edit View: Initialization ================ */


QAction *
OQEditView::newAction(const QString& text, const QString& icon,
					const char *slot, bool toggle, QKeySequence accel)
{
	QAction *action;
	if (icon.isEmpty())
		action = new QAction(text, accel, this);
	else
		action = new QAction(OQPixmap(icon), text, accel, this);
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
						const QString& text, const QString& icon,
						int rtti, QKeySequence accel)
{
	QAction *action = newAction(text, icon, 0, false, accel);
	if (pm)
		action->addTo(pm);
	if (tb)
		action->addTo(tb);
	creator_map->setMapping(action, rtti);
	connect(action, SIGNAL(activated()), creator_map, SLOT(map()));
	return action;
}


OQEditView::OQEditView(OQEditWindow *parent)
	:	OQCanvasView(parent),
		mouse_x(-1), mouse_y(-1),
		hover_widget(0),
		statusbar(0),
		status_timer_id(0),
		grid_ci(0),
		sel_widget(0),
		resize_mode(0),
		saved_resize_mode(false),
		left_button_pressed(false),
		size_timer_id(0),
		changed(false)
{
	fileNew();
	viewport()->setMouseTracking(true);

	statusbar = parent->statusBar();
	statusbar->setFont(QFont("Courier New", 12, QFont::Bold));
	setTempStatus("Hello!");

	QMenuBar *menubar = parent->menuBar();
	QToolBar *toolbar = new QToolBar(parent);

	// Menu bar
	QPopupMenu *fileMenu = new QPopupMenu(this);
	menubar->insertItem("&File", fileMenu);
	QPopupMenu *newMenu = new QPopupMenu(this);
	menubar->insertItem("&Create", newMenu);
	QPopupMenu *optMenu = new QPopupMenu(this);
	menubar->insertItem("&Options", optMenu);

	menubar->insertSeparator();		// never worked
	QPopupMenu *helpMenu = new QPopupMenu(this);
	menubar->insertItem("&Help", helpMenu);

	QAction *fileNewAc = newAction("&New", "kpic/filenew.png", SLOT(fileNew()),
									false, CTRL+Key_N);
	fileNewAc->addTo(fileMenu);
	QAction *fileOpenAc = newAction("&Open", "kpic/fileopen.png", SLOT(fileOpen()),
									false, CTRL+Key_O);
	fileOpenAc->addTo(fileMenu);
	QAction *fileSaveAc = newAction("&Save", "kpic/filesave.png", SLOT(fileSave()),
									false, CTRL+Key_S);
	fileSaveAc->addTo(fileMenu);
	QAction *fileSaveAsAc = newAction("Save &as...", "kpic/filesaveas.png",
									SLOT(fileSaveAs()));
	fileSaveAsAc->addTo(fileMenu);
	QAction *fileQuitAc = newAction("&Quit", "kpic/exit.png", SLOT(fileQuit()),
									false, CTRL+Key_Q);
	fileQuitAc->addTo(fileMenu);

	fileNewAc->addTo(toolbar);
	fileOpenAc->addTo(toolbar);
	fileSaveAc->addTo(toolbar);
	fileSaveAsAc->addTo(toolbar);
	toolbar->addSeparator();

	QAction *editPropsAc = newAction("Change item properties", "kpic/edit.png",
									SLOT(editProps()), false, CTRL+Key_E);
	QAction *editCopyAc = newAction("Copy item", "kpic/editcopy.png",
									SLOT(editCopy()), false, CTRL+Key_C);
	QAction *editDeleteAc = newAction("Delete item", "kpic/editdelete.png",
									SLOT(editDelete()), false, Key_Delete);
	QAction *editSnapLessAc = newAction("Snap down", "kpic/fontsizedown.png",
									SLOT(editSnapLess()), false, ALT+Key_Left);
	QAction *editSnapMoreAc = newAction("Snap up", "kpic/fontsizeup.png",
									SLOT(editSnapMore()), false, ALT+Key_Right);
	QAction *editRaiseAc = newAction("Raise", "kpic/2uparrow.png",
									SLOT(editRaise()), false, CTRL+Key_Up);
	QAction *editLowerAc = newAction("Lower", "kpic/2downarrow.png",
									SLOT(editLower()), false, CTRL+Key_Down);
	resize_mode = newAction("Resize item", "kpic/move.png",
							SLOT(setResizeMode(bool)), true, CTRL+Key_R);

	editPropsAc->addTo(toolbar);
	editCopyAc->addTo(toolbar);
	editDeleteAc->addTo(toolbar);
	toolbar->addSeparator();

	QAction *optBgImageAc = newAction("&Background image", "", SLOT(editBgImage()));
	optBgImageAc->addTo(optMenu);
	QAction *optGrid1_Ac = newAction("Grid: &1", "", SLOT(setGrid1()),
									false, ALT+Key_1);
	optGrid1_Ac->addTo(optMenu);
	QAction *optGrid4_Ac = newAction("Grid: &4", "", SLOT(setGrid4()),
									false, ALT+Key_2);
	optGrid4_Ac->addTo(optMenu);
	QAction *optGrid8_Ac = newAction("Grid: &8", "", SLOT(setGrid8()),
									false, ALT+Key_3);
	optGrid8_Ac->addTo(optMenu);
	QAction *optGrid16_Ac = newAction("Grid: &16", "", SLOT(setGrid16()),
									false, ALT+Key_4);
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

	// Widget popup menu
	widget_menu = new QPopupMenu(this);
	newAction("Edit", "kpic/edit.png",
				SLOT(editProps()))->addTo(widget_menu);
	newAction("Copy", "kpic/editcopy.png",
				SLOT(editCopy()))->addTo(widget_menu);
	newAction("Delete", "kpic/editdelete.png",
				SLOT(editDelete()))->addTo(widget_menu);
	newAction("Raise", "kpic/2uparrow.png",
				SLOT(editRaise()))->addTo(widget_menu);
	newAction("Lower", "kpic/2downarrow.png",
				SLOT(editLower()))->addTo(widget_menu);
	newAction("Cancel", "kpic/button_cancel.png", 0)->addTo(widget_menu);

	// Canvas popup menu
	canvas_menu = new QPopupMenu(this);
	newAction("bg image", "kpic/frame_image.png",
				SLOT(editBgImage()))->addTo(canvas_menu);
	newAction("text font", "kpic/math_brackets.png",
				SLOT(editDefFont()))->addTo(canvas_menu);
	newAction("text color", "kpic/colorize.png",
				SLOT(editDefColor()))->addTo(canvas_menu);
	newAction("grid : 1", 0, SLOT(setGrid1()))->addTo(canvas_menu);
	newAction("grid : 4", 0, SLOT(setGrid4()))->addTo(canvas_menu);
	newAction("grid : 8", 0, SLOT(setGrid8()))->addTo(canvas_menu);
	newAction("grid : 16", 0, SLOT(setGrid16()))->addTo(canvas_menu);
	newAction("cancel", "kpic/button_cancel.png", 0)->addTo(canvas_menu);

	// Item creators
	creator_map = new QSignalMapper(this);
	connect(creator_map, SIGNAL(mapped(int)), SLOT(newWidget(int)));

	newCreator(pmLabel, toolbar, "Label", "kpic/frame_text.png", ORTTI_Label);
	newCreator(pmLabel, toolbar, "String", "kpic/frame_formula.png", ORTTI_String);
	newCreator(pmLabel, toolbar, "Pixmap", "kpic/frame_image.png", ORTTI_Pixmap);
	newCreator(pmLabel, "Bar", ORTTI_Bar);
	newCreator(pmButton, "Button", ORTTI_Button);
	newCreator(pmButton, toolbar, "Switch", "kpic/frame_chart.png", ORTTI_Switch);
	newCreator(pmButton, "Animation", ORTTI_Anim);
	newCreator(pmRange, "Ruler", ORTTI_Ruler);
	newCreator(pmRange, "Dial", ORTTI_Dial);
	newCreator(pmMeter, "Big Tank", ORTTI_BigTank);
	newCreator(pmMeter, "Small Tank", ORTTI_SmallTank);
	newCreator(pmMeter, "Round Tank", ORTTI_RoundTank);
	newCreator(pmMeter, "Thermometer", ORTTI_Thermometer);
	newCreator(pmMeter, "Volume", ORTTI_Volume);
	newCreator(pmMeter, "Manometer", ORTTI_Manometer);
	newCreator(pmIndic, "LED", ORTTI_LED);
	newCreator(pmIndic, "Curve", ORTTI_Curve);
	newCreator(pmControl, "Runner", ORTTI_Runner);
	newCreator(pmControl, "Offnominal", ORTTI_Offnom);
	newCreator(pmControl, "Link", ORTTI_Link);
	toolbar->addSeparator();

	editSnapLessAc->addTo(toolbar);
	editSnapMoreAc->addTo(toolbar);
	editRaiseAc->addTo(toolbar);
	editLowerAc->addTo(toolbar);
	resize_mode->addTo(toolbar);
	toolbar->addSeparator();

	toolbar->setStretchableWidget(new QWidget(toolbar));	// spacer
	fileQuitAc->addTo(toolbar);
}


/* ================ Main Window ================ */


OQEditWindow::OQEditWindow()
	: OQCanvasWindow("Edit")
{
	setCanvasView(new OQEditView(this));
	QMainWindow::setCaption("Optikus Screen Editor");
	setIcon(OQPixmap("kpic/colorize.png"));
}


int
main(int argc, char **argv)
{
	OQApp app("oqedit", argc, argv);
	OQEditWindow win;
	app.setMainWidget(&win);
	OQEditView *view = win.editView();
	app.setQuitFilter(view);

	bool loaded = false;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '\0' && argv[i][0] != '-') {
			view->setFileName(argv[i]);
			loaded = view->load();
			break;
		}
	}
	if (!loaded)
		win.resize(600, 500);

	win.show();
	return app.exec();
}
