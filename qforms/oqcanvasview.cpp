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
#include "oqcommon.h"

#include <qfileinfo.h>


#define qDebug(...)

#define VIEWPORT_MARGIN		4


/* ================ Canvas View ================ */


OQCanvasView::OQCanvasView(QWidget *parent)
	: QCanvasView(parent, 0, WNoAutoErase | WStaticContents)
{
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	clear();
}


OQCanvasView::~OQCanvasView()
{
	OQCanvas *c = getCanvas();
	setCanvas(0);
	delete c;
}


void
OQCanvasView::setFileName(const QString& file, bool polish, bool update_title)
{
	if (!file.isNull())
		filename = file;
	if (polish) {
		filename = OQApp::screens.filePath(filename);
		if (QFileInfo(filename).extension().isEmpty())
			filename += OQCANVAS_FILE_EXT;
	}
	QString name = QFileInfo(filename).baseName();
	setName(name);
	if (update_title)
		parentWidget()->setCaption(name);
}


void
OQCanvasView::clear()
{
	// OQCanvas::clear() results in Segmentation Faule !
	// Here we workaround the problem.
	OQCanvas *oc = getCanvas();
	OQCanvas *nc = new OQCanvas(viewport());
	if (oc) {
		nc->resize(oc->width(), oc->height());
		nc->setEdit(oc->isEdit());
		nc->setTest(oc->isTest());
		nc->setEditValue(oc->getEditValue());
	}
	setCanvas(nc);
	delete oc;
}


void
OQCanvasView::adjustWindowSize()
{
	// Temporarily set fixed size for window to adjust correctly
	QSizePolicy saved = sizePolicy();
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	QSize sz = getCanvas()->getBaseSize();
	// Request some so damn small size that it will be corrected
	// by layout to actually fit the contents.
	parentWidget()->resize(10, 10);
	parentWidget()->adjustSize();
	setSizePolicy(saved);
}

bool
OQCanvasView::load(QString *ermes_p)
{
	clear();
	QString ermes;
	bool ok = getCanvas()->load(filename, &ermes);
	if (ermes_p)
		*ermes_p = ermes;
	else if (!ok)
		OCWidget::log("WARNING[Loading]: " + ermes);
	return ok;
}


bool
OQCanvasView::save()
{
	return getCanvas()->save(filename);
}


bool
OQCanvasView::canClose()
{
	// By default allow to close us.
	qDebug("sure you can close");
	return true;
}


QSize
OQCanvasView::sizeHint() const
{
	return getCanvas()->size() + QSize(VIEWPORT_MARGIN, VIEWPORT_MARGIN);
}


void
OQCanvasView::viewportResizeEvent(QResizeEvent * e)
{
	int cw, ch;
	viewportToContents(e->size().width(), e->size().height(), cw, ch);
	getCanvas()->resize(cw, ch);
}


void
OQCanvasView::processMouseEvent(QMouseEvent *e)
{
	OCWidget *topw = 0;

	// Note: item lists are not z-sorted !

	QCanvasItemList l_trk = getCanvas()->mouseTrackers();
	QCanvasItemList l_pos = getCanvas()->collisions(e->pos());

	qDebug("mouse event %d (%d,%d) t#=%d p#=%d",
			e->type(), e->x(), e->y(), l_trk.count(), l_pos.count());

	QCanvasItemList::Iterator it = l_trk.begin();
	bool trk = true;

	while (true)
	{
		if (trk && it == l_trk.end()) {
			it = l_pos.begin();
			trk = false;
		}
		if (!trk && it == l_pos.end())
			break;

		QCanvasItem *ci = *it;
		++it;

		if (!OQCanvas::isMaybeOCWidget(ci))
			continue;
		OCWidget *w = (OCWidget *) ci;

		if (!trk && w->tracksMouse())
			continue;

		if (!topw || topw->z() < w->z())
			topw = w;
		if (w->tracksMouse())
				processMouseEvent(e, w);
	}

	if (!(topw && topw->tracksMouse()))
		processMouseEvent(e, topw);
}


void
OQCanvasView::processMouseEvent(QMouseEvent *e, OCWidget *w)
{
	if (!w)
		return;
	QPoint rel(e->x() - int(w->x()), e->y() - int(w->y()));
	QMouseEvent we(e->type(), rel, e->button(), e->state());
	switch (e->type()) {
		case QEvent::MouseButtonPress:
			w->mousePressEvent(&we);
			w->mouseAnyEvent(&we);
			break;
		case QEvent::MouseButtonRelease:
			w->mouseReleaseEvent(&we);
			w->mouseAnyEvent(&we);
			break;
		case QEvent::MouseButtonDblClick:
			w->mouseDoubleClickEvent(&we);
			w->mouseAnyEvent(&we);
			break;
		case QEvent::MouseMove:
			w->mouseMoveEvent(&we);
			w->mouseAnyEvent(&we);
			break;
		default:
			break;
	}

	qDebug("mouse %s event (%d,%d) => widget %s (%d,%d) as (%d,%d)",
			type == QEvent::MouseButtonPress ? "press" :
			type == QEvent::MouseButtonRelease ? "release" :
			type == QEvent::MouseButtonDblClick ? "dblclick" :
			type == QEvent::MouseMove ? "move" : "?",
			e->x(), e->y(),
			w->getClass().ascii(), int(w->x()), int(w->y()),
			we.x(), we.y());
	if (we.isAccepted())
		e->accept();
}


/* ================= Canvas Window ================ */


OQCanvasWindow::OQCanvasWindow(const QString& _titleprefix)
	:	QMainWindow(),
		titleprefix(_titleprefix)
{
	setCaption();
}


OQCanvasWindow::~OQCanvasWindow()
{
}


void
OQCanvasWindow::setCaption(const QString& title)
{
	QMainWindow::setCaption(titleprefix+": "+(title.isEmpty() ? "--" : title));
}


void
OQCanvasWindow::setCanvasView(OQCanvasView *_cview)
{
	cview = _cview;
	setCentralWidget(cview);
}


void
OQCanvasWindow::closeEvent(QCloseEvent *e)
{
	// the view decides whether we can close or not.
	if (!cview || cview->canClose())
		e->accept();
}
