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

  Simple GTK+ data visualizer.

*/

#include "see-priv.h"
#include <optikus/log.h>
#include <optikus/watch.h>
#include <config.h>				/* for package requisites */
#include <optikus/util.h>		/* for osMsClock,strTrim */
#include <optikus/conf-mem.h>	/* for oxnew,oxfree,oxstrdup,oxvzero */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


char    osee_url[80] = "localhost";

long    osee_last_disp_ms = 0;

enum
{
	LCOL_DESC,
	LCOL_VALUE,
	LCOL_INFO,
	LCOL_PARAMS,
	NUM_LCOLS,
};

int     osee_nrows = 0;

typedef struct
{
	char    subject[40];
	GtkWidget *w_label;
	GtkWidget *w_box;
	GtkWidget *w_on;
	GtkWidget *w_off;
} OseeSubjectWidget_t;

OseeSubjectWidget_t *osee_subjects = NULL;
int     osee_subject_qty = 0;

#define OSEE_DIM_REP   1
#define OSEE_FILE_REP  2

typedef struct
{
	int     type;
	bool_t  ignore;
	int     imin[4];
	int     imax[4];
	int     icur[4];
	int     inum;
	char    desc[200];
	char    format[200];
	char  **names;
	bool_t *as_hexes;
	int     fcur;
	int     fnum;
	int     op_type;
	bool_t  as_hex;
	bool_t  ready;
	char    value[4];
} OseeRepeater_t;


#define MAX_STR_LEN  120
#define MAX_HEX_LEN  16

typedef struct OseeReadRecord_st
{
	ooid_t ooid;
	owquark_t info;
	ulong_t stamp;
	int     row;
	long    last_disp_ms;
	OseeRepeater_t *repeater;
	struct OseeReadRecord_st *next;
	bool_t  can_be_hex;
	bool_t  as_hex;
	char    str_buf[MAX_STR_LEN];
	char    hex_buf[MAX_HEX_LEN];
	char    desc[4];
} OseeReadRecord_t;


typedef struct OseeShotReadRecord_st
{
	char    desc[256];
	oval_t val;
	char    data_buf[8192];
} OseeShotReadRecord_t;


OseeReadRecord_t *osee_sel_prec = NULL;

OseeRepeater_t *osee_read_rep = NULL;
OseeRepeater_t *osee_write_rep = NULL;

bool_t  osee_def_hex = FALSE;
bool_t  osee_deletion_confirmation = TRUE;

bool_t  osee_logging = FALSE;

oret_t  oseeRead(const char *desc, bool_t as_hex, OseeRepeater_t * prep);
oret_t  oseeWrite(const char *desc, char *sval, OseeRepeater_t * prep);
bool_t  oseeRepeat(OseeRepeater_t * prep);
oret_t  oseeCancelRepeat(OseeRepeater_t * prep);
oret_t  oseeUnselectAll(void);


/*
 * .
 */
oret_t
oseeRowAppend(OseeReadRecord_t * prr)
{
	ooid_t ooid = prr->ooid;
	long    data;
	char    info_s[200];
	owop_t  op = owatchGetInfoByOoid(ooid, &prr->info);

	prr->can_be_hex = FALSE;
	if (op != OWOP_OK) {
		owatchCancelOp(op);
		sprintf(info_s, "!!! read{%s} no info (op=%xh ooid=%lu)",
				prr->desc, op, ooid);
	} else {
		sprintf(info_s, "ooid=%lu type=%c ptr=%c len=%d bits=%d:%d desc=\"%s\"",
				ooid, prr->info.type, prr->info.ptr, prr->info.len,
				prr->info.bit_off, prr->info.bit_len, prr->info.desc);
		switch (prr->info.type) {
		case 'b':
		case 'B':
		case 'h':
		case 'H':
		case 'i':
		case 'I':
		case 'l':
		case 'L':
			prr->can_be_hex = TRUE;
			break;
		}
	}
	prr->as_hex = prr->as_hex && prr->can_be_hex;
	data = 0;
	owatchGetMonitorData(ooid, &data);
	prr->next = (OseeReadRecord_t *) data;
	owatchSetMonitorData(ooid, (long) prr);
	prr->row = osee_nrows++;
	{
		char   *colval[3];

		colval[0] = prr->desc;
		colval[1] = "---";
		colval[2] = info_s;
		gtk_clist_append(osee_table, colval);
		gtk_clist_set_row_data(osee_table, prr->row, prr);
	}
	return OK;
}


oret_t
oseeRowRemove(OseeReadRecord_t * prr)
{
	ooid_t ooid = prr->ooid;
	int     row = prr->row;
	OseeReadRecord_t *cur, *head;
	int     i;
	long    data;

	/* remove row itself */
	gtk_clist_remove(osee_table, row);
	osee_nrows--;

	/* update row numbers in all records */
	for (i = 0; i < osee_nrows; i++) {
		cur = (OseeReadRecord_t *) gtk_clist_get_row_data(osee_table, i);
		if (NULL == cur)
			oseeLog(1, "oops! row has no prec");
		else if (cur->row > row)
			cur->row--;
	}

	/* remove from chain */
	data = 0;
	owatchGetMonitorData(ooid, &data);
	head = (OseeReadRecord_t *) data;
	if (NULL == head) {
		oseeLog(1, "chain not found for ooid=%lu", ooid);
	} else {
		if (prr == head) {
			head = head->next;
		} else {
			for (cur = head; cur; cur = cur->next) {
				if (cur->next == prr) {
					cur->next = prr->next;
					break;
				}
			}
		}
		owatchSetMonitorData(ooid, (long) head);
	}

	/* decrease reference count of the monitor */
	owatchRemoveMonitor(ooid);

	/* deallocate the record */
	oxfree(prr);

	return OK;
}

void
oseeSubjectClicked(GtkButton * button, gpointer user_data)
{
	char   *subject = (char *) user_data;
	const char *state;
	owop_t  op;
	oret_t rc = OK;
	int     err_code;

	if (!subject || !*subject)
		return;
	if (!strcmp(subject, "HUB") || !strcmp(subject, "DOMAIN"))
		return;
	state = owatchGetSubjectState(subject);
	if (state && *state)
		return;
	oseeLog(6, "start triggering [%s]", subject);
	op = owatchTriggerSubject(subject);
	err_code = 0;
	rc = owatchWaitOp(op, OWATCH_FOREVER, &err_code);
	owatchCancelOp(op);
	oseeLog(6, "triggering [%s] rc=%d err=[%s]",
			subject, rc, owatchErrorString(err_code));
}


int
oseeAliveHandler(long param, const char *subject, const char *state)
{
	OseeSubjectWidget_t *psubj;
	char    buf[120];
	char   *s, *d;
	const char *cs;
	int     n, k;
	GtkWidget *alive_combo;

	if (!strcmp(subject, "*"))
		subject = "HUB";
	else if (!strcmp(subject, "+"))
		subject = "DOMAIN";
	if (!osee_subjects) {
		if (strcmp(subject, "HUB") != 0 || !*state)
			return 0;
		owatchGetSubjectList(buf, sizeof(buf));
		n = 0;
		for (s = buf; *s; s++)
			if (*s == ' ')
				n++;
		gtk_container_remove(GTK_CONTAINER(osee_alive_bar), osee_alive_combo);
		osee_subject_qty = n + 3;	/* spaces + 1 + conn + fullness */
		osee_subjects = oxnew(OseeSubjectWidget_t, osee_subject_qty);
		psubj = osee_subjects;
		for (n = 0; n < osee_subject_qty; n++, psubj++) {
			switch (n) {
			case 0:
				strcpy(psubj->subject, "HUB");
				break;
			case 1:
				strcpy(psubj->subject, "DOMAIN");
				break;
			default:
				k = 2;
				s = buf;
				while (k < n) {
					while (*s != 0 && *s != ' ')
						s++;
					while (*s == ' ')
						s++;
					k++;
				}
				d = psubj->subject;
				while (*s && *s != ' ')
					*d++ = *s++;
				*d = 0;
			}
			alive_combo = gtk_button_new();
			gtk_widget_show(alive_combo);
			gtk_toolbar_append_widget(GTK_TOOLBAR(osee_alive_bar),
									  alive_combo,
									  psubj->subject, psubj->subject);
			psubj->w_box = gtk_hbox_new(FALSE, 0);
			gtk_widget_show(psubj->w_box);
			gtk_container_add(GTK_CONTAINER(alive_combo), psubj->w_box);
			psubj->w_on = oseeCreatePixmap(osee_window, "on");
			gtk_widget_hide(psubj->w_on);
			gtk_box_pack_start(GTK_BOX(psubj->w_box), psubj->w_on,
							   FALSE, FALSE, 0);
			psubj->w_off = oseeCreatePixmap(osee_window, "off");
			gtk_widget_hide(psubj->w_off);
			gtk_box_pack_start(GTK_BOX(psubj->w_box), psubj->w_off,
							   FALSE, FALSE, 0);
			psubj->w_label = gtk_label_new(psubj->subject);
			gtk_widget_show(psubj->w_label);
			gtk_box_pack_end(GTK_BOX(psubj->w_box), psubj->w_label,
							 FALSE, FALSE, 0);
			gtk_signal_connect(GTK_OBJECT(alive_combo), "clicked",
							   GTK_SIGNAL_FUNC(oseeSubjectClicked),
							   psubj->subject);
		}
		psubj = osee_subjects;
		for (n = 0; n < osee_subject_qty; n++, psubj++) {
			if (!strcmp(psubj->subject, "HUB"))
				cs = owatchGetSubjectState("*");
			else if (!strcmp(psubj->subject, "DOMAIN"))
				cs = owatchGetSubjectState("+");
			else
				cs = owatchGetSubjectState(psubj->subject);
			oseeAliveHandler(param, psubj->subject, cs);
		}
		return 0;
	}
	psubj = osee_subjects;
	for (n = 0; n < osee_subject_qty; n++, psubj++) {
		if (strcmp(psubj->subject, subject) != 0)
			continue;
		if (*state) {
			gtk_widget_hide(psubj->w_off);
			gtk_widget_show(psubj->w_on);
		} else {
			gtk_widget_hide(psubj->w_on);
			gtk_widget_show(psubj->w_off);
		}
		return 0;
	}
	return 0;
}


oret_t
oseeSetDeletionConfirmation(bool_t enable)
{
	osee_deletion_confirmation = enable;
	oseeSetStatus(enable ? "confirmations ON" : "no confirmations");
	return OK;
}


oret_t
oseeUpdateDisplayRow(OseeReadRecord_t * prec)
{
	char   *str_val;

	if (prec->row < 0) {
		oseeLog(1, "oops! row not set for ooid=%lu", prec->ooid);
		return ERROR;
	}
	oseeLog(9, ">>> ooid=%lu row=%d", prec->ooid, prec->row);
	str_val = prec->as_hex && prec->can_be_hex ? prec->hex_buf : prec->str_buf;
	gtk_clist_set_text(osee_table, prec->row, 1, str_val);
	return OK;
}


oret_t
oseeSetCurHex(bool_t enable)
{
	OseeReadRecord_t *prec = osee_sel_prec;

	if (!prec)
		return ERROR;
	prec->as_hex = enable && prec->can_be_hex;
	gtk_toggle_button_set_active(osee_hex_btn, prec->as_hex);
	gtk_check_menu_item_set_active(osee_hex_check, prec->as_hex);
	oseeUpdateDisplayRow(prec);
	return OK;
}


oret_t
oseeToggleCurHex()
{
	OseeReadRecord_t *prec = osee_sel_prec;

	if (!prec)
		return ERROR;
	return oseeSetCurHex(!prec->as_hex);
}


oret_t
oseeSetDefHex(bool_t enable)
{
	osee_def_hex = enable;
	return OK;
}


oret_t
oseeValToString(const oval_t * pval, char *str_buf, int buf_len)
{
	oval_t tmp_val;

	tmp_val.type = 's';
	strcpy(str_buf, "???");
	if (pval->undef)
		strcpy(str_buf, "?");
	else if (pval->type != 's')
		owatchConvertValue(pval, &tmp_val, str_buf, buf_len);
	else {
		str_buf[0] = '"';
		strcpy(str_buf + 1, pval->v.v_str);
		strcat(str_buf, "\"");
	}
	return OK;
}

oret_t
oseeLogOneValue(OseeReadRecord_t * prec, ulong_t stamp,
				 const char *hex_buf, const char *str_buf)
{
	int     sec, msec, usec;

	if (!osee_logging)
		return ERROR;
	if (!stamp)
		stamp = prec->stamp;
	if (!hex_buf)
		hex_buf = prec->hex_buf;
	if (!str_buf)
		str_buf = prec->str_buf;
	sec = (stamp / 1000000) % 1000;
	msec = (stamp / 1000) % 1000;
	usec = stamp % 1000;
	olog("[%03d.%03d.%03d] [%-32s] [%s]", sec, msec, usec, prec->desc,
		 prec->as_hex ? hex_buf : str_buf);
	return OK;
}

oret_t
oseeLogAllValues()
{
	OseeReadRecord_t *prec;
	int     i;

	if (!osee_logging)
		return ERROR;
	olog("-------- CUT begins --------");
	for (i = 0; i < osee_nrows; i++) {
		prec = (OseeReadRecord_t *) gtk_clist_get_row_data(osee_table, i);
		if (NULL == prec) {
			oseeLog(1, "oops! row %d has no prec", i);
			continue;
		}
		oseeLogOneValue(prec, 0, NULL, NULL);
	}
	olog("-------- CUT ends --------");
	return OK;
}


int
oseeDataHandler(long param, ooid_t ooid, const oval_t * pval)
{
	OseeReadRecord_t *prec;
	char    str_buf[MAX_STR_LEN];
	char    hex_buf[MAX_HEX_LEN];
	oret_t rc;
	long    data;
	long    now;

	oseeValToString(pval, str_buf, MAX_STR_LEN);
	switch (pval->type) {
	case 'b':
	case 'B':
		sprintf(hex_buf, "%02xh", pval->v.v_uchar);
		break;
	case 'h':
	case 'H':
		sprintf(hex_buf, "%04xh", pval->v.v_ushort);
		break;
	case 'i':
	case 'I':
		sprintf(hex_buf, "%08xh", pval->v.v_uint);
		break;
	case 'l':
	case 'L':
		sprintf(hex_buf, "%08lxh", pval->v.v_ulong);
		break;
	default:
		strcpy(hex_buf, "###");
		break;
	}
	data = 0;
	rc = owatchGetMonitorData(ooid, &data);
	prec = (OseeReadRecord_t *) data;
	if (!prec) {
		oseeLog(1, "oops! no record for ooid=%lu", ooid);
		return 0;
	}
	if (osee_logging)
		oseeLogOneValue(prec, pval->time, hex_buf, str_buf);
	now = osMsClock();
	if (now - prec->last_disp_ms < OSEE_LOCAL_DISP_THRESHOLD_MS
		&& prec->last_disp_ms) {
		owatchRenewMonitor(ooid);
		return 0;
	}
	if (now - osee_last_disp_ms < OSEE_GLOBAL_DISP_THRESHOLD_MS
		&& osee_last_disp_ms) {
		owatchRenewMonitor(ooid);
		return 0;
	}
	prec->last_disp_ms = osee_last_disp_ms = now;
	for (; prec; prec = prec->next) {
		prec->stamp = pval->time;
		strcpy(prec->str_buf, str_buf);
		strcpy(prec->hex_buf, hex_buf);
		oseeUpdateDisplayRow(prec);
	}
	return 0;
}


int
oseeReadHandler(long param, owop_t op, int err_code)
{
	OseeReadRecord_t *prec = (OseeReadRecord_t *) param;
	OseeRepeater_t *prep = prec->repeater;

	prec->repeater = NULL;
	if (err_code) {
		oseeSetStatus("read{%s} error=%s",
					   prec->desc, owatchErrorString(err_code));
		oxfree(prec);
	} else {
		oseeRowAppend(prec);
		oseeSetStatus("read{%s} OK", prec->desc);
	}
	if (prep) {
		if (err_code && !prep->ignore)
			oseeCancelRepeat(prep);
		else {
			prep->ready = TRUE;
			if (oseeRepeat(prep))
				oseeCancelRepeat(prep);
		}
	}
	return 0;
}

oret_t
oseeRemove()
{
	char    message[200];
	OseeReadRecord_t *prec = osee_sel_prec;

	oseeUnselectAll();
	if (!prec) {
		oseeSetStatus("please select a row to remove");
		return ERROR;
	}
	if (osee_deletion_confirmation) {
		sprintf(message, "Are you sure to remove \"%s\" ?", prec->desc);
		if (!oseeQuestion(message))
			return ERROR;
	}
	oseeRowRemove(prec);
	return OK;
}


int
oseeWriteHandler(long param, owop_t op, int err_code)
{
	OseeReadRecord_t *prec = (OseeReadRecord_t *) param;
	OseeRepeater_t *prep = prec->repeater;

	prec->repeater = NULL;
	if (err_code)
		oseeSetStatus("write{%s} error=%s",
					   prec->desc, owatchErrorString(err_code));
	else
		oseeSetStatus("write{%s} OK", prec->desc);
	oxfree(prec);
	if (prep) {
		if (err_code && !prep->ignore)
			oseeCancelRepeat(prep);
		else {
			prep->ready = TRUE;
			if (oseeRepeat(prep))
				oseeCancelRepeat(prep);
		}
	}
	return 0;
}


int
oseeShotReadHandler(long param, owop_t op, int err_code)
{
	char    str_buf[140];
	OseeShotReadRecord_t *psr = (OseeShotReadRecord_t *) param;

	if (err_code) {
		oseeSetStatus("1read{%s} error=%s",
					   psr->desc, owatchErrorString(err_code));
	} else {
		oseeValToString(&psr->val, str_buf, sizeof(str_buf));
		oseeSetStatus("%s:=\"%s\"", psr->desc, str_buf);
	}
	oxfree(psr);
	return 0;
}


bool_t
oseeRepeat(OseeRepeater_t * prep)
{
	int     i;

	if (!prep || !prep->ready)
		return FALSE;
	if (prep->type == OSEE_DIM_REP) {
		prep->ready = FALSE;
		for (i = prep->inum - 1; i >= 0; i--) {
			if (++prep->icur[i] <= prep->imax[i])
				break;
			if (i > 0)
				prep->icur[i] = prep->imin[i];
		}
		if (prep->icur[0] > prep->imax[0]) {
			switch (prep->op_type) {
			case 1:
				oseeSetStatus("read{%s} OK", prep->format);
				break;
			case 2:
				oseeSetStatus("write{%s} OK", prep->format);
				break;
			}
			return TRUE;
		}
		sprintf(prep->desc, prep->format,
				prep->icur[0], prep->icur[1], prep->icur[2], prep->icur[3]);
	} else if (prep->type == OSEE_FILE_REP) {
		if (++prep->fcur >= prep->fnum) {
			switch (prep->op_type) {
			case 1:
				oseeSetStatus("[read] file OK", prep->format);
				break;
			case 2:
				oseeSetStatus("[write] file OK", prep->format);
				break;
			}
			return TRUE;
		}
		strcpy(prep->desc, prep->names[prep->fcur]);
		prep->as_hex = prep->as_hexes[prep->fcur];
	} else {
		return ERROR;
	}
	switch (prep->op_type) {
	case 1:
		oseeRead(prep->desc, prep->as_hex, prep);
		break;
	case 2:
		oseeWrite(prep->desc, prep->value, prep);
		break;
	}
	return FALSE;
}


oret_t
oseeCancelRepeat(OseeRepeater_t * prep)
{
	int     i;

	if (NULL == prep)
		return ERROR;
	if (prep->type == OSEE_FILE_REP) {
		if (NULL != prep->names) {
			for (i = 0; i < prep->fnum; i++)
				oxfree(prep->names[i]);
		}
		oxfree(prep->names);
		oxfree(prep->as_hexes);
	}
	oxvzero(prep);
	oxfree(prep);
	return OK;
}


gboolean
oseeBackgroundJob(gpointer data)
{
	if (oseeRepeat(osee_read_rep)) {
		oxfree(osee_read_rep);
		osee_read_rep = NULL;
	}
	if (oseeRepeat(osee_write_rep)) {
		oxfree(osee_write_rep);
		osee_write_rep = NULL;
	}
	owatchWork(5);
	return TRUE;
}


void
oseeRowUnselected(GtkCList * clist, gint row, gint col,
				   GdkEventButton * event, gpointer user_data)
{
	osee_sel_prec = NULL;
}


void
oseeRowSelected(GtkCList * clist, gint row, gint col,
				 GdkEventButton * event, gpointer user_data)
{
	OseeReadRecord_t *prec = gtk_clist_get_row_data(osee_table, row);

	osee_sel_prec = prec;
	gtk_entry_set_text(osee_entry, prec->desc);
	if (prec->can_be_hex) {
		gtk_check_menu_item_set_active(osee_hex_check, prec->as_hex);
		gtk_toggle_button_set_active(osee_hex_btn, prec->as_hex);
	} else {
		gtk_check_menu_item_set_active(osee_hex_check, FALSE);
		gtk_toggle_button_set_active(osee_hex_btn, FALSE);
	}
}


oret_t
oseeUnselectAll()
{
	osee_sel_prec = NULL;
	gtk_clist_unselect_all(osee_table);
	if (osee_hex_check) {
		gtk_check_menu_item_set_active(osee_hex_check, FALSE);
	}
	if (osee_hex_btn) {
		gtk_toggle_button_set_active(osee_hex_btn, FALSE);
	}
	return OK;
}


oret_t
oseeInitGtk()
{
	gtk_signal_connect(GTK_OBJECT(osee_table), "select_row",
					   GTK_SIGNAL_FUNC(oseeRowSelected), NULL);
	gtk_signal_connect(GTK_OBJECT(osee_table), "unselect_row",
					   GTK_SIGNAL_FUNC(oseeRowUnselected), NULL);
	oseeUnselectAll();
	return OK;
}

oret_t
oseeSetDebug(bool_t enable)
{
	if (enable) {
		ologSetOutput(0, OSEE_LOG_FILE, 0);
		ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
		owatch_verb = 8;
	} else {
		ologSetOutput(0, NULL, 0);
		ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_STDOUT | OLOG_FLUSH);
		owatch_verb = 3;
	}
	return OK;
}


oret_t
oseeSetLogging(bool_t enable)
{
	enable = (enable != 0);
	gtk_toggle_button_set_active(osee_log_btn, enable);
	if (enable) {
		if (osee_logging)
			return ERROR;
		ologSetOutput(0, OSEE_LOG_FILE, 0);
		ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
		osee_logging = TRUE;
		olog("-------- LOGGING BEGIN --------");
		oseeLogAllValues();
	} else {
		if (!osee_logging)
			return ERROR;
		oseeLogAllValues();
		olog("-------- LOGGING END --------");
		ologSetOutput(0, NULL, 0);
		ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_STDOUT | OLOG_FLUSH);
		osee_logging = FALSE;
	}
	return OK;
}


oret_t
oseeToggleLogging(void)
{
	return oseeSetLogging(!osee_logging);
}


oret_t
oseeInit()
{
	/*extern bool_t owatch_handlers_are_lazy;
	   owatch_handlers_are_lazy = TRUE; */
	ologOpen("osee1", NULL, 0);
	oseeSetDebug(FALSE);
	oseeInitGtk();
	owatchInit("osee1");
	owatchAddDataHandler("", oseeDataHandler, 0);
	owatchAddAliveHandler(oseeAliveHandler, 0);
	owatchConnect(osee_url);
	gtk_timeout_add(OSEE_BG_PERIOD_MS, oseeBackgroundJob, 0);
	oseeSetLogging(FALSE);
	return OK;
}


oret_t
oseeExit(void *gtk_object_p, void *bool_t_ask)
{
	bool_t  ask = (bool_t) bool_t_ask;
	bool_t  ok = TRUE;

	if (ask)
		ok = oseeQuestion("Are you sure you want to exit ?");
	if (ok) {
		owatchExit();
		puts("good bye");
		exit(0);
	}
	return OK;
}

oret_t
oseeAbout()
{
	static const char *icons[1] = { "ok" };
	static const char *labels[1] = { "OK" };
	oseeRunDialog("info",
		"\n  Optikus See " PACKAGE_VERSION ",  \n  handy data viewer.  \n\n"
		"  " PACKAGE_BUGREPORT "  \n\n  " PACKAGE_COPYRIGHT "  \n"
		"\n  " PACKAGE_WEBSITE "  \n",
		1, icons, labels);
	return OK;
}

oret_t
oseeCheckWrite(const char *desc, char *strval)
{
	char    c;
	int     n;

	if (!*desc || !*strval) {
		oseeSetStatus("syntax error");
		return ERROR;
	}
	c = *strval;
	n = strlen(strval);
	if (c == '"' || c == '\'') {
		if (n < 2 || strval[n - 1] != c) {
			oseeSetStatus("syntax error");
			return ERROR;
		}
		strcpy(strval, strval + 1);
		strval[n - 2] = 0;
	}
	return OK;
}


oret_t
oseeWrite(const char *desc, char *strval, OseeRepeater_t * prep)
{
	OseeReadRecord_t *prec;
	owop_t  op;
	oval_t val;

	if (oseeCheckWrite(desc, strval))
		return ERROR;
	prec = (OseeReadRecord_t *) oxnew(char, sizeof(OseeReadRecord_t)
											+ strlen(desc));
	strcpy(prec->desc, desc);
	prec->ooid = 0;
	prec->row = -1;
	prec->next = NULL;
	prec->repeater = prep;
	val.undef = 0;
	val.time = 0;
	val.type = 's';
	val.len = strlen(strval) + 1;
	val.v.v_str = strval;		/* never write here :) */
	oseeSetStatus("writing{%s=%s}...", desc, strval);
	op = owatchWriteByName(prec->desc, &val);
	owatchLocalOpHandler(op, oseeWriteHandler, (long) prec);
	return OK;
}


oret_t
oseeRead(const char *desc, bool_t as_hex, OseeRepeater_t * prep)
{
	OseeReadRecord_t *prec;
	owop_t  op;
	prec = (OseeReadRecord_t *) oxnew(char, sizeof(OseeReadRecord_t)
											+ strlen(desc));
	strcpy(prec->desc, desc);
	prec->ooid = 0;
	prec->row = -1;
	prec->next = NULL;
	prec->repeater = prep;
	prec->as_hex = as_hex;
	prec->can_be_hex = FALSE;
	oseeSetStatus("reading{%s}...", desc);
	op = owatchAddMonitorByDesc(prec->desc, &prec->ooid, FALSE);
	owatchLocalOpHandler(op, oseeReadHandler, (long) prec);
	return OK;
}


oret_t
oseeShotRead(const char *desc)
{
	OseeShotReadRecord_t *psr;
	owop_t  op;

	psr = oxnew(OseeShotReadRecord_t, 1);
	strcpy(psr->desc, desc);
	oseeSetStatus("1reading{%s}...", desc);
	op = owatchReadByName(psr->desc, &psr->val,
						 psr->data_buf, sizeof(psr->data_buf));
	owatchLocalOpHandler(op, oseeShotReadHandler, (long) psr);
	return OK;
}


oret_t
oseeParseMultiple(const char *desc, OseeRepeater_t * prep)
{
	char    spath[200] = { 0 };
	char   *s, *d, *p;
	int     i, n, imin, imax;

	strcpy(spath, desc);
	s = spath;
	d = prep->format;
	i = 0;
	while (*s) {
		if (*s != '[') {
			if (*s == '?' || *s == ':')
				goto SYNTAX;
			*d++ = *s++;
			continue;
		}
		p = ++s;
		n = 0;
		while (*p >= '0' && *p <= '9') {
			n = n * 10 + (*p - '0');
			p++;
		}
		if (p == s)
			goto SYNTAX;
		imin = imax = n;
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
				goto SYNTAX;
			imax = n;
		}
		if (*p != ']')
			goto SYNTAX;
		s = p + 1;
		if (imin > imax) {
			oseeSetStatus("invalid range: %d > %d", imin, imax);
			goto FAIL;
		}
		if (imin == imax) {
			d += sprintf(d, "[%d]", imin);
			continue;
		}
		if (i >= 4) {
			oseeSetStatus("too many indexes");
			goto FAIL;
		}
		prep->imin[i] = imin;
		prep->imax[i] = imax;
		i++;
		*d++ = '[';
		*d++ = '%';
		*d++ = 'd';
		*d++ = ']';
	}
	*d = 0;
	prep->inum = i;

	while (i < 4) {
		prep->imin[i] = prep->imax[i] = 0;
		i++;
	}
	for (i = 0; i < 4; i++)
		prep->icur[i] = 0;
	for (i = 0; i < prep->inum; i++)
		prep->icur[i] = prep->imin[i];
	if (prep->inum > 0)
		prep->icur[prep->inum - 1] -= 1;
	prep->type = OSEE_DIM_REP;
	prep->ignore = FALSE;
	prep->as_hex = osee_def_hex;
	prep->ready = TRUE;
	return OK;

  SYNTAX:
	oseeSetStatus("syntax error");
	return ERROR;
  FAIL:
	return ERROR;
}


oret_t
oseeReadMultiple(const char *desc)
{
	oret_t rc;
	OseeRepeater_t *prep;

	if (osee_read_rep) {
		oseeSetStatus("still reading... please wait.");
		return ERROR;
	}
	prep = oxnew(OseeRepeater_t, 1);
	prep->inum = prep->format[0] = prep->value[0];
	rc = oseeParseMultiple(desc, prep);
	if (rc != OK) {
		oseeCancelRepeat(prep);
		return ERROR;
	}
	if (prep->inum == 0) {
		oseeCancelRepeat(prep);
		return oseeRead(desc, osee_def_hex, NULL);
	}
	prep->op_type = 1;
	/*osee_read_rep = prep; */
	oseeRepeat(prep);
	return OK;
}


oret_t
oseeWriteMultiple(const char *desc, char *strval)
{
	oret_t rc;
	OseeRepeater_t *prep;

	if (oseeCheckWrite(desc, strval))
		return ERROR;
	if (osee_write_rep) {
		oseeSetStatus("still writing... please wait.");
		return ERROR;
	}
	prep = (OseeRepeater_t *) oxnew(char, sizeof(OseeRepeater_t)
											+ strlen(strval));
	prep->inum = prep->format[0] = prep->value[0];
	strcpy(prep->value, strval);
	rc = oseeParseMultiple(desc, prep);
	if (rc != OK) {
		oseeCancelRepeat(prep);
		return ERROR;
	}
	if (prep->inum == 0) {
		oseeCancelRepeat(prep);
		return oseeWrite(desc, strval, NULL);
	}
	prep->op_type = 2;
	/*osee_write_rep = prep; */
	oseeRepeat(prep);
	return OK;
}


oret_t
oseeAdd()
{
	const char *text;
	char    desc[200];
	char   *s;

	oseeUnselectAll();

	text = gtk_entry_get_text(osee_entry);
	strcpy(desc, text);
	strTrim(desc);
	if (!*desc) {
		oseeSetStatus("---");
		return ERROR;
	}
	s = strchr(desc, '=');
	if (s) {
		*s++ = 0;
		strTrim(desc);
		strTrim(s);
		if (!strcmp(s, "?"))
			return oseeShotRead(desc);
		else if (strchr(desc, ':') || strstr(desc, ".."))
			return oseeWriteMultiple(desc, s);
		else
			return oseeWrite(desc, s, NULL);
	}
	if (strchr(desc, ':') || strstr(desc, ".."))
		return oseeReadMultiple(desc);
	else
		return oseeRead(desc, osee_def_hex, NULL);
	return OK;
}


oret_t
oseeClear()
{
	if (osee_logging) {
		olog("-------- CLEAR --------");
	}
	oseeUnselectAll();
	owatchRemoveAllMonitors();
	gtk_clist_clear(osee_table);
	osee_nrows = 0;
	oseeSetStatus("cleared");
	return OK;
}


oret_t
oseeStart()
{
	if (osee_logging) {
		olog("-------- RESUME --------");
	}
	owatchBlockMonitoring(FALSE, TRUE);
	owatchEnableMonitoring(TRUE);
	oseeSetStatus("start");
	return OK;
}


oret_t
oseeStop()
{
	if (osee_logging) {
		olog("-------- PAUSE --------");
	}
	owatchBlockMonitoring(TRUE, FALSE);
	owatchEnableMonitoring(FALSE);
	oseeSetStatus("stop");
	return OK;
}


void
oseeOpenSelDone(GtkWidget * w, GtkFileSelection * filew)
{
	OseeRepeater_t *prep;
	/* FIXME: magic sizes */
	char    filename[200], line[800], desc[200], shex[20], dpath[200];
	char   *s;
	FILE   *file;
	int     n, row;
	bool_t  as_hex;

	s = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filew));
	strcpy(filename, s ? s : "");
	gtk_widget_destroy(GTK_WIDGET(filew));
	if (!*filename) {
		oseeSetStatus("save aborted");
		return;
	}
	prep = oxnew(OseeRepeater_t, 1);
	prep->type = OSEE_FILE_REP;
	prep->ignore = TRUE;
	prep->op_type = 1;
	prep->names = oxnew(char *, 900);		/* FIXME: magic size */
	prep->as_hexes = oxnew(bool_t, 900);
	prep->ready = TRUE;
	prep->fnum = 0;
	file = fopen(filename, "r");
	if (!file) {
		oseeSetStatus("cannot read from %s", filename);
		return;
	}
	while (!feof(file)) {
		if (!fgets(line, sizeof(line), file))
			break;
		strTrim(line);
		if (*line == ';' || !*line || *line == '\r' || *line == '\t')
			continue;
		n = sscanf(line, "%d %s %s %s", &row, desc, shex, dpath);
		if (n == 4) {
			if (!strcmp(shex, "hex"))
				as_hex = TRUE;
			else if (!strcmp(shex, "dec"))
				as_hex = FALSE;
			else
				n = 0, as_hex = FALSE;
		}
		if (n != 4) {
			olog("wrong line [%s]", line);
			continue;
		}
		prep->names[prep->fnum] = oxstrdup(desc);
		prep->as_hexes[prep->fnum] = as_hex;
		prep->fnum++;
	}
	fclose(file);
	oseeSetStatus("opened %s", filename);
	prep->fcur = -1;
	oseeRepeat(prep);
}


void
oseeSaveSelDone(GtkWidget * w, GtkFileSelection * filew)
{
	OseeReadRecord_t *prec;
	char    message[200];
	char    filename[200];
	char   *s;
	FILE   *file;
	bool_t  exists;
	int     i;

	s = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filew));
	strcpy(filename, s ? s : "");
	gtk_widget_destroy(GTK_WIDGET(filew));
	if (!*filename) {
		oseeSetStatus("save aborted");
		return;
	}
	file = fopen(filename, "r");
	exists = (file != NULL);
	if (file)
		fclose(file);
	if (exists) {
		sprintf(message, "Are you sure to replace \"%s\" ?", filename);
		if (!oseeQuestion(message)) {
			oseeSetStatus("save aborted");
			return;
		}
	}
	file = fopen(filename, "w");
	if (!file) {
		oseeSetStatus("cannot write into %s", filename);
		return;
	}
	fprintf(file, "; Optikus see-1 file v.1\n");
	for (i = 0; i < osee_nrows; i++) {
		prec = (OseeReadRecord_t *) gtk_clist_get_row_data(osee_table, i);
		fprintf(file, "%d %s %s %s\n",
				i, prec->desc, prec->as_hex ? "hex" : "dec", prec->info.desc);
	}
	fclose(file);
	oseeSetStatus("saved %s", filename);
}


oret_t
oseeOpen()
{
	GtkWidget *filew;

	oseeUnselectAll();
	filew = oseeCreateFileSelection("Open file", oseeOpenSelDone);
	oseeSetStatus("opening...");
	return OK;
}


oret_t
oseeSave()
{
	GtkWidget *filew;

	oseeUnselectAll();
	filew = oseeCreateFileSelection("Save to file", oseeSaveSelDone);
	oseeSetStatus("saving...");
	return OK;
}
