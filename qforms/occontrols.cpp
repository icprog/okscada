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
#include "oqpiclabel.h"

#include <qvariant.h>
#include <qprocess.h>


#define qDebug(...)


/* ================ Link ================ */


OCLink::OCLink(OQCanvas *c)
	:	OCWidget(c),
		link(0)
{
	attachChild(new OQPicButton(getParent()));
	connect(button(), SIGNAL(clicked()), SLOT(clicked()));
}


void
OCLink::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("Label", "glabel");
	atts.replace("link", "string");
}


bool
OCLink::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Label") {
		setLabel(value.toString());
		return true;
	}
	if (name == "link") {
		link = value.toString();
		return true;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCLink::getAttr(const QString& name) const
{
	if (name == "Label")
		return getLabel();
	if (name == "link")
		return link;
	return OCWidget::getAttr(name);
}


void
OCLink::setLabel(const QString& label)
{
	button()->setLabel(label);
	redraw();
}


void
OCLink::clicked()
{
	if (!isEdit())
		getCanvas()->doOpenScreen(link);
}


void
OCLink::drawShape(QPainter& p)
{
	// button will draw itself
}


/* ================ Runner ================ */


OCRunner::OCRunner(OQCanvas *c)
	:	OCWidget(c),
		command(0)
{
	attachChild(new OQPicButton(getParent()));
	connect(button(), SIGNAL(clicked()), SLOT(clicked()));
}


void
OCRunner::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("Label", "glabel");
	atts.replace("Command", "string");
}


bool
OCRunner::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Label") {
		setLabel(value.toString());
		return true;
	}
	if (name == "Command") {
		setCommand(value.toString());
		return true;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCRunner::getAttr(const QString& name) const
{
	if (name == "Label")
		return getLabel();
	if (name == "Command")
		return getCommand();
	return OCWidget::getAttr(name);
}


void
OCRunner::setLabel(const QString& label)
{
	button()->setLabel(label);
	redraw();
}


void
OCRunner::clicked()
{
	if (isTest() || isEdit())
		return;
	int pid = OQApp::oqApp->runScript(command, "perl");
	log("run perl [%d] ``%s''", pid, command.ascii());
}


void
OCRunner::drawShape(QPainter& p)
{
	// button will draw itself
}


/* ================ Offnominal ================ */


#define OFFNOM_WIDTH	270
#define OFFNOM_HEIGHT	40

QString OCOffnom::desc_filename = "offnominals.lst";
QMap<QString,QString> OCOffnom::descriptions;
bool OCOffnom::got_descriptions;


void
OCOffnom::assignDescriptions(const QString& filename)
{
	desc_filename = filename;
	got_descriptions = false;
	readDescriptions();
}


void
OCOffnom::readDescriptions()
{
	if (got_descriptions)
		return;
	got_descriptions = true;
	descriptions.clear();
	if (desc_filename.isEmpty())
		return;
	QFile file;
	file.setName(OQApp::etc.filePath(desc_filename));
	if (!file.exists()) {
		file.setName(OQApp::root.filePath(desc_filename));
		if (!file.exists()) {
			file.setName(desc_filename);
			if (!file.exists()) {
				qWarning(desc_filename + ": cannot find offnominals file");
				return;
			}
		}
	}
	if (!file.open(IO_ReadOnly)) {
		qWarning(desc_filename + ": cannot read offnominals file");
		return;
	}
	int count = 0;
	QString line;
	while (file.readLine(line, 1024) >= 0) {
		int pos = line.find("==>");
		if (pos < 0)
			continue;
		QString key = line.left(pos).stripWhiteSpace();
		QString val = line.mid(pos + 3).stripWhiteSpace();
		descriptions.replace(key, val);
		count++;
	}
	qDebug("parsed %d offnominal descriptions", count);
}


OCOffnom::OCOffnom(OQCanvas *c)
	:	OCWidget(c),
		toggling(false)
{
	readDescriptions();
	attachChild(new QPushButton(getParent()));
	button()->setFocusPolicy(QWidget::NoFocus);
	button()->setToggleButton(true);
	button()->setText("---");
	updatePixmap();
	button()->resize(OFFNOM_WIDTH, OFFNOM_HEIGHT);
	updateSize(OFFNOM_WIDTH, OFFNOM_HEIGHT);
	connect(button(), SIGNAL(toggled(bool)), SLOT(toggled(bool)));
}


void
OCOffnom::attrList(OCAttrList& atts) const
{
	OCWidget::attrList(atts);
	atts.replace("Label", "string");
	atts.replace("name", "string");
	atts.replace("descr", "string");
}


bool
OCOffnom::setAttr(const QString& name, const QVariant& value)
{
	if (name == "Label") {
		setLabel(value.toString());
		return true;
	}
	if (name == "name") {
		setName(value.toString());
		return true;
	}
	if (name == "descr") {
		setDesc(value.toString());
		return true;
	}
	return OCWidget::setAttr(name, value);
}


QVariant
OCOffnom::getAttr(const QString& name) const
{
	if (name == "Label")
		return getLabel();
	if (name == "name")
		return getName();
	if (name == "descr")
		return getDesc();
	return OCWidget::getAttr(name);
}


void
OCOffnom::setLabel(const QString& _label)
{
	label = _label;
	button()->setText(label);
	redraw();
}


void
OCOffnom::setName(const QString& _name)
{
	name = _name;
}


void
OCOffnom::setDesc(const QString& _desc, bool visual_only)
{
	QString ndesc = _desc;
	if (!ndesc.isNull())
		ndesc = ndesc.simplifyWhiteSpace().replace("\\n", "\n");
	if (!visual_only)
		desc = ndesc;
	setTip(ndesc, true);
}


QString
OCOffnom::getDesc() const
{
	return desc.isNull() ? desc : QString(desc).replace("\n", "\\n");
}


void
OCOffnom::polish()
{
	OCWidget::polish();
	if (desc.isNull() && descriptions.contains(name))
		setDesc(descriptions[name], true);
}


void
OCOffnom::toggled(bool on)
{
	if (!toggling) {
		toggling = true;
		setOn(on);
		toggling = false;
	}
}


void
OCOffnom::updatePixmap()
{
	if (!button())
		return;
	QString s = button()->isOn() ? "png/offnom_run.png" : "png/offnom_idle.png";
	button()->setIconSet(OQPixmap(s));
}


void
OCOffnom::setOn(bool on)
{
	updatePixmap();
	if (name.isNull() || isTest() || isEdit())
		return;
	QString state = on ? "on" : "off";
	QString s = name;
	for (unsigned i = 0; i < s.length(); i++) {
		char c = s.constref(i);
		if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c <='9')
				|| c=='+' || c=='-'))
			s[i] = ' ';
	}
	s = s.simplifyWhiteSpace().replace(' ', '_'); + ".ofn_" + state;
	s = QString("sample_script(\"%1.ofn_%2\")").arg(s).arg(state);
	int pid = OQApp::oqApp->runScript(s, "perl");
	log("[%d] Offnominal \"%s\" is %s", pid, name.ascii(), state.ascii());
}


bool
OCOffnom::isOn()
{
	return (button() ? button()->isOn() : false);
}


void
OCOffnom::drawShape(QPainter& p)
{
	// button will draw itself
}
