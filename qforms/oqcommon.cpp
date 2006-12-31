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

#include <qdir.h>
#include <qtooltip.h>
#include <qstringlist.h>
#include <qobjectlist.h>

#include "oqcommon.h"
#include "oqwatch.h"

#include <stdlib.h>
#include <optikus/log.h>


OQApplication* OQApplication::oqApp;

// FIXME: these must be QDir's, not strings.
QString OQApplication::root_dir;
QString OQApplication::forms_home;
QString OQApplication::share_home;
QString OQApplication::pic_home;
QString OQApplication::fmt_home;
QString OQApplication::etc_home;
QString OQApplication::bin_home;
QString OQApplication::log_home;
QString OQApplication::log_file;


OQApplication::OQApplication(const char *client_name, int &argc, char **argv)
	: QApplication(argc, argv), quitFilter(0)
{
	oqApp = this;

	root_dir = getenv("OPTIKUS_HOME");
	if (root_dir.isEmpty()) {
		QDir dir(applicationDirPath());
		dir.cdUp();
		if ((root_dir = dir.canonicalPath()).isNull())
			root_dir = dir.absPath();
		if (QDir("/home/vit/optikus").exists()) root_dir = "/home/vit/optikus";
		setenv("OPTIKUS_HOME", root_dir.latin1(), 1);
	}

	forms_home = root_dir;
	share_home = root_dir + "/share";
	if (!QDir(share_home).exists() && QDir(root_dir + "/data").exists())
		share_home = root_dir + "/data";
	pic_home = share_home + "/pic";
	fmt_home = share_home + "/forms";
	etc_home = root_dir + "/etc";
	bin_home = root_dir + "/bin";
	if (!QDir(etc_home).exists() && QDir(share_home + "/etc").exists())
		etc_home = share_home + "/etc";
	log_home = root_dir + "/var/log";

	if (NULL == client_name || '\0' == *client_name)
		client_name = "qforms";
	log_file = QDir(log_home).exists() ? log_home + "/qforms.log" : NULL;
	//puts("root="+root_dir+" share="+share_home+" etc="+etc_home
	//	+" pic="+pic_home+" log_home="+log_home+" log_file="+log_file);
	ologOpen(client_name, log_file, 4096);
	ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH | (log_file ? 0 : OLOG_STDOUT));
}


void
OQApplication::quit()
{
	if (!quitFilter || quitFilter->canQuit()) {
		qDebug("quitFilter=%p", quitFilter);
		QApplication::quit();
	}
}


/*				OQPixmap				*/


// FIXME: cache pixmaps
OQPixmap::OQPixmap(const QString& file)
	: QPixmap()
{
	QString path = OQApplication::pic_home + "/" + file;
	if (!load(path)) {
		olog("qforms picture %s not found", path.latin1());
		path = OQApplication::pic_home + "/tools/red_ex.png";
		if (!load(path)) {
			ologAbort("picture substitute %s not found", path.latin1());
		}
	}
}


/*				OQPushButton			*/


OQPushButton::OQPushButton( const char *text, const char *icon,
							QWidget *parent )
	: QPushButton(text, parent)
{
	if (icon) {
		setIconSet(OQPixmap(icon));
	}
}


/*				OQSubjectBox				*/


OQSubjectBox::OQSubjectBox(const char *name, QWidget *parent, QBoxLayout *layout)
	: OQPushButton(name, 0, parent)
{
	setName(name);
	setFocusPolicy(NoFocus);
	setFlat(true);
	setToggleButton(false);
	setState(false);
	if (layout)
		layout->addWidget(this);
	connect(this, SIGNAL(clicked()), SLOT(subjectClicked()));
}


void
OQSubjectBox::subjectClicked()
{
	OQWatch::triggerSubject(name());
}


void
OQSubjectBox::setState(bool online)
{
	setIconSet(OQPixmap(online ? "png/green.png" : "png/red.png"));
	QToolTip::add(this, text() + ": " + (online ? "online" : "offline"));
}


void
OQSubjectBox::updateGroupWidget(const QString& all_subjects,
								QWidget *group, QBoxLayout *layout)
{
	QStringList subjects(QStringList::split(' ', all_subjects));
	QObjectList *boxes = group->queryList("OQSubjectBox");
	for (int i = boxes->count() - 1; i >= 0; i--) {
		if (!subjects.contains(boxes->at(i)->name())) {
			delete boxes->at(i);
		}
	}
	delete boxes;
	for (unsigned j = 0; j < subjects.count(); j++) {
		if (!group->child(subjects[j], "OQSubjectBox")) {
			OQSubjectBox *newbox = new OQSubjectBox(subjects[j], group, layout);
			newbox->show();
		}
	}
}


void
OQSubjectBox::updateGroupWidget(const QString& subject, const QString& state,
								QWidget *group, QBoxLayout *layout)
{
	OQSubjectBox *box = (OQSubjectBox *) group->child(subject, "OQSubjectBox");
	if (box) {
		box->setState(state);
	}
}

