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

#include "ocwidgets.h"
#include "oqcommon.h"

#include <qlabel.h>
#include <qvariant.h>
#include <qpainter.h>


#define qDebug(...)
#if 0
#define qfDebug(x...)	qDebug(x)
#else
#define qfDebug(...)
#endif

#define LABEL_FLAGS		(AlignLeft | AlignTop)
#define STRING_FLAGS	(AlignLeft | AlignTop)


/* ================ Formatted ================ */


OCFormatted::OCFormatted(OQCanvas *c)
	: OCDataWidget(c), warned(false)
{
}


void
OCFormatted::updateText(bool reading)
{
	int pos = 0;
	int flen = format.length();
	const char *f = format.ascii();
	int i, j;
	char fmt1[32];
	int varno = 0;
	QString txt = "";

	qfDebug(">> start format=[%s]", format.ascii());
	for (i = 0; i < flen; i++) {
		qfDebug("..%d rest=[%s] varno=%u", i, format.mid(i).ascii(), varno);
		if (f[i] != '%')
			continue;
		if (i > pos) {
			txt.append(format.mid(pos, i - pos));
			qfDebug("..append [%s]", format.mid(pos,i-pos).ascii());
		}
		if (f[i+1] == '%') {
			txt.append("%");
			pos = ++i + 1;
			qfDebug("..append %%");
			continue;
		}

		j = i+1;

		if (j < flen && (f[j] == '-' || f[j] == '+'))
			j++;
		while (j < flen && f[j] >= '0' && f[j] <= '9')
			j++;
		if (j < flen && f[j] == '.')
			j++;
		while (j < flen && f[j] >= '0' && f[j] <= '9')
			j++;
		char modifier = ' ';
		if (j < flen && (f[j] == 'h' || f[j] == 'l' || f[j] == 'L')) {
			modifier = f[j];
			j++;
		}
		if (j >= flen)
			break;
		if (j-i >= int(sizeof fmt1)-1) {
			if (!warned) {
				log("broken format \"" + format + "\"");
				warned = false;
			}
			varno++;
			i = j;
			pos = i+1;
			continue;
		}
		for (int k = i; k <= j; k++)
			fmt1[k-i] = f[k];
		fmt1[j-i+1] = '\0';

		QString chunk;
		bool ok = (varno < varNum());
		QVariant val;
		if (reading)
			val = readValue(varno, &ok);
		qfDebug("..chunk fmt1=[%s] varno=%u of %u", fmt1, varno, varNum());

		if (ok && reading) {
			switch (f[j]) {
			case 'd':
				chunk.sprintf(fmt1, val.toInt(&ok));
				break;
			case 'u':
			case 'x':
				chunk.sprintf(fmt1, val.toUInt(&ok));
				break;
			case 'f':
			case 'e':
			case 'g':
				chunk.sprintf(fmt1, val.toDouble(&ok));
				break;
			case 's':
				chunk.sprintf(fmt1, val.toString().ascii());
				break;
			default:
				if (!warned) {
					log("invald format \"" + format + "\"");
					warned = true;
				}
				ok = false;
				break;
			}
		}

		if (!ok) {
			int k = j - i;
			if (modifier != ' ')
				k--;
			fmt1[k] = 's';
			fmt1[k+1] = '\0';
			qfDebug("...failed chunk fmt=[%s]", fmt1);
			chunk.sprintf(fmt1, "?");
			chunk.fill('-');
		}

		qfDebug("..chunk=[%s]", chunk.ascii());
		txt.append(chunk);
		i = j;
		pos = i+1;
		varno++;
	}
	if (flen > pos)
		txt.append(format.mid(pos, flen - pos));
	qfDebug("<< text=[%s] for format=[%s]", txt.ascii(), format.ascii());
	text = txt;
	updateGeometry();
}


void
OCFormatted::update()
{
	updateText();
	OCDataWidget::update();
}


void
OCFormatted::attrList(OCAttrList& atts) const
{
	OCDataWidget::attrList(atts);
	atts.replace("format", "string");
}


bool
OCFormatted::setAttr(const QString& name, const QVariant& value)
{
	if (name == "format") {
		setFormat(value.toString());
		return true;
	}
	return OCDataWidget::setAttr(name, value);
}


QVariant
OCFormatted::getAttr(const QString& name) const
{
	if (name == "format")
		return getFormat();
	return OCDataWidget::getAttr(name);
}


void
OCFormatted::setFormat(const QString& _format)
{
	format = QString(_format).replace("\\n", "\n");
	warned = false;
	updateText();
}


QString
OCFormatted::getFormat() const
{
	return QString(format).replace("\n", "\\n");
}


/* ================ String ================ */


OCString::OCString(OQCanvas *c)
	: OCFormatted(c)
{
}


void
OCString::attrList(OCAttrList& atts) const
{
	OCFormatted::attrList(atts);
	atts.replace("font", "font");
	atts.replace("color", "color");
}


bool
OCString::setAttr(const QString& name, const QVariant& value)
{
	if (name == "font") {
		if (value.type() == QVariant::Font) {
			setFont(value.toFont());
			return true;
		}
		if (value.type() == QVariant::String) {
			setFontName(value.toString());
			return true;
		}
		return false;
	}
	if (name == "color") {
		if (value.type() == QVariant::Color) {
			setColor(value.toColor());
			return true;
		}
		if (value.type() == QVariant::String) {
			setColorName(value.toString());
			return true;
		}
		return false;
	}
	return OCFormatted::setAttr(name, value);
}


QVariant
OCString::getAttr(const QString& name) const
{
	if (name == "font")
		return getFontName();
	if (name == "color")
		return getColorName();
	return OCFormatted::getAttr(name);
}


void
OCString::updateGeometry()
{
	QSize sz = QFontMetrics(font).size(STRING_FLAGS, text);
	updateSize(sz.width() + 1, sz.height());
}


void
OCString::setFont(const QFont& _font)
{
	font_name = fontToString(font = _font);
	updateGeometry();
}


void
OCString::setFontName(const QString& _font)
{
	font_name = _font;
	font = stringToFont(_font.isEmpty() ? getCanvas()->getDefTextFont()
										: _font);
	updateGeometry();
}


void
OCString::setColor(const QColor& _color)
{
	if ((color = _color).isValid()) {
		color_name = color.name();
	} else {
		color_name = QString::null;
		color.setNamedColor(getCanvas()->getDefTextColor());
	}
	redraw();
}


void
OCString::setColorName(const QString& _color)
{
	color_name = _color;
	color.setNamedColor(_color.isEmpty() ? getCanvas()->getDefTextColor()
										: _color);
	redraw();
}


void
OCString::updateDefaults()
{
	if (font_name.isEmpty()) {
		QFont def_font = stringToFont(getCanvas()->getDefTextFont());
		if (def_font != font) {
			font = def_font;
			updateGeometry();
		}
	}
	if (color_name.isEmpty()) {
		QColor def_color(getCanvas()->getDefTextColor());
		if (def_color != color) {
			color = def_color;
			redraw();
		}
	}
}


void
OCString::drawShape(QPainter& p)
{
	p.setFont(font);
	p.setPen(color);
	p.drawText(boundingRect(), STRING_FLAGS, text);
}


void
OCString::testValueProps(int no, double& minv, double& maxv,
						double& speed, double& offset, bool& is_int)
{
	OCDataWidget::testValueProps(no, minv, maxv, speed, offset, is_int);
	minv = 0;
	maxv = 10;
	is_int = false;
}


/* ================ Label ================ */


OCLabel::OCLabel(OQCanvas *c)
	: OCWidget(c)
{
}


void
OCLabel::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("Label", "string");
	atts.replace("font", "font");
	atts.replace("color", "color");
}


bool
OCLabel::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Label") {
		setText(value.toString());
		return true;
	}
	if (name == "font") {
		if (value.type() == QVariant::Font) {
			setFont(value.toFont());
			return true;
		}
		if (value.type() == QVariant::String) {
			setFontName(value.toString());
			return true;
		}
		return false;
	}
	if (name == "color") {
		if (value.type() == QVariant::Color) {
			setColor(value.toColor());
			return true;
		}
		if (value.type() == QVariant::String) {
			setColorName(value.toString());
			return true;
		}
		return false;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCLabel::getAttr(const QString& name) const
{
	if (name == "Label")
		return getText();
	if (name == "font")
		return getFontName();
	if (name == "color")
		return getColorName();
	return OCWidget::getAttr(name);
}


void
OCLabel::setFont(const QFont& _font)
{
	font_name = fontToString(font = _font);
	updateGeometry();
}


void
OCLabel::setFontName(const QString& _font)
{
	font_name = _font;
	font = stringToFont(_font.isEmpty() ? getCanvas()->getDefTextFont()
										: _font);
	updateGeometry();
}


void
OCLabel::setColor(const QColor& _color)
{
	if ((color = _color).isValid()) {
		color_name = color.name();
	} else {
		color_name = QString::null;
		color.setNamedColor(getCanvas()->getDefTextColor());
	}
	redraw();
}


void
OCLabel::setColorName(const QString& _color)
{
	color_name = _color;
	color.setNamedColor(_color.isEmpty() ? getCanvas()->getDefTextColor()
										: _color);
	redraw();
}


void
OCLabel::updateDefaults()
{
	if (font_name.isEmpty()) {
		QFont def_font = stringToFont(getCanvas()->getDefTextFont());
		if (def_font != font) {
			font = def_font;
			updateGeometry();
		}
	}
	if (color_name.isEmpty()) {
		QColor def_color(getCanvas()->getDefTextColor());
		if (def_color != color) {
			color = def_color;
			redraw();
		}
	}
}


void
OCLabel::setText(const QString& _text)
{
	vtext = utext = QString(_text);
	vtext.replace("\\n", "\n");
	updateGeometry();
}


void
OCLabel::updateGeometry()
{
	QSize sz = QFontMetrics(font).size(LABEL_FLAGS, vtext);
	updateSize(sz.width() + 1, sz.height());
}


void
OCLabel::drawShape(QPainter& p)
{
	p.setFont(font);
	p.setPen(color);
	p.drawText(boundingRect(), LABEL_FLAGS, vtext);
}


/* ================ Pixmap ================ */


OCPixmap::OCPixmap(OQCanvas *c)
	: OCWidget(c)
{
}


void
OCPixmap::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("Image", "image");
}


bool
OCPixmap::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Image") {
		filename = value.toString();
		pixmap = OQPixmap(filename);
		return true;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCPixmap::getAttr(const QString& name) const
{
	if (name == "Image")
		return filename;
	return OCWidget::getAttr(name);
}


void
OCPixmap::drawShape(QPainter& p)
{
	p.drawPixmap(boundingRect(), pixmap);
}


/* ================ Bar ================ */


OCBar::OCBar(OQCanvas *c)
	: OCWidget(c)
{
}


void
OCBar::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("color", "color");
}


bool
OCBar::setAttr(const QString& name, const QVariant& value)
{
	if (name == "color") {
		color = QColor(value.toString());
		return true;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCBar::getAttr(const QString& name) const
{
	if (name == "color")
		return color.name();
	return OCWidget::getAttr(name);
}


void
OCBar::drawShape(QPainter& p)
{
	p.fillRect(boundingRect(), color);
}
