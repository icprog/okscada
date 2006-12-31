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

  Core client API: initialization, shutdown and main loop.

*/

#include "watch-priv.h"
#include <optikus/util.h>		/* for osMsClock */

#include <stdio.h>		/* for sprintf */
#include <unistd.h>		/* for getpid,gethostname */
#include <string.h>		/* for strncpy */


int     owatch_verb = 1;

char    owatch_watcher_desc[OWATCH_MAX_PATH];

bool_t  owatch_initialized;

oret_t
owatchInit(const char *client_name)
{
	char    trunc_name[OWATCH_MAX_NAME + 4];
	char    trunc_host[OWATCH_MAX_PATH + 4];
	int     pid;

	if (NULL == client_name || '\0' == client_name) {
		return ERROR;
	}
	trunc_name[0] = 0;
	strncpy(trunc_name, client_name, OWATCH_MAX_NAME);
	trunc_name[OWATCH_MAX_NAME] = 0;
	trunc_host[0] = 0;
	gethostname(trunc_host, OWATCH_MAX_PATH);
	trunc_host[OWATCH_MAX_NAME] = 0;
	pid = getpid();
	sprintf(owatch_watcher_desc, "%s/%u@%s", trunc_name, pid, trunc_host);
	owatchExit();
	if (owatchInitOperations())
		goto FAIL;
	if (owatchInitSubjects())
		goto FAIL;
	if (owatchSetupMessages())
		goto FAIL;
	if (owatchInitMonitoring())
		goto FAIL;
	owatchFlushInfoCache();
	owatchSetAsyncTimeout(OWATCH_NOWAIT);
	owatchEnableMonitoring(TRUE);
	owatch_initialized = TRUE;
	return OK;
  FAIL:
	owatchExit();
	return ERROR;
}


oret_t
owatchExit()
{
	owatchClearTriggers();
	owatchEnableMonitoring(FALSE);
	owatchClearMessages();
	owatchDisconnect();
	owatchExitMonitoring();
	owatchFlushInfoCache();
	owatchExitSubjects();
	owatchExitOperations();
	owatchRemoteLog(FALSE);
	owatch_initialized = FALSE;
	return OK;
}


oret_t
owatchWork(int timeout)
{
	oret_t rc;
	long    end, now, d;

	if (timeout == OWATCH_NOWAIT) {
		rc = owatchInternalPoll(timeout);
		owatchPerformLazyCalls();
		owatchCallIdleHandler();
		return OK;
	}
	if (timeout == OWATCH_FOREVER) {
		rc = owatchInternalPoll(-1);
		owatchPerformLazyCalls();
		owatchCallIdleHandler();
		return OK;
	}
	if (timeout < 0)
		return ERROR;
	now = osMsClock();
	end = now + timeout;
	d = end - now;
	while (d > 0) {
		if (d > OWATCH_POLL_INTERVAL_MS)
			d = OWATCH_POLL_INTERVAL_MS;
		owatchLog(14, "poll for %ldms", d);
		rc = owatchInternalPoll(d);
		owatchPerformLazyCalls();
		owatchCallIdleHandler();
		now = osMsClock();
		d = end - now;
	}
	return OK;
}


oret_t
owatchHandleIncoming(int kind, int type, int len, char *buf)
{
	switch (type) {
	case OLANG_SUBJECTS:
		owatchLog(8, "got subjects");
		owatchHandleSubjects(kind, type, len, buf);
		return OK;
	case OLANG_INFO_BY_ANY_REPLY:
		owatchLog(8, "got inf_req reply");
		owatchHandleGetInfoReply(kind, type, len, buf);
		return OK;
	case OLANG_MON_REG_ACK:
	case OLANG_MON_REG_NAK:
	case OLANG_MON_UNREG_ACK:
	case OLANG_MON_UNREG_NAK:
		owatchHandleRegistration(kind, type, len, buf);
		return OK;
	case OLANG_MON_COMBO_REPLY:
		owatchHandleMonComboReply(kind, type, len, buf);
		return OK;
	case OLANG_MONITORING:
		owatchLog(9, "got monitoring");
		owatchHandleMonitoring(kind, type, len, buf);
		return OK;
	case OLANG_OOID_WRITE_REPLY:
		owatchLog(8, "got write reply");
		owatchHandleWriteReply(kind, type, len, buf);
		return OK;
	case OLANG_OOID_READ_REPLY:
		owatchLog(8, "got read reply");
		owatchHandleReadReply(kind, type, len, buf);
		return OK;
	case OLANG_TRIGGER_REPLY:
		owatchLog(8, "got trigger reply");
		owatchHandleTriggerReply(kind, type, len, buf);
		return OK;
	case OLANG_MSG_SEND_REPLY:
		owatchLog(8, "got msg send reply");
		owatchHandleMsgSendReply(kind, type, len, buf);
		return OK;
	case OLANG_MSG_RECV:
		owatchLog(8, "got msg recv");
		owatchHandleMsgRecv(kind, type, len, buf);
		return OK;

	default:
		owatchLog(4, "unknown packet kind=%d type=%04xh len=%d",
					kind, type, len);
		return ERROR;
	}
}
