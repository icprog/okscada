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

  Optikus Message Viewer.

*/

#include <qwidget.h>
#include <qlineedit.h>
#include <qmultilineedit.h>
#include <qprocess.h>


class OQEcho : public QWidget
{
	Q_OBJECT
public:
	OQEcho();
	virtual ~OQEcho();
	void command(const QString& cmd);
	void output(const QString& str);
	void restart();
	void kill();

public slots:
	void commandEntered();
	void helpClicked();

protected:
	void timerEvent(QTimerEvent *e);

	QProcess *proc;
	QLineEdit *line;
	QMultiLineEdit *scroll;
};

