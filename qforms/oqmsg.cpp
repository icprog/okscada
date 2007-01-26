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

  Optikus Message Center.

*/

#include <qsplitter.h>
#include <qvgroupbox.h>
#include <qlayout.h>
#include <qheader.h>
#include <qtextstream.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qfile.h>
#include <qmessagebox.h>
#include <qregexp.h>
#include <qlineedit.h>

#include "oqcommon.h"
#include "oqmsg.h"

#include <signal.h>
#include <optikus/log.h>


#define qDebug(...)

#define DELIMITER		";,"

static const QFont VALUE_FONT("Courier New", 13, QFont::Bold);



/*			XML description parser			*/


bool
OQMsgTreeParser::startDocument()
{
	target = group = item = 0;
	in_desc = false;
	return true;
}


bool
OQMsgTreeParser::startElement(const QString& nsuri, const QString& localname,
							const QString& qname, const QXmlAttributes& atts)
{
	bool ok = true;

	if (qname == "cmtarget") {
		target = new OQMsgItem(msgr, atts.value("name"));
		target->setOpen(true);
	}
	else if (qname == "cmgroup") {
		if (!target)
			return false;
		group = new OQMsgItem(msgr, target, atts.value("groupno").toInt(&ok),
								atts.value("code"), atts.value("name"));
		if (!ok)
			qWarning("cannot parse group " + group->getKey());
	}
	else if (qname == "cmessage") {
		if (!group)
			return false;
		item = new OQMsgItem(msgr, group, atts.value("datano").toInt(&ok),
								atts.value("name"));
		if (!ok)
			qWarning("cannot parse message " + item->getKey());
	}
	else if (qname == "cmdesc") {
		if (!item)
			return false;
		in_desc = true;
	}
	else if (qname == "cmparam") {
		if (!item)
			return false;
		QString type = atts.value("type").stripWhiteSpace();
		if (type.isEmpty() || !OQMessageCenter::ptypes.contains(type)) {
			qWarning("invalid parameter type \"" + type
					+ "\" for message " + item->getKey());
			return false;
		}
		item->addParam(type);
	}
	return ok;
}


bool
OQMsgTreeParser::endElement(const QString& nsuri, const QString& localname,
							const QString& qname)
{
	if (qname == "cmessage") {
		item = 0;
	}
	else if (qname == "cmgroup") {
		group = 0;
	}
	else if (qname == "cmtarget") {
		target = 0;
	}
	else if (qname == "cmdesc") {
		in_desc = false;
	}
	return true;
}


bool
OQMsgTreeParser::characters(const QString& ch)
{
	if (item && in_desc)
		item->addDesc(ch);
	return true;
}


/*				OQMessageCenter				*/		


void
OQMessageCenter::msgItemSelected(QListViewItem *lvi)
{
	OQMsgItem *item = (OQMsgItem *) lvi;
	QString label_str;
	QString desc_str;

	keepValues();
	if (!item->isSendable()) {
		label->setText("?");
		descbox->setText("");
		parambox->setNumRows(0);
		cur_item = 0;
		return;
	}
	cur_item = item;
	label->setText(QString("%1/%2 - %3").arg(item->getGroupNo())
					.arg(item->getDataNo()).arg(item->getName()));
	descbox->setText(item->getDesc());
	const QStringList params(item->getParams());
	parambox->setNumRows(item->paramCount());
	for (int i = 0; i < item->paramCount(); i++) {
		parambox->verticalHeader()->setLabel(i, QString("%1").arg(i));
		parambox->setText(i, 0, ptypes[params[i]].name);
		parambox->setItem(i, 1, new OQValueCell(this, item->value(i)));
	}
}


void
OQMessageCenter::msgSendClicked()
{
	keepValues();
	if (!cur_item->isSendable())
		return;

	int i;
	QString ermes;
	int num = cur_item->paramCount();
	QString format;
	double *data = new double[num];

	QRegExp rxchar1("'.'"), rxchar2("\".\""), rxchar3("[^\\-0-9]");
	QRegExp rxint1("\\-?\\d{1,11}"), rxint2("\\-?0x[0-9a-fA-F]{1,8}");
	QRegExp rxfloat("\\-?\\d+(\\.\\d+)?([eE][\\-+]?\\d+)?");

	for (i = 0; i < num; i++) {
		QString val = getValue(i).stripWhiteSpace();
		if (val.isEmpty()) {
			ermes = "Value not provided";
			break;
		}
		OQParamType& type = ptypes[cur_item->getType(i)];
		format.append(type.format);
		if (type.ischar) {
			const char *s = val.ascii();
			int c;
			if (rxchar1.exactMatch(val) || rxchar2.exactMatch(val))
				c = type.issigned ? (char) s[1] : (uchar) s[1];
			else if (rxchar3.exactMatch(val))
				c = type.issigned ? (char) s[0] : (uchar) s[0];
			else if (rxint1.exactMatch(val) || rxint2.exactMatch(val))
				c = val.toInt(0, 0);
			else {
				ermes = "The value does not designate a character";
				break;
			}
			if (c < type.minval || c > type.maxval) {
				ermes = QString("Character value %1 out of range [%2..%3]")
								.arg(c).arg(type.minval).arg(type.maxval);
				break;
			}
			data[i] = c;
			qDebug(QString("char(%1) = %2").arg(i).arg(data[i]));
			continue;
		}
		if (type.isfloat) {
			if (!rxfloat.exactMatch(val)) {
				ermes = "The value is not numeric";
				break;
			}
			data[i] = val.toDouble();
			qDebug(QString("real(%1) = %2").arg(i).arg(data[i]));
			continue;
		}
		if (!rxint1.exactMatch(val) && !rxint2.exactMatch(val)) {
			ermes = "This is not an integer value";
			break;
		}
		if (!type.issigned && val[0] == '-') {
			ermes = "Value must be positive";
			break;
		}
		data[i] = type.issigned ? (double) (long) val.toLong(0, 0)
								: (double) (ulong) val.toULong(0, 0);
		if (	(type.minval != -1 && data[i] < type.minval)
				|| (type.maxval != -1 && data[i] > type.maxval)) {
			ermes = QString("Integer value %1 not in the range [%2..%3]")
					.arg(data[i], 0, 'f', 0).arg(type.minval).arg(type.maxval);
			break;
		}
		qDebug(QString("int(%1) = %2").arg(i).arg(data[i]));
	}

	if (!ermes.isEmpty()) {
		delete data;
		QMessageBox::warning(this, "Cannot Send a Message",
			QString("Error in value number %1:\n\n").arg(i) + ermes,
			"OK");
		parambox->setFocus();
		parambox->setCurrentCell(i, 1);
		return;
	}

	watch.sendCMsgAsDoubleArray(cur_item->getTarget(),
								cur_item->getGroupNo(), cur_item->getDataNo(),
								format, data);
	delete data;
}


/*				OQValueCell					*/


QSize
OQValueCell::sizeHint()
{
	QString txt = text();
	if (txt.isEmpty())
		txt = "WyQ`wji";
	return QFontMetrics(VALUE_FONT).size(0, txt);
}


void
OQValueCell::paint(QPainter *p, const QColorGroup &cg, const QRect &cr, bool sel)
{
	const int w = cr.width(), h = cr.height();
	p->fillRect(0, 0, w, h, cg.brush(sel ? QColorGroup::Highlight
										: QColorGroup::Base));
	p->setPen(sel ? cg.highlightedText() : cg.text());
	p->setFont(VALUE_FONT);
    p->drawText(2, 0, w - 2 - 4, h, AlignLeft | AlignVCenter, text());
}


QWidget *
OQValueCell::createEditor() const
{
	QLineEdit *le = new QLineEdit(table()->viewport(), "OQValueCellEditor");
	le->setFrame(true);
	le->setText(text());
	le->setFont(VALUE_FONT);
	return le;
}


/*				Value Cache					*/


bool
OQMsgItem::setValue(int i, const QString& _newval)
{
	QString newval = _newval.stripWhiteSpace();
	if (newval.contains("  ")) {
		qWarning(QString("value \"%1\" should not contain %2")
				.arg(newval).arg(DELIMITER));
		return false;
	}
	if ((values[i].isEmpty() && newval.isEmpty()) || values[i] == newval) {
		//qDebug(QString("value %1 no change in %2 : '%3' == '%4'")
		//		.arg(i).arg(getKey()).arg(values[i]).arg(newval));
		return false;
	}
	qDebug(QString("value %1 changed in %2 : '%3' -> '%4'")
			.arg(i).arg(getKey()).arg(values[i]).arg(newval));
	values[i] = newval;
	changed = true;
	return true;
}


void
OQMessageCenter::keepValues()
{
	if (!cur_item)
		return;
	qDebug(QString("keeping %1 values for %2")
			.arg(cur_item->paramCount()).arg(cur_item->getKey()));
	parambox->setReadOnly(true);	// flush editorial changes
	parambox->setReadOnly(false);
	for (int i = 0; i < cur_item->paramCount(); i++) {
		if (cur_item->setValue(i, getValue(i)))
			changed = true;
	}
}


QString
OQMsgItem::takeForCache()
{
	for (int i = 0; i < paramCount(); i++)
		if (!values[i].isEmpty())
			return values.join(DELIMITER);
	return QString();
}


bool
OQMsgItem::setFromCache(const QString& cache)
{
	QStringList newvals = QStringList::split(DELIMITER, cache, true);
	if (newvals.count() != values.count()) {
		qWarning(QString("error loading item %1 from cache: expected %2"
						" parameters but got %3")
				.arg(getKey()).arg(values.count()).arg(newvals.count()));
		return false;
	}
	for (int i = 0; i < paramCount(); i++)
		values[i] = values[i].stripWhiteSpace();
	values = newvals;
	return true;
}


void
OQMessageCenter::hashItem(OQMsgItem *item)
{
	QString key;
	key.sprintf("%s-%04d-%04d", (const char *)item->getTarget(),
					item->getGroupNo(), item->getDataNo());
	item->setKey(key);
	qDebug(QString("hashing %1 as %2").arg(key).arg((int)item, 0, 16));
	itemdict.replace(key, item);
}


void
OQMessageCenter::loadValueCache()
{
	QFile file(OQApp::etc.filePath("cmsg.params"));
	if (!file.open(IO_ReadOnly))
		return;
	QString line;
	while (file.readLine(line, 1024) >= 0) {
		line = line.stripWhiteSpace();
		if (line.startsWith("#") || line.isEmpty())
			continue;
		QString key = line.section(' ', 0, 0);
		QString vals = line.section(' ', 1);
		if (key.isEmpty() || vals.isEmpty()) {
			qWarning("invalid cache format for \"" + line + "\"");
			continue;
		}
		OQMsgItem *item = itemdict[key];
		if (!item) {
			qWarning("cannot find message with key " + key);
			continue;
		}
		item->setFromCache(vals);
    }
	file.close();
}


void
OQMessageCenter::saveValueCache()
{
	if (!changed)
		return;
	OQApp::etc.remove("cmsg.params.bak");
	OQApp::etc.rename("cmsg.params", "cmsg.params.bak");
	QFile file(OQApp::etc.filePath("cmsg.params"));
	if (!file.open(IO_WriteOnly))
		return;
	QTextStream out(&file);

	out << "# saved on "
		<< QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss")
		<< "\n";

	QListViewItemIterator it(tree);
	while (*it) {
		OQMsgItem& item = *(OQMsgItem *)*it;
		QString vals = item.takeForCache();
		if (!vals.isEmpty()) {
			out << item.getKey() << " " << item.takeForCache() << "\n";
			qDebug(item.getKey() + ":" + item.takeForCache());
		}
		++it;
    }

	file.close();
	qDebug("cache saved");
}


/*				Initialization				*/


OQDefaultParamMap OQMessageCenter::ptypes;


OQMessageCenter::OQMessageCenter()
	: itemdict(251)
{
	watch.init("oqmsg");
	watch.connect();
	watch.setVerbosity(3);

	QSplitter *split = new QSplitter(Horizontal, this);
	split->setChildrenCollapsible(false);

	tree = new QListView(split);
	tree->addColumn("Target");
	tree->addColumn("Group");
	tree->setColumnAlignment(1, Qt::AlignHCenter);
	tree->addColumn("No");
	tree->setColumnAlignment(2, Qt::AlignHCenter);
	tree->addColumn("Name", 200);
	tree->addColumn("(Sort)", 0);
	tree->header()->setResizeEnabled(false, 4);
	tree->setRootIsDecorated(true);
	tree->setAllColumnsShowFocus(true);

	OQMsgTreeParser tree_handler(this);
	QXmlSimpleReader tree_reader;
	tree_reader.setContentHandler(&tree_handler);
	QFile tree_desc(OQApp::etc.filePath("cmsg.xml"));
	QXmlInputSource tree_source(&tree_desc);
	tree_reader.parse(tree_source);

	tree->setSortColumn(4);
	tree->sort();
	connect(tree, SIGNAL(selectionChanged(QListViewItem*)),
					SLOT(msgItemSelected(QListViewItem *)));

	QSplitter *right = new QSplitter(Vertical, split);
	right->setChildrenCollapsible(false);

	QWidget *up = new QWidget(right);
	QVBoxLayout *layup = new QVBoxLayout(up);
	layup->setMargin(6);
	layup->setSpacing(4);

	QWidget *down = new QWidget(right);
	QVBoxLayout *laydown = new QVBoxLayout(down);
	laydown->setMargin(6);
	laydown->setSpacing(4);

	label = new QLabel(up);
	layup->addWidget(label, 0, AlignTop | AlignHCenter);

	QGroupBox *desc_gb = new QVGroupBox("Description", up);
	desc_gb->setMinimumSize(400, 140);
	layup->addWidget(desc_gb, 10);
	descbox = new QTextEdit(desc_gb);
	descbox->setReadOnly(true);

	QGroupBox *param_gb = new QVGroupBox("Parameters", down);
	param_gb->setMinimumSize(400, 140);
	laydown->addWidget(param_gb, 10);

	parambox = new OQParamBox(0, 2, param_gb);
	parambox->setSelectionMode(QTable::SingleRow);
	parambox->horizontalHeader()->setLabel(0, "Type", 130);
	parambox->setColumnReadOnly(0, true);
	parambox->horizontalHeader()->setLabel(1, "Value", 230);
	parambox->setColumnReadOnly(1, false);
	cur_item = 0;
	loadValueCache();
	changed = false;
	connect(parambox, SIGNAL(valueChanged(int,int)), SLOT(keepValues()));

	QButton *sendbtn = new OQPushButton("&Send", "kpic/mail_send.png", down);
	laydown->addWidget(sendbtn, 0, AlignBottom | AlignHCenter);
	QObject::connect(sendbtn, SIGNAL(clicked()), SLOT(msgSendClicked()));
}


OQMessageCenter::~OQMessageCenter()
{
	keepValues();
	saveValueCache();
}


void
terminate(int sig)
{
	QApplication::postEvent(qApp, new QCloseEvent);
}


int
main(int argc, char **argv)
{
	OQApp app("oqmsg", argc, argv);
	OQMessageCenter win;
	app.setMainWidget(&win);
	win.msgItemSelected(0);
	win.resize(900, 600);
	win.setCaption("Optikus Message Center");
	win.setIcon(OQPixmap("tools/nav.png"));
	win.show();
	signal(SIGINT, terminate);
	signal(SIGTERM, terminate);
	signal(SIGQUIT, terminate);
	return app.exec();
}

