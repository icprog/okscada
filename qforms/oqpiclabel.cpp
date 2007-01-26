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

  Picture-rich buttons.

  Qt does not let place a complex text/image layout on a button
  surface (unlike GTK where this is possible).
  The OQPicLabel/OQPicButton classes fill this gap and let mix
  images and text over a push or toggle button.

  OQPicLabel is a pseudo-widget that creates the text/image
  layout given a text description in the form:
    [pic(file1)] text1 pic(file2) text2 ... textN [pic(fileN)]
  File names are relative to the optikus picture directory.
  The 1st and Nst pictures (one or both can be omitted) are
  aligned horizontally with the contents described between them.
  The contents in the middle also can contain picture tags.
  The "middle" pictures are aligned vertically. If a picture
  should be placed at the bottom the picture tag should be
  followed by a single dot (".") to avoid confusion with
  "rightmost" picture role. This dot will be implicitly
  stripped. Similarly, a topmost picture should be prefixed
  by the dot that discriminates it from a leftmost picture.

  The OQPicLabel widget leverages Qt layout manager to arrange
  image and text sub-items. It parses the layout description
  and builds internal sub-widget hierarchy. However, inserting
  this hierarchy directly in other widgets would reveal a
  Qt 3 deficiency: child containers are always drawn with
  opaque background which is impossible to disable. Placed on
  a push button the opaque box would distort e.g. highlight
  effects or hinder the text. As a workaround, after laying
  out the hierachy's topmost widget is hidden. Thus, OQPicLabel
  does respond to paint events. Instead, it provides the
  drawContents() method which traverses the hierarchy and
  manually draws the contents on a QPainter: widget or pixmap.

  OQPicButton inherits from QPushButton and draws an OQPicLabel
  on its top.

*/

#include "oqpiclabel.h"
#include "oqcommon.h"

#include <qlabel.h>
#include <qpainter.h>
#include <qregexp.h>
#include <qobjectlist.h>


#define qDebug(...)

#define DEBUGBOXES		0
#define DRAWCONTENTS	1

#define ACENTER			(AlignHCenter | AlignVCenter)
#define BUTTON_GAP		4
#define LABEL_GAP		4


/* ================ Label ================ */


OQPicLabel::OQPicLabel(QWidget *parent, const char *name, WFlags f)
	:	QWidget(parent, name, f),
		widget(0),
		//blay(0),
		sz(0,0),
		gap(LABEL_GAP)
{
	//setWFlags(WNoAutoErase | WPaintDesktop);
	//setBackgroundOrigin(AncestorOrigin);
	hide();
}


void
OQPicLabel::setLabel(const QString& _desc)
{
	desc = _desc;
	rebuild();
}


void
OQPicLabel::drawContents(QPainter *p, const QRect& brect)
{
	if (!widget)
		return;

	int x = brect.x();
	int y = brect.y();

	if (brect.isValid()) {
		x += (brect.width() - sz.width()) / 2;
		y += (brect.height() - sz.height()) / 2;
	}

	QObjectList * lablist = widget->queryList("QLabel", 0, false, false);
	QObjectListIt it(*lablist);
	QLabel *lab;

	while ((lab = (QLabel *)it.current()) != 0) {
		++it;
		QRect r = lab->geometry();
		r.moveBy(x, y);

#if DRAWCONTENTS
		if (lab->pixmap()) {
			p->drawPixmap(r.x(), r.y(), *lab->pixmap());
		}
		else if (!lab->text().isNull()) {
			p->setFont(font());
			p->setPen(paletteForegroundColor());
			p->drawText(r, ACENTER, lab->text());
		}
#endif

#if DEBUGBOXES
		p->setPen(QPen(QColor("#000080"), 2));
		p->drawRect(r);
#endif
	}

	delete lablist;

#if DEBUGBOXES
	p->setPen(QPen(QColor("#800000"), 2));
	p->drawRect(geometry());
	p->setPen(QPen(QColor("#008000"), 2));
	p->drawRect(widget->geometry());
	p->setPen(QPen(QColor("#c0c000"), 2));
	p->drawRect(QRect(x, y, sz.width(), sz.height()));
#endif
}


void
OQPicLabel::rebuild()
{
	//hide();

	delete widget;
	//delete blay;

	//blay = new QVBoxLayout(this, gap, gap);
	widget = new QWidget(this);
	//blay->addWidget(widget, 0, ACENTER);

	if (desc.simplifyWhiteSpace().isEmpty()) {
		sz = QSize(0, 0);
		return;
	}

	QString t(desc);
	t.replace("\\n", "\n");

	int pos;
	QLabel *l;
	QString lpic, rpic, mpic, lt, rt;

	QRegExp rxlp("^\\s*pic\\(([^\\)]+)\\)");
	if ((pos = rxlp.search(t)) != -1) {
		lpic = rxlp.cap(1).stripWhiteSpace();
		t = t.mid(rxlp.matchedLength()).stripWhiteSpace();
		if (t.startsWith("."))
			t = t.mid(1).stripWhiteSpace();
	}

	QRegExp rxrp("\\bpic\\(([^\\)]+)\\)\\s*$");
	if ((pos = rxrp.search(t)) != -1) {
		rpic = rxrp.cap(1).stripWhiteSpace();
		t = t.left(pos).stripWhiteSpace();
		if (t.endsWith("."))
			t = t.left(t.length()-1).stripWhiteSpace();
	}

	QHBoxLayout *hbox = new QHBoxLayout(widget, gap, gap);
	if (lpic.length() > 0) {
		l = new QLabel(widget);
		l->setPixmap(OQPixmap(lpic));
		hbox->addWidget(l, 1, ACENTER);
	}
	QVBoxLayout *vbox = new QVBoxLayout(hbox, gap);
	if (rpic.length() > 0) {
		l = new QLabel(widget);
		l->setPixmap(OQPixmap(rpic));
		hbox->addWidget(l, 1, ACENTER);
	}
	qDebug(QString("QPL:= [%1] lp[%2] rp[%3]\r\n   .. mid[%4]")
			.arg(desc).arg(lpic).arg(rpic)
			.arg(QString(t).replace("\n","//")).utf8());

	QRegExp rxmp("\\bpic\\(([^\\)]+)\\)\\s*");
	while ((pos = rxmp.search(t)) != -1)
	{
		mpic = rxmp.cap(1).stripWhiteSpace();
		lt = t.left(pos).stripWhiteSpace();
		rt = t.mid(pos + rxmp.matchedLength()).stripWhiteSpace();

		if (lt[0] == '.')
			lt = lt.mid(1).stripWhiteSpace();
		if (rt.endsWith("."))
			rt = rt.left(rt.length()-1).stripWhiteSpace();

		if (lt.length() > 0) {
			l = new QLabel(widget);
			l->setText(lt);
			l->setAlignment(ACENTER);
			vbox->addWidget(l, 1, ACENTER);
			qDebug(QString("   :+ txt[%1]")
					.arg(QString(lt).replace("\n","//")).utf8());
		}

		if (mpic.length() > 0) {
			l = new QLabel(widget);
			l->setPixmap(OQPixmap(mpic));
			vbox->addWidget(l, 1, ACENTER);
			qDebug(QString("   :+ pic[%1]").arg(mpic).utf8());
		}

		t = rt;
	}

	if (t.length() > 0) {
		l = new QLabel(widget);
		l->setText(t);
		l->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		vbox->addWidget(l, 1, ACENTER);
		qDebug(QString("   :+ txt[%1]")
				.arg(QString(t).replace("\n","//")).utf8());
	}

	widget->adjustSize();
	sz = widget->size();
	resize(sz);

	//widget->move(-1000, -1000);
	//widget->setShown(false);

	qDebug(QString("   :. size(%1,%2)").arg(sz.width()).arg(sz.height()));
}


/* ================ Button ================ */


OQPicButton::OQPicButton(QWidget *parent, bool toggle)
	: QPushButton(parent), gotsize(false), jumpy(false)
{
	setToggleButton(toggle);
	setFocusPolicy(NoFocus);
	QHBoxLayout *lo = new QHBoxLayout(this, 0, 0);
	label = new OQPicLabel(this);
	lo->addWidget(label, 1, ACENTER);
}


void
OQPicButton::resize(int w, int h)
{
	QPushButton::resize(w, h);
	gotsize = true;
}


void
OQPicButton::setGeometry(int x, int y, int w, int h)
{
	QPushButton::setGeometry(x, y, w, h);
	gotsize = true;
}


void
OQPicButton::setLabel(const QString& desc)
{
	label->setLabel(desc);
	if (!gotsize) {
		qDebug(("resizing ["+desc.left(20)+"] !").utf8());
		resize(label->width() + BUTTON_GAP, label->height() + BUTTON_GAP);
	}
}


void
OQPicButton::drawButtonLabel(QPainter *p)
{
	QRect br = rect();
	bool shifted = (isToggleButton() && !jumpy ? isOn() : isDown());
	if (shifted)
		br.moveBy(2, 2);
	label->drawContents(p, br);
}
