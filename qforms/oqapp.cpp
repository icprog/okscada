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

  Optikus application class.

*/

#include "oqcommon.h"
#include "oqwatch.h"

#include <qsignalmapper.h>
#include <qprocess.h>
#include <qobjectlist.h>
#include <qthread.h>

#include <optikus/log.h>


#define qDebug(...)

#define QUANT_MS		10
#define EXIT_WAIT_MS	100

#define QUERY_SCRIPTS()		queryList("QProcess", "script_.*", true, false)

OQApp* OQApp::oqApp;

QDir  OQApp::root;
QDir  OQApp::share;
QDir  OQApp::pics;
QDir  OQApp::screens;
QDir  OQApp::etc;
QDir  OQApp::bin;
QDir  OQApp::var;
QDir  OQApp::logs;
QFile OQApp::log;
bool  OQApp::log_to_stdout = true;


OQApp::OQApp(const QString& _client_name, int &argc, char **argv,
			const QString& theme)
	:	QApplication(argc, argv),
		quit_filter(0),
		sm_start(0), sm_exit(0)
{
	oqApp = this;

	sm_start = new QSignalMapper(this);
	connect(sm_start, SIGNAL(mapped(const QString&)),
			SLOT(scriptStartSlot(const QString&)));
	sm_exit = new QSignalMapper(this);
	connect(sm_start, SIGNAL(mapped(const QString&)),
			SLOT(scriptExitSlot(const QString&)));

	QString ohome = getenv("OPTIKUS_HOME");
	if (ohome.isEmpty()) {
		QDir dir(applicationDirPath());
		dir.cdUp();
		ohome = dir.canonicalPath();
		if (ohome.isNull())
			ohome = dir.absPath();
		// FIXME: FIXME: FIXME: HACK !!!
		char *HACK="/home/vit/optikus"; if (QDir(HACK).exists()) ohome=HACK;
		setenv("OPTIKUS_HOME", ohome, 1);
	}
	root.setPath(ohome);

	share.setPath(root.filePath("share"));
	pics.setPath(share.filePath("pic"));
	screens.setPath(share.filePath("screens"));
	etc.setPath(root.filePath("etc"));
	bin.setPath(root.filePath("bin"));
	var.setPath(root.filePath("var"));
	logs.setPath(var.filePath("log"));

	if (!theme.isEmpty())
		setTheme(theme);

	QString client_name = _client_name;
	if (client_name.isEmpty())
		client_name = "qforms";
	log.setName(logs.exists() ? logs.filePath("qforms.log") : 0);

	ologOpen(client_name, log.name(), 4096);
	int flag_stdout = log.name().isNull() || log_to_stdout ? OLOG_STDOUT : 0;
	ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH | flag_stdout);

	qDebug("root=%s pic=%s logs=%s log=%s", root.path().ascii(),
			pics.path().ascii(), logs.path().ascii(), log.name().ascii());
}


OQApp::~OQApp()
{
	killAllScripts();
	delete sm_start;
	delete sm_exit;
	oqApp = 0;
}


void
OQApp::quit()
{
	bool can = quit_filter ? quit_filter->canQuit() : true;
	if (can)
		quitForce();
}


void
OQApp::quitForce()
{
	qDebug("bye");
	QApplication::quit();
}


int
OQApp::runScript(const QString& script, const QString& lang)
{
	bool langok = false;
	int pid = -1;
	QProcess * proc = 0;
	if (lang == "perl")
	{
		langok = true;
		QString perl_libs = root.filePath("lib/perl5/site_perl");
		QString cmd;
		cmd.append("use strict; ");
		cmd.append("use lib \"" + perl_libs + "\"; ");
		cmd.append("use Optikus::Log \":all\"; ");
		cmd.append("use Optikus::Watch \":all\"; ");
		cmd.append("begin_test(\"(DEFAULT)\"); ");
		cmd.append(script);
		QStringList args;
		args.append("perl");
		args.append("-e");
		args.append(cmd);
		proc = new QProcess(args, this);
		proc->start();
		pid = proc->processIdentifier();
		qDebug(QString("runScript<perl>(%1): [" + cmd + "]").arg(pid));
	}
	if (!langok)
		qWarning("runScript: language \"" + lang + "\" is unknown");
	if (proc) {
		QString name;
		name.sprintf("script_%d", pid);
		proc->setName(name);
		sm_start->setMapping(proc, name);
		connect(proc, SIGNAL(launchFinished()), sm_start, SLOT(map()));
		sm_exit->setMapping(proc, name);
		connect(proc, SIGNAL(processExited()), sm_start, SLOT(map()));
	}
	return pid;
}


void
OQApp::scriptStartSlot(const QString& name)
{
	QProcess *proc = (QProcess *) child(name, "QProcess", false);
	if (!proc) {
		qDebug("unknown script " + name + " started");
		return;
	}
	qDebug(name + ": started");
	sm_start->removeMappings(proc);
}


void
OQApp::scriptExitSlot(const QString& name)
{
	QProcess *proc = (QProcess *) child(name, "QProcess", false);
	if (!proc) {
		qDebug("unknown script " + name + " exited");
		return;
	}
	int pid = proc->processIdentifier();
	int status = proc->normalExit() ? proc->exitStatus() : -1;
	qDebug(QString(name + ": (%1) exited with status %2").arg(pid).arg(status));
	emit scriptFinished(pid, status);
	killScript(pid);
}


bool
OQApp::killScript(int pid)
{
	QString name;
	name.sprintf("script_%d", pid);
	QProcess *proc = (QProcess *) child(name, "QProcess", false);
	if (!proc) {
		qDebug(name + ": not found for kill"); 
		return false;
	}
	if (proc->isRunning()) {
		qDebug(name + ": terminating");
		proc->tryTerminate();
		for (int i = 0; i < EXIT_WAIT_MS / QUANT_MS; i++) {
			if (!proc->isRunning())
				break;
			msleep(QUANT_MS);
		}
		if (proc->isRunning()) {
			qDebug(name + ": killed");
			proc->kill();
		}
	}
	sm_start->removeMappings(proc);
	sm_exit->removeMappings(proc);
	delete proc;
	qDebug(name + ": removed");
	return true;
}


void
OQApp::killAllScripts()
{
    QProcess *proc;
	int nscripts = 0;
	int nterm = 0;

	QObjectList *lterm = QUERY_SCRIPTS();
	QObjectListIterator iterm(*lterm);
	while ((proc = (QProcess *)iterm.current()) != 0) {
		++iterm;
		++nscripts;
		if (proc->isRunning()) {
			proc->tryTerminate();
			qDebug(QString("script_%1: next terminating")
					.arg(proc->processIdentifier()));
			++nterm;
		}
    }
	delete lterm;

	if (nterm > 0) {
		for (int i = 0; i < EXIT_WAIT_MS / QUANT_MS; i++) {
			int nrun = 0;
			QObjectList *lrun = QUERY_SCRIPTS();
			QObjectListIterator irun(*lrun);
			while ((proc = (QProcess *)irun.current()) != 0) {
				++irun;
				if (proc->isRunning()) {
					++nrun;
					break;
				}
			}
			delete lrun;
			if (!nrun)
				break;
			msleep(QUANT_MS);
		}
	}

	int nkill = 0;
	QObjectList *lkill = QUERY_SCRIPTS();
	QObjectListIterator ikill(*lkill);
	while ((proc = (QProcess *)ikill.current()) != 0) {
		++ikill;
		if (proc->isRunning()) {
			proc->kill();
			qDebug(QString("script_%1: next killed")
					.arg(proc->processIdentifier()));
			++nkill;
		}
		sm_start->removeMappings(proc);
		sm_exit->removeMappings(proc);
		delete proc;
    }
	delete lkill;

	qDebug(QString("killed %1 scripts").arg(nscripts));
}


void
OQApp::msleep(int msec, bool process_events)
{
	if (msec > 0) {
		class Sleeper : public QThread {
			public:
				Sleeper(int msec)	{ msleep(msec); }
				void run()	{}
		};
		int nq = msec / QUANT_MS;
		for (int i = 0; i < nq; i++) {
			Sleeper sleeper(QUANT_MS);
			if (process_events) {
				if (OQWatch::getInstance())
					OQWatch::idle();
				if (oqApp)
					oqApp->processEvents();
			}
		}
		Sleeper sleeper(msec % QUANT_MS);
	}
}


void
OQApp::setTheme(const QString& theme)
{
	QDir themes(share.filePath("themes"));
	QDir themedir(themes.filePath(theme));
	QFile qtrc(themedir.filePath("qtrc"));
	if (!qtrc.open(IO_ReadOnly)) {
		qWarning("theme config [" + qtrc.name() + "] not found");
		return;
	}
	qDebug("reading theme config " + qtrc.name());

	QMap<QString,QString> params;
	QMap<QString,QString> fontsubs;
	QString section;
	QString line, key, val;
	QStringList lst;
	const QString sep = "^e";

	while (qtrc.readLine(line, 4096) >= 0) {
		line = line.stripWhiteSpace();
		if (line.isEmpty() || line[0] == '#')
			continue;
		if (line.startsWith("[")) {
			if (!line.endsWith("]"))
				qWarning("invalid qtrc line: " + line);
			line.remove('[');
			line.remove(']');
			section = line;
			continue;
		}
		int pos = line.find('=');
		if (pos < 0) {
			qWarning("invalid qtrc line: " + line);
			continue;
		}
		QString key = line.left(pos).stripWhiteSpace();
		QString val = line.mid(pos + 1).stripWhiteSpace();
		val.replace("\\0", "");
		val.replace("\\n", "\n");
		val.replace("\\\\", "\\");
		if (section == "Font Substitutions")
			fontsubs.replace(key, val);
		key = section + "/" + key;
		params.replace(key, val);
		qDebug("param[" + key + "]=[" + val + "]");
	}
	qtrc.close();

	val = params["General/style"];
	if (!val.isEmpty()) {
		setStyle(val);
		qDebug("set style " + val);
	}

	unsigned i;
	QPalette pal(palette());
	unsigned n = QColorGroup::NColorRoles;
	lst = QStringList::split(sep, params["Palette/active"], false);
    if (lst.count() == n) {
		for (i = 0; i < n; i++)
			pal.setColor(QPalette::Active,
							(QColorGroup::ColorRole) i, QColor(lst[i]));
    }
	lst = QStringList::split(sep, params["Palette/inactive"], false);
    if (lst.count() == n) {
		for (i = 0; i < n; i++)
			pal.setColor(QPalette::Inactive,
							(QColorGroup::ColorRole) i, QColor(lst[i]));
    }
	lst = QStringList::split(sep, params["Palette/disabled"], false);
    if (lst.count() == n) {
		for (i = 0; i < n; i++)
			pal.setColor(QPalette::Disabled,
							(QColorGroup::ColorRole) i, QColor(lst[i]));
			qDebug("set palette");
    }
	if (pal != palette())
		setPalette(pal, true);

	val = params["General/font"];
	if (!val.isEmpty()) {
		QFont fnt(font());
		fnt.fromString(val);
		if (fnt != font()) {
			setFont(fnt, true);
			qDebug("set font " + fnt.toString());
		}
	}

	QMap<QString,QString>::Iterator it = fontsubs.begin();
	while (it != fontsubs.end()) {
		lst = QStringList::split(sep, it.data(), false);
		if (!lst.empty()) {
			QFont::insertSubstitutions(it.key(), lst);
			qDebug("set font subst [" + it.key() + "]=[" + lst.join(",") + "]");
		}
		++it;
	}

	lst = QStringList::split(sep, params["General/GUIEffects"], false);
	if (!lst.empty()) {
		setEffectEnabled(UI_General, lst.contains("general"));
		setEffectEnabled(UI_AnimateMenu, lst.contains("animatemenu"));
		setEffectEnabled(UI_FadeMenu, lst.contains("fademenu"));
		setEffectEnabled(UI_AnimateCombo, lst.contains("animatecombo"));
		setEffectEnabled(UI_AnimateTooltip, lst.contains("animatetooltip"));
		setEffectEnabled(UI_FadeTooltip, lst.contains("fadetooltip"));
		setEffectEnabled(UI_AnimateToolBox, lst.contains("animatetoolbox"));
		qDebug("set effects " + lst.join(","));
	}
}
