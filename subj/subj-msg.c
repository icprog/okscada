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

  Messaging service.

*/

#include "subj-priv.h"
#include <optikus/conf-net.h>	/* for OUHTONS,... */
#include <string.h>				/* for strlen,memcpy */


/*			Data Types & Local Variables				*/

ooid_t osubj_auto_msg_id;
OsubjMsgHandler_t osubj_msg_handler;
ushort_t osubj_msg_classes[OSUBJ_MAX_CLASS];


/*			Initialization & Shutdown				*/

oret_t
osubjInitMessages(OsubjMsgHandler_t phandler)
{
	osubj_msg_handler = phandler;
	osubj_auto_msg_id = (osubjClock() & 0x1ff) * 3 + 4;
	return OK;
}


oret_t
osubjClearMessages(void)
{
	ushort_t buf[OSUBJ_MAX_CLASS * 2 + 2];
	int i, cnt;
	oret_t rc;

	osubj_msg_handler = NULL;

	buf[0] = 0;		/* ... disable	*/
	for (i = cnt = 0; i < OSUBJ_MAX_CLASS; i++) {
		if (osubj_msg_classes[i] != 0) {
			buf[2 + cnt] = OUHTONS(osubj_msg_classes[i]);
			osubj_msg_classes[i] = 0;
			cnt++;
		}
	}
	if (cnt > 0 && osubjIsConnected()) {
		buf[1] = OUHTONS(cnt);
		rc = osubjSendPacket(OK, OLANG_DATA, OLANG_MSG_CLASS_ACT,
							(cnt + 2 ) * 2, buf);
	}

	return OK;
}


/*				Message Classes				*/

oret_t
osubjSendClasses(void)
{
	ushort_t klass;
	ushort_t buf[1 + OSUBJ_MAX_CLASS];
	int i, cnt;
	oret_t rc;

	if (!osubj_initialized || osubj_aborted)
		return ERROR;
	buf[0] = OHTONS(2);		/* flush and add */
	for (i = cnt = 0; i < OSUBJ_MAX_CLASS; i++) {
		klass = osubj_msg_classes[i];
		if (klass != 0) {
			buf[2 + cnt] = OUHTONS(klass);
			cnt++;
		}
	}
	if (cnt > 0) {
		buf[1] = OUHTONS(cnt);
		rc = osubjSendPacket(OK, OLANG_DATA, OLANG_MSG_CLASS_ACT,
							(cnt + 2) * 2, buf);
	} else {
		rc = OK;
	}
	return rc;
}


oret_t
osubjEnableMsgClass(int klass, bool_t enable)
{
#if OSUBJ_LOGGING
	static const char *me = "osubjEndableMsgClass";
#endif
	int i;
	ushort_t buf[3];

	if (klass < 0 || klass > OLANG_MAX_MSG_KLASS) {
		return ERROR;
	}
	if (klass == 0) {
		return OK;
	}

	buf[1] = OHTONS(1);
	buf[2] = OUHTONS(klass);

	if (enable) {
		for (i = 0; i < OSUBJ_MAX_CLASS; i++) {
			if (osubj_msg_classes[i] == klass)
				return ERROR;
		}
		for (i = 0; i < OSUBJ_MAX_CLASS; i++) {
			if (osubj_msg_classes[i] == 0)
				break;
		}
		buf[0] = OUHTONS(1);
	} else {
		for (i = 0; i < OSUBJ_MAX_CLASS; i++) {
			if (osubj_msg_classes[i] == klass)
				break;
		}
		buf[0] = OUHTONS(0);
		klass = 0;
	}
	if (i == OSUBJ_MAX_CLASS)
		return ERROR;
	osubj_msg_classes[i] = klass;
	if (osubjIsConnected())
		osubjSendPacket(OK, OLANG_DATA, OLANG_MSG_CLASS_ACT, 6, buf);
	osubjLogAt(7, "%s: cls=%d/%xh,enable=%d", me, klass, klass, enable);

	return OK;
}


/*				Message Receival				*/

oret_t
osubjHandleMsgRecv(int kind, int type, int len, char *buf)
{
	static const char *me = "osubjHandleMsgRecv";
	ooid_t		m_id;
	int 		m_klass, m_type;
	int			m_len, s_len, d_len;
	char 		*m_src, *m_dest, *m_data;
	ulong_t		ulv;
	ushort_t	usv;

	if (len < 18) {
		osubjLogAt(5, "%s: tiny len=%d", me, len);
		return ERROR;
	}

	ulv = *(ulong_t *)(buf + 4);
	m_id = (ooid_t) OUNTOHL(ulv);
	usv = *(ushort_t *)(buf + 8);
	m_len = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 10);
	s_len = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 12);
	d_len = OUNTOHS(usv);

	if (len != 18 + s_len + d_len + m_len) {
		osubjLogAt(5, "%s: invalid len=%d", me, len);
		return ERROR;
	}

	usv = *(ushort_t *)(buf + 14);
	m_klass = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 16);
	m_type = OUNTOHS(usv);
	m_data = buf + 18 + s_len + d_len;

	if (NULL == osubj_msg_handler) {
		osubjLogAt(5, "%s: message handler not installed", me);
		return ERROR;
	}

	/*
		Source and destination names in the message are not zero-terminated,
		and we need to add zero characters to them. Since the packet is not
		going to be buffered, we can do this via the following semi-dirty
		trick: relocate characters right in the packet using the space of
		the type field that was already decoded.
		We presume that screwing packet on return is no problem.
	*/
	m_src = buf + 16;
	m_dest = buf + 16 + s_len + 1;
	osubjCopy(buf + 18, m_src, s_len);
	m_src[s_len] = '\0';
	osubjCopy(buf + 18 + s_len, m_dest, d_len);
	m_dest[d_len] = '\0';

	/* The handler should return as fast as possible */
	osubjLogAt(7, "%s: received id=%lu,src=%s,dst=%s,cls=%d/%xh,typ=%d,len=%d",
				me, m_id, m_src, m_dest, m_klass, m_klass, m_type, m_len);
	(*osubj_msg_handler)(m_id, m_src, m_dest, m_klass, m_type, m_data, m_len);

	return OK;
}


/*					Message Sending					*/

oret_t
osubjSendMessage(ooid_t id, const char *dest, int klass, int type,
				void *data, int len)
{
#if OSUBJ_LOGGING
	static const char *me = "osubjSendMessage";
#endif
	const char *src;
	char   *buf = NULL;
	int     n, s_len, d_len;
	ulong_t ulv;
	ushort_t usv;
	oret_t rc;

	if (len < 0 || len > OLANG_MAX_PACKET || (len > 0 && NULL == data)
		|| klass < 0 || klass > OLANG_MAX_MSG_KLASS
		|| type < 0 || type > OLANG_MAX_MSG_TYPE) {
		osubjLogAt(5, "%s: id=%lu, invalid parameters", me, id);
		return ERROR;
	}

	if (NULL == dest) {
		dest = "";		/* broadcast */
	}
	if (0 == id) {
		id = ++osubj_auto_msg_id;	/* by default: sequential ID */
	}

	/* FIXME: there must be a way to specify source subject */

	if (osubj_subject_qty > 0)
		src = osubj_subjects[0].subject_name;
	else
		src = ".";		/* FIXME: stupid fallback */

	s_len = strlen(src);
	d_len = strlen(dest);
	n = len + s_len + d_len + 18;
	if (NULL == (buf = osubjAlloc(sizeof(char), n, 0))) {
		osubjLogAt(5, "%s: id=%lu, no space for packet", me, id);
		return ERROR;
	}

	/* FIXME: hub should report us the state of recipients */

	*(ulong_t *)(buf + 0) = OUHTONL(0);		/* 0 = reply is not expected */
	ulv = id;
	*(ulong_t *)(buf + 4) = OUHTONL(ulv);
	usv = len;
	*(ushort_t *)(buf + 8) = OUHTONS(usv);
	usv = s_len;
	*(ushort_t *)(buf + 10) = OUHTONS(usv);
	usv = d_len;
	*(ushort_t *)(buf + 12) = OUHTONS(usv);
	usv = klass;
	*(ushort_t *)(buf + 14) = OUHTONS(usv);
	usv = type;
	*(ushort_t *)(buf + 16) = OUHTONS(usv);
	osubjCopy(src, buf + 18, s_len);
	osubjCopy(dest, buf + 18 + s_len, d_len);
	osubjCopy(data, buf + 18 + s_len + d_len, len);

	rc = osubjSendPacket(OK, OLANG_DATA, OLANG_MSG_SEND_REQ, n, buf);
	osubjLogAt((rc == OK ? 7 : 5),
			"%s: sent rc=%d id=%u src=`%s' dst=`%s' cls=%xh type=%xh len=%d",
			me, rc, (unsigned) id, src, dest, klass, type, len);
	osubjFree(buf, 0);
	return rc;
}
