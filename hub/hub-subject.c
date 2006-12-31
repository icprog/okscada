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

  Subject handling.

*/

#include "hub-priv.h"
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for OHTONL,OHTONS */
#include <optikus/conf-mem.h>	/* for oxnew,oxfree,oxazero */
#include <string.h>				/* for memcpy,strcpy,strlen */


#define UNDEFINED_STATE   "???????"

#define OHUB_TRIGGER_TIMEOUT_MS  240

int     ohub_subject_waiters_qty;


/*
 * .
 */
oret_t
ohubSetSubjectStateByRef(OhubSubject_t * psubject, const char *state,
						  bool_t * changed_p)
{
	OhubDomain_t *pdom;
	OhubModule_t *pmod;
	int    *subj_mods;
	int     subj_mod_qty;
	bool_t  is_off;
	int     i, j;

	if (strcmp(psubject->subject_state, state) == 0)
		return OK;
	strcpy(psubject->subject_state, state);
	if (changed_p)
		*changed_p = TRUE;
	is_off = ('\0' == *state);
	ohubLog(5, "subject '%s' is [%s]",
			psubject->subject_name, psubject->subject_state);
	pdom = psubject->pagent->pdomain;
	subj_mods = oxnew(int, pdom->module_qty);

	subj_mod_qty = 0;
	pmod = &pdom->modules[0];
	for (i = 0; i < pdom->module_qty; i++, pmod++) {
		if (pmod->psubject == psubject) {
			subj_mods[subj_mod_qty++] = i;
			if (is_off) {
				/* segments become undefined */
				for (j = 0; j < SEG_MAX_NO; j++)
					pmod->segment_addr[j] = -1;
			}
		}
	}
	ohubRegisterMsgNode(NULL, psubject,
						is_off ? OHUB_MSGNODE_REMOVE : OHUB_MSGNODE_ADD,
						psubject->ikey, psubject->subject_name);
	ohubUpdateMonAddresses(pdom, subj_mods, subj_mod_qty);
	ohubUpdateDataAddresses(pdom, subj_mods, subj_mod_qty);
	oxfree(subj_mods);
	return OK;
}


oret_t
ohubSetAgentState(OhubAgent_t * pagent, const char *state)
{
	OhubDomain_t *pdom;
	OhubSubject_t *psubject;
	int     i;
	bool_t  is_undef = (strcmp(state, UNDEFINED_STATE) == 0);
	bool_t  is_off = (state[0] == 0);
	bool_t  is_on = (!is_undef && !is_off);
	bool_t  f_changed = FALSE;
	int     j, qty;
	char    reply[8];

	if (NULL == pagent)
		return ERROR;
	pdom = pagent->pdomain;
	if (strcmp(state, pagent->agent_state) == 0 && !is_undef)
		return OK;
	if (is_undef) {
		state = UNDEFINED_STATE;
		strcpy(pagent->agent_state, state);
	} else if (is_off) {
		state = "";
		strcpy(pagent->agent_state, state);
		ohubLog(5, "agent <%s> is OFF", pagent->agent_name);
		ohubCancelAgentData(pagent);
		pagent->cur_reg_count = 0;
		pagent->cur_write_count = 0;
	} else {
		/* agent is ON */
		state = "ON";
		strcpy(pagent->agent_state, state);
		ohubLog(5, "agent <%s> is ON", pagent->agent_name);
		pagent->cur_reg_count = 0;
		pagent->cur_write_count = 0;
	}
	if (!is_on) {
		for (i = 0, psubject = pdom->subjects;
				i < pdom->subject_qty; i++, psubject++) {
			if (psubject->pagent == pagent) {
				if (is_undef)
					strcpy(psubject->subject_state, UNDEFINED_STATE);
				else if (is_off)
					ohubSetSubjectStateByRef(psubject, "", &f_changed);
			}
		}
		if (f_changed) {
			ohubSendSubjects(NULL, FALSE, "*");
		}
	}
	if (is_on && ohub_subject_waiters_qty > 0) {
		qty = 0;
		for (i = 0, psubject = pdom->subjects;
				i < pdom->subject_qty; i++, psubject++) {
			if (psubject->pagent != pagent)
				continue;
			if (psubject->waiters_qty < 0)
				psubject->waiters_qty = 0;
			if (!psubject->waiters_qty)
				continue;
			for (j = 0; j < OHUB_MAX_SUBJECT_WAITERS; j++) {
				if (!psubject->waiters[j].pnwatch)
					continue;
				*(ulong_t *) (reply + 0) = OHTONL(psubject->waiters[j].op);
				*(ushort_t *) (reply + 4) = 0;
				ohubWatchSendPacket(psubject->waiters[j].pnwatch, OLANG_DATA,
									OLANG_TRIGGER_REPLY, 6, reply);
				qty++;
				ohub_subject_waiters_qty--;
				psubject->waiters_qty--;
			}
		}
		if (qty > 0) {
			ohubLog(7, "unlocked %d subject waiters", qty);
		}
	}
	return OK;
}


oret_t
ohubSendSubjects(struct OhubNetWatch_st * pnwatch, bool_t initial, const char *url)
{
	OhubDomain_t *pdom = ohub_pdomain;
	OhubSubject_t *psubject;
	oret_t rc;
	char    buf[256];
	char   *s;
	int     i, n, k, p = 0;

	buf[p++] = (char) ((initial ? 1 : 0) + 40);
	buf[p++] = '.';
	for (i = k = 0, psubject = pdom->subjects;
			i < pdom->subject_qty; i++, psubject++) {
		if (!psubject->pagent->enable)
			continue;
		s = psubject->subject_name;
		n = strlen(s);
		if (p + n + 4 > sizeof(buf))
			goto FAIL;
		buf[p++] = n + 40;
		oxbcopy(s, buf + p, n);
		p += n;
		buf[p++] = '/';
		switch (psubject->availability) {
		case OHUB_AVAIL_OFF:
			buf[p++] = '0';
			break;
		case OHUB_AVAIL_TEMP:
			buf[p++] = 'T';
			break;
		case OHUB_AVAIL_PERM:
			buf[p++] = 'P';
			break;
		default:
			buf[p++] = '?';
			break;
		}
		buf[p++] = '=';
		/* FIXME: currently all states are off */
		s = psubject->subject_state;
		n = strlen(s);
		if (p + n + 4 > sizeof(buf))
			goto FAIL;
		buf[p++] = n + 40;
		oxbcopy(s, buf + p, n);
		p += n;
		buf[p++] = ';';
		if (++k > 80) {
			ohubLog(2, "cannot handle more than 80 subjects");
			return ERROR;
		}
	}
	buf[1] = k + 40;
	buf[p] = 0;
	rc = ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_SUBJECTS, p, buf);
	ohubLog(7, "send subjects %s to [%s] rc=%d", buf, url, rc);
	return rc;
  FAIL:
	ohubLog(2, "subject buffer too small");
	return ERROR;
}


oret_t
ohubHandleSegments(OhubAgent_t * pagent, int kind,
					int type, int len, char *buf)
{
	OhubDomain_t *pdom = pagent->pdomain;
	OhubSubject_t *psubject;
	OhubModule_t *pmod;
	int     i, j, k, m, ni, nj, nk, p, q, f, segment_no, l;
	char    subject_name[OHUB_MAX_NAME];
	char    subject_state[OHUB_MAX_NAME];
	char    module_name[OHUB_MAX_NAME];
	char    segment_name;
	ulong_t addr;
	bool_t  f_changed, f_clean, f_found;

	/* first pass: check packet validity */
	p = 0;
	f = buf[p++];
	if (f < 0 || f > 1)
		goto FAIL;
	ni = buf[p++];
	if (ni < 0)
		goto FAIL;
	for (i = 0; i < ni; i++) {
		q = buf[p++];
		p += q;
		if (q >= OHUB_MAX_NAME || q <= 0 || p > len)
			goto FAIL;
		nj = buf[p++];
		if (nj < 0)
			goto FAIL;
		for (j = 0; j < nj; j++) {
			q = buf[p++];
			p += q;
			if (q >= OHUB_MAX_NAME || q <= 0 || p > len)
				goto FAIL;
			nk = buf[p++];
			if (nk < 0)
				goto FAIL;
			for (k = 0; k < nk; k++) {
				segment_name = buf[p++];
				p += 4;
				if (p > len)
					goto FAIL;
			}
		}
	}
	if (buf[p++] != 0)
		goto FAIL;
	if (p != len)
		goto FAIL;
	/* second pass: actually work on packet */
	p = 0;
	f = buf[p++];
	f_clean = (f != 0);
	if (f_clean) {
		/* subj asks us to do clean setup */
		ohubSetAgentState(pagent, UNDEFINED_STATE);
	}
	f_changed = FALSE;
	ni = buf[p++];
	for (i = 0; i < ni; i++) {
		/* get name */
		q = buf[p++];
		if (q)
			oxbcopy(&buf[p], subject_name, q);
		subject_name[q] = 0;
		p += q;
		/* get state */
		strcpy(subject_state, subject_name);
		/* find appropriate subject */
		f_found = FALSE;
		for (psubject = pdom->subjects, m = 0;
				m < pdom->subject_qty; m++, psubject++)
			if (psubject->pagent == pagent &&
				strcmp(psubject->subject_name, subject_name) == 0) {
				f_found = TRUE;
				break;
			}
		if (!f_found) {
			psubject = NULL;
			ohubLog(3, "subject '%s' not found in agent '%s'",
					subject_name, pagent->agent_name);
		}
		/* subject modules and segments */
		nj = buf[p++];
		for (j = 0; j < nj; j++) {
			q = buf[p++];
			oxbcopy(buf + p, module_name, q);
			module_name[q] = 0;
			p += q;
			if (!psubject)
				pmod = NULL;
			else {
				f_found = FALSE;
				for (pmod = pdom->modules, m = 0; m < pdom->module_qty;
					 m++, pmod++)
					if (pmod->psubject == psubject
						&& strcmp(pmod->nick_name, module_name) == 0) {
						f_found = TRUE;
						break;
					}
				if (!f_found) {
					l = strlen(module_name);
					/* try to find without suffix ".o" */
					if (l > 2 && strcmp(module_name + l - 2, ".o") == 0)
						module_name[l - 2] = 0;
					for (pmod = pdom->modules, m = 0; m < pdom->module_qty;
						 m++, pmod++)
						if (pmod->psubject == psubject
							&& strcmp(pmod->nick_name, module_name) == 0) {
							f_found = TRUE;
							break;
						}
				}
				if (!f_found) {
					pmod = NULL;
					ohubLog(3,
						"module='%s' not found in subject='%s', agent='%s'",
						module_name, subject_name, pagent->agent_name);
				}
			}
			nk = buf[p++];
			for (k = 0; k < nk; k++) {
				segment_name = buf[p++];
				memcpy(&addr, buf + p, 4);
				p += 4;
				addr = ONTOHL(addr);
				if (!pmod)
					continue;
				if (SEG_IS_VALID(segment_name)) {
					segment_no = SEG_NAME_2_NO(segment_name);
					pmod->segment_addr[segment_no] = addr;
					ohubLog(6, "got segment='%c' addr=%04lxh mod='%s', "
							"subj='%s', agent='%s'",
							segment_name, addr, module_name, subject_name,
							pagent->agent_name);
				} else {
					ohubLog(3, "invalid segment=%02xh in mod='%s', "
							"subj='%s', agent='%s'",
							(int) (uchar_t) segment_name, module_name,
							subject_name, pagent->agent_name);
				}
			}
		}
		if (NULL != psubject)
			ohubSetSubjectStateByRef(psubject, subject_state, &f_changed);
	}
	for (psubject = pdom->subjects, i = 0;
			i < pdom->subject_qty; i++, psubject++) {
		if (psubject->pagent == pagent &&
			strcmp(psubject->subject_state, UNDEFINED_STATE) == 0) {
			/* subj agent did not know the subject, so it's turned off */
			ohubSetSubjectStateByRef(psubject, "", &f_changed);
		}
	}
	if (f_changed) {
		ohubSendSubjects(NULL, FALSE, "*");
	}
	return OK;
  FAIL:
	ohubLog(3, "invalid segment packet");
	return ERROR;
}


oret_t
ohubHandleTriggerSubject(struct OhubNetWatch_st * pnwatch,
						  ushort_t watch_id, const char *url,
						  int kind, int type, int len, char *buf)
{
	OhubDomain_t *pdom = ohub_pdomain;
	OhubSubject_t *psubject;
	int     i;
	ushort_t err;
	ulong_t op, ulv;
	char    subject[OHUB_MAX_NAME + 4];
	char    reply[8];
	oret_t rc;
	long    now, wdog;

	if (len < 5 || len > OHUB_MAX_NAME)
		return ERROR;
	ulv = *(ulong_t *) (buf + 0);
	*(ulong_t *) (reply + 0) = ulv;
	*(ushort_t *) (reply + 4) = err = 0;
	op = ONTOHL(ulv);
	memcpy(subject, buf + 4, len - 4);
	subject[len - 4] = 0;
	ohubLog(8, "triggering subject [%s] for [%s] op=%lx", subject, url, op);
	for (psubject = pdom->subjects, i = 0;
			i < pdom->subject_qty; i++, psubject++) {
		if (0 == strcmp(psubject->subject_name, subject))
			break;
	}
	if (i >= pdom->subject_qty) {
		err = 1;
		goto REPLY;
	}
	if (psubject->subject_state[0] != 0) {
		err = 0;
		goto REPLY;
	}
	if (!psubject->pagent || !psubject->pagent->nsubj_p) {
		err = 3;
		goto REPLY;
	}
	rc = ohubSubjTrigger(psubject->pagent->nsubj_p);
	if (rc != OK) {
		err = 3;
		goto REPLY;
	}
	if (psubject->waiters_qty >= OHUB_MAX_SUBJECT_WAITERS) {
		err = 3;
		goto REPLY;
	}
	for (i = 0; i < OHUB_MAX_SUBJECT_WAITERS; i++)
		if (!psubject->waiters[i].pnwatch)
			break;
	if (i >= OHUB_MAX_SUBJECT_WAITERS) {
		err = 3;
		goto REPLY;
	}
	now = osMsClock();
	wdog = now + OHUB_TRIGGER_TIMEOUT_MS;
	psubject->waiters[i].pnwatch = pnwatch;
	psubject->waiters[i].op = op;
	psubject->waiters[i].watchdog = wdog;
	psubject->waiters_qty++;
	ohub_subject_waiters_qty++;
	ohubLog(7, "triggered subject [%s] for [%s] op=%lx now=%ld wd=%ld",
			subject, url, op, now, wdog);
	return OK;
  REPLY:
	ohubLog(6, "cannot trigger subject [%s] for [%s] op=%lx err=%d",
			subject, url, op, err);
	*(ushort_t *) (reply + 4) = OHTONS(err);
	ohubWatchSendPacket(pnwatch, OLANG_DATA, OLANG_TRIGGER_REPLY, 6, reply);
	return ERROR;
}


oret_t
ohubClearSubjectWaiters(struct OhubNetWatch_st * pnwatch, const char *url)
{
	OhubDomain_t *pdom = ohub_pdomain;
	OhubSubject_t *psubject;
	int     i, j, qty;

	if (pnwatch != NULL) {
		qty = 0;
		for (i = 0, psubject = pdom->subjects;
				i < pdom->subject_qty; i++, psubject++) {
			if (psubject->waiters_qty < 0)
				psubject->waiters_qty = 0;
			if (!psubject->waiters_qty)
				continue;
			for (j = 0; j < OHUB_MAX_SUBJECT_WAITERS; j++) {
				if (psubject->waiters[j].pnwatch != pnwatch)
					continue;
				psubject->waiters[j].pnwatch = NULL;
				psubject->waiters[j].watchdog = 0;
				qty++;
				ohub_subject_waiters_qty--;
				if (--psubject->waiters_qty <= 0)
					break;
			}
			if (ohub_subject_waiters_qty <= 0)
				break;
		}
	} else {
		for (i = 0, psubject = pdom->subjects;
				i < pdom->subject_qty; i++, psubject++) {
			oxazero(psubject->waiters);
			psubject->waiters_qty = 0;
		}
		ohub_subject_waiters_qty = 0;
	}
	return OK;
}


oret_t
ohubHandleTriggerTimeouts(void)
{
	OhubDomain_t *pdom = ohub_pdomain;
	OhubSubject_t *psubject;
	int     i, j, qty;
	long    now, wdog;
	char    reply[8];

	if (ohub_subject_waiters_qty <= 0)
		return OK;
	qty = 0;
	now = osMsClock();
	for (i = 0, psubject = pdom->subjects;
			i < pdom->subject_qty; i++, psubject++) {
		if (psubject->waiters_qty < 0)
			psubject->waiters_qty = 0;
		if (!psubject->waiters_qty)
			continue;
		for (j = 0; j < OHUB_MAX_SUBJECT_WAITERS; j++) {
			if (NULL == psubject->waiters[j].pnwatch)
				continue;
			wdog = psubject->waiters[j].watchdog;
			ohubLog(10,
					"checking subject [%s] trigger now=%ld wd=%ld delta=%ld",
					psubject->subject_name, now, wdog, now - wdog);
			if (now - wdog < 0)
				continue;
			*(ulong_t *) (reply + 0) = OHTONL(psubject->waiters[j].op);
			*(ushort_t *) (reply + 4) = OHTONS(2);
			ohubLog(7, "subject [%s] now=%ld wd=%ld delta=%ld - TIMEOUT",
					psubject->subject_name, now, wdog, now - wdog);
			ohubWatchSendPacket(psubject->waiters[j].pnwatch, OLANG_DATA,
								OLANG_TRIGGER_REPLY, 6, reply);
			psubject->waiters[j].pnwatch = NULL;
			psubject->waiters[j].watchdog = 0;
			qty++;
			ohub_subject_waiters_qty--;
			if (--psubject->waiters_qty <= 0)
				break;
		}
		if (ohub_subject_waiters_qty <= 0)
			break;
	}
	return OK;
}
