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

  Common utilities.

*/

#ifndef OQCOMMON_H
#define OQCOMMON_H

#include <qpixmap.h>
#include <qstring.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qlayout.h>


class OQPixmap : public QPixmap
{
public:
	OQPixmap(const QString& file);
};


class OQPushButton : public QPushButton
{
public:
	OQPushButton(const char *text, const char *icon = 0, QWidget *parent = 0);
};


class OQSubjectBox : public OQPushButton
{
	Q_OBJECT
public:
	OQSubjectBox(const char *name, QWidget *parent, QBoxLayout *layout = 0);
	void setState(bool online);
	void setState(const QString& state)		{ setState(!state.isEmpty()); }
	static void updateGroupWidget(const QString& all_subjects,
								QWidget *group, QBoxLayout *layout = 0);
	static void updateGroupWidget(const QString& subject, const QString& state,
								QWidget *group, QBoxLayout *layout = 0);
protected slots:
	void subjectClicked();
};


class OQuitFilter
{
public:
	virtual bool canQuit()		{ return true; }
};


class OQApplication : public QApplication
{
	Q_OBJECT

public:
	OQApplication(const char *client_name, int &argc, char **argv);
	void setQuitFilter(OQuitFilter *filter)		{ quitFilter = filter; }

private:
	OQuitFilter *quitFilter;

public slots:
	virtual void quit();

public:
	static OQApplication* oqApp;

	// FIXME: these must be qDir`s
	static QString root_dir;
	static QString forms_home;
	static QString share_home;
	static QString pic_home;
	static QString fmt_home;
	static QString etc_home;
	static QString bin_home;
	static QString log_home;
	static QString log_file;
};

#endif // OQCOMMON_H
