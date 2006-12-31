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

  Dynamic log display.

*/

#include <qmultilineedit.h>
#include <qfile.h>
#include <qsocketnotifier.h>
#include <qtabwidget.h>
#include <sys/stat.h>


class OQLogView : public QMultiLineEdit
{
	Q_OBJECT

public:
	OQLogView(  const QString& title, QWidget *parent );
	void recharge();

public slots:
	void rescan();

protected:
	QFile file;
	ino_t inode;
	int idle;
	QSocketNotifier *notifier;

	void timerEvent(QTimerEvent *);
	void put(const QString&);
};


class OQLogTab : public QWidget
{
public:
	OQLogTab(const QString& title, QTabWidget *tabs);
};
