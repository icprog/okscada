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

#include <qtooltip.h>
#include <qobjectlist.h>

#include "oqcommon.h"
#include "oqwatch.h"

#include <optikus/log.h>


/* ======== Pixmap ======== */


// FIXME: cache pixmaps
OQPixmap::OQPixmap(const QString& file, bool substitute)
	: QPixmap()
{
	if (file.isEmpty())
		return;
	QString path = OQApp::pics.filePath(file);
	if (load(path))
		return;
	olog("optikus picture %s not found", path.ascii());
	if (substitute) {
		path = OQApp::pics.filePath("tools/red_ex.png");
		if (!load(path))
			ologAbort("picture substitute %s not found", path.ascii());
	}
}


/* ======== Push Button ======== */


OQPushButton::OQPushButton(const QString& text, const QString& icon,
							QWidget *parent)
	: QPushButton(text, parent)
{
	if (icon) {
		setIconSet(OQPixmap(icon));
	}
}


/* ========  Subject Box ======== */


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
		if (!subjects.contains(boxes->at(i)->name()))
			delete boxes->at(i);
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
