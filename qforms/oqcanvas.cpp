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
#include "oqwatch.h"

#include <qfile.h>
#include <qdom.h>
#include <qvariant.h>
#include <qdatetime.h>


#define qDebug(...)


#define OQCANVAS_UPDATE_PERIOD		100
#define OQCANVAS_DEFAULT_SIZE		100


OQCanvas::OQCanvas(QWidget *_parent)
	:	QCanvas(),
		is_test(false),
		test_timer_id(0),
		test_sec(0),
		is_edit(false),
		parent(_parent)
{
	setupClassMaps();
	setBaseSize(OQCANVAS_DEFAULT_SIZE, OQCANVAS_DEFAULT_SIZE);
	setUpdatePeriod(OQCANVAS_UPDATE_PERIOD);
	setDoubleBuffering(true);
	clear();
}


void
OQCanvas::setBgImage(const QString& filename)
{
	bg_image = filename;
	setBackgroundColor(QWidget().paletteBackgroundColor());
	OQPixmap tile(filename, false);
	setBackgroundPixmap(tile);
}


void
OQCanvas::setBaseSize(int w, int h, bool also_resize)
{
	if (w > 0 && h > 0) {
		base_size.setWidth(w);
		base_size.setHeight(h);
		if (also_resize)
			resize(w, h);
	}
}


void
OQCanvas::resize(int w, int h)
{
	if (w >= base_size.width() && h >= base_size.height())
		QCanvas::resize(w, h);
}


QRect
OQCanvas::boundingRect(bool all_items)
{
	QRect brect(0, 0, 9, 9);
	QCanvasItemList all = allItems();
	for (QCanvasItemList::Iterator it = all.begin(); it != all.end(); ++it) {
		if (all_items || isMaybeOCWidget(*it))
			brect |= (*it)->boundingRect();
	}
	brect.addCoords(0, 0, 1, 1);
	return brect;
}


QCanvasItemList
OQCanvas::mouseTrackers()
{
	QCanvasItemList lst;
	QCanvasItemList::Iterator it;
	for (it = mtrackers.begin(); it != mtrackers.end(); ++it)
		lst.prepend(*it);
    return lst;
}


void
OQCanvas::clear()
{
	// FIXME: this ends up in Segmentation Fault !
	mtrackers.clear();
	QCanvasItemList lst = allItems();
	QCanvasItemList::Iterator it;
	for (it = lst.begin(); it != lst.end(); ++it)
		delete *it;
	def_text_font = QString::null;
	def_text_color = QString::null;
	setBgImage(0);
	setAllChanged();
}


int
OQCanvas::zRange(double& zmin, double& zmax)
{
	QCanvasItemList lst = allItems();
	QCanvasItemList::Iterator it;
	int count;
	for (count = 0, it = lst.begin(); it != lst.end(); ++it, ++count) {
		double z = (*it)->z();
		if (count == 0) {
			zmin = zmax = z;
		} else {
			if (z < zmin)
				zmin = z;
			if (z > zmax)
				zmax = z;
		}
	}
	if (count == 0) {
		zmin = zmax = 0;
	}
	return count;
}


void
OQCanvas::setDefTextFont(const QString& font)
{
	if (font != def_text_font) {
		def_text_font = font;
		updateDefaults();
	}
}


void
OQCanvas::setDefTextColor(const QString& color)
{
	if (color != def_text_color) {
		def_text_color = color;
		updateDefaults();
	}
}


void
OQCanvas::updateDefaults()
{
	QCanvasItemList lst = allItems();
	QCanvasItemList::Iterator it;
	for (it = lst.begin(); it != lst.end(); ++it) {
		if (isMaybeOCWidget(*it))
			((OCWidget *)(*it))->updateDefaults();
	}
}


/* ======== Test Mode ======== */


void
OQCanvas::setTest(bool on)
{
	if (is_test == on)
		return;
	is_test = on;
	if (test_timer_id) {
		killTimer(test_timer_id);
		test_timer_id = 0;
	}
	if (on) {
		test_timer_id = startTimer(OQCANVAS_UPDATE_PERIOD);
	} else {
		QCanvasItemList all = allItems();
		QCanvasItemList::Iterator it;
		QVariant nodata;
		for (it = all.begin(); it != all.end(); ++it) {
			if (!isMaybeOCWidget(*it))
				continue;
			OCWidget *w = (OCWidget *) *it;
			if (!w->inherits("OCDataWidget"))
				continue;
			OCDataWidget *dw = (OCDataWidget *) w;
			for (int no = 0; no < dw->varNum(); no++)
				dw->updateValue(no, nodata);
		}
		OQWatch::renewAllMonitors();
	}
}


void
OQCanvas::timerEvent(QTimerEvent *e)
{
	if (!test_timer_id || e->timerId() != test_timer_id) {
		QCanvas::timerEvent(e);
		return;
	}
	QDateTime now = QDateTime::currentDateTime();
	test_sec = now.toTime_t() + now.time().msec() / 1000.0;
	QCanvasItemList all = allItems();
	QCanvasItemList::Iterator it;
	for (it = all.begin(); it != all.end(); ++it) {
		if (!isMaybeOCWidget(*it))
			continue;
		OCWidget *w = (OCWidget *) *it;
		if (!w->inherits("OCDataWidget"))
			continue;
		OCDataWidget *dw = (OCDataWidget *) w;
		for (int no = 0; no < dw->varNum(); no++)
			dw->updateValue(no, dw->testValue(no));
	}
}


/* ======== Load XML ======== */


static inline
QDomElement
getChild(QDomElement parent, const char *name, bool *ok = 0)
{
	if (parent.isNull()) {
		if (ok)
			*ok = false;
		return parent;
	}
	QDomElement child = parent.namedItem(name).toElement();
	if (child.isNull()) {
		if (ok)
			*ok = false;
	}
	return child;
}


static inline
QString
getChildText(QDomElement parent, const char *name, bool *ok = 0)
{
	QDomElement child = getChild(parent, name, ok);
	if (child.isNull())
		return 0;
	child.normalize();
	return child.firstChild().nodeValue().stripWhiteSpace();
}


static inline
int
getChildInt(QDomElement parent, const char *name, bool *ok = 0)
{
	QString text = getChildText(parent, name, ok);
	bool conv_ok;
	int val = text.toInt(&conv_ok, 0);
	if (ok && !conv_ok)
		ok = false;
	return val;
}


bool
OQCanvas::load(const QString& filename, QString *ermes_p)
{
	qDebug(">>> Loading from " + filename);
	QFile file(OQApp::screens.filePath(filename));
	QString ermes;
	if (!file.open(IO_ReadOnly)) {
		ermes = "Cannot open " + filename;
		if (ermes_p)
			*ermes_p = ermes;
		return false;
	}

	QDomDocument doc;
	int erline, ercol;
	bool ok = doc.setContent(&file, &ermes, &erline, &ercol);
	file.close();
	if (!ok) {
		ermes = QString("%1 [%2:%3]: %4")
						.arg(filename).arg(erline).arg(ercol).arg(ermes);
		if (ermes_p)
			*ermes_p = ermes;
		return false;
	}

	QDomElement screen = doc.documentElement();
	ok = (!screen.isNull() && screen.nodeName() == "screen");

	QDomElement window = getChild(screen, "window", &ok);
	int w = getChildInt(window, "width", &ok);
	int h = getChildInt(window, "height", &ok);
	QString bg = getChildText(window, "background");
	setDefTextFont(getChildText(window, "textfont"));
	setDefTextColor(getChildText(window, "textcolor"));
	qDebug("window: w=%d h=%d bg=[%s]", w, h, bg.ascii());
	setBaseSize(w, h);
	setBgImage(bg);
	if (!ok)
		ermes = "Error in window description";

	QDomElement widgets = getChild(screen, "widgets", &ok);
	QDomNodeList objs = doc.elementsByTagName("widget");
	for (unsigned i = 0; i < objs.count(); i++) {
		int z = i;
		QDomElement obj = objs.item(i).toElement();
		if (obj.parentNode() != widgets) {
			if (ermes.isNull())
				ermes = "A widget element outside the <widgets> group";
			ok = false;
			continue;
		}
		QString klass = getChildText(obj, "class", &ok);
		int rtti = classToRtti(klass);
		OCWidget *ptr = newBasicWidget(rtti);
		if (ptr == 0) {
			ok = false;
			if (ermes.isNull())
				ermes = QString("Class \"%1\" not recognized").arg(klass);
			continue;
		}
		OCWidget& widget = *ptr;

		int x = getChildInt(obj, "posx", &ok);
		int y = getChildInt(obj, "posy", &ok);
		widget.move(x, y);
		widget.setZ(z);

		bool size_ok = true;
		int w = getChildInt(obj, "width", &size_ok);
		int h = getChildInt(obj, "height", &size_ok);
		if (size_ok)
			widget.resize(w, h);

		qDebug("%u: %s xy=(%d,%d) wh=(%d,%d)%s z=%d",
				i, klass.ascii(), x, y, w, h, size_ok ? "" : "!", z);

		OCAttrList atts;
		widget.attrList(atts);
		for (int j = 0; j < atts.count(); j++) {
			const QString& key = atts.key(j);
			if (key=="posx" || key=="posy" || key=="width" || key=="height")
				continue;
			bool miss_ok = true;
			QString val(getChildText(obj, key, &miss_ok));
			if (atts.type(j) == "boolean") {
				val = val.simplifyWhiteSpace().lower();
				bool flag = (val[0]=='y' || val[0]=='t' || val=="on" || val.toInt() > 0);
				val = flag ? "1" : "0";
			}
			if (!miss_ok) {
				qDebug(widget.getClass() + ": missed " + key);
				continue;
			}
			widget.setAttr(key, val);
			qDebug((widget.getClass() + ": "+key+":=\""
					+ val.replace("%","%%") + "\"").utf8());
		}

		widget.polish();
		widget.show();
	}
	if (!ok) {
		if (ermes.isNull())
			ermes = "File format error";
		ermes = filename + ": " + ermes;
		if (ermes_p)
			*ermes_p = ermes;
		return false;
	}
	qDebug("loading %s success", filename.ascii());
	return true;
}


/* ======== Save XML ======== */


static inline
QDomElement
newChild(QDomElement& parent, const char *name, const QString& text = 0)
{
	QDomDocument doc = parent.ownerDocument();
	QDomElement elem = doc.createElement(name);
	parent.appendChild(elem);
	if (!text.isNull())
		elem.appendChild(doc.createTextNode(text));
	return elem;
}


static inline
QDomElement
newChild(QDomElement& parent, const char *name, int value)
{
	QDomDocument doc = parent.ownerDocument();
	QDomElement elem = doc.createElement(name);
	parent.appendChild(elem);
	elem.appendChild(doc.createTextNode(QString::number(value)));
	return elem;
}


static inline
QDomElement
newChild(QDomElement& parent, const char *name,
		const QDir& dir, const QString& path)
{
	QDomDocument doc = parent.ownerDocument();
	QDomElement elem = doc.createElement(name);
	parent.appendChild(elem);
	QDomNode text;
	if (!QDir::isRelativePath(path) && path.startsWith(dir.path()))
		text = doc.createTextNode(QString(path).mid(dir.path().length()));
	else
		text = doc.createTextNode(path);
	elem.appendChild(text);
	return elem;
}


bool
OQCanvas::save(const QString& filename)
{
	qDebug(">>> Saving to " + filename);
	QString backup(filename + ".bak");
	QDir dir = OQApp::screens;
	dir.remove(backup);
	dir.rename(filename, backup);
	QDomDocument doc("screen");
	QDomElement screen = doc.createElement("screen");
	doc.appendChild(screen);
	QDomElement window = newChild(screen, "window");
	newChild(window, "width", getBaseSize().width());
	newChild(window, "height", getBaseSize().height());
	if (!bg_image.isEmpty())
		newChild(window, "background", OQApp::pics, bg_image);
	if (!def_text_font.isEmpty())
		newChild(window, "textfont", def_text_font);
	if (!def_text_color.isEmpty())
		newChild(window, "textcolor", def_text_color);
	QDomElement widgets = newChild(screen, "widgets");

	class ZSortedWidgets : public QPtrList<OCWidget>
	{
	protected:
		int compareItems(QPtrCollection::Item a, QPtrCollection::Item b) {
			double dz = ((OCWidget *)a)->z() - ((OCWidget *)b)->z();
			return (dz == 0 ? 0 : dz > 0 ? 1 : -1);
		}
	}
	zsorted;

	QCanvasItemList all = allItems();
	for (QCanvasItemList::Iterator it = all.begin(); it != all.end(); ++it) {
		if (!isValidOCWidget(*it)) {
			int rtti = (*it)->rtti();
			if (rtti < 0 || rtti > 8)
				qWarning("unexpected rtti=0x%x", rtti);
			continue;
		}
		zsorted.append((OCWidget *)(*it));
	}
	zsorted.sort();

	for (OCWidget *ptr = zsorted.first(); ptr; ptr = zsorted.next()) {
		OCWidget& widget = *ptr;
		QDomElement obj = newChild(widgets, "widget");
		qDebug(QString("save " + widget.toString()).utf8());
		bool empty = true;

		OCAttrList atts;
		widget.attrList(atts);
		newChild(obj, "class", widget.getClass());
		//newChild(obj, "z-ord", (int) widget.z()); // for testing
		newChild(obj, "posx", int(widget.x()));
		newChild(obj, "posy", int(widget.y()));
		if (widget.width() > 0 && widget.height() > 0) {
			newChild(obj, "width", (int) widget.width());
			newChild(obj, "height", (int) widget.height());
			empty = false;
		}

		for (int j = 0; j < atts.count(); j++) {
			const QString& key = atts.key(j);
			if (key=="posx" || key=="posy" || key=="width" || key=="height")
				continue;
			QVariant val(widget.getAttr(key));

			if (key=="no_border" || key=="readonly" || key=="vertical") {
				if (atts.type(j) == "boolean" && val.toBool() == false) {
					// FIXME: for output compatibility with obsolete releases.
					val = QVariant();
				}
			}

			if (!val.isNull()) {
				QString str;
				if (atts.type(j) == "boolean")
					str = val.toBool() ? "yes" : "no";
				else
					str = val.toString();
				newChild(obj, key, str);
				empty = false;
			}
		}

		if (empty) {
			// detected a stale element
			qDebug(QString("stale: " + widget.toString()).utf8());
			obj.parentNode().removeChild(obj);
			obj.clear();
		}
	}

	QFile file(dir.filePath(filename));
	if (!file.open(IO_WriteOnly))
		return false;
	QTextStream out(&file);
	out << "<?xml version=\"1.0\"?>\n";
	out << doc.toString().remove("<!DOCTYPE screen>\n");
	file.close();
	return true;
}
