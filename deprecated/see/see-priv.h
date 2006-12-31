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

  Internal declarations for simple GTK-based data visualizer.

*/

#ifndef OPTIKUS_SEE_PRIV_H
#define OPTIKUS_SEE_PRIV_H

#include <optikus/log.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

OPTIKUS_BEGIN_C_DECLS


#define OSEE_MAX_NAME 40

#define OSEE_LOG_FILE "osee1.log"

#define oseeLog(verb,args...)	(owatch_verb >= (verb) ? olog(args) : -1)

oret_t  oseeInit(void);
oret_t  oseeExit(void *gtk_object_p, void *bool_t_ask);
oret_t  oseeAbout(void);
oret_t  oseeOpen(void);
oret_t  oseeSave(void);
oret_t  oseeAdd(void);
oret_t  oseeRemove(void);
oret_t  oseeStart(void);
oret_t  oseeStop(void);
oret_t  oseeAdd(void);
oret_t  oseeClear(void);
oret_t  oseeSetDeletionConfirmation(bool_t enable);
oret_t  oseeSetCurHex(bool_t enable);
oret_t  oseeToggleCurHex(void);
oret_t  oseeSetDefHex(bool_t enable);
oret_t  oseeSetDebug(bool_t enable);
oret_t  oseeUnselectAll(void);
oret_t  oseeSetStatus(const char *format, ...);

GtkWidget *oseeCreatePixmap(GtkWidget * widget, const char *filename);
GtkWidget *oseeCreateButton(GtkWidget * window,
							 const char *icon, const char *label,
							 void *func, void *param);
oret_t  oseeChangeButton(GtkWidget * button,
						   const char *icon, const char *text);
GtkWidget *oseeCreateFileSelection(const char *title, void *func);
int     oseeRunDialog(const char *icon, const char *message,
					   int button_num, const char *button_icons[],
					   const char *button_labels[]);
bool_t  oseeQuestion(const char *question);

extern GtkWidget *osee_window;
extern GtkWidget *osee_filesel_open;
extern GtkWidget *osee_filesel_save;
extern GtkWidget *command_entry;

extern GtkCList *osee_table;
extern GtkEntry *osee_entry;
extern GtkStatusbar *osee_statusbar;
extern GtkCheckMenuItem *osee_hex_check;
extern GtkToggleButton *osee_hex_btn;
extern GtkToggleButton *osee_log_btn;
extern GtkWidget *osee_alive_bar;
extern GtkWidget *osee_alive_combo;

typedef struct
{
	char   *name;
	char  **data;
} OseeXpmIcon_t;

extern OseeXpmIcon_t osee_icons[];

#define OSEE_BG_PERIOD_MS              40
#define OSEE_LOCAL_DISP_THRESHOLD_MS   40
#define OSEE_GLOBAL_DISP_THRESHOLD_MS  0

oret_t  oseeSetLogging(bool_t active);
oret_t  oseeToggleLogging(void);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_SEE_PRIV_H */
