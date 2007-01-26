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

  Picture-rich button.

*/

#ifndef OQPICBUTTON_H
#define OQPICBUTTON_H

#include <qpushbutton.h>

class QBoxLayout;


class OQPicLabel : public QWidget
{
public:

	OQPicLabel(QWidget *parent = 0, const char *name = 0, WFlags f = 0);

	void setLabel(const QString& desc);
	const QString& getLabel() const		{ return desc; }

	void rebuild();
	QSize sizeHint() const		{ return sz; }
	QSize minimumSize() const	{ return sz; }
	QSize getSize() const		{ return sz; }
	void drawContents(QPainter *p, const QRect& rect = QRect());

protected:

	QString desc;
	QWidget * widget;
	//QBoxLayout * blay;
	QSize sz;
	int gap;
};


class OQPicButton : public QPushButton
{
	Q_OBJECT

public:

	OQPicButton(QWidget *parent = 0, bool toggle = false);
	void setLabel(const QString& desc);
	const QString& getLabel() const		{ return label->getLabel(); }
	void resize(int w, int h);
	void setGeometry(int x, int y, int w, int h);
	void setJumpy(bool j)	{ jumpy = j; }

protected:

	void drawButtonLabel(QPainter *p);

	OQPicLabel * label;
	bool gotsize : 1;
	bool jumpy : 1;
};


#endif // OQPICBUTTON_H
