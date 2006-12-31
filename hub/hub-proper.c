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

  The main polling loop of the hub daemon.

*/

#include "hub-priv.h"
#include <optikus/util.h>	/* for osMsClock,osMsSleep */
#include <config.h>		/* for package requisites */
#include <stdio.h>		/* for fprintf,fscanf,sprintf */
#include <unistd.h>		/* for getpid,gethostname */
#include <stdlib.h>		/* for exit */
#include <string.h>		/* for strcmp */
#include <errno.h>		/* for errno */


#define OHUB_POLL_INTERVAL_MS	50
#define OHUB_SUBJ_START_DELAY	500
#define OHUB_WATCH_START_DELAY	1000

int     ohub_spin;
int     ohub_dbg[32];
bool_t  ohub_reloaded;
bool_t  ohub_debug_updated;

char    ohub_hub_desc[OHUB_MAX_PATH];

long    ohub_subj_start;
long    ohub_watchers_start;


/*
 *   Initialization of the hub.
 */
oret_t
ohubInit(void)
{
	FILE   *file;
	char    trunc_host[OHUB_MAX_PATH + 4];
	int     pid;

	ohub_watchers_enable = FALSE;
	ohub_subjs_enable = FALSE;
	trunc_host[0] = 0;
	gethostname(trunc_host, OHUB_MAX_PATH);
	trunc_host[OHUB_MAX_NAME] = 0;
	pid = getpid();
	sprintf(ohub_hub_desc, "%s:%d@%s", "ohub", pid, trunc_host);

	if (*ohub_pid_file) {
		file = fopen(ohub_pid_file, "w");
		if (file != NULL) {
			fprintf(file, "%u", (unsigned) getpid());
			fclose(file);
		}
	}
	if (ohubInitOoidFactory(ohub_ooid_cache_file) != OK) {
		olog("ooid factory initialization failure");
		goto FAIL;
	}
	ohubFlushInfoCache();
	if (ohubWatchInitNetwork(ohub_server_port) != OK) {
		olog("watch network initialization failure");
		goto FAIL;
	}
	if (ohub_remote_log_file != NULL && *ohub_remote_log_file != '\0') {
		if (strcmp(ohub_remote_log_file, ohub_log_file) == 0) {
			*ohub_remote_log_file = '\0';
		} else {
			ologSetOutput(1, ohub_remote_log_file, ohub_remote_log_limit);
			ologEnableOutput(1, FALSE);
		}
	}
	if (ohubSubjInitNetwork() != OK) {
		olog("subj network initialization failure");
		goto FAIL;
	}
	if (ohubInitMonitoring() != OK) {
		olog("monitorint initialization failure");
		goto FAIL;
	}
	if (ohubInitData() != OK) {
		olog("data IO initialization failure");
		goto FAIL;
	}
	ohub_subj_start = osMsClock() + OHUB_SUBJ_START_DELAY;
	ohub_watchers_start = osMsClock() + OHUB_WATCH_START_DELAY;
	return OK;

  FAIL:
	ohubExit(ERROR);
	return ERROR;
}


/*
 *   Called at the hub shutdown.
 */
void
ohubExit(int param)
{
	OhubDomain_t *pdom = ohub_pdomain;
	OhubAgent_t *pagent;
	int     i;

	ohub_watchers_enable = FALSE;
	ohub_subjs_enable = FALSE;
	if (!ohub_reloaded)
		fprintf(stdout, "hub exits\n");
	/* disconnect from agents */
	for (i = 0, pagent = pdom->agents; i < pdom->agent_qty; i++, pagent++) {
		if (!pagent->enable)
			continue;
		ohubCancelAgentData(pagent);
		switch (pagent->proto) {
		case OHUB_PROTO_OLANG:
			if (pagent->nsubj_p)
				ohubSubjClose(pagent->nsubj_p);
			pagent->nsubj_p = NULL;
			break;
		default:
			break;
		}
	}
	ohubExitData();
	ohubExitMonitoring();
	ohubSubjShutdownNetwork();
	ohubWatchersShutdownNetwork();
	ohubFlushInfoCache();
	ohubClearSubjectWaiters(NULL, "*");
	ohubExitOoidFactory(param != ERROR ? ohub_ooid_cache_file : "");
	if (!ohub_reloaded) {
		if (*ohub_pid_file)
			remove(ohub_pid_file);
		exit(0);
	}
}


/*
 *  main daemon loop
 */
void
ohubInternalDaemonLoop(int timeout)
{
	long    routine_wd = 0;
	int     i;
	fd_set  set[2];
	struct timeval to, *pto;
	int     nfds, num;
	int     n_events, n_watches, n_subjs;
	long    cur, delta;

	/* enable subj/watcher after a pause */
	if (!ohub_subjs_enable && ohub_subj_start
		&& osMsClock() - ohub_subj_start > 0) {
		ohub_subjs_enable = TRUE;
		ohub_subj_start = 0;
		ohubLog(5, "subj network enabled");
	}
	if (!ohub_watchers_enable && ohub_watchers_start
		&& osMsClock() - ohub_watchers_start > 0) {
		ohub_watchers_enable = TRUE;
		ohub_watchers_start = 0;
		ohubLog(5, "watch network enabled");
	}
	/* activate periodic tasks */
	delta = (cur = osMsClock()) - routine_wd;
	if (delta < 0 || delta > OLANG_STAT_MS || !routine_wd) {
		routine_wd = cur;
		ohubWatchBackgroundTask();
		ohubSubjBackgroundTask();
		if (ohub_spin & 1)
			fprintf(stderr, "&");
	}
	ohubHandleTriggerTimeouts();
	ohubSecureMoreMonitors(ohub_pdomain);
	ohubSecureMoreWrites(ohub_pdomain);
	/* calculate timeout */
	if (timeout < 0)
		pto = NULL;
	else {
		to.tv_sec = timeout / 1000;
		to.tv_usec = (timeout % 1000) * 1000;
		pto = &to;
	}
	/* prepare descriptors */
	FD_ZERO(set + 0);
	FD_ZERO(set + 1);
	nfds = 0;
	nfds = ohubWatchPrepareEvents(set, nfds);
	nfds = ohubSubjPrepareEvents(set, nfds);
	/* poll descriptors */
	ohub_debug_updated = FALSE;
	num = select(nfds + 1, set + 0, set + 1, NULL, pto);
	if (num < 0) {
		if (ohub_spin & 1)
			fprintf(stderr, "?");
		if (ohub_reloaded)
			return;
		if (ohub_debug_updated)
			return;
		ohubLog(2, "select error errno=%d nfds=%d num=%d", errno, nfds, num);
		osMsSleep(timeout);
		return;
	}
	/* handle events */
	n_events = 0;
	if (num > 0) {
		n_watches = ohubWatchHandleEvents(set, num);
		n_subjs = ohubSubjHandleEvents(set, num);
		n_events += n_watches + n_subjs;
		if (ohub_spin & 1) {
			fprintf(stderr, n_watches && n_subjs ? "#"
							: n_watches ? "+" : n_subjs ? "*" : "-");
		}
	} else {
		if (ohub_spin & 1)
			fprintf(stderr, ".");
	}
	ohubWatchLazyShutdown();
	ohubSubjLazyShutdown();
	if (ohub_spin & 2) {
		for (i = 0; i < 32; i++) {
			if (ohub_dbg[i]) {
				fprintf(stderr, "d[%d]", i);
				ohub_dbg[i] = 0;
			}
		}
	}
	ohubLog(12, "next poll timeout=%d nevents=%d", timeout, n_events);
}


/*
 *  daemon with reloading
 */
void
ohubDaemon(void)
{
	OhubDomain_t *pdom = ohub_pdomain;
	OhubAgent_t *pagent;
	int     i;

	/* initialize */
	ohubLog(4, "Optikus hub " PACKAGE_VERSION " " PACKAGE_COPYRIGHT "\n");
	if (!ohub_debug) {
		freopen("/dev/null", "a", stdout);
		freopen("/dev/null", "a", stderr);
	}
	if (ohub_log_file != NULL && *ohub_log_file != '\0'
		&& strcmp(ohub_log_file, "stdout") != 0) {
		ohubLog(5, "redirecting log to %s", ohub_log_file);
		ologSetOutput(0, ohub_log_file, ohub_log_limit);
		ologFlags(OLOG_DATE | OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
	}
	ohubInit();

	for (;;) {
		/* define which agents to connect to */
		for (i = 0, pagent = pdom->agents; i < pdom->agent_qty; i++, pagent++) {
			if (!pagent->enable)
				continue;
			switch (pagent->proto) {
			case OHUB_PROTO_OLANG:
				pagent->nsubj_p = ohubSubjOpen(pagent->url, pagent);
				ohubLog(7, "agent name=[%s] url=[%s] subj=%p",
						pagent->agent_name, pagent->url, pagent->nsubj_p);
				break;
			default:
				break;
			}
		}
		ohub_reloaded = FALSE;
		while (!ohub_reloaded)
			ohubInternalDaemonLoop(OHUB_POLL_INTERVAL_MS);
		ohubLog(3, "reloading...");
		ohubExit(0);
		ohubLoadDomainQuarks(ohub_pdomain);
		ohubInit();
	}
}


void
ohubMarkReload(int spare)
{
	ohub_reloaded = TRUE;
}


void
ohubUpdateDebugging(int spare)
{
	int     num, verb;
	FILE   *file;

	ohub_debug_updated = TRUE;
	if (!*ohub_debug_file) {
		olog("debug file not configured");
		return;
	}
	file = fopen(ohub_debug_file, "r");
	if (!file) {
		olog("cannot open debug file %s", ohub_debug_file);
		return;
	}
	num = fscanf(file, " %d", &verb);
	fclose(file);
	if (num < 1) {
		olog("invalid format of %s", ohub_debug_file);
		return;
	}
	ohub_verb = verb;
	olog("new verbosity = %d", ohub_verb);
}


/*
 *   remote logging.
 */
oret_t
ohubHandleRemoteLog(int type, int len, char *buf, const char *url)
{
	while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\0'))
		len--;
	buf[len] = '\0';

	if (ohub_remote_log_file != NULL && *ohub_remote_log_file != '\0') {
		ologEnableOutput(1, TRUE);
		ologEnableOutput(0, FALSE);
		olog(buf);
		ologEnableOutput(1, FALSE);
		ologEnableOutput(0, TRUE);
	} else {
		olog(buf);
	}

	return OK;
}


/*
 *   handle packets from agents
 */
oret_t
ohubAgentHandleIncoming(OhubAgent_t * pagent,
						int kind, int type, int len, char *buf)
{
	oret_t rc;

	switch (type) {
	case OLANG_SEGMENTS:
		ohubLog(7, "got segments from <%s>", pagent->agent_name);
		rc = ohubHandleSegments(pagent, kind, type, len, buf);
		return OK;
	case OLANG_DATA_REG_ACK:
		ohubLog(8, "got data_reg_ack from <%s>", pagent->agent_name);
		if (--pagent->cur_reg_count < 0)
			pagent->cur_reg_count = 0;
		return OK;
	case OLANG_DATA_REG_NAK:
		ohubLog(8, "got data_reg_nak from <%s>", pagent->agent_name);
		if (--pagent->cur_reg_count < 0)
			pagent->cur_reg_count = 0;
		return OK;
	case OLANG_DATA_UNREG_ACK:
		ohubLog(8, "got data_unreg_ack from <%s>", pagent->agent_name);
		return OK;
	case OLANG_DATA_UNREG_NAK:
		ohubLog(8, "got data_unreg_nak from <%s>", pagent->agent_name);
		return OK;
	case OLANG_DATA_MON_REPLY:
		ohubLog(9, "got data_mon_reply from <%s>", pagent->agent_name);
		rc = ohubHandleDataMonReply(pagent, kind, type, len, buf);
		return OK;
	case OLANG_MONITORING:
	case OLANG_SEL_MONITORING:
		if (ohub_spin & 4)
			fprintf(stderr, "$");
		rc = ohubHandleMonitoring(kind, type, len, buf, pagent);
		return OK;
	case OLANG_DATA_WRITE_REPLY:
		ohubLog(8, "got data_write from <%s>", pagent->agent_name);
		rc = ohubHandleDataWrite(pagent, kind, type, len, buf);
		if (--pagent->cur_write_count < 0)
			pagent->cur_write_count = 0;
		return OK;
	case OLANG_DATA_READ_REPLY:
		ohubLog(8, "got data_read from <%s>", pagent->agent_name);
		rc = ohubHandleDataRead(pagent, kind, type, len, buf);
		return OK;
	case OLANG_MSG_CLASS_ACT:
		ohubLog(7, "got msg_class from [%s]", pagent->agent_name);
		rc = ohubHandleMsgClassAct(NULL, pagent, kind, type, len, buf,
									pagent->url);
		return OK;
	case OLANG_MSG_SEND_REQ:
		ohubLog(7, "got msg_send from [%s]", pagent->url);
		rc = ohubHandleMsgSend(NULL, pagent, kind, type, len, buf, pagent->url);
		return OK;
	case OLANG_MSG_HIMARK:
	case OLANG_MSG_LOMARK:
		ohubLog(7, "got watermark from [%s]", pagent->url);
		rc = ohubHandleMsgWaterMark(NULL, pagent, kind, type, len, buf,
									pagent->url);
		return OK;
	case OLANG_REMOTE_LOG:
		rc = ohubHandleRemoteLog(type, len, buf, pagent->url);
		return OK;
	default:
		ohubLog(4, "unknown packet kind=%d type=%04xh len=%d from <%s>",
				kind, type, len, pagent->agent_name);
		return ERROR;
	}
}


/*
 *   handle packets from watchers
 */
oret_t
ohubWatchHandleIncoming(struct OhubNetWatch_st * pnwatch,
						ushort_t watch_id, const char *watch_url,
						int kind, int type, int len, char *buf)
{
	oret_t rc;

	switch (type) {
	case OLANG_INFO_BY_DESC_REQ:
		rc = ohubHandleInfoReq(pnwatch, kind, type, len, buf);
		ohubLog(8, "info_by_desc reply to [%s] rc=%d", watch_url, rc);
		return OK;
	case OLANG_INFO_BY_OOID_REQ:
		rc = ohubHandleInfoReq(pnwatch, kind, type, len, buf);
		ohubLog(8, "info_by_ooid reply to [%s] rc=%d", watch_url, rc);
		return OK;
	case OLANG_MON_REG_REQ:
		ohubLog(7, "got mon_reg from [%s]", watch_url);
		rc = ohubHandleMonRegistration(pnwatch, watch_url, kind, type,
										len, buf);
		return OK;
	case OLANG_MON_UNREG_REQ:
		ohubLog(7, "got mon_unreg from [%s]", watch_url);
		rc = ohubHandleMonRegistration(pnwatch, watch_url,
										kind, type, len, buf);
		return OK;
	case OLANG_MON_COMBO_REQ:
		ohubLog(7, "got mon_combo_req from [%s]", watch_url);
		rc = ohubHandleMonComboRequest(pnwatch, watch_url, kind, type,
										len, buf);
		return OK;
	case OLANG_MON_GROUP_REQ:
		ohubLog(7, "got mon_group_req from [%s]", watch_url);
		rc = ohubHandleMonGroupReq(pnwatch, watch_url, kind, type, len, buf);
		return OK;
	case OLANG_OOID_WRITE_REQ:
		ohubLog(7, "got ooid_write from [%s]", watch_url);
		rc = ohubHandleOoidWrite(pnwatch, watch_id, watch_url, kind, type,
								len, buf);
		return OK;
	case OLANG_OOID_READ_REQ:
		ohubLog(7, "got ooid_read from [%s]", watch_url);
		rc = ohubHandleOoidRead(pnwatch, watch_id, watch_url, kind, type,
								len, buf);
		return OK;
	case OLANG_TRIGGER_REQUEST:
		ohubLog(7, "got trigger_req from [%s]", watch_url);
		rc = ohubHandleTriggerSubject(pnwatch, watch_id, watch_url, kind,
										type, len, buf);
		return OK;
	case OLANG_MSG_CLASS_ACT:
		ohubLog(7, "got msg_class from [%s]", watch_url);
		rc = ohubHandleMsgClassAct(pnwatch, NULL, kind, type, len, buf,
									watch_url);
		return OK;
	case OLANG_MSG_SEND_REQ:
		ohubLog(7, "got msg_send from [%s]", watch_url);
		rc = ohubHandleMsgSend(pnwatch, NULL, kind, type, len, buf, watch_url);
		return OK;
	case OLANG_MSG_HIMARK:
	case OLANG_MSG_LOMARK:
		ohubLog(7, "got watermark from [%s]", watch_url);
		rc = ohubHandleMsgWaterMark(pnwatch, NULL, kind, type, len, buf,
									watch_url);
		return OK;
	case OLANG_REMOTE_LOG:
		rc = ohubHandleRemoteLog(type, len, buf, watch_url);
		return OK;
	default:
		ohubLog(5, "unknown packet kind=%d type=%04xh len=%d from [%s]",
				kind, type, len, watch_url);
		return ERROR;
	}
}
