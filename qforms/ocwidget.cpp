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
#include "oqwatch.h"
#include "oqcommon.h"

#include <qtooltip.h>
#include <qpainter.h>

#include <stdarg.h>
#include <math.h>
#include <optikus/log.h>


#define qDebug(...)
#if 0
#define qvDebug(x...)	qDebug(x)
#else
#define qvDebug(...)
#endif
#if 0
#define qfDebug(x...)	qDebug(x)
#else
#define qfDebug(...)
#endif


// ================ Widget ================


OCWidget::OCWidget(OQCanvas *c)
	:	QCanvasRectangle(c),
		user_w(0), user_h(0),
		brect_w(-1), brect_h(-1),
		child(0),
		track_mouse(false),
		mark_bounds(false)
{
}


OCWidget::~OCWidget()
{
	setTrackMouse(false);
	destroyChild();
}


void
OCWidget::setTrackMouse(bool on)
{
	on = (on != false);
	if (on == track_mouse)
		return;
	track_mouse = on;
	OQCanvas *c = getCanvas();
	if (!c)
		return;
	if (on)
		c->mtrackers.prepend(this);
	else
		c->mtrackers.remove(this);
}


void
OCWidget::attrList(OCAttrList& atts) const
{
	atts.replace("posx", "int");
	atts.replace("posy", "int");
	atts.replace("width", "int");
	atts.replace("height", "int");
	atts.replace("tooltip", "string");
}


bool
OCWidget::setAttr(const QString& name, const QVariant& value)
{
	bool ok = true;
	if (name == "posx") {
		double _x = value.toDouble(&ok);
		if (ok)
			move(_x, y());
		return ok;
	}
	if (name == "posy") {
		double _y = value.toDouble(&ok);
		if (ok)
			move(x(), _y);
		return ok;
	}
	if (name == "width") {
		double _w = value.toDouble(&ok);
		if (ok)
			resize(_w, height());
		return ok;
	}
	if (name == "height") {
		double _h = value.toDouble(&ok);
		if (ok)
			resize(width(), _h);
		return ok;
	}
	if (name == "tooltip") {
		const QString tip = value.toString();
		if (tip.isNull())
			return false;
		setTip(tip);
		return true;
	}
	return false;
}


QVariant
OCWidget::getAttr(const QString& name) const
{
	if (name == "posx")
		return x();
	if (name == "posy")
		return y();
	if (name == "width")
		return width();
	if (name == "height")
		return height();
	if (name == "tooltip")
		return getTip();
	return QVariant();
}


QRect
OCWidget::boundingRect() const
{
	if (brect_w >= 0 && brect_h >= 0)
		return QRect(int(x()), int(y()), brect_w, brect_h);
	if (child)
		return QRect(int(x()), int(y()), child->width(), child->height());
	return QRect(int(x()), int(y()), int(user_w), int(user_h));
}


void
OCWidget::raise()
{
	double zmin, zmax;
	getCanvas()->zRange(zmin, zmax);
	setZ(zmax + 1);
	if (child)
		child->raise();
	redraw();
}



void
OCWidget::lower()
{
	double zmin, zmax;
	getCanvas()->zRange(zmin, zmax);
	setZ(zmin - 1);
	if (child)
		child->lower();
	redraw();
}


void
OCWidget::move(double _x, double _y)
{
	QCanvasRectangle::move(_x, _y);
	if (hasChild())
		childWidget()->move(int(x()), int(y()));
}


bool
OCWidget::resize(double w, double h)
{
	if (w <=0 || h <= 0)
		return false;
	if (w != user_w || h != user_h) {
		QRect old_br = boundingRect();
		user_w = w;
		user_h = h;
		if (brect_w < 0 || brect_h < 0) {
			QRect new_br = boundingRect();
			if (new_br.width() < old_br.width()
					|| new_br.height() < old_br.height()) {
				// tell canvas to redraw cropped areas
				getCanvas()->setChanged(old_br);
			}
			// update the founding rectangle size
			int iw = int(w+.5);
			int ih = int(h+.5);
			setSize(iw, ih);
			// update child size
			if (child)
				child->resize(iw, ih);
		}
	}
	return true;
}


void
OCWidget::updateSize(int br_w, int br_h)
{
	if (brect_w != br_w || brect_h != br_h) {
		if (br_w < brect_w || br_h < brect_h) {
			// tell canvas to redraw cropped areas
			redraw();
		}
		brect_w = br_w;
		brect_h = br_h;
		// update the founding rectangle size
		setSize(br_w, br_h);
	}
}


void
OCWidget::setMarkBounds(bool on)
{
	if (on != mark_bounds) {
		mark_bounds = on;
		redraw();
	}
}


void
OCWidget::draw(QPainter& p)
{
	drawShape(p);
	if (mark_bounds) {
		QRect br = boundingRect();
		if (br.isValid()) {
			p.setBrush(NoBrush);
			p.setPen(QPen(QColor(255,255,255), 0, DotLine));
			p.drawRect(br);
			br.addCoords(1, 1, -1, -1);
			p.setPen(QPen(QColor(0,0,128), 0, DotLine));
			p.drawRect(br);
			if (false) {
				br.addCoords(-2, -2, 2, 2);
				p.setPen(QPen(QColor(128,0,0), 0, DotLine));
				p.drawRect(br);
			}
		}
	}
}


void
OCWidget::polish()
{
	if (boundingRect().isEmpty() && child != 0) {
		QRect r = boundingRect();
		qWarning("%s: br(%d,%d,%d,%d) wh(%g,%g) ch(%d,%d) bwh(%d,%d)",
				getClass().ascii(), r.x(), r.y(), r.width(), r.height(),
				width(), height(), child->width(), child->height(),
				brect_w, brect_h);
		updateSize(child->size());
	} else {
		setSize(bsize().width(), bsize().height());
	}
	if (child)
		child->show();
	updateDefaults();
}


void
OCWidget::attachChild(QWidget *_child)
{
	if (child == _child)
		return;

	if (child) {
		if (!tip.isNull())
			QToolTip::remove(child);
		child->removeEventFilter(this);
	}

	child = _child;
	if (!child)
		return;

	child->installEventFilter(this);
	child->clearFocus();

	if (tip.isNull())
		QToolTip::remove(child);
	else
		QToolTip::add(child, tip);

	// FIXME: This approach is probably too bloated.
	//	Given that in most cases all widgets are destroyed
	//	together with canvas, we can just mark all children
	//	as destroyed in advance.
	connect(child, SIGNAL(destroyed(QObject*)),
			SLOT(childDestroyed(QObject*)));
}


QWidget *
OCWidget::detachChild()
{
	QWidget *retchild = child;
	attachChild(0);
	return retchild;
}


bool
OCWidget::eventFilter(QObject *watched, QEvent *e)
{
	if (watched && watched == (QObject *) child) {
		switch (e->type()) {
			case QEvent::MouseButtonPress:
			case QEvent::MouseButtonRelease:
			case QEvent::MouseButtonDblClick:
			case QEvent::MouseMove:
				if (!triageChildEvent(e)) {
					// pass to canvas
					QMouseEvent *m = (QMouseEvent *)e;
					QMouseEvent cm(m->type(), m->pos() + child->pos(),
									m->button(), m->state());
					QApplication::sendEvent(getParent(), &cm);
					if (cm.isAccepted())
						m->accept();
					return true;
				}
				break;
			case QEvent::KeyPress:
			case QEvent::KeyRelease:
				if (!triageChildEvent(e)) {
					// pass to canvas
					QKeyEvent *k = (QKeyEvent *)e;
					QApplication::sendEvent(getParent(), k);
					return true;
				}
				break;
			default:
				break;
		}
	}
	// pass to child
	return false;
}


bool
OCWidget::triageChildEvent(QEvent *e)
{
	return !isEdit();
}


void
OCWidget::childDestroyed(QObject *obj)
{
	if (obj == child)
		child = 0;
}


void
OCWidget::destroyChild()
{
	delete child;
	child = 0;
}

	
void
OCWidget::setTip(const QString& _tip, bool visual_only)
{
	QString ntip = _tip;
	if (!ntip.isNull())
		ntip = ntip.replace("\\n", "\n");
	if (!visual_only)
		tip = ntip;
	if (child) {
		if (ntip.isNull())
			QToolTip::remove(child);
		else
			QToolTip::add(child, ntip);
	}
}


QString
OCWidget::getTip() const
{
	return tip.isNull() ? tip : QString(tip).replace("\n", "\\n");
}


void
OCWidget::updateValue(int no, const QVariant& value)
{
	if (no == 0)
		updateValue(value);
	else
		update();
}


QString
OCWidget::toString()
{
	QString s("{" + getClass() + "}: ");
	OCAttrList atts;
	attrList(atts);
	for (OCAttrListIterator it = atts.begin(); it != atts.end(); ++it) {
		QString key(it.key());
		s.append(key);
		s.append("=\"");
		s.append(getAttr(key).toString());
		s.append("\", ");
	}
	s.replace(s.length() - 2, 2, ".");
	return s;
}


void
OCWidget::log(const char * format, ...)
{
	va_list varargs;
	va_start(varargs, format);
	ologWrite(ologFormat(0, format, &varargs));
	va_end(varargs);
}


QFont
OCWidget::stringToFont(const QString& _desc, bool *ok)
{
	QString desc = _desc.simplifyWhiteSpace();
	bool bold = false;
	bool italic = false;
	int size = 0;
	int pos = desc.findRev(' ');
	if (pos != -1) {
		int i;
		for (i = desc.length() - 1; i > pos; i--)
			if (!desc[i].isDigit())
				break;
		if (i == pos) {
			size = desc.mid(i+1).toInt();
			desc = desc.left(pos);
		}
	}
	if (desc.lower().endsWith(" italic")) {
		italic = true;
		desc = desc.left(desc.length() - 7);
	}
	if (desc.lower().endsWith(" bold")) {
		bold = true;
		desc = desc.left(desc.length() - 5);
	}
	QString& family = desc;
	if (ok)
		*ok = true;
	qfDebug("string2font: \"%s\" => family=\"%s\",bold=%d,italic=%d,size=%d",
			_desc.ascii(), family.ascii(), bold, italic, size);
	if (!size)
		size = 12;
	return QFont(family, size, bold ? QFont::Bold : QFont::Normal, italic);
}


QString
OCWidget::fontToString(const QFont& font)
{
	QString desc(font.family());
	if (font.bold())
		desc.append(" Bold");
	if (font.italic())
		desc.append(" Italic");
	desc.append(" ");
	desc.append(QString::number(font.pointSize()));
	qfDebug("font2string: \"%s\" => \"%s\"",
			font.toString().ascii(), desc.ascii());
	return desc;
}


// ================ Data Widget ================


OCDataWidget::OCDataWidget(OQCanvas *c)
	: OCWidget(c),
	connected(false), readonly(false),
	num(0), maxnum(0), bgids(0), ooids(0)
{
}


void
OCDataWidget::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("readonly", "boolean");
	atts.replace("var", "string");
}


bool
OCDataWidget::setAttr(const QString& name, const QVariant& value)
{
	if (name == "readonly") {
		setReadOnly(value.toBool());
		return true;
	}
	if (name == "var") {
		bind(value.toString());
		return true;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCDataWidget::getAttr(const QString& name) const
{
	if (name == "readonly")
		return readonly;
	if (name == "var")
		return varList();
	return OCWidget::getAttr(name);
}


void
OCDataWidget::polish()
{
	OCWidget::polish();
	if (isEdit()) {
		for (int i = 0; i < num; i++) {
			updateValue(i, getCanvas()->getEditValue());
		}
	}
}


bool
OCDataWidget::triageChildEvent(QEvent *e)
{
	return !readonly && OCWidget::triageChildEvent(e);
}


QVariant
OCDataWidget::testValue(int no)
{
	if (no < 0 || no >= num)
		return 0;
	double minv, maxv, speed, offset;
	bool is_int;
	testValueProps(no, minv, maxv, speed, offset, is_int);
	return testValue(no, minv, maxv, speed, offset, is_int);
	return fmod(currentTestSec(), 3);
}


void
OCDataWidget::testValueProps(int no, double& minv, double& maxv,
							double& speed, double& offset, bool& is_int)
{
	minv = 0;
	maxv = 10;
	speed = 2;
	offset = z();
	is_int = false;
}


double
OCDataWidget::testValue(int no, double minv, double maxv,
						double speed, double offset, bool is_int)
{
	if (speed < 1e-6)
		speed = 2;
	double v = (sin(currentTestSec() / speed + offset * M_PI/50) + 1)
				* (maxv - minv) / 2 + minv;
	return is_int ? int(v) : v;
}


QVariant
OCDataWidget::readValue(int no, bool *ok)
{
	if (no < 0 || no >= num) {
		if (ok)
			*ok = false;
		return QVariant();
	}
	if (isTest()) {
		if (ok)
			*ok = true;
		return testValue(no);
	}
	if (isEdit()) {
		if (ok)
			*ok = true;
		return getCanvas()->getEditValue();
	}
	return OQWatch::readValue(vars[no], 0, ok);
}


QVariant
OCDataWidget::readValue(bool *ok)
{
	return readValue(0, ok);
}


bool
OCDataWidget::writeValue(int no, const QVariant& value)
{
	if (no < 0 || no >= num)
		return false;
	if (isEdit() || isTest())
		return true;
	OQWatch *watch = OQWatch::getInstance();
	if (!watch)
		return false;
	watch->writeBg(vars[no], value);
	return true;
}


bool
OCDataWidget::bind(const QString& _vars)
{
	unbind();
	vars = QStringList::split(' ', _vars.simplifyWhiteSpace());
	num = vars.count();
	if (maxnum > 0 && num > maxnum) {
		log("will bind only %d variables of %d (%s)",
			maxnum, num, vars.join(" ").ascii());
		for (int i = num; i < maxnum; i++)
			vars.pop_back();
		num = maxnum;
	}
	if (num > 0) {
		bgids = new int[num];
		ooids = new unsigned[num];
	}
	qDebug("bind " + varList());
	OQWatch *watch = OQWatch::getInstance();
	if (!isEdit() && watch) {
		if (!connected) {
			connect(watch,
					SIGNAL(monitorDone(long,const owquark_t&,int)),
					SLOT(monitorDone(long,const owquark_t&,int)));
			connect(watch,
					SIGNAL(dataUpdated(const owquark_t&,const QVariant&,long)),
					SLOT(dataUpdated(const owquark_t&,const QVariant&,long)));
			connected = true;
		}
		for (int i = 0; i < num; i++)
			bgids[i] = watch->monitorBg(vars[i], long(this));
	}
	return true;
}


void
OCDataWidget::monitorDone(long param, const owquark_t& inf, int err)
{
	if (long(this) == param) {
		qDebug("dataW(%s).gotMon(%s)=%d", varList().ascii(), inf.desc, err);
		if (err != OK && false)
			log("cannot bind \"%s\": %s",
				inf.desc, OQWatch::errorString(err).ascii());
		QString desc(inf.desc);
		for (int i = 0; i < num; i++) {
			if (vars[i] == inf.desc) {
				if (err == 0) {
					ooids[i] = inf.ooid;
					ooid2idx[inf.ooid] = i;
				}
				bgids[i] = 0;
				break;
			}
		}
	}
}


void
OCDataWidget::dataUpdated(const owquark_t& inf, const QVariant& val, long time)
{
	if (!ooid2idx.contains(inf.ooid) || isTest())
		return;
	int no = ooid2idx[inf.ooid];
	qvDebug("dataW(%s).update(%s)=%d", varList().ascii(), inf.desc, i);
	updateValue(no, val);
}


void
OCDataWidget::unbind()
{
	if (num)
		qDebug("unbind " + varList());
	OQWatch *watch = OQWatch::getInstance();
	if (!isEdit() && watch) {
		for (int i = 0; i < num; i++) {
			if (bgids[i])
				watch->cancelBg(bgids[i]);
			if (ooids[i])
				watch->removeMonitor(ooids[i]);
		}
	}
	delete bgids;
	bgids = 0;
	delete ooids;
	ooids = 0;
	vars.clear();
	ooid2idx.clear();
	num = 0;
}
