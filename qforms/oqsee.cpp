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

  Optikus Data Viewer.

*/

#include <qpopupmenu.h>
#include <qmenubar.h>
#include <qlayout.h>
#include <qvgroupbox.h>
#include <qhgroupbox.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qstatusbar.h>
#include <qscrollview.h>
#include <qhbox.h>
#include <qpainter.h>
#include <qstyle.h>
#include <qfiledialog.h>
#include <qtooltip.h>

#include "oqcommon.h"
#include "oqsee.h"

#include <config.h>				/* for package requisites */
#include <optikus/conf-mem.h>	/* for oxbcopy */
#include <optikus/log.h>


#define qDebug(x...)
#if 0
#define qDebugData(x...)	qDebug(x)
#else
#define qDebugData(x...)
#endif

const QFont VALUE_FONT("Courier New", 12, QFont::Bold);

#define ROW_GROW_CHUNK		40
#define OQSEE_LOG_FILE		"oqsee.log"
#define OQSEE_FILE_EXT		"oqs"
#define OQSEE_FILE_FILTER	"oqsee files (*.oqs);;any files (*)"

int OQSeeView::next_no = 12300000;


void
OQSeeView::fileOpen()
{
	if (changed && numRows()) {
		if (!canDo(false,
				"Your changes will be lost !\n\nAre you sure ?"))
			return;
		fileSave();
	}
	QString s = QFileDialog::getOpenFileName(
					filename, OQSEE_FILE_FILTER, this, "Open", "Load List");
	if (s.isEmpty())
		return;
	QFile f(s);
	if (!f.open(IO_ReadOnly)) {
		setStatus("cannot open " + s);
		return;
	}

	removeAll(true);
	// FIXME: workaround for a bug in the owatch library
	// Without this delay the server will not be notified about
	// removed monitors.
	watch.idle(100);

	filename = s;
	while (f.readLine(s, 1024) != -1) {
		s = s.stripWhiteSpace().simplifyWhiteSpace();
		if (s.isEmpty() || s.startsWith(";") || s.startsWith("#"))
			continue;
		QString no = s.section(' ', 0, 0);
		QString name = s.section(' ', 1, 1);
		QString fmt  = s.section(' ', 2, 2).lower();
		QString path = s.section(' ', 3, 3);
		if (name.isEmpty()) {
			qWarning("skip invalid line: " + s);
			continue;
		}
		bool ok;
		no.toUInt(&ok);
		bool warn = !ok;
		if (path.isEmpty()) {
			path = name;
			warn = true;
		}
		if (fmt != "dec" && fmt != "hex")
			warn = true;
		if (warn)
			qWarning("wrong format: " + s);
		appendItem(name, (fmt == "hex"));
	}
	f.close();
	changed = false;
	selectRow(0);
	setStatus("opened " + filename);
}


void
OQSeeView::fileSave()
{
	if (!numRows()) {
		setStatus("list is empty");
		return;
	}
	if (filename.isEmpty())
		fileSaveAs();
	else if (!QFile(filename).exists()
			|| canDo(false,
					"File already exists:\n"+filename+"\nRewrite it ?"))
		saveTo(filename);
}


void
OQSeeView::fileSaveAs()
{
	if (!numRows()) {
		setStatus("list is empty");
		return;
	}
	QString s = QFileDialog::getSaveFileName(
					filename, OQSEE_FILE_FILTER, this, "Save", "Save List");
	if (s.isEmpty())
		return;
	if (QFileInfo(s).extension().isEmpty())
		s = s + "." + OQSEE_FILE_EXT;
	saveTo(s);
}


bool
OQSeeView::saveTo(const QString& name)
{
	QFile file(name);
	if (!file.open(IO_WriteOnly)) {
		setStatus("cannot save to " + name);
		return false;
	}
	QTextStream out(&file);
	out << "# oqsee v.1" << "\n"
		<< "# saved on "
		<< QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss")
		<< "\n";

	for (int i = 0; i < numRows(); i++) {
		Var *v = byrow[i];
		owquark_t inf;
		if (v->id)
			watch.getInfo(v->id, inf);
		else
			watch.getInfo(v->desc, inf);
		out << i << " " << v->desc << " " << (v->isHex() ? "hex":"dec")
			<< " " << inf.desc << "\n";
	}
	file.close();
	filename = name;
	changed = false;
	setStatus("saved " + name);
	return true;
}


bool
OQSeeView::canQuit()
{
	qDebug("canQuit: changed=%d numRows=%d", changed, numRows());

	if (write_repeater->isActive()) {
		if (!canDo(false,
				"You are trying to quit the viewer,\n"
				"but background writing is not completed yet.\n"
				"Cancel background writing ?"))
			return false;
		else
			write_repeater->cancel();
	}

	if (!changed
		|| numRows() == 0
		|| canDo(false, "You are trying to quit the viewer.\n\n"
						"All your modifications will be LOST !\n\n"
						"Are you sure ?")) {
		mon_repeater->cancel();
		return true;
	}
	return false;
}


void
OQSeeView::helpAbout()
{
	QMessageBox::about(this, "About OqSee",
		"OqSee " PACKAGE_VERSION ", the Optikus data viewer\n\n"
		PACKAGE_BUGREPORT "\n\n" PACKAGE_COPYRIGHT "\n\n" PACKAGE_WEBSITE);
}


void
OQSeeView::viewStateBox(bool on)
{
	if (on)
		stategroup->show();
	else
		stategroup->hide();
}


void
OQSeeView::viewHex(bool on)
{
	if (currentRow() >= 0 && currentRow() < numRows()) {
		Var *v = byrow[currentRow()];
		setHexData(v, on);
		viewHexAc->setOn(v->isHex());
		updateCell(currentRow(), 1);
	} else {
		viewHexAc->setOn(false);
	}
}


void OQSeeView::setHexData(Var * v, bool on)
{
	if (!v->id)
		return;
	if (!on) {
		v->hex = 0;
		return;
	}
	owquark_t inf;
	if (!watch.getInfo(v->id, inf))
		return;
	static QString hexable("bBhHiIlL");
	int oldhex = v->hex;
	v->hex = hexable.contains(inf.type) ? inf.len * 2: 0;
	if (v->hex != oldhex) {
		qDebug("changed hex from %d to %d", oldhex, v->hex);
		changed = true;
	}
}


void
OQSeeView::setLogging(bool on)
{
	if (on == logging)
		return;
	if (on) {
		logging = true;
		ologSetOutput(0, OQSEE_LOG_FILE, 0);
		ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
		olog("-------- LOGGING BEGIN --------");
		logAllValues();
		setStatus(QString("logging to \"%1\" started").arg(OQSEE_LOG_FILE));
	} else {
		logAllValues();
		olog("-------- LOGGING END --------");
		ologSetOutput(0, NULL, 0);
		ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_STDOUT | OLOG_FLUSH);
		setStatus(QString("logging stopped, see \"%1\".").arg(OQSEE_LOG_FILE));
		logging = false;
	}
}


void
OQSeeView::playStart(bool on)
{
	playStopAc->setOn(!on);
	watch.blockMonitoring(!on, on);
	watch.enableMonitoring(on);
	setStatus(on ? "started" : "paused");
	ologIf(logging, "-------- %s --------", on ? "RESUME" : "PAUSE");
	if (!on && write_repeater->isActive()) {
		if (canDo(false,
				"Not all values are written yet.\nCancel writing ?")) {
			write_repeater->cancel();
		}
	}
}


void
OQSeeView::playStop(bool on)
{
	playStartAc->setOn(!on);
}


void
OQSeeView::newCommand()
{
	QString cmd = inedit->text().stripWhiteSpace();
	qDebug(QString("newCommand: \"%1\"").arg(cmd));
	if (cmd.isEmpty())
		return;
	if (cmd.find('=') != -1) {
		QString name = cmd.section('=', 0, 0).stripWhiteSpace();
		QString val = cmd.section('=', 1).stripWhiteSpace();
		if (name.isEmpty()) {
			setStatus("syntax error");
			return;
		}
		if (val.isEmpty() || val == "?") {
			readValue(name);
		} else {
			writeValue(name, val);
		}
	} else {
		if (mon_repeater->isRepeater(cmd)) {
			if (mon_repeater->isActive()) {
				if (!canDo(false,
						"Another repeater is running.\nAbort it ?"))
					return;
				mon_repeater->cancel();
			}
			qDebug("monitor repeater " + cmd);
			mon_repeater->initMonitor(cmd, viewDefHexAc->isOn());
		} else {
			appendItem(cmd, viewDefHexAc->isOn());
		}
	}
}


int
OQSeeView::readValue(const QString& name, int *bg_p)
{
	setStatus("reading " + name + " ...");
	int no = ++next_no;
	int bg = watch.readBg(name, no, -1);
	if (bg_p)
		*bg_p = bg;
	return no;
}


void
OQSeeView::readDone(long param, const owquark_t& inf,
					const QVariant& val, int err)
{
	if (err == OK)
		setStatus(QString("read: ") + inf.desc + " = " + val.toString());
	else
		setStatus(QString("read ") + inf.desc + " " + watch.errorString(err));
}


int
OQSeeView::writeValue(const QString& name, const QString& val, int *bg_p)
{
	if (bg_p)
		*bg_p = 0;
	setStatus("writing " + name + " = " + val + " ...");
	QString v(val);
	if (v.startsWith("\"")) {
		if (!v.endsWith("\"")) {
			setStatus("syntax error");
			return 0;
		}
		v = v.mid(1, v.length() - 2);
	} else if (v.startsWith("'")) {
		if (!v.endsWith("'")) {
			setStatus("syntax error");
			return 0;
		}
		v = v.mid(1, v.length() - 2);
	}
	if (write_repeater->isRepeater(name)) {
		if (write_repeater->isActive()) {
			if (!canDo(false, "Another repeater is running.\nAbort it ?"))
				return 0;
			write_repeater->cancel();
		}
		qDebug("write repeater " + name);
		write_repeater->initWrite(name, val);
		return 0;
	} else {
		qDebug("single write " + name);
		int no = ++next_no;
		int bg = watch.writeBg(name, v, no, -1);
		if (bg_p)
			*bg_p = bg;
		return no;
	}
}


void
OQSeeView::writeDone(long param, const owquark_t& inf, int err)
{
	setStatus(QString("write ") + inf.desc + " " + watch.errorString(err));
}


int
OQSeeView::appendItem(const QString& name, bool hex, int *bg_p)
{
	Var *v = new Var;
	v->no = ++next_no;
	v->id = 0;
	v->next = 0;
	v->desc = name;
	v->row = numRows();
	v->hex = hex ? 1 : 0;
	v->stamp = 0;

	qDebug("start adding row=%d no=%d name=\"%s\"",
			v->row, v->no, (const char *) name);
	setStatus("searching " + name + "...");

	if (v->row >= (int) byrow.size())
		byrow.resize(v->row + ROW_GROW_CHUNK);

	byno.insert(v->no, v);
	byrow[v->row] = v;
	v->bg_id = watch.monitorBg(name, v->no, -1);
	if (bg_p)
		*bg_p = v->bg_id;
	setNumRows(v->row + 1);
	selectRow(v->row);
	changed = true;
	return v->no;
}


void
OQSeeView::monitorDone(long param, const owquark_t& inf, int err)
{
	setStatus(QString("%1 %2")
				.arg(inf.desc).arg(watch.errorString(err)));
	int no = (int) param;
	if (!byno.contains(no)) {
		qDebug(QString("monitorDone: no=%1 not found").arg(no));
		return;
	}
	Var *v = byno[no];
	qDebug("update: id=%lu row=%d desc=%s no=%d err=%s", inf.ooid, v->row,
			(const char *) v->desc, no, (const char *) watch.errorString(err));
	v->bg_id = 0;
	if (err == OK) {
		v->id = inf.ooid;
		Var *v_id = byid[inf.ooid];
		if (v_id) {
			v->next = v_id->next;
			v_id->next = v;
		} else {
			v->next = 0;
			byid.replace(inf.ooid, v);
		}
		qDebug("add %s to id %lu", v_id ? "next" : "first", v->id);
		setHexData(v, v->isHex());
	} else {
		v->hex = 0;
		v->ermes = watch.errorString(err);
		v->next = 0;
	}
	updateCell(v->row, 2);
}


void
OQSeeView::swapRows(int ra, int rb)
{
	int nr = numRows();
	if (ra < 0 || ra >= nr || rb < 0 || rb >= nr)
		return;
	Var *va = byrow[ra];
	Var *vb = byrow[rb];
	va->row = rb;
	byrow[rb] = va;
	vb->row = ra;
	byrow[ra] = vb;
	for (int c = 0; c <= 2; c++)
		updateCell(ra, c);
	for (int c = 0; c <= 2; c++)
		updateCell(rb, c);
	setCurrentCell(rb, 1);
	setStatus(QString("move %1 to row %2").arg(va->desc).arg(rb));
}


void
OQSeeView::selectRow(int row)
{
	if (row >= 0 && row < numRows())
		setCurrentCell(row, 1);
}


bool
OQSeeView::areYouSure(bool sure, const QString& text)
{
	if (sure)
		return true;
	int ans = QMessageBox::warning(this, "Are you sure ?", text,
						QMessageBox::Yes | QMessageBox::Default,
						QMessageBox::No | QMessageBox::Escape);
	return (ans == QMessageBox::Yes);
}


void
OQSeeView::removeRow(int row, bool sure)
{
	int nrows = numRows();
	if (row < 0 || row >= nrows) {
		qDebug("cannot delete row %d", row);
		return;
	}

	if (!canDo(sure, QString("Really remove row %1 ?").arg(row)))
		return;

	qDebug("deleting row %d ..", row);
	Var *v = byrow[row];

	if (v->bg_id)
		watch.cancelBg(v->bg_id);

	if (v->id) {
		watch.removeMonitor(v->id);
		Var *v_id = byid[v->id];
		if (v_id == v) {
			if (v->next) {
				byid.replace(v->id, v->next);
				qDebug("remove first of id %lu", v->id);
			} else {
				byid.remove(v->id);
				qDebug("remove last of id %lu", v->id);
			}
		} else {
			while (v_id) {
				if (v_id->next == v) {
					v_id->next = v->next;
					qDebug("remove some of id %lu", v->id);
					break;
				}
				v_id = v_id->next;
			}
		}
	}

	byno.remove(v->no);

	for (int r = row; r < nrows - 1; r++) {
		byrow[r] = byrow[r+1];
		byrow[r]->row --;
	}

	int crow = currentRow();
	QTable::removeRow(row);
	// Workaround for Qt jumping to the bottom row.
	setCurrentCell(crow == nrows - 1 ? nrows - 1 : crow, 1);

	delete v;
	changed = true;
	setStatus(QString("row %1 deleted").arg(row + 1));
}


void
OQSeeView::removeBroken()
{
	qDebug("removing broken items...");
	int count = 0;
	QMap<int,VarPtr>::iterator it = byno.begin();
	bool sure = false;
	int crow = currentRow();
	while (it != byno.end()) {
		Var *v = *it;
		++it;
		if (v->bg_id || !v->ermes.isEmpty()) {
			if (!canDo(sure, "Really remove broken items ?"))
				return;
			sure = true;
			qDebug("broken: %s (row=%d)", (const char *) v->desc, v->row);
			removeRow(v->row);
			if (v->row < crow)
				crow--;
			count++;
		}
	}
	setCurrentCell(crow, 1);
	changed = true;
	setStatus(count ? QString("removed %1 broken items").arg(count)
					: "all items OK");
}


void
OQSeeView::removeAll(bool sure)
{
	if (numRows() == 0) {
		if (mon_repeater->isActive()) {
			if (canDo(sure, "More items would be added to the list.\n"
							"Stop the repeater ?"))
				mon_repeater->cancel();
		}
		return;
	}

	if (!canDo(sure,
				"Are you ready to clear\n\nALL\n\nitems in the list ?"))
		return;

	if (mon_repeater->isActive())
		mon_repeater->cancel();

	qDebug("remove all items");
	for (QMap<int,VarPtr>::iterator it = byno.begin(); it != byno.end(); ++it) {
		Var *v = *it;
		if (v->bg_id)
			watch.cancelBg(v->bg_id);
		delete v;
	}
	setNumRows(0);
	byno.clear();
	byrow.truncate(0);
	byid.clear();
	watch.removeAllMonitors();
	changed = true;
	setStatus("list cleared");
	ologIf(logging, "-------- CLEAR --------");
}


void
OQSeeView::currentRowChanged(int row, int col)
{
	qDebug("current row = %d", row);
	if (row >= 0 && row < numRows()) {
		Var *v = byrow[row];
		viewHexAc->setOn(v->isHex());
		inedit->setText(v->desc);
	} else {
		viewHexAc->setOn(false);
	}
}


void
OQSeeView::paintCell(QPainter *p, int row, int col, const QRect& cr,
					bool sel, const QColorGroup &cg)
{
	p->setClipRect(cellRect(row, col), QPainter::CoordPainter);

	int w = cr.width();
	int h = cr.height();

	Var *v = byrow[row];
	qDebugData("paint(%d,%d): v=%p desc=%s",
				row, col, v, v?(const char *)v->desc : "?");
	QString text;
	owquark_t inf;

	switch (col) {
		case 0:
			text = v->desc;
			break;
		case 1:
			text = v->id ? v->toString() : "--";
			break;
		case 2:
			if (!v->ermes.isEmpty()) {
				text = v->ermes;
			} else if (!v->id) {
				text = "not available";
			} else if (watch.getInfo(v->id, inf)) {
				text.sprintf("id=%03d type=%c ptr=%c seg=%c len=%-2d desc=%s",
							(int) inf.ooid, inf.type, inf.ptr, inf.seg,
							inf.len, inf.desc);
			} else {
				text = "no information";
			}
			break;
		default:
			text = "?";
			break;
	}

	p->fillRect(0, 0, w, h, cg.brush(sel ? QColorGroup::Highlight
										: QColorGroup::Base));
	p->setPen(sel ? cg.highlightedText() : cg.text());
	p->setFont(VALUE_FONT);
    p->drawText(2, 0, w - 2 - 4, h, AlignLeft | AlignVCenter, text);

	int gridc = style().styleHint(QStyle::SH_Table_GridLineColor, this);
	const QPalette &pal = palette();
	if (gridc != -1 && (cg == colorGroup() || cg == pal.disabled()
						|| cg == pal.inactive())) {
		p->setPen((QRgb)gridc);
	} else {
		p->setPen(cg.mid());
	}
	w--;
	h--;
	p->drawLine(w, 0, w, h);
	p->drawLine(0, h, w, h);

	p->setClipping(false);
}


void
OQSeeView::dataUpdated(const owquark_t& inf, const QVariant& val, long stamp)
{
	Var *v = byid[inf.ooid];
	bool log1 = logging;
	while (v) {
		qDebugData("data(row=%d id=%lu): %s = %s", v->row, inf.ooid,
					(const char *) inf.desc, (const char *) val.toString());
		v->stamp = stamp;
		if (log1) {
			logOneValue(v, &val);
			log1 = false;	// log once
		}
		updateCell(v->row, 1);
		v = v->next;
	}
}


void
OQSeeView::logAllValues()
{
	if (numRows() > 0) {
		olog("-------- CUT begins --------");
		for (int r = 0; r < numRows(); r++)
			logOneValue(byrow[r]);
		olog("-------- CUT ends --------");
	}
}


void
OQSeeView::logOneValue(Var *v, const QVariant * val)
{
	ulong stamp = v->stamp;
	int sec = (stamp / 1000000) % 1000;
	int msec = (stamp / 1000) % 1000;
	int usec = stamp % 1000;
	olog("[%03d.%03d.%03d] [%-24s] [%s]",
		sec, msec, usec, (const char *) v->desc,
		(const char *) v->toString(val));
}


void
OQSeeView::beginUpdates()
{
	//setUpdatesEnabled(false);
}


void
OQSeeView::endUpdates()
{
	//setUpdatesEnabled(true);
}


void
OQSeeView::subjectsUpdated(const QString& all,
						const QString& subject, const QString& state)
{
	qDebug("subject [" + subject + "] : [" + state + "]");
	OQSubjectBox::updateGroupWidget(all, statebox);
	OQSubjectBox::updateGroupWidget(subject, state, statebox);
	if (subject == "*") {
		bool on = !state.isEmpty();
		inbtn->setPixmap(OQPixmap(on ? "kpic/button_ok.png" : "kpic/apply.png"));
		setStatus(QString("server is %1").arg(on ? "active" : "off"));
	}
}


void
OQSeeView::setStatus(const QString &msg)
{
	statusbar->message(msg);
}


/* ======== Repeater ======== */


OQSeeRepeater::OQSeeRepeater(OQSeeView *_view, OQWatch *_watch)
	: view(_view), watch(_watch)
{
	active = false;
	cur_no = 0;
	cancel();
	connect(watch,
			SIGNAL(monitorDone(long, const owquark_t&, int)),
			SLOT(monitorDone(long, const owquark_t&, int)));
	connect(watch,
			SIGNAL(writeDone(long, const owquark_t&, int)),
			SLOT(writeDone(long, const owquark_t&, int)));
}




bool
OQSeeRepeater::parse(const QString& desc, bool onlycheck)
{
	char *tmp_fmt = new char[desc.length() + 1];
	QString ermes = "syntax error";
	const char *s = desc;
	char *d = tmp_fmt;
	int i = 0;

	while (*s)
	{
		if (*s != '[') {
			if (*s == '?' || *s == ':')
				goto FAIL;
			*d++ = *s++;
			continue;
		}
		const char *p = ++s;
		int n = 0;
		while (*p >= '0' && *p <= '9') {
			n = n * 10 + (*p - '0');
			p++;
		}
		if (p == s)
			goto FAIL;
		int imin = n;
		int imax = n;
		if (*p == ':' || (*p == '.' && p[1] == '.')) {
			s = ++p;
			if (*p == '.')
				s = ++p;
			n = 0;
			while (*p >= '0' && *p <= '9') {
				n = n * 10 + (*p - '0');
				p++;
			}
			if (p == s)
				goto FAIL;
			imax = n;
		}
		if (*p != ']')
			goto FAIL;
		s = p + 1;
		if (imin > imax) {
			ermes = QString("invalid range: %1 > %2").arg(imin).arg(imax);
			goto FAIL;
		}
		if (imin == imax) {
			d += sprintf(d, "[%d]", imin);
			continue;
		}
		if (i >= OQSEE_REP_MAX_IDX) {
			ermes = "too many indexes";
			goto FAIL;
		}
		if (!onlycheck) {
			idx_min[i] = imin;
			idx_max[i] = imax;
		}
		i++;
		strcpy(d, "[%d]");
		d += 4;
	}
	*d = '\0';
	if (onlycheck)
		return i ? true : false;
	format = QString(tmp_fmt);
	delete tmp_fmt;
	idx_num = i;
	while (i < OQSEE_REP_MAX_IDX) {
		idx_min[i] = idx_max[i] = 0;
		i++;
	}
	for (i = 0; i < OQSEE_REP_MAX_IDX; i++)
		idx_cur[i] = idx_min[i];
	if (idx_num > 0)
		idx_cur[idx_num - 1] -= 1;
	return true;
  FAIL:
	delete tmp_fmt;
	if (onlycheck)
		return true; // This IS a repeater, trigger error later.
	setStatus(ermes);
	return false;
}


bool
OQSeeRepeater::initMonitor(const QString& desc, bool _hex)
{
	cancel();
	if (!parse(desc))
		return false;
	hex = _hex;
	is_write = false;
	active = true;
	next();
	return true;
}


bool
OQSeeRepeater::initWrite(const QString& desc, const QString& _val)
{
	cancel();
	if (!parse(desc))
		return false;
	value = _val;
	is_write = true;
	active = true;
	next();
	return true;
}


bool
OQSeeRepeater::next()
{
	while (true) {
		if (!active)
			return false;
		for (int i = idx_num - 1; i >= 0; i--) {
			if (++idx_cur[i] <= idx_max[i])
				break;
			if (i > 0)
				idx_cur[i] = idx_min[i];
		}
		if (idx_cur[0] > idx_max[0]) {
			setStatus(QString("%1 %2 OK")
						.arg(is_write ? "write" : "add").arg(format));
			active = false;
			return false;
		}
		QString name;
		name.sprintf(format, idx_cur[0], idx_cur[1], idx_cur[2], idx_cur[3]);

		last_no = 0;
		last_err = 0;
		cur_bg = 0;
		if (is_write)
			cur_no = view->writeValue(name, value, &cur_bg);
		else
			cur_no = view->appendItem(name, hex, &cur_bg);
		if (!cur_no)
			break;
		if (last_no == cur_no) {
			if (last_err == OK) {
				continue;
			} else {
				break;
			}
		}
		return true;
	}
	cancel();
	return false;
}


void
OQSeeRepeater::done(long param, int err)
{
	if (!active)
		return;
	qDebug("repeater_done: param=%ld cur_no=%d err=%d", param, cur_no, err);
	if (param == cur_no) {
		if (err == OK) {
			next();
		} else {
			cancel();
		}
	} else {
		last_no = param;
		last_err = err;
	}
}


void
OQSeeRepeater::cancel()
{
	if (active) {
		qDebug("cancel repeater %p", this);
		if (cur_bg)
			watch->cancelBg(cur_bg);
	}
	active = false;
	cur_bg = 0;
	cur_no = 0;
	last_no = 0;
	last_err = 0;
}


/* ======== Startup ======== */


OQSeeView::OQSeeView(QWidget *parent)
	: QTable(0, 3, parent)
{
	items.setAutoDelete(true);
	widgets.setAutoDelete(true);
	setReadOnly(true);
	setSelectionMode(SingleRow);
	QHeader *hh = horizontalHeader();
	hh->setLabel(0, "Name", 150);
	hh->setLabel(1, "Value", 120);
	hh->setLabel(2, "Information", 700);

	watch.init("oqsee");
	watch.connect();
	connect(&watch,
		SIGNAL(subjectsUpdated(const QString&, const QString&, const QString&)),
		SLOT(subjectsUpdated(const QString&, const QString&, const QString&)));
	connect(&watch,
			SIGNAL(monitorDone(long, const owquark_t&, int)),
			SLOT(monitorDone(long, const owquark_t&, int)));
	connect(&watch,
			SIGNAL(writeDone(long, const owquark_t&, int)),
			SLOT(writeDone(long, const owquark_t&, int)));
	connect(&watch,
			SIGNAL(readDone(long, const owquark_t&, const QVariant&, int)),
			SLOT(readDone(long, const owquark_t&, const QVariant&, int)));
	connect(&watch,
			SIGNAL(dataUpdated(const owquark_t&, const QVariant&, long)),
			SLOT(dataUpdated(const owquark_t&, const QVariant&, long)));
	connect(&watch, SIGNAL(beginUpdates()), SLOT(beginUpdates()));
	connect(&watch, SIGNAL(endUpdates()), SLOT(endUpdates()));
	connect(this, SIGNAL(currentChanged(int,int)),
			SLOT(currentRowChanged(int,int)));

	changed = false;
	logging = false;
	mon_repeater = new OQSeeRepeater(this, &watch);
	write_repeater = new OQSeeRepeater(this, &watch);
}


QAction *
OQSeeView::newAction(QPopupMenu *menu, const char *text, const char *icon,
					const char *slot, QKeySequence accel, bool toggle)
{
	QAction *action;
	if (icon && *icon)
		action = new QAction(OQPixmap(icon), text, accel, this);
	else
		action = new QAction(text, accel, this);
	action->setStatusTip("");	// disable status tips
	if (toggle) {
		action->setToggleAction(true);
		if (slot)
			connect(action, SIGNAL(toggled(bool)), slot);
	} else {
		if (slot)
			connect(action, SIGNAL(activated()), slot);
	}
	if (menu) {
		action->addTo(menu);
	}
	return action;
}


void
OQSeeView::setupWidgets(QWidget *panel, QStatusBar *_statusbar)
{
	QBoxLayout *vbox = (QBoxLayout *) panel->layout();

	QHGroupBox *ingroup = new QHGroupBox("Commands", panel);
	vbox->addWidget(ingroup);
	inedit = new QLineEdit(ingroup);
	inbtn = new QPushButton(ingroup);
	int h = inedit->height();
	inbtn->setMinimumSize(h, h);
	inbtn->setPixmap(OQPixmap("kpic/apply.png"));
	inbtn->setFocusPolicy(NoFocus);

	QHGroupBox *stategroup = new QHGroupBox("Subjects", panel);
	vbox->addWidget(stategroup, 1, AlignCenter | AlignBottom);
	QScrollView *statescroll = new QScrollView(stategroup);
	this->stategroup = stategroup;
	statebox = new QHBox;
	OQSubjectBox *hubbox = new OQSubjectBox("*", statebox);
	statescroll->addChild(statebox);
	statescroll->setResizePolicy(QScrollView::AutoOneFit);
	h = hubbox->height() + statescroll->horizontalScrollBar()->height();
	statescroll->setMaximumHeight(h);
	statescroll->setVScrollBarMode(QScrollView::AlwaysOff);
	stategroup->hide();

	connect(inedit, SIGNAL(returnPressed()), SLOT(newCommand()));
	connect(inbtn, SIGNAL(clicked()), SLOT(newCommand()));

	statusbar = _statusbar;
	statusbar->clear();
	inedit->setFocus();
}


void
OQSeeView::setupActions(QMenuBar *menubar, QToolBar *toolbar)
{
	this->statusbar = statusbar;

	QPopupMenu *fileMenu = new QPopupMenu(this);
	menubar->insertItem("&File", fileMenu);
	QPopupMenu *editMenu = new QPopupMenu(this);
	menubar->insertItem("&Edit", editMenu);
	QPopupMenu *viewMenu = new QPopupMenu(this);
	menubar->insertItem("&View", viewMenu);
	QPopupMenu *helpMenu = new QPopupMenu(this);
	menubar->insertItem("&Help", helpMenu);

	QAction *fileOpenAc = newAction(fileMenu, "&Open", "kpic/fileopen.png",
									SLOT(fileOpen()), CTRL | Key_O);
	QAction *fileSaveAc = newAction(fileMenu, "&Save", "kpic/filesave.png",
									SLOT(fileSave()), CTRL | Key_S);
	newAction(fileMenu, "Save &as...", "kpic/filesaveas.png", SLOT(fileSaveAs()));
	fileMenu->insertSeparator();
	QAction *fileQuitAc = newAction(fileMenu, "&Quit", "kpic/exit.png",
									SLOT(fileQuit()), CTRL | Key_Q);

	QAction *removeCurrentAc = newAction(editMenu,
									"D&elete current item", "kpic/editcut.png",
									SLOT(removeCurrent()), Key_Delete);
	QAction *removeBrokenAc = newAction(editMenu, "Remove broken items",
									"kpic/editdelete.png",
									SLOT(removeBroken()), CTRL | Key_B);
	QAction *removeAllAc = newAction(editMenu, "&Clear list",
									"kpic/editclear.png",
									SLOT(removeAll()), CTRL | Key_C);
	editMenu->insertSeparator();
	QAction *moveUpAc = newAction(editMenu,
									"Move item &up", "kpic/2uparrow.png",
									SLOT(moveUp()), CTRL | Key_Up);
	QAction *moveDownAc = newAction(editMenu,
									"Move item &down", "kpic/2downarrow.png",
									SLOT(moveDown()), CTRL | Key_Down);
	editMenu->insertSeparator();
	editConfirmAc = newAction(editMenu, "con&firm Actions", "", 0,
								CTRL | Key_F, true);

	playStartAc = newAction(viewMenu, "&Resume updates", "kpic/player_play.png",
							SLOT(playStart(bool)), CTRL | Key_P, true);
	playStopAc = newAction(viewMenu, "&Pause updates", "kpic/stop.png",
							SLOT(playStop(bool)), 0, true);
	viewMenu->insertSeparator();
	newAction(viewMenu, "Up", "", SLOT(selectUp()), Key_Up);
	newAction(viewMenu, "Down", "", SLOT(selectDown()), Key_Down);
	viewHexAc = newAction(viewMenu, "&Hexadecimal", "kpic/math_brackets.png",
							SLOT(viewHex(bool)), CTRL | Key_H, true);
	viewMenu->insertSeparator();
	viewDefHexAc = newAction(viewMenu, "By &Default Hexadecimal", "", 0,
								CTRL | Key_X, true);
	viewLogAc = newAction(viewMenu, "&Logging", "kpic/toggle_log.png",
							SLOT(setLogging(bool)), CTRL | Key_L, true);
	viewMenu->insertSeparator();
	viewStateBoxAc = newAction(viewMenu, "Show State Box",
								"kpic/connect_creating.png",
								SLOT(viewStateBox(bool)), CTRL | Key_V, true);

	newAction(helpMenu, "About", "kpic/help.png",
				SLOT(helpAbout()), CTRL | Key_F1);

	playStartAc->addTo(toolbar);
	viewLogAc->addTo(toolbar);
	toolbar->addSeparator();

	viewHexAc->addTo(toolbar);
	moveUpAc->addTo(toolbar);
	moveDownAc->addTo(toolbar);
	toolbar->addSeparator();

	removeCurrentAc->addTo(toolbar);
	removeBrokenAc->addTo(toolbar);
	removeAllAc->addTo(toolbar);
	toolbar->addSeparator();

	fileOpenAc->addTo(toolbar);
	fileSaveAc->addTo(toolbar);
	toolbar->addSeparator();

	toolbar->setStretchableWidget(new QWidget(toolbar));	// spacer
	fileQuitAc->addTo(toolbar);

	editConfirmAc->setOn(true);
	playStartAc->setOn(true);
	viewDefHexAc->setOn(false);
}


/* ======== The Window ======== */


OQSeeWindow::OQSeeWindow()
	: QMainWindow()
{
	setDockWindowsMovable(true);

	QWidget *panel = new QWidget(this);
	setCentralWidget(panel);
	QVBoxLayout *vbox = new QVBoxLayout(panel);
	vbox->setMargin(2);
	vbox->setSpacing(2);

	QVGroupBox *dataframe = new QVGroupBox("Data", panel);
	vbox->addWidget(dataframe, 10);
	seeview = new OQSeeView(dataframe);
	seeview->setupWidgets(panel, statusBar());
	seeview->setupActions(menuBar(), new QToolBar(this));

	setCaption("Optikus Data Viewer");
	setIcon(OQPixmap("kpic/viewmag.png"));
	resize(400, 500);
}


void
OQSeeWindow::closeEvent(QCloseEvent *e)
{
	if (getView()->canQuit())
		QMainWindow::closeEvent(e);
}


int
main(int argc, char **argv)
{
	OQApp app("oqsee", argc, argv);
	OQSeeWindow win;
	app.setMainWidget(&win);
	app.setQuitFilter(win.getView());
	win.show();
	return app.exec();
}

