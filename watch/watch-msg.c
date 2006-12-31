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

#include "watch-priv.h"
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/conf-net.h>	/* for OHTONS,... */
#include <optikus/conf-mem.h>	/* for oxnew,malCopy,... */
#include <string.h>		/* for strcpy,strcat,strncmp */
#include <stdarg.h>		/* for va_list */
#include <stdio.h>		/* for sprintf,sscanf */

/*			Data Types & Local Variables				*/

typedef struct OwatchPendingMsg_st
{
	struct OwatchPendingMsg_st *next, *prev;
	ooid_t	ooid;
	char *	src;
	char *	dest;
	int		klass;
	int		type;
	int		len;
	char *	data;
} OwatchPendingMsg_t;


typedef struct OwatchMsgClassHandlerRec_st
{
	struct OwatchMsgClassHandlerRec_st *next, *prev;
	int		klass;
	OwatchMsgHandler_t phandler;
	long	param;
} OwatchMsgClassHandlerRec_t;


typedef struct OwatchMsgRcvRecord_st
{
	struct OwatchMsgRcvRecord_st *next, *prev;
	owop_t	op;
	ooid_t *ooid_p;
	char *	src_p;
	char *	dest_p;
	int	*	klass_p;
	int	*	type_p;
	int	*	len_p;
	char *	data_p;
} OwatchMsgRecvRecord_t;


ooid_t owatch_auto_msg_ooid;

OwatchMsgClassHandlerRec_t *owatch_msg_class_first;
OwatchMsgClassHandlerRec_t *owatch_msg_class_last;
OwatchPendingMsg_t *		owatch_msg_q_first;
OwatchPendingMsg_t *		owatch_msg_q_last;
OwatchMsgRecvRecord_t *		owatch_msg_rcvr_first;
OwatchMsgRecvRecord_t *		owatch_msg_rcvr_last;

bool_t		owatch_msg_himark;
int			owatch_msg_cur_count;
int			owatch_msg_cur_bytes;
int			owatch_msg_hi_count;
int			owatch_msg_hi_bytes;

int         owatch_ack_count;


/*			Initialization & Shutdown				*/

static void
owatchFreePendingMsg(OwatchPendingMsg_t *pmsg)
{
	if (NULL != pmsg) {
		oxfree(pmsg->src);
		oxfree(pmsg->dest);
		oxfree(pmsg->data);
		oxfree(pmsg);
	}
}


oret_t
owatchSetupMessages(void)
{
	owatch_auto_msg_ooid = (osMsClock() & 0x1ff) * 3 + 4;
	owatch_ack_count = 0;
	return OK;
}


oret_t
owatchClearMessages(void)
{
	OwatchMsgClassHandlerRec_t *pclass, *pclass_next;
	OwatchMsgRecvRecord_t *precv, *precv_next;
	OwatchPendingMsg_t *pmsg, *pmsg_next;
	owop_t op;

	pclass_next = owatch_msg_class_first;
	owatch_msg_class_first = owatch_msg_class_last = NULL;
	while (NULL != pclass_next) {
		pclass = pclass_next;
		pclass_next = pclass->next;
		oxfree(pclass);
	}

	pmsg_next = owatch_msg_q_first;
	owatch_msg_q_first = owatch_msg_q_last = NULL;
	while (NULL != pmsg_next) {
		pmsg = pmsg_next;
		pmsg_next = pmsg->next;
		owatchFreePendingMsg(pmsg);
	}

	precv_next = owatch_msg_rcvr_first;
	owatch_msg_rcvr_first = owatch_msg_rcvr_last = NULL;
	while (NULL != precv_next) {
		precv = precv_next;
		precv_next = precv->next;
		op = precv->op;
		owatchSetOpData(op, 0, 0);
		owatchCancelOp(op);
		oxfree(precv);
	}

	owatch_msg_himark = FALSE;
	owatch_msg_cur_count = owatch_msg_hi_count = 0;
	owatch_msg_cur_bytes = owatch_msg_hi_bytes = 0;

	owatch_ack_count = 0;

	return OK;
}


/*				Message Classes				*/

oret_t
owatchAddMsgHandler(int klass, OwatchMsgHandler_t phandler, long param)
{
	owop_t op;
	OwatchMsgClassHandlerRec_t *pclass;
	int count;
	ushort_t buf[3];

	if (klass < 0 || klass > OLANG_MAX_MSG_KLASS) {
		return ERROR;
	}

	if (klass == 0) {
		pclass = owatch_msg_class_last;
		if (NULL != pclass && pclass->klass == 0) {
			return ERROR;
		}

		if (NULL == (pclass = oxnew(OwatchMsgClassHandlerRec_t, 1))) {
			return ERROR;
		}
		pclass->klass = 0;
		pclass->phandler = phandler;
		pclass->param = param;

		if (NULL == (pclass->prev = owatch_msg_class_last))
			owatch_msg_class_first = pclass;
		else
			pclass->prev->next = pclass;
		owatch_msg_class_last = pclass;

		return OK;
	}

	count = 0;
	for (pclass = owatch_msg_class_first; pclass; pclass = pclass->next) {
		if (pclass->klass == klass) {
			count++;
			if (pclass->phandler == phandler) {
				return ERROR;
			}
		}
	}

	if (NULL == (pclass = oxnew(OwatchMsgClassHandlerRec_t, 1))) {
		return ERROR;
	}
	pclass->klass = klass;
	pclass->phandler = phandler;
	pclass->param = param;

	if (count == 0 && owatchIsConnected()) {
		/* first handler for this class */
		buf[0] = OHTONS(1);				/* ...enable		*/
		buf[1] = OUHTONS(1);			/* ...one handler	*/
		buf[2] = OUHTONS(klass);		/* ...of the class	*/
		op = owatchSendPacket(OLANG_DATA, OLANG_MSG_CLASS_ACT, 6, buf);
		if (op == OWOP_ERROR) {
			oxfree(pclass);
			return ERROR;
		}
		if (op != OWOP_OK) {
			/* operation will silently deallocate itself when completed */
			owatchDetachOp(op);
		}
	}

	if (NULL == (pclass->next = owatch_msg_class_first))
		owatch_msg_class_last = pclass;
	else
		pclass->next->prev = pclass;
	owatch_msg_class_first = pclass;

	return OK;
}


oret_t
owatchRemoveMsgHandler(int klass, OwatchMsgHandler_t phandler)
{
	owop_t op;
	OwatchMsgClassHandlerRec_t *pcur, *pclass;
	int count;
	ushort_t buf[3];

	if (klass < 0 || klass > OLANG_MAX_MSG_KLASS) {
		return ERROR;
	}

	if (klass == 0) {
		pclass = owatch_msg_class_last;
		if (NULL == pclass
				|| pclass->klass != 0 || pclass->phandler != phandler) {
			return ERROR;
		}
		if (NULL == (owatch_msg_class_last = pclass->prev))
			owatch_msg_class_first = NULL;
		oxfree(pclass);
		return OK;
	}

	pclass = NULL;
	count = 0;
	for (pcur = owatch_msg_class_first; NULL != pcur; pcur = pcur->next) {
		if (pcur->klass == klass) {
			count++;
			if (pcur->phandler == phandler) {
				pclass = pcur;
			}
			if (NULL != pclass && count > 1) {
				break;
			}
		}
	}
	if (NULL == pclass) {
		return ERROR;
	}

	if (count == 1 && owatchIsConnected()) {
		/* last handler of the class */
		buf[0] = OHTONS(0);				/* ...disable		*/
		buf[1] = OUHTONS(1);			/* ...one handler	*/
		buf[2] = OUHTONS(klass);		/* ..of the class	*/
		op = owatchSendPacket(OLANG_DATA, OLANG_MSG_CLASS_ACT, 6, buf);
		if (op == OWOP_ERROR) {
			return ERROR;
		}
		if (op != OWOP_OK) {
			/* operation will silently deallocate itself when completed */
			owatchDetachOp(op);
		}
	}

	if (NULL == pclass->next)
		owatch_msg_class_last = pclass->prev;
	else
		pclass->next->prev = pclass->prev;
	if (NULL == pclass->prev)
		owatch_msg_class_first = pclass->next;
	else
		pclass->prev->next = pclass->next;
	oxfree(pclass);

	return OK;
}


oret_t
owatchMsgClassRegistration(bool_t enable)
{
	OwatchMsgClassHandlerRec_t *pclass;
	ushort_t klass;
	ushort_t buf[1 + 1024];		/* FIXME: magic size */
	int cnt;
	oret_t rc;

	buf[0] = OHTONS(enable ? 2 : 0);	/* flush and add / remove */
	cnt = 0;
	for (pclass = owatch_msg_class_first; pclass; pclass = pclass->next) {
		klass = pclass->klass;
		buf[2 + cnt] = OUHTONS(klass);
		cnt++;
	}
	if (cnt > 0) {
		buf[1] = OUHTONS(cnt);
		rc = owatchSendPacket(OLANG_DATA, OLANG_MSG_CLASS_ACT,
							(cnt + 2) * 2, buf);
	} else {
		rc = OK;
	}
	return rc;
}


/*				Queue Limits				*/

oret_t
owatchSetMsgQueueLimits(int max_queued_messages, int max_queued_bytes)
{
	if ((max_queued_messages != 0 && max_queued_messages < 4)
			|| (max_queued_bytes != 0 && max_queued_bytes < 4)) {
		return ERROR;
	}
	owatch_msg_hi_count = max_queued_messages;
	owatch_msg_hi_bytes = max_queued_bytes;
	return OK;
}


/*				Message Receival				*/

int
owatchGetAckCount(bool_t flush)
{
	int count = owatch_ack_count;
	if (flush)
		owatch_ack_count = 0;
	return count;
}


static int
owatchGiveAwayMsg(OwatchMsgRecvRecord_t *precv, OwatchPendingMsg_t *pmsg)
{
	int		m_len = pmsg->len;
	int		err;

	if (NULL != precv->ooid_p)
		*precv->ooid_p = pmsg->ooid;
	if (NULL != precv->klass_p)
		*precv->klass_p = pmsg->klass;
	if (NULL != precv->type_p)
		*precv->type_p = pmsg->type;
	if (NULL != precv->src_p)
		strcpy(precv->src_p, pmsg->src);
	if (NULL != precv->dest_p)
		strcpy(precv->dest_p, pmsg->dest);
	if (m_len <= *precv->len_p) {
		*precv->len_p = m_len;
		err = 0;
	} else {
		m_len = *precv->len_p;
		err = OWATCH_ERR_MSG_TRUNC;
	}
	if (m_len > 0 && NULL != precv->data_p)
		oxbcopy(pmsg->data, precv->data_p, m_len);
	owatchFinalizeOp(precv->op, err);
	return err;
}


oret_t
owatchHandleMsgRecv(int kind, int type, int len, char *buf)
{
	static const char *me = "handleMsgRecv";
	ooid_t		m_ooid;
	int 		m_klass, m_type;
	int			m_len, s_len, d_len;
	char *		m_data;
	ulong_t		ulv;
	ushort_t	usv;
	int			err;
	OwatchPendingMsg_t *pmsg = NULL;
	OwatchMsgClassHandlerRec_t *pclass;
	OwatchMsgRecvRecord_t *precv;

	if (len < 18) {
		owatchLog(5, "%s: tiny len=%d", me, len);
		return ERROR;
	}

	ulv = *(ulong_t *)(buf + 4);
	m_ooid = (ooid_t) OUNTOHL(ulv);
	usv = *(ushort_t *)(buf + 8);
	m_len = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 10);
	s_len = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 12);
	d_len = OUNTOHS(usv);

	if (len != 18 + s_len + d_len + m_len) {
		owatchLog(5, "%s: invalid len=%d s_len=%d d_len=%d m_len=%d",
					me, len, s_len, d_len, m_len);
		return ERROR;
	}

	usv = *(ushort_t *)(buf + 14);
	m_klass = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 16);
	m_type = OUNTOHS(usv);
	m_data = buf + 18 + s_len + d_len;

	/* Packet acknowledgements are special. */
	if (m_klass == OLANG_MSG_CLASS_ACK && m_type == OLANG_MSG_TYPE_ACK) {
		owatch_ack_count++;
	}

	/* FIXME: allocate pmsg->data only when message is buffered */
	if (NULL == (pmsg = oxnew(OwatchPendingMsg_t, 1))
		|| NULL == (pmsg->src = oxnew(char, s_len+1))
		|| NULL == (pmsg->dest = oxnew(char, d_len+1))
		|| (m_len > 0 && NULL == (pmsg->data = oxnew(char, m_len)))) {
		err = OWATCH_ERR_NOSPACE;
		owatchLog(3, "%s: not enough space", me);
		owatchFreePendingMsg(pmsg);
		return ERROR;
	}

	pmsg->ooid = m_ooid;
	pmsg->klass = m_klass;
	pmsg->type = m_type;
	pmsg->len = m_len;
	oxbcopy(buf + 18, pmsg->src, s_len);
	pmsg->src[s_len] = '\0';
	oxbcopy(buf + 18 + s_len, pmsg->dest, d_len);
	pmsg->dest[d_len] = '\0';
	if (m_len > 0) {
		oxbcopy(m_data, pmsg->data, m_len);
	} else {
		pmsg->data = NULL;
	}

	for (pclass = owatch_msg_class_first; pclass; pclass = pclass->next) {
		if (pclass->klass == pmsg->klass || pclass->klass == 0) {
			(*pclass->phandler)(pmsg->ooid, pmsg->src, pmsg->dest, pmsg->klass,
							pmsg->type, pmsg->data, pmsg->len, pclass->param);
			owatchFreePendingMsg(pmsg);
			return OK;
		}
	}

	/*
		Packet acknowledgements are very special. They are not buffered
		and can not be received by owatchMessageReceive.
		User can only register a class handler for them.
		FIXME: this should be implemented in upper layers.
	*/
	if (m_klass == OLANG_MSG_CLASS_ACK && m_type == OLANG_MSG_TYPE_ACK) {
		owatchFreePendingMsg(pmsg);
		return OK;
	}

	if (NULL != (precv = owatch_msg_rcvr_first)) {
		if (NULL == (owatch_msg_rcvr_first = precv->next))
			owatch_msg_rcvr_last = NULL;
		else
			precv->next->prev = NULL;
		owatchGiveAwayMsg(precv, pmsg);
		oxfree(precv);
		owatchFreePendingMsg(pmsg);
		return OK;
	}

	/* message will be buffered */
	if ((owatch_msg_hi_count > 0
			&& owatch_msg_cur_count + 1 > owatch_msg_hi_count)
		|| (owatch_msg_hi_bytes > 0
			&& owatch_msg_cur_bytes + m_len > owatch_msg_hi_bytes)) {
		/* queue overflow */
		owatchLog(4, "%s: message queue overflow", me);
		owatchFreePendingMsg(pmsg);
		return ERROR;
	}

	pmsg->next = NULL;
	if (NULL == (pmsg->prev = owatch_msg_q_last))
		owatch_msg_q_first = pmsg;
	else
		pmsg->prev->next = pmsg;
	owatch_msg_q_last = pmsg;

	owatch_msg_cur_count++;
	owatch_msg_cur_bytes += pmsg->len;

	if (!owatch_msg_himark &&
		((owatch_msg_hi_count > 0
			&& owatch_msg_cur_count > owatch_msg_hi_count * 2 / 3)
		|| (owatch_msg_hi_bytes
			&& owatch_msg_cur_bytes > owatch_msg_hi_bytes * 2 / 3))) {
		owatch_msg_himark = TRUE;
		/* FIXME: error code is ignored */
		if (owatchSendPacket(OLANG_DATA, OLANG_MSG_HIMARK, 0, NULL) != OK) {
			owatchLog(5, "cannot tell HI");
		}
	}

	return OK;
}


int
owatchMsgRecvCanceller(owop_t op, long data1, long data2)
{
	OwatchMsgRecvRecord_t *precv = (OwatchMsgRecvRecord_t *) data2;
	if (NULL != precv) {
		/* unlink from chain */
		if (NULL == precv->next)
			owatch_msg_rcvr_last = precv->prev;
		else
			precv->next->prev = precv->prev;
		if (NULL == precv->prev)
			owatch_msg_rcvr_first = precv->next;
		else
			precv->prev->next = precv->next;
		oxfree(precv);
	}
	return 0;
}


owop_t
owatchNowaitMsgRecv(ooid_t *ooid_p, char *src_p, char *dest_p,
					int *klass_p, int *type_p, void *data_p, int *len_p)
{
	OwatchMsgRecvRecord_t *precv;
	OwatchPendingMsg_t *pmsg;
	owop_t  op = OWOP_ERROR;
	int     m_len;

	if (NULL == data_p || NULL == len_p || *len_p <= 0) {
		/* invalid parameters */
		return OWOP_ERROR;
	}

	op = owatchAllocateOp(owatchMsgRecvCanceller, OWATCH_OPT_MSG_RECV, 0);
	if (op == OWOP_ERROR) {
		return OWOP_ERROR;
	}

	/* FIXME: allocate only when putting request on wait */
	if (NULL == (precv = oxnew(OwatchMsgRecvRecord_t, 1))) {
		owatchFinalizeOp(op, OWATCH_ERR_NOSPACE);
		return op;
	}

	precv->op = op;
	precv->ooid_p = ooid_p;
	precv->src_p = src_p;
	precv->dest_p = dest_p;
	precv->klass_p = klass_p;
	precv->type_p = type_p;
	precv->len_p = len_p;
	precv->data_p = data_p;

	owatchSetOpData(op, OWATCH_OPT_MSG_RECV, (long) precv);

	if (NULL != (pmsg = owatch_msg_q_first)) {
		if (NULL == (owatch_msg_q_first = pmsg->next))
			owatch_msg_q_last = NULL;
		else
			pmsg->next->prev = NULL;

		m_len = pmsg->len;
		owatchGiveAwayMsg(precv, pmsg);
		oxfree(precv);
		owatch_msg_cur_count--;
		owatch_msg_cur_bytes -= m_len;
		owatchFreePendingMsg(pmsg);

		if (owatch_msg_himark &&
			((owatch_msg_hi_count <= 0
				|| owatch_msg_cur_count <= owatch_msg_hi_count * 1 / 3)
			&& (owatch_msg_hi_bytes <= 0
				|| owatch_msg_cur_bytes <= owatch_msg_hi_bytes * 1 / 3))) {
			owatch_msg_himark = FALSE;
			if (owatchSendPacket(OLANG_DATA, OLANG_MSG_LOMARK, 0, NULL) != OK) {
				owatchLog(5, "cannot tell LO");
			}
		}
		return op;
	}

	precv->next = NULL;
	if (NULL != (precv->prev = owatch_msg_rcvr_last))
		precv->prev->next = precv;
	else
		owatch_msg_rcvr_first = precv;
	owatch_msg_rcvr_last = precv;

	return op;
}


owop_t
owatchReceiveMessage(ooid_t *id_p, char *src_p, char *dest_p,
					int *klass_p, int *type_p, void *data_p, int *len_p)
{
	owop_t op = owatchNowaitMsgRecv(id_p, src_p, dest_p, klass_p, type_p,
									data_p, len_p);
	return owatchInternalWaitOp(op);
}


/*					Message Sending					*/

oret_t
owatchHandleMsgSendReply(int kind, int type, int len, char *buf)
{
	static const char *me = "handleMsgSendReply";
	owop_t  op;
	ulong_t ulv;
	int     err;

	if (len != 8) {
		owatchLog(5, "%s: invalid len=%d", me, len);
		return ERROR;
	}
	ulv = *(ulong_t *)(buf + 0);
	op = (owop_t) OUNTOHL(ulv);
	ulv = *(ulong_t *)(buf + 4);
	err = (int) ONTOHL(ulv);
	switch (err) {
	case OLANG_ERR_RCVR_OFF:
		err = OWATCH_ERR_RCVR_OFF;
		break;
	case OLANG_ERR_RCVR_FULL:
		err = OWATCH_ERR_RCVR_FULL;
		break;
	case OLANG_ERR_RCVR_DOWN:
		err = OWATCH_ERR_NETWORK;
		break;
	}
	if (!owatchIsValidOp(op)) {
		err = OWATCH_ERR_TOOLATE;
		owatchLog(5, "%s: (op=%xh,err=%d) too late", me, op, err);
		return ERROR;
	}
	owatchLog(6, "%s: (op=%xh) err=%d", me, op, err);
	owatchFinalizeOp(op, err);
	return OK;
}


int
owatchMsgSendDone(owop_t m_op, owop_t s_op, int err, long param1, long param2)
{
	owatchUnchainOp(m_op);
	if (err)
		owatchLog(7, "msgsnd ooid=%ld: send error=%d", param1, err);
	else
		owatchLog(7, "msgsnd ooid=%ld: send complete", param1);
	return 0;
}


owop_t
owatchNowaitMsgSend(ooid_t ooid, const char *dest,
					int klass, int type, void *data, int len)
{
	owop_t  m_op = OWOP_ERROR;
	owop_t  s_op = OWOP_ERROR;
	const char *src;
	char   *buf = NULL;
	int     n, s_len, d_len;
	ulong_t ulv;
	ushort_t usv;

	if (len < 0 || len > OLANG_MAX_PACKET || (len > 0 && NULL == data)
		|| klass < 0 || klass > OLANG_MAX_MSG_KLASS
		|| type < 0 || type > OLANG_MAX_MSG_TYPE) {
		owatchLog(7, "msg send ooid=%ld: invalid parameters", ooid);
		return OWOP_ERROR;
	}

	if (NULL == dest) {
		dest = "";		/* broadcast */
	}
	if (0 == ooid) {
		ooid = ++owatch_auto_msg_ooid;	/* by default: sequential ID */
	}

	if (OWOP_ERROR == (m_op = owatchAllocateOp(NULL, OWATCH_OPT_MSG_SEND, 0))) {
		owatchLog(7, "msg send ooid=%ld: cannot allocate op", ooid);
		return OWOP_ERROR;
	}

	src = owatch_watcher_desc;
	s_len = strlen(src);
	d_len = strlen(dest);
	n = len + s_len + d_len + 18;
	if (NULL == (buf = oxnew(char, n))) {
		owatchFinalizeOp(m_op, OWATCH_ERR_NOSPACE);
		owatchLog(7, "msg send ooid=%ld: no space for packet", ooid);
		return OWOP_ERROR;
	}

	ulv = m_op;
	*(ulong_t *)(buf + 0) = OUHTONL(ulv);
	ulv = ooid;
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
	oxbcopy(src, buf + 18, s_len);
	oxbcopy(dest, buf + 18 + s_len, d_len);
	oxbcopy(data, buf + 18 + s_len + d_len, len);

	s_op = owatchSecureSend(OLANG_DATA, OLANG_MSG_SEND_REQ, n, buf);
	if (s_op == OWOP_ERROR) {
		owatchFinalizeOp(m_op, OWATCH_ERR_NETWORK);
		owatchLog(7, "msg send ooid=%ld: network is off", ooid);
		return OWOP_ERROR;
	}
	if (s_op != OWOP_OK) {
		owatchChainOps(m_op, s_op, owatchMsgSendDone, (long) ooid, 0);
	}
	owatchLog(7, "initiated message send: op=%xh ooid=%lu pkt_len=%d",
				m_op, ooid, n);
	oxfree(buf);
	return m_op;
}


owop_t
owatchSendMessage(ooid_t id, const char *dest,
					int klass, int type, void *data, int len)
{
	owop_t op = owatchNowaitMsgSend(id, dest, klass, type, data, len);
	return owatchInternalWaitOp(op);
}


/*				Control Messages					*/


/* FIXME: it should simply fill user buffer without sending the message */
owop_t
owatchSendCtlMsg(const char *dest, int klass, int type, const char *format, ...)
{
	static const char *me = "sendCtlMsg";
	char	xbuf[1024];		/* FIXME: magic size */
	OlangCtlMsg_t *cmsg = (OlangCtlMsg_t *) xbuf;
	int     i, len, no, n, qty;
	va_list params;
	char   *ptr;
	const char *csp;
	uchar_t u8;
	ushort_t u16;
	ulong_t u32, u32b;
	float   vf;
	double  vd;
	int     wc;
	char    sbuf[512];		/* FIXME: magic size */
	char    mb[40][16];
	bool_t  as_string;
	float  *as_float;
	double *as_double;
	char    method;
	bool_t  shorter, longer, is_signed;

	/*		Prepare			*/
	if (NULL == format)
		format = "";
	va_start(params, format);
	as_string = FALSE;
	as_float = 0;
	as_double = 0;
	method = 'V'; /* default method: vararg */

	for (csp = format, qty = 0; *csp; csp++) {
		if (*csp == '%')
			qty++;
	}

	oxazero(sbuf);
	if (qty > 38) {
		owatchLog(4, "%s: class=%d type=%d fmt=[%s] too many params %d",
					me, klass, type, format, qty);
		return OWOP_ERROR;
	}

	/*		Choose Format Type			*/

	if (!strncmp(format, "$(V)", 4)) {
		method = 'V';	/* vararg */
		format += 4;
		strcpy(sbuf, "(V): ");
	} else if (!strncmp(format, "$(S)", 4)) {
		method = 'S';	/* scanf */
		ptr = va_arg(params, char *);
		as_string = TRUE;
		strcpy(sbuf, "(S): ");
		strcat(sbuf, ptr == (char *) 0 || ptr == (char *) -1 ? "" : ptr);
		oxazero(mb);
		format += 4;
		n = sscanf(sbuf + 5, format,
				   mb[0], mb[1], mb[2], mb[3], mb[4], mb[5], mb[6], mb[7],
				   mb[8], mb[9], mb[10], mb[11], mb[12], mb[13], mb[14], mb[15],
				   mb[16], mb[17], mb[18], mb[19], mb[20], mb[21], mb[22],
				   mb[23], mb[24], mb[25], mb[26], mb[27], mb[28], mb[29],
				   mb[30], mb[31], mb[32], mb[33], mb[34], mb[35], mb[36],
				   mb[37], mb[38], mb[39]);
		if (n != qty) {
			owatchLog(4, "%s: cls=%d typ=%d fmt=[%s] DATA:[%s] need=%d got=%d",
						me, klass, type, format, sbuf, qty, n);
			return OWOP_ERROR;
		}
	} else if (0 == strncmp(format, "$(F)", 4)) {
		method = 'F';	/* array of floats */
		as_float = va_arg(params, float *);
		format += 4;
		strcpy(sbuf, "(F): ");
	} else if (0 == strncmp(format, "$(D)", 4)) {
		method = 'D';	/* array of doubles */
		as_double = va_arg(params, double *);
		format += 4;
		strcpy(sbuf, "(D): ");
	}

	no = 0;
	ptr = OLANG_CMSG_DATA(cmsg);
	len = strlen(format);
	wc = 0;

	/*			Loop over Specifiers			*/

	for (i = 0; i < len; i++) {
		if (format[i] != '%')
			continue;
		shorter = longer = FALSE;
		switch (format[++i]) {
		case 'h':
			shorter = TRUE;
			i++;
			break;
		case 'l':
			longer = TRUE;
			i++;
			break;
		case 0:
			continue;
		}
		switch (format[i]) {
		case 'c':
			switch (method) {
			case 'V':
				u8 = (unsigned char) va_arg(params, /*unsigned char */ int);
				break;
			case 'S':
				oxbcopy(mb[no], &u8, 1);
				break;
			case 'F':
				u8 = (signed char) as_float[no];
				break;
			case 'D':
				u8 = (signed char) as_double[no];
				break;
			}
			if (!as_string) {
				sprintf(sbuf + strlen(sbuf), "%u,", u8);
			}
			oxbcopy(&u8, ptr, 1);
			ptr[1] = 0;
			ptr += 2;
			wc += 1;
			break;
		case 'i':
		case 'd':
		case 'u':
		case 'x':
		case 'X':
			is_signed = format[i] == 'i' || format[i] == 'd';
			if (shorter) {
				switch (method) {
				case 'V':
					u16 = (unsigned short)
								va_arg(params, /*unsigned short */ int);
					break;
				case 'S':
					oxbcopy(mb[no], &u16, 2);
					break;
				case 'F':
					if (is_signed)
						u16 = (signed short) as_float[no];
					else
						u16 = (unsigned short) as_float[no];
					break;
				case 'D':
					if (is_signed)
						u16 = (signed short) as_double[no];
					else
						u16 = (unsigned short) as_double[no];
					break;
				}
				if (!as_string) {
					if (is_signed)
						sprintf(sbuf + strlen(sbuf), "%hd,", (short) u16);
					else
						sprintf(sbuf + strlen(sbuf), "%hu,", u16);
				}
				u16 = OUHTONS(u16);
				oxbcopy(&u16, ptr, 2);
				wc += 1;
				ptr += 2;
			} else {
				switch (method) {
				case 'V':
					u32 = (unsigned long) va_arg(params, unsigned long);

					break;
				case 'S':
					oxbcopy(mb[no], &u32, 4);
					break;
				case 'F':
					if (is_signed)
						u32 = (signed long) as_float[no];
					else
						u32 = (unsigned long) as_float[no];
					break;
				case 'D':
					if (is_signed)
						u32 = (signed long) as_double[no];
					else
						u32 = (unsigned long) as_double[no];
					break;
				}
				if (!as_string) {
					if (is_signed)
						sprintf(sbuf + strlen(sbuf), "%ld,", (long) u32);
					else
						sprintf(sbuf + strlen(sbuf), "%lu,", u32);
				}
				u32 = OUHTONL(u32);
				oxbcopy(&u32, ptr, 4);
				wc += 2;
				ptr += 4;
			}
			break;
		case 'f':
		case 'e':
		case 'g':
			if (longer) {
				switch (method) {
				case 'V':
					vd = (double) va_arg(params, double);

					break;
				case 'S':
					oxbcopy(mb[no], &vd, 8);
					break;
				case 'F':
					vd = as_float[no];
					break;
				case 'D':
					vd = as_double[no];
					break;
				}
				if (!as_string) {
					sprintf(sbuf + strlen(sbuf), "%g,", vd);
				}
				oxbcopy((char *) &vd + 0, &u32, 4);
				oxbcopy((char *) &vd + 4, &u32b, 4);
				u32 = OUHTONL(u32);
				u32b = OUHTONL(u32b);
				if (1 == OUHTONL(1)) {
					oxbcopy(&u32, ptr + 0, 4);
					oxbcopy(&u32b, ptr + 4, 4);
				} else {
					oxbcopy(&u32, ptr + 4, 4);
					oxbcopy(&u32b, ptr + 0, 4);
				}
				wc += 4;
				ptr += 8;
			} else {
				switch (method) {
				case 'V':
					vf = (float) va_arg(params, /*float */ double);
					break;
				case 'S':
					oxbcopy(mb[no], &vf, 4);
					break;
				case 'F':
					vf = as_float[no];
					break;
				case 'D':
					vf = (float) as_double[no];
					break;
				}
				if (!as_string) {
					sprintf(sbuf + strlen(sbuf), "%g,", (double) vf);
				}
				oxbcopy(&vf, &u32, 4);
				u32 = OUHTONL(u32);
				oxbcopy(&u32, ptr, 4);
				wc += 2;
				ptr += 4;
			}
			break;
		}
		no++;
	}

	/*			Finish and Send the Message				*/

	va_end(params);
	n = strlen(sbuf);
	if (n > 0 && sbuf[n-1] == ',')
		sbuf[n-1] = '\0';

	u16 = OUHTONS(OLANG_CMSG_SUFFIX);
	oxbcopy(&u16, ptr, 2);
	ptr += 2;
	wc += 1;

	cmsg->prefix = OUHTONS(OLANG_CMSG_PREFIX);
	cmsg->klass = OUHTONS(klass);
	cmsg->type = OUHTONS(type);
	cmsg->narg = OUHTONS(qty);
	n = wc * 2 + OLANG_CMSG_HEADER_LEN;
	owatchLog(3, "%s: class=%d type=%d fmt=[%s] data=[%s] no=%lu nb=%d",
				me, klass, type, format, sbuf, owatch_auto_msg_ooid, n);

	return owatchSendMessage(0, dest, OLANG_MSG_CLASS_SEND, OLANG_MSG_TYPE_SEND, xbuf, n);
}


/*				Remote Logging					*/

bool_t  owatch_remote_log;
static ologhandler_t owatch_saved_log_handler;
static bool_t owatch_in_remote_log;		/* FIXME: must be atomic */

static int
owatchRemoteLogHandler(const char *msg)
{
	if (owatch_initialized && owatch_remote_log) {
		if (!owatch_in_remote_log) {
			owatch_in_remote_log = TRUE;
			owatchSendPacket(OLANG_DATA, OLANG_REMOTE_LOG,
							strlen(msg)+1, (char *) msg);
			owatch_in_remote_log = 0;
		}
	}
	return 0;
}


bool_t
owatchRemoteLog(bool_t enable)
{
	bool_t old_enable = owatch_remote_log;

	if (enable == TRUE || enable == FALSE) {
		if (owatch_remote_log != enable) {
			if (enable) {
				owatch_saved_log_handler = ologSetHandler(owatchRemoteLogHandler);
			} else {
				ologSetHandler(owatch_saved_log_handler);
				owatch_saved_log_handler = NULL;
			}
		}
		owatch_remote_log = enable;
	}
	return old_enable;
}

