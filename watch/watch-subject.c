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

  Subject watch handlers and callbacks.

*/

#include "watch-priv.h"
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for ONTOHL,... */
#include <optikus/conf-mem.h>	/* for oxrenew,oxnew,oxfree,oxbcopy */
#include <string.h>				/* for strlen,strcpy */

typedef struct
{
	char    name[OWATCH_MAX_NAME];
	char    state[OWATCH_MAX_NAME];	/* state: "" = off, "name" = on,
									 * "othername" - channel switch */
	char    availability;			/* '0' - off, 'P' - permanent,
									 * 'T' - temporary, '\0' - don't care */
} OwatchSubjectRecord_t;

OwatchSubjectRecord_t *owatch_subject;
int     owatch_subject_qty;

typedef struct
{
	OwatchAliveHandler_t phand;
	long    param;
} OwatchAliveHandlerRecord_t;

OwatchAliveHandlerRecord_t *owatch_alive_handlers;

bool_t  owatch_conn_is_alive;
bool_t  owatch_domain_is_full;


#define OWATCH_MAX_TRIGGER         16
#define OWATCH_TRIGGER_TIMEOUT_MS  320

typedef struct
{
	owop_t  op;
	long    watchdog;
	char    subject[OWATCH_MAX_NAME];
} OwatchTriggerSubject_t;

OwatchTriggerSubject_t owatch_triggers[OWATCH_MAX_TRIGGER];
int     owatch_trigger_qty = 0;


oret_t
owatchInitSubjects(void)
{
	owatch_alive_handlers = oxnew(OwatchAliveHandlerRecord_t,
								  OWATCH_MAX_ALIVE_HANDLERS);
	if (NULL == owatch_alive_handlers)
		return ERROR;
	return OK;
}


oret_t
owatchExitSubjects(void)
{
	int     i;

	owatch_conn_is_alive = FALSE;
	owatch_domain_is_full = FALSE;
	if (NULL != owatch_alive_handlers) {
		for (i = 0; i < OWATCH_MAX_ALIVE_HANDLERS; i++)
			owatch_alive_handlers[i].phand = NULL;
	}
	oxfree(owatch_alive_handlers);
	owatch_alive_handlers = NULL;

	owatch_subject_qty = 0;
	oxfree(owatch_subject);
	owatch_subject = NULL;

	return OK;
}


oret_t
owatchHandleSubjects(int kind, int type, int len, char *buf)
{
	int     i, n, p, q, f;
	OwatchSubjectRecord_t tmp;

	/* first pass: check packet validity */
	if (owatch_verb > 8) {
		buf[len] = 0;
		olog("got subjects: [%s]", buf);
	}
	p = 0;
	f = buf[p++] - 40;
	n = buf[p++] - 40;
	if (n < 1 || f < 0 || f > 1)
		goto FAIL;
	for (i = 0; i < n + n; i++) {
		q = buf[p++] - 40;
		p += q + 1;
		if ((i % 2) == 0)
			p += 2;
		if (q > OWATCH_MAX_NAME || q < 0 || p > len) {
			owatchLog(8, "bad subject in packet: q=%d p=%d len=%d", q, p, len);
			goto FAIL;
		}
	}
	/* second pass: actually work on packet */
	p = 0;
	f = buf[p++] - 40;
	n = buf[p++] - 40;
	if (f) {
		/* server asks us to do clean setup */
		oxfree(owatch_subject);
		owatch_subject = NULL;
		owatch_subject_qty = 0;
	}
	for (i = 0; i < n; i++) {
		/* get name */
		q = buf[p++] - 40;
		oxbcopy(&buf[p], tmp.name, q);
		tmp.name[q] = 0;
		p += q + 1;
		/* get availability */
		tmp.availability = buf[p];
		p += 2;
		/* get state */
		q = buf[p++] - 40;
		oxbcopy(&buf[p], tmp.state, q);
		tmp.state[q] = 0;
		p += q + 1;
		owatchSetSubjectState(tmp.name, tmp.state, tmp.availability);
	}
	return OK;
  FAIL:
	owatchLog(4, "invalid subject packet");
	return ERROR;
}


static void
owatchLazyCallAliveHandlers(const char *name, const char *state)
{
	if (owatch_handlers_are_lazy) {
		int     nl = strlen(name);
		int     sl = strlen(state);
		char   *p = oxnew(char, nl + sl + 2);

		strcpy(p + 0, name);
		strcpy(p + nl + 1, state);
		owatchLog(8, "recording subject name=[%s] state=[%s]", name, state);
		owatchRecordLazyCall(2, (long) p);
	} else {
		int     i;
		OwatchAliveHandlerRecord_t *hrec;

		hrec = &owatch_alive_handlers[0];
		owatchLog(5, "subject name=[%s] state=[%s]", name, state);
		for (i = 0; i < OWATCH_MAX_ALIVE_HANDLERS; i++, hrec++)
			if (hrec->phand != NULL)
				(*hrec->phand) (hrec->param, name, state);
	}
}


void
owatchCallAliveHandlers(long p)
{
	char   *name = (char *) p;
	char   *state = name + strlen(name) + 1;
	int     i;
	OwatchAliveHandlerRecord_t *hrec;

	owatchLog(5, "calling subject name=[%s] state=[%s]", name, state);
	hrec = &owatch_alive_handlers[0];
	for (i = 0; i < OWATCH_MAX_ALIVE_HANDLERS; i++, hrec++)
		if (hrec->phand != NULL)
			(*hrec->phand) (hrec->param, name, state);
	oxfree(name);
}


/*
 *   name:  subject name
 *   state: "" = off, same as name = on, other string = multichannel switch
 *   availability: '0' - off, 'T' - temporary, 'P' - permanent,
 *                 '?' - dont care, '\0' - not changed
 */
oret_t
owatchSetSubjectState(const char *name, const char *state, char availability)
{
	OwatchSubjectRecord_t *new_arr;
	bool_t  alive, removal, permanent;
	int     i, j;

	if (!name || !*name)
		return ERROR;
	owatchLog(8, "got subject name=[%s] state=[%s] avail=[%c]",
				name, state, availability ? availability : '.');
	if (0 == strcmp(name, "*")) {
		alive = state != NULL && *state != 0;
		if (alive == owatch_conn_is_alive)
			return OK;
		owatch_conn_is_alive = alive;
		state = alive ? "*" : "";
		if (!alive) {
			owatchFlushInfoCache();
		}
		owatchLazyCallAliveHandlers(name, state);
		return OK;
	}
	for (i = 0; i < owatch_subject_qty; i++) {
		if (0 == strcmp(owatch_subject[i].name, name))
			break;
	}
	removal = FALSE;
	if (!state) {
		/* it means we should remove the subject */
		removal = TRUE;
		state = "";
	}
	if (i >= owatch_subject_qty) {
		if (removal)
			return ERROR;

		/* add a new subject */
		new_arr = oxrenew(OwatchSubjectRecord_t, owatch_subject_qty + 1,
							owatch_subject_qty, owatch_subject);
		if (NULL == new_arr) {
			owatchLog(1, "subjects out of memory");
			return ERROR;
		}
		owatch_subject = new_arr;
		i = owatch_subject_qty++;
		strcpy(owatch_subject[i].name, name);

		/* there should be a change */
		owatch_subject[i].state[0] = (char) (state[0] + 1);
		owatch_subject[i].state[1] = 0;
	}
	/* change state of existing or just created subject */
	alive = (*state != 0);
	if (availability != '\0') {
		owatch_subject[i].availability = availability;
	}
	permanent = owatch_subject[i].availability == 'P';
	if (strcmp(state, owatch_subject[i].state) != 0) {
		if (owatch_domain_is_full && !alive && permanent) {
			owatch_domain_is_full = FALSE;
			owatchLazyCallAliveHandlers("+", "");
		}
		strcpy(owatch_subject[i].state, state);
		owatchLazyCallAliveHandlers(name, state);
		if (!owatch_domain_is_full && alive && permanent) {
			owatch_domain_is_full = TRUE;
			owatchLazyCallAliveHandlers("+", "+");
		}
	}
	if (removal) {
		for (j = i; j < owatch_subject_qty; j++) {
			strcpy(owatch_subject[j].name, owatch_subject[j + 1].name);
			strcpy(owatch_subject[j].state, owatch_subject[j + 1].state);
		}
		owatch_subject_qty--;
	}
	return OK;
}


oret_t
owatchGetSubjectList(char *list_buffer, int buf_len)
{
	const char *s;
	int     i, n, k;

	if (!list_buffer)
		return ERROR;
	*list_buffer = 0;
	k = owatch_subject_qty - 1;
	for (i = 0; i <= k; i++) {
		s = owatch_subject[i].name;
		n = strlen(s) + 1;
		if (n >= buf_len) {
			list_buffer[0] = 0;
			return ERROR;
		}
		strcat(list_buffer, s);
		if (i < k)
			strcat(list_buffer, " ");
		buf_len -= n;
	}
	return OK;
}


const char *
owatchGetSubjectState(const char *subject_name)
{
	int     i;

	if (NULL == subject_name || '\0' == *subject_name)
		return "";
	if (!owatch_initialized)
		return "";
	if (!strcmp(subject_name, "?"))		/* FIXME: undocumented option */
		return "?";
	if (!strcmp(subject_name, "*"))
		return (owatch_conn_is_alive ? "*" : "");
	if (!strcmp(subject_name, "+"))
		return (owatch_domain_is_full ? "+" : "");
	for (i = 0; i < owatch_subject_qty; i++)
		if (0 == strcmp(owatch_subject[i].name, subject_name))
			return (owatch_subject[i].state);
	return "";
}


bool_t
owatchGetAlive(const char *subject_name)
{
	const char *s = owatchGetSubjectState(subject_name);

	return (s != NULL && *s != '\0');
}


oret_t
owatchAddAliveHandler(OwatchAliveHandler_t phand, long param)
{
	int     i;

	if (!owatch_alive_handlers || !phand)
		return ERROR;
	for (i = 0; i < OWATCH_MAX_ALIVE_HANDLERS; i++) {
		if (!owatch_alive_handlers[i].phand) {
			owatch_alive_handlers[i].param = param;
			owatch_alive_handlers[i].phand = phand;
			return OK;
		}
	}
	return ERROR;
}


oret_t
owatchRemoveAliveHandler(OwatchAliveHandler_t phand)
{
	int     i, n;

	if (!owatch_alive_handlers || !phand)
		return ERROR;
	for (i = n = 0; i < OWATCH_MAX_ALIVE_HANDLERS; i++) {
		if (owatch_alive_handlers[i].phand == phand) {
			owatch_alive_handlers[i].phand = NULL;
			n++;
		}
	}
	return n ? OK : ERROR;
}


/*
 *  force hub to quickly connect to the subject
 */

oret_t
owatchClearTriggers(void)
{
	int     no;

	for (no = 0; no < OWATCH_MAX_TRIGGER; no++) {
		owatch_triggers[no].op = OWOP_NULL;
		owatch_triggers[no].watchdog = 0;
	}
	owatch_trigger_qty = 0;
	return OK;
}


oret_t
owatchCancelAllTriggers(void)
{
	int     no;

	for (no = 0; no < OWATCH_MAX_TRIGGER; no++)
		if (owatch_triggers[no].op != OWOP_NULL) {
			owatchFinalizeOp(owatch_triggers[no].op, OWATCH_ERR_NETWORK);
			owatch_triggers[no].op = OWOP_NULL;
			owatch_triggers[no].watchdog = 0;
		}
	owatch_trigger_qty = 0;
	return OK;
}


oret_t
owatchHandleTriggerReply(int kind, int type, int len, char *buf)
{
	owop_t  op;
	ulong_t ulv;
	ushort_t usv;
	int     no, err;
	long    data1, data2;

	if (len != 6) {
		err = OWATCH_ERR_SCREWED;
		return ERROR;
	}
	ulv = *(ulong_t *) (buf + 0);
	ulv = ONTOHL(ulv);
	op = (owop_t) ulv;
	usv = *(ushort_t *) (buf + 4);
	usv = ONTOHS(usv);
	err = (int) (short) usv;
	if (!owatchIsValidOp(op)) {
		err = OWATCH_ERR_TOOLATE;
		owatchLog(5, "TRIGS(%xh) too late", op);
		return ERROR;
	}
	data1 = data2 = 0;
	owatchGetOpData(op, &data1, &data2);
	no = (int) data2;
	if (data1 != OWATCH_OPT_TRIGGER
		|| no < 1 || no >= OWATCH_MAX_TRIGGER
		|| owatch_triggers[no].op == OWOP_NULL || owatch_triggers[no].op != op) {
		owatchLog(5, "TRIGS(%xh) op not TRIGS data1=%ld data2=%ld",
					op, data1, data2);
		return ERROR;
	}
	owatchLog(6, "TRIGS(%xh) op retval=%d", op, err);
	switch (err) {
	case 0:
		err = OWATCH_ERR_OK;
		break;					/* successfully attached to subject */
	case 1:
		err = OWATCH_ERR_NOTFOUND;
		break;					/* no such subject */
	case 2:
		err = OWATCH_ERR_REFUSED;
		break;					/* cannot attach to subject */
	case 3:
		err = OWATCH_ERR_INTERNAL;
		break;					/* problems in hub */
	default:
		err = OWATCH_ERR_SCREWED;
		break;					/* unknown answer from hub */
	}
	owatchLog(7, "trigger [%s] reply op=%x err=%d qty=%d",
				owatch_triggers[no].subject, op, err, owatch_trigger_qty - 1);
	owatch_triggers[no].op = OWOP_NULL;
	owatch_triggers[no].watchdog = 0;
	owatch_trigger_qty--;
	owatchFinalizeOp(op, err);
	return OK;
}


oret_t
owatchHandleTriggerTimeouts(void)
{
	int     no;
	long    now, wdog;
	owop_t  op;

	if (owatch_trigger_qty <= 0)
		return OK;
	now = osMsClock();
	for (no = 0; no < OWATCH_MAX_TRIGGER; no++) {
		op = owatch_triggers[no].op;
		if (op == OWOP_NULL)
			continue;
		wdog = owatch_triggers[no].watchdog;
		owatchLog(9, "checking trigger [%s] op=%x now=%ld wd=%ld delta=%ld",
					owatch_triggers[no].subject, op, now, wdog, now - wdog);
		if (now - wdog < 0)
			continue;
		owatchLog(6, "triggering [%s] timeout op=%x now=%ld wdog=%ld",
					owatch_triggers[no].subject, op, now, wdog);
		owatchFinalizeOp(op, OWATCH_ERR_TOOLATE);	/* answer timeout */
		owatch_triggers[no].op = OWOP_NULL;
		owatch_triggers[no].watchdog = 0;
		if (--owatch_trigger_qty <= 0)
			break;
	}
	return OK;
}


int
owatchTriggerSubjectCanceller(owop_t op, long data1, long data2)
{
	int     no = (int) data2;

	owatch_triggers[no].op = OWOP_NULL;
	owatch_triggers[no].watchdog = 0;
	owatch_trigger_qty--;
	return 0;
}


owop_t
owatchTriggerSubject(const char *subject)
{
	owop_t  op;
	oret_t rc;
	int     i, no;
	char    buf[OWATCH_MAX_NAME + 8];
	long    now, wdog;

	if (!subject || !*subject) {
		owatchLog(7, "null subject to trigger");
		return OWOP_ERROR;
	}
	if (strlen(subject) + 1 >= OWATCH_MAX_NAME) {
		owatchLog(7, "huge subject name to trigger");
		return OWOP_ERROR;
	}
	for (i = 0; i < owatch_subject_qty; i++)
		if (!strcmp(owatch_subject[i].name, subject)) {
			if (owatch_subject[i].state[0] != 0) {
				owatchLog(7, "subject [%s] already OK", subject);
				return OWOP_OK;
			}
			break;
		}
	if (i >= owatch_subject_qty) {
		owatchLog(7, "subject [%s] not found", subject);
		return OWOP_ERROR;
	}
	for (no = 1; no < OWATCH_MAX_TRIGGER; no++)
		if (owatch_triggers[no].op != OWOP_NULL
			&& strcmp(owatch_triggers[no].subject, subject) == 0) {
			owatchLog(7, "already triggering subject [%s]", subject);
			return OWOP_ERROR;
		}
	for (no = 1; no < OWATCH_MAX_TRIGGER; no++)
		if (owatch_triggers[no].op == OWOP_NULL) {
			owatch_triggers[no].op = OWOP_OK;
			owatch_triggers[no].watchdog = 0;
			break;
		}
	if (no >= OWATCH_MAX_TRIGGER) {
		owatchLog(7, "out of descriptors, cannot trigger [%s]", subject);
		return OWOP_ERROR;
	}
	op = owatchAllocateOp(owatchTriggerSubjectCanceller, OWATCH_OPT_TRIGGER, no);
	if (op == OWOP_ERROR) {
		owatch_triggers[no].op = OWOP_NULL;
		owatchLog(7, "out of ops, cannot trigger [%s]", subject);
		return OWOP_ERROR;
	}
	*(ulong_t *) (buf + 0) = OHTONL((ulong_t) op);
	strcpy(buf + 4, subject);
	rc = owatchSendPacket(OLANG_DATA, OLANG_TRIGGER_REQUEST,
						 4 + strlen(buf + 4), buf);
	if (rc != OK) {
		owatchFinalizeOp(op, OWATCH_ERR_NETWORK);
		owatch_triggers[no].op = OWOP_NULL;
		owatchLog(7, "network error, cannot trigger [%s]", subject);
		return op;
	}
	now = osMsClock();
	wdog = now + OWATCH_TRIGGER_TIMEOUT_MS;
	owatch_triggers[no].op = op;
	owatch_triggers[no].watchdog = wdog;
	strcpy(owatch_triggers[no].subject, subject);
	owatch_trigger_qty++;
	owatchLog(7, "triggering [%s] op=%x data1=%d data2=%d now=%ld wdog=%ld",
				subject, op, OWATCH_OPT_TRIGGER, no, now, wdog);
	return op;
}
