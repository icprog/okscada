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

#include <qlistview.h>
#include <qxml.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qtextedit.h>
#include <qtable.h>
#include <qdict.h>
#include <qmap.h>
#include <qpainter.h>

#include "oqwatch.h"


class OQMsgItem;
class OQParamBox;


struct OQParamType
{
	OQParamType(const char *_type, const char *_name, const char *_format,
				int _minval = -1, int _maxval = -1, bool _issigned = true,
				bool _ischar = false, bool _isfloat = false)
		: type(_type), name(_name), format(_format),
		minval(_minval), maxval(_maxval),
		issigned(_issigned), ischar(_ischar), isfloat(_isfloat)
	{}

	OQParamType()
		: type(0), name(0), format(0),
		minval(-1), maxval(-1), issigned(false), ischar(false), isfloat(false)
	{}

	const char *type, *name, *format;
	int minval, maxval;
	bool issigned, ischar, isfloat;
};

typedef QMap<QString,OQParamType> OQParamMap;


class OQDefaultParamMap : public OQParamMap
{
public:
	void add(const OQParamType& pt) { replace(pt.type, pt); }

	OQDefaultParamMap()
	{
		add(OQParamType("c", "signed char", "%c", -128, 127, true, true));
		add(OQParamType("C", "unsigned char", "%c", 0, 255, false, true));
		add(OQParamType("h", "signed short", "%hd", -32768, 32767, true));
		add(OQParamType("H", "unsigned short", "%hu", 0, 65535, false));
		add(OQParamType("l", "signed long", "%ld", -1, -1, true));
		add(OQParamType("L", "unsigned long", "%lu", -1, -1, false));
		add(OQParamType("f", "float", "%f", -1, -1, true, false, true));
		add(OQParamType("d", "double", "%lf", -1, -1, true, false, true));
	}
};


class OQParamBox : public QTable
{
	Q_OBJECT
public:
	OQParamBox(int rows, int cols, QWidget *parent)
		: QTable(rows, cols, parent)
	{}
};


class OQMessageCenter : public QVBox
{
	Q_OBJECT

public:

	OQMessageCenter();
	virtual ~OQMessageCenter();

	void hashItem(OQMsgItem *item);

	void loadValueCache();
	void saveValueCache();

	QListView *getTree()	{ return tree; }
	OQParamBox *getParamBox()	{ return parambox; }
	QString getValue(int i)	{ return parambox->text(i, 1); }

	static OQDefaultParamMap ptypes;

public slots:

	void msgItemSelected(QListViewItem *);
	void msgSendClicked();
	void keepValues();

protected:

	OQWatch watch;
	QDict<OQMsgItem> itemdict;
	QListView *tree;
	QLabel *label;
	QTextEdit *descbox;
	OQParamBox *parambox;
	OQMsgItem *cur_item;
	bool changed;
};


class OQValueCell : public QTableItem, public QObject
{
public:
	OQValueCell(OQMessageCenter *_msgr, const QString& text)
		: QTableItem(_msgr->getParamBox(), WhenCurrent, text),
		msgr(_msgr)
	{}

	QSize sizeHint();
	void paint(QPainter *p, const QColorGroup &cg, const QRect &cr, bool sel);
	QWidget *createEditor() const;

private:
	OQMessageCenter *msgr;
};


class OQMsgItem : public QListViewItem
{

public:

	OQMsgItem(OQMessageCenter *_msgr, const QString& target)
		: QListViewItem(_msgr->getTree(), target),
		msgr(_msgr), groupno(0), datano(0), changed(false)
	{
		msgr->hashItem(this);
	}

	OQMsgItem(OQMessageCenter *_msgr, OQMsgItem *target, int _groupno,
				const QString& code, const QString& name)
		: QListViewItem(target, target->getTarget(),
						QString("%1").arg(_groupno), code, name),
		msgr(_msgr), groupno(_groupno), datano(0), changed(false)
	{
		msgr->hashItem(this);
	}

	OQMsgItem(OQMessageCenter *_msgr, OQMsgItem *group, int _datano,
				const QString& name)
		: QListViewItem(group, group->getTarget(),
						QString("%1").arg(group->getGroupNo()),
						QString("%1").arg(_datano), QString(name)),
		msgr(_msgr), groupno(group->getGroupNo()), datano(_datano),
		changed(false)
	{
		msgr->hashItem(this);
	}

	QString getTarget()		{ return text(0); }

	int getGroupNo()		{ return groupno; }

	int getDataNo()			{ return datano; }

	QString getKey()		{ return text(4); }

	void setKey(const QString& key)	{ setText(4, key); }

	QString getName()		{ return text(3); }

	const QString& getDesc()			{ return desc; }

	void addDesc(const QString& text)	{ desc.append(text); }

	const QStringList& getParams()		{ return params; }

	void addParam(const QString& type)
	{
		params.append(type);
		values.append("");
	}

	QString& getType(int i)		{ return params[i]; }

	const QString& value(int i)	{ return values[i]; }

	int paramCount()		{ return params.count(); }

	bool setValue(int i, const QString& newval);

	QString takeForCache();
	bool setFromCache(const QString& cache);

	bool isChanged()		{ return changed; }

	bool isSendable()		{ return (this != 0 && datano != 0); }

protected:

	OQMessageCenter *msgr;
	int groupno;
	int datano;
	QString desc;
	QStringList params;
	QStringList values;
	bool changed;
};


class OQMsgTreeParser : public QXmlDefaultHandler
{
public:
	OQMsgTreeParser(OQMessageCenter *_msgr) : msgr(_msgr)		{}

	bool startDocument();
	bool startElement(const QString& nsuri, const QString& localname,
					const QString& qname, const QXmlAttributes& atts);
	bool endElement(const QString& nsuri, const QString& localname,
					const QString& qname);
	bool characters(const QString& ch);
private:
	OQMessageCenter *msgr;
	OQMsgItem *target, *group, *item;
	bool in_desc;
};

