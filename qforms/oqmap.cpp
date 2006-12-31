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

#include <qpainter.h>

#include "oqcommon.h"
#include "oqmap.h"

#include <time.h>
#include <stdlib.h>
#include <iostream>


OQMap::OQMap()
{
	watch.init("oqmap");
	watch.connect();
	watch.optimizeReading("map_x");
	watch.optimizeReading("map_y");
	watch.optimizeReading("map_h");
	watch.optimizeReading("map_time");

	static
	struct { char ch; const char *str; }
	glyph_tr[] = {
		{'0'}, {'1'}, {'2'}, {'3'}, {'4'},{'5'},{'6'},{'7'},{'8'},{'9'},
		{'\'',"apostrophe"}, {'"',"dblquote"}, {'-',"dash"},
		{':',"colon"}, {' ',"space"}, {'.',"dot"}, {',',"comma"},
		{}
	};
	for (int i = 0; glyph_tr[i].ch; i++) {
		char ch = glyph_tr[i].ch;
		char buf[2] = { ch, 0 };
		const char *str = glyph_tr[i].str ?: buf;
		glyphs[ch] = OQPixmap(QString("controls/cntr_") + str + ".png");
	}

	bg = OQPixmap("tiles/photo_800.jpg");
	star = OQPixmap("controls/sputnik6.png");
	pic = bg;
	setErasePixmap(bg);
	setFixedSize(SIZE_X, SIZE_Y);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	trace = QPointArray(TRACEMAX);
	trace_cur = trace_len = 0;
	startTimer(POLL_PERIOD);
	connect(&watch,
		SIGNAL(dataUpdated(const owquark_t&, const QVariant&, long)),
		SLOT(dataUpdated(const owquark_t&, const QVariant&, long)));
}


void
OQMap::dataUpdated(const owquark_t&, const QVariant&, long)
{
	got_data = true;
}


QPoint
OQMap::xform(float rx, float ry)
{
	int x = (int)(rx * 400 / 180. + 400);
	while (x < 0)  x += 800;
	while (x >= 800)  x -= 800;
	int y = (int)(200 - ry * 200 / 90.);
	return QPoint(x, y);
}


static QString&
fmt_lat_long(QString& s, float v)
{
	int n = (int) v;
	int g = (int) ((v - n) * 60);
	if (v < 0)
		s.sprintf("%04d:%02d'", n, -g);
	else
		s.sprintf(" %03d:%02d'", n, g);
	return s;
}


void
OQMap::timerEvent(QTimerEvent *)
{
	if (!got_data)
		return;
	got_data = false;

	bool err_x, err_y;
	float map_x = watch.getFloat("map_x", 0, &err_x);
	float map_y = watch.getFloat("map_y", 0, &err_y);
	if (err_x || err_y)
		return;

	pic = bg;
	QPainter p;
	p.begin(&pic);

	QPoint loc = xform(map_x, map_y);

	trace.setPoint(trace_cur, loc);
	if (trace_len < TRACEMAX)
		++trace_len;
	if (++trace_cur == TRACEMAX)
		trace_cur = 0;

	p.setPen(QColor(220,220,255));
	for (int i = 0; i < trace_len; i++)
		p.drawPoint(trace.point(i));

	p.drawPixmap(loc - QPoint(star.width() / 2, star.height() / 2), star);
	QString msg;

	time_t t = (time_t) watch.getULong("map_time");
	bottomTime(p, t, 1, 1, NULL);
	bottomTime(p, t, 1, 2, "GMT");
	bottomTime(p, t, 1, 3, "CST6CDT");

	t = time(0);
	bottomTime(p, t, 2, 1, NULL);
	bottomTime(p, t, 2, 2, "GMT");
	bottomTime(p, t, 2, 3, "CST6CDT");

	bottomMsg(p, 3, 1, fmt_lat_long(msg, map_x));
	bottomMsg(p, 3, 2, fmt_lat_long(msg, map_y));
	bottomMsg(p, 3, 3, msg.sprintf(" %6.3f", watch.getFloat("map_h")));

	p.end();
	setErasePixmap(pic);
}


int
OQMap::glyphMsg(QPainter&p, int x, int y, const char *msg)
{
	for (const char *s = msg; *s; s++) {
		//std::cerr << "ch='"  << *s << "\n";
		if (glyphs.contains(*s))
			p.drawPixmap(x, y, glyphs[*s]);
		x += 16;
	}
	return x;
}


void
OQMap::bottomMsg(QPainter& p, int c, int r, const QString& msg)
{
	glyphMsg(p, (c==1 ? 240 : c==2 ? 424 : 800) - 16*8, 400 - 18*(4-r), msg);
}


static bool
localtime_tz(const char *tz, const time_t t, struct tm *result)
{
	QString saved_tz(getenv("TZ"));
	if (tz)
		setenv("TZ", tz, 1);
	tzset();
	localtime_r(&t, result);
	if (saved_tz.isNull())
		unsetenv("TZ");
	else
		setenv("TZ", saved_tz, 1);
	return true;
}


void
OQMap::bottomTime(QPainter& p, const time_t t, int c, int r, const char *tz)
{
	struct tm tm;
	localtime_tz(tz, t, &tm);
	QString msg;
	msg.sprintf("%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	bottomMsg(p, c, r, msg);
}


int
main(int argc, char **argv)
{
	OQApplication app("oqmap", argc, argv);
	OQMap win;
	app.setMainWidget(&win);
	win.setCaption("Optikus Map");
	win.setIcon(OQPixmap("kpic/fork.png"));
	win.show();
	return app.exec();
}

