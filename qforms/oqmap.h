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

  Optikus Map.

*/

#include <qwidget.h>
#include <qpixmap.h>
#include <qpointarray.h>
#include <qpainter.h>

#include "oqwatch.h"

#include <time.h>		/* for time_t */


class OQMap : public QWidget
{
	Q_OBJECT
public:
	static const int TRACEMAX = 400;
	static const int SIZE_X = 800;
	static const int SIZE_Y = 400;
	static const int POLL_PERIOD = 100;
	OQWatch watch;
	QPointArray trace;
	int trace_cur, trace_len;
	QPixmap pic;
	QPixmap bg;
	QPixmap star;
	bool got_data;
	QMap<QChar, QPixmap> glyphs;
public:
	OQMap();
	static QPoint xform(float rx, float ry);
	int glyphMsg(QPainter& p, int x, int y, const char *msg);
	void bottomMsg(QPainter& p, int c, int r, const QString& msg);
	void bottomTime(QPainter& p, const time_t t, int c, int r, const char *tz);
public slots:
	void dataUpdated(const owquark_t&, const QVariant& value, long time);
protected:
	void timerEvent(QTimerEvent *);
};

