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

#ifndef OCWIDGETS_H
#define OCWIDGETS_H

#include "oqcanvas.h"
#include "oqpiclabel.h"

#include <qdict.h>


#define ORTTI_Label				0xb0100
#define ORTTI_String			0xb0101
#define ORTTI_Pixmap			0xb0102
#define ORTTI_Bar				0xb0103
#define ORTTI_Button			0xb0200
#define ORTTI_Switch			0xb0201
#define ORTTI_Anim				0xb0202
#define ORTTI_Range				0xb0300
#define ORTTI_Ruler				0xb0301
#define ORTTI_Dial				0xb0302
#define ORTTI_Meter				0xb0400
#define ORTTI_BigTank			0xb0401
#define ORTTI_SmallTank			0xb0402
#define ORTTI_RoundTank			0xb0403
#define ORTTI_Thermometer		0xb0404
#define ORTTI_Volume			0xb0405
#define ORTTI_Manometer			0xb0406
#define ORTTI_LED				0xb0407
#define ORTTI_Curve				0xb0500
#define ORTTI_Control			0xb0600
#define ORTTI_Runner			0xb0601
#define ORTTI_Offnom			0xb0602
#define ORTTI_Link				0xb0603
#define ORTTI_SolArray			0xb0701


class QLabel;
class QSlider;


class OCLabel : public OCWidget
{
	Q_OBJECT

public:

	OCLabel(OQCanvas *c);

	int rtti() const	{ return ORTTI_Label; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	QString getText() const	{ return utext; }
	void setText(const QString& text);

	const QFont& getFont() const			{ return font; }
	const QString& getFontName() const		{ return font_name; }
	void setFont(const QFont& font);
	void setFontName(const QString& font);

	const QColor& getColor() const			{ return color; }
	const QString& getColorName() const 	{ return color_name; }
	void setColor(const QColor& color);
	void setColorName(const QString& color);

	void updateDefaults();
	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);

protected:

	QString utext;
	QString vtext;
	QFont font;
	QColor color;
	QString font_name;
	QString color_name;

	void updateGeometry();
};


class OCFormatted : public OCDataWidget
{
	Q_OBJECT

public:

	OCFormatted(OQCanvas *c);

	int rtti() const	{ return -1; }		// not instantiable
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	QString getFormat() const;
	void setFormat(const QString& text);

	void update();

protected:

	QString format;
	QString text;
	bool warned;

	void updateText(bool reading = true);
};


class OCString : public OCFormatted
{
	Q_OBJECT

public:

	OCString(OQCanvas *c);

	int rtti() const	{ return ORTTI_String; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	const QFont& getFont() const			{ return font; }
	const QString& getFontName() const		{ return font_name; }
	void setFont(const QFont& font);
	void setFontName(const QString& font);

	const QColor& getColor() const			{ return color; }
	const QString& getColorName() const 	{ return color_name; }
	void setColor(const QColor& color);
	void setColorName(const QString& color);

	void updateDefaults();
	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);
	void testValueProps(int, double&, double&, double&, double&, bool&);

protected:

	QFont font;
	QColor color;
	QString font_name;
	QString color_name;

	void updateGeometry();
};


class OCPixmap : public OCWidget
{
public:

	OCPixmap(OQCanvas *c);

	int rtti() const	{ return ORTTI_Pixmap; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void drawShape(QPainter& p);

private:

	QString filename;
	QPixmap pixmap;
};


class OCBar : public OCWidget
{
public:

	OCBar(OQCanvas *c);

	int rtti() const	{ return ORTTI_Bar; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void drawShape(QPainter& p);

private:

	QColor color;
};


class OCButton : public OCDataWidget
{
	Q_OBJECT

public:

	OCButton(OQCanvas *c);

	int rtti() const	{ return ORTTI_Button; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setLabel(const QString& label);
	const QString& getLabel() const		{ return button()->getLabel(); }

	void drawShape(QPainter& p);
	void updateValue(const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

protected slots:

	void toggled(bool on);

private:

	bool updating;
	bool down;

	OQPicButton *button() const		{ return (OQPicButton *) childWidget(); }
};


class OCSwitch : public OCDataWidget
{
	Q_OBJECT

public:

	OCSwitch(OQCanvas *c);
	~OCSwitch()		{ clear(); }

	int rtti() const	{ return ORTTI_Switch; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	bool setImages(const QStringList& picnames);
	const QStringList& getImages() const	{ return picnames; }

	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);
	void updateValue(const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

protected:

	void updatePixmap();
	void clear();

protected slots:

	void toggled(bool on);

private:

	int margin;
	bool updating;
	bool down;
	QStringList picnames;
	QPixmap *pics;

	QPushButton *button() const		{ return (QPushButton *) childWidget(); }
};


class OCAnim : public OCDataWidget
{
	Q_OBJECT

public:

	OCAnim(OQCanvas *c);
	~OCAnim()	{ clear(); }

	int rtti() const	{ return ORTTI_Anim; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	bool setImages(const QStringList& picnames);
	const QStringList& getImages() const	{ return picnames; }

	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);
	void updateValue(const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

protected:

	int cur;
	QStringList picnames;
	QPixmap *pics;

	void clear();
	void updateGeometry();
};


class OCRuler : public OCDataWidget
{
	Q_OBJECT

public:

	OCRuler(OQCanvas *c);

	int rtti() const	{ return ORTTI_Ruler; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;
	void polish();

	void setMin(double _min)	{ vmin = _min; update(); }
	double getMin() const		{ return vmin; }
	void setMax(double _max)	{ vmax = _max; update(); }
	double getMax() const		{ return vmax; }
	void setVertical(bool vert);
	bool isVertical() const;

	void update();
	void drawShape(QPainter& p);
	void updateValue(const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

	void move(double x, double y);
	bool resize(double w, double h);

protected:

	void updateGeometry();

protected slots:

	void valueChanged(int value);
	void sliderMoved(int value);

private:

	double vmin, vmax, vcur, vslide;
	int off_x, off_y;

	QSlider *slider() const		{ return (QSlider *) childWidget(); }
};


class OCDial : public OCDataWidget
{
	Q_OBJECT

public:

	OCDial(OQCanvas *c);

	int rtti() const	{ return ORTTI_Dial; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setMin(double _min)	{ vmin = _min; update(); }
	double getMin() const		{ return vmin; }
	void setMax(double _max)	{ vmax = _max; update(); }
	double getMax() const		{ return vmax; }

	void polish();
	bool resize(double w, double h);
	void drawShape(QPainter& p);
	void updateValue(const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

protected:

	void mousePressEvent(QMouseEvent* e);
	void mouseReleaseEvent(QMouseEvent* e);
	void mouseMoveEvent(QMouseEvent* e);
	void updateMouse(int x, int y);

private:

	double vmin, vmax, vcur, vnew, angle;
	QPixmap face;
	int button;
};


class OCMeter : public OCDataWidget
{
	Q_OBJECT

public:

	OCMeter(OQCanvas *c, int ymin, int ymax,
			const QString& subclass, const QString& colorclass = "empty");

	int rtti() const	{ return -1; }		// not directly instantiable
	void attrList(OCAttrList& atts) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setMin(double _min);
	double getMin() const				{ return vmin; }
	void setMax(double _max);
	double getMax() const				{ return vmax; }
	void setColorClass(const QString& colorclass);
	QString getColorClass() const		{ return colorclass; }

	void polish();
	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);
	void updateValue(const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

private:

	QString subclass, colorclass;
	int ymin, ymax;
	double vmin, vmax, vcur;
	QPixmap pxempty, pxfill;
};


class OCBigTank : public OCMeter
{
public:
	OCBigTank(OQCanvas *c) : OCMeter(c, 125, 1, "bigtank", "red")	{}
	int rtti() const	{ return ORTTI_BigTank; }
};


class OCSmallTank : public OCMeter
{
public:
	OCSmallTank(OQCanvas *c) : OCMeter(c, 62, 1, "smalltank", "red")	{}
	int rtti() const	{ return ORTTI_SmallTank; }
};


class OCRoundTank : public OCMeter
{
public:
	OCRoundTank(OQCanvas *c) : OCMeter(c, 44, 1, "roundtank", "red")	{}
	int rtti() const	{ return ORTTI_RoundTank; }
};


class OCThermometer : public OCMeter
{
public:
	OCThermometer(OQCanvas *c) : OCMeter(c, 119, 9, "thermometer", "red")	{}
	int rtti() const	{ return ORTTI_Thermometer; }
};


class OCVolume : public OCMeter
{
public:
	OCVolume(OQCanvas *c) : OCMeter(c, 118, 7, "volume", "red")		{}
	int rtti() const	{ return ORTTI_Volume; }
};


class OCManometer : public OCDataWidget
{
	Q_OBJECT

public:

	OCManometer(OQCanvas *c);

	int rtti() const	{ return ORTTI_Manometer; }
	void attrList(OCAttrList& atts) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setMin(double _min);
	double getMin() const				{ return vmin; }
	void setMax(double _max);
	double getMax() const				{ return vmax; }
	void setSubclass(const QString& _subclass);
	QString getSubclass() const			{ return subclass; }

	void polish();
	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);
	void updateValue(const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

private:

	double vmin, vmax, vcur;
	QString subclass;
	QPixmap bg;
};


class OCLED : public OCFormatted
{
	Q_OBJECT

public:

	OCLED(OQCanvas *c);
	int rtti() const	{ return ORTTI_LED; }

	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setSubclass(const QString& format);
	QString getSubclass() const			{ return subclass; }

	void polish();
	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);
	void testValueProps(int, double&, double&, double&, double&, bool&);

protected:

	QString subclass;
	QDict<QPixmap> glyphs;

	void updateGeometry();
};


class OCCurve : public OCDataWidget
{
	Q_OBJECT

public:

	OCCurve(OQCanvas *c);
	virtual ~OCCurve();

	int rtti() const	{ return ORTTI_Curve; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setMin(double _min);
	double getMin() const		{ return vmin; }
	void setMax(double _max);
	double getMax() const		{ return vmax; }

	void setFreq(int _freq)		{ timerBind(_freq, when); }
	void setWhen(int _when)		{ timerBind(freq, _when); }
	int getFreq() const			{ return freq; }
	int getWhen() const			{ return when; }

	void timerBind(int freq, int when);
	void timerUnbind();

	void polish();
	bool resize(double w, double h);
	void drawShape(QPainter& p);
	void updateValue(int no, const QVariant& value);
	void testValueProps(int, double&, double&, double&, double&, bool&);

protected:

	void timerEvent(QTimerEvent *e);
	virtual void updatePeriodic();

private:

	QPointArray plot;
	int *ys;
	int nx, ny;
	double vmin, vmax;
	int freq, when;
	int timer_id;
};


class OCRunner : public OCWidget
{
	Q_OBJECT

public:

	OCRunner(OQCanvas *c);

	int rtti() const	{ return ORTTI_Runner; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setCommand(const QString& cmd)		{ command = cmd; }
	const QString& getCommand() const		{ return command; }
	void setLabel(const QString& label);
	const QString& getLabel() const		{ return button()->getLabel(); }

	void drawShape(QPainter& p);

protected slots:

	void clicked();

private:

	QString command;

private:

	OQPicButton* button() const		{ return (OQPicButton *) childWidget(); }
};


class OCOffnom : public OCWidget
{
	Q_OBJECT

public:

	OCOffnom(OQCanvas *c);

	int rtti() const	{ return ORTTI_Offnom; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setLabel(const QString& label);
	const QString& getLabel() const		{ return label; }
	void setName(const QString& label);
	const QString& getName() const		{ return name; }
	void setDesc(const QString& label, bool visual_only = false);
	QString getDesc() const;

	void polish();
	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);

	void setOn(bool);
	bool isOn();

	static void assignDescriptions(const QString& filename);

private slots:

	void toggled(bool);

private:

	void updatePixmap();
	QPushButton* button() const	{ return (QPushButton *) childWidget(); }

	QString label, name, desc;
	bool toggling;

	static QString desc_filename;
	static QMap<QString,QString> descriptions;
	static bool got_descriptions;

	static void readDescriptions();
};


class OCLink : public OCWidget
{
	Q_OBJECT

public:

	OCLink(OQCanvas *c);

	int rtti() const	{ return ORTTI_Link; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	void setLabel(const QString& label);
	const QString& getLabel() const		{ return button()->getLabel(); }

	void drawShape(QPainter& p);

protected slots:

	void clicked();

private:

	QString link;

private:

	OQPicButton* button() const		{ return (OQPicButton *) childWidget(); }
};


class OCSolArray : public OCDataWidget
{
	Q_OBJECT

public:

	OCSolArray(OQCanvas *c);

	int rtti() const	{ return ORTTI_SolArray; }
	void attrList(OCAttrList&) const;
	bool setAttr(const QString& name, const QVariant& value);
	QVariant getAttr(const QString& name) const;

	bool setNumber(int no);
	int getNumber() const		{ return no; }

	void polish();
	bool resize(double, double)		{ return false; }	// not resizeable
	void drawShape(QPainter& p);
	void testValueProps(int, double&, double&, double&, double&, bool&);

private:

	int no;
	QPixmap bg, sun;
};


#endif // OCWIDGETS_H
