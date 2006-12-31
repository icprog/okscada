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

  Messaging Service.

*/

#include "hub-priv.h"
#include <optikus/conf-net.h>	/* for ONTOHL,... */
#include <optikus/conf-mem.h>	/* for oxstrdup,oxnew */
#include <string.h>				/* for strcmp,strncmp */


typedef struct OhubMsgNode_st
{
	struct OhubMsgNode_st *next;
	char *name;
	bool_t is_full;
	int klass_num;
	OhubSubject_t *subject_p;
	struct OhubNetWatch_st *watch_p;
} OhubMsgNode_t;

typedef struct OhubMsgClassNode_st
{
	struct OhubMsgClassNode_st *next;
	OhubMsgNode_t *node;
} OhubMsgClassNode_t;

typedef struct OhubMsgClass_st
{
	struct OhubMsgClass_st *next;
	int klass;
	OhubMsgClassNode_t *node_list;
} OhubMsgClass_t;


OhubMsgNode_t *ohub_msg_nodes;
OhubMsgClass_t *ohub_msg_classes;


/*
 * .
 */
oret_t
ohubInitMessages(void)
{
	return OK;
}


oret_t
ohubCleanupMessages(void)
{
	return OK;
}


oret_t
ohubRegisterMsgNode(
		struct OhubNetWatch_st * pnwatch, OhubSubject_t * psubject,
		OhubNodeAction action, ushort_t node_id, const char *node_name)
{
	static const char *me = "registerMsgNode";
	OhubMsgNode_t *pnode, *pnprev;
	OhubMsgClass_t *pklass, *pkprev;
	OhubMsgClassNode_t *cn_cur, *cn_prev;

	if ((pnwatch && psubject) || (!pnwatch && !psubject)) {
		ohubLog(1, "%s: invalid references", me);
		return ERROR;
	}

	/* FIXME: hashes would bring a speedup */

	if (action == OHUB_MSGNODE_ADD) {
		/* register a new node */
		for (pnode = ohub_msg_nodes; NULL != pnode; pnode = pnode->next) {
			if ((pnode->watch_p == pnwatch && pnode->subject_p == psubject)
					|| 0 == strcmp(pnode->name, node_name)) {
				ohubLog(1, "%s: `%s' registered twice", me, node_name);
				return ERROR;
			}
		}
		if (NULL == (pnode = oxnew(OhubMsgNode_t, 1))) {
			ohubLog(1, "%s: out of memory", me);
			return ERROR;
		}
		if (NULL == (pnode->name = oxstrdup(node_name))) {
			oxfree(pnode);
			ohubLog(1, "%s: out of memory", me);
			return ERROR;
		}
		pnode->is_full = FALSE;
		pnode->klass_num = 0;
		pnode->watch_p = pnwatch;
		pnode->subject_p = psubject;
		pnode->next = ohub_msg_nodes;
		ohub_msg_nodes = pnode;
	}
	else if (action == OHUB_MSGNODE_REMOVE || action == OHUB_MSGNODE_FLUSH) {
		/* find a node to remove/flush */
		for (pnode = ohub_msg_nodes, pnprev = NULL; NULL != pnode;
							pnprev = pnode, pnode = pnode->next) {
			if (pnode->watch_p == pnwatch && pnode->subject_p == psubject)
				break;
		}
		if (NULL == pnode) {
			ohubLog(1, "%s: node `%s' not found", me, node_name);
			return ERROR;
		}
		if (0 != strcmp(pnode->name, node_name)) {
			ohubLog(1, "%s: `%s' is invalid", me, node_name);
			/* still fall through */
		}
		ohubLog(5, "%s: %sing node `%s'", me,
				action == OHUB_MSGNODE_REMOVE ? "unregister" : "flush",
				node_name);
		/* remove node classes */
		pklass = ohub_msg_classes;
		pkprev = NULL;
		while (NULL != pklass && pnode->klass_num > 0) {
			for (cn_cur = pklass->node_list, cn_prev = NULL; NULL != cn_cur;
								cn_prev = cn_cur, cn_cur = cn_cur->next) {
				if (cn_cur->node == pnode) {
					if (NULL == cn_prev)
						pklass->node_list = cn_cur->next;
					else
						cn_prev->next = cn_cur->next;
					oxfree(cn_cur);
					if (0 == --pnode->klass_num)
						break;
				}
			}
			if (NULL == pklass->node_list) {
				if (NULL == pkprev)
					ohub_msg_classes = pklass->next;
				else
					pkprev->next = pklass->next;
				pkprev = pklass;
				pklass = pklass->next;
				oxfree(pkprev);
			} else {
				pkprev = pklass;
				pklass = pklass->next;
			}
		}
		if (action == OHUB_MSGNODE_REMOVE) {
			/* remove node from the list */
			if (NULL == pnprev)
				ohub_msg_nodes = pnode->next;
			else
				pnprev->next = pnode->next;
			/* free memory */
			oxfree(pnode->name);
			oxfree(pnode);
		}
	}
	else {
		ohubLog(1, "%s: incorrect action %d", me, action);
		return ERROR;
	}
	return OK;
}


oret_t
ohubHandleMsgClassAct(struct OhubNetWatch_st * pnwatch,
					OhubAgent_t * pagent,	/* FIXME: asymmetric ! */
					int kind, int type, int len, char *buf,
					const char *url)
{
	static const char *me = "handleMsgAct";
	ushort_t *pkt = (ushort_t *) buf;
	bool_t    enable, flush;
	int       i, count, klass;
	OhubMsgNode_t *pnode, *pnprev;
	OhubMsgClass_t *pklass, *pkprev;
	OhubMsgClassNode_t *cn_cur, *cn_prev;
	const char *node_name;

	if ((pnwatch && pagent) || (!pnwatch && !pagent)) {
		ohubLog(1, "%s: invalid references for %s", me, url);
		return ERROR;
	}

	if (len < 6) {
		ohubLog(5, "%s: tiny class action from %s len=%d", me, url, len);
		return ERROR;
	}

	switch(ONTOHS(pkt[0])) {
	case 1:
		enable = TRUE;
		flush = FALSE;
		break;
	case 0:
		enable = FALSE;
		flush = FALSE;
		break;
	case 2:
		enable = TRUE;
		flush = TRUE;
		break;
	default:
		ohubLog(3, "%s: invalid class action from %s enable=%d",
				me, url, ONTOHS(pkt[0]));
		return ERROR;
	}

	count = OUNTOHS(pkt[1]);
	if (len != count * 2 + 4) {
		ohubLog(3, "%s: invalid class action from %s count=%d len=%d",
				me, url, count, len);
		return ERROR;
	}

	/* find a node to act upon */
	for (pnode = ohub_msg_nodes, pnprev = NULL; NULL != pnode;
						pnprev = pnode, pnode = pnode->next) {
		if (((NULL != pnode->subject_p && pnode->subject_p->pagent == pagent)
			|| (NULL == pnode->subject_p && NULL == pagent))
			&& pnode->watch_p == pnwatch) {
			break;
		}
	}
	if (NULL == pnode) {
		ohubLog(3, "%s: class action node not found for %s", me, url);
		return ERROR;
	}
	node_name = pnode->name;
	if (flush) {
		ohubRegisterMsgNode(pnode->watch_p, pnode->subject_p,
							OHUB_MSGNODE_FLUSH, 0, node_name);
	}

	for (i = 0; i < count; i++) {
		klass = OUNTOHS(pkt[i+2]);
		if (klass <= 0) {
			ohubLog(3, "%s: invalid class %d(0x%x) from %s",
					me, klass, klass, url);
			continue;
		}
		/* find the class and class-to-node descriptors */
		cn_cur = cn_prev = NULL;
		for (pklass = ohub_msg_classes, pkprev = NULL;
				NULL != pklass; pkprev = pklass, pklass = pklass->next) {
			if (pklass->klass == klass) {
				for (cn_cur = pklass->node_list; NULL != cn_cur;
									cn_prev = cn_cur, cn_cur = cn_cur->next) {
					if (cn_cur->node == pnode) {
						break;
					}
				}
				break;
			}
		}
		if (enable) {
			/* add the class-to-node descriptor */
			if (NULL != cn_cur) {
				ohubLog(3,
					"%s: %s wants class %d(0x%x) that is already at node %s",
					me, url, klass, klass, node_name);
				return ERROR;
			}
			if (NULL == (cn_cur = oxnew(OhubMsgClassNode_t, 1))) {
				ohubLog(1, "%s: out of memory", me);
				return ERROR;
			}
			if (NULL == pklass) {
				if (NULL == (pklass = oxnew(OhubMsgClass_t, 1))) {
					oxfree(cn_cur);
					ohubLog(1, "%s: out of memory", me);
					return ERROR;
				}
				pklass->klass = klass;
				pklass->next = ohub_msg_classes;
				ohub_msg_classes = pklass;
			}
			cn_cur->node = pnode;
			cn_cur->next = pklass->node_list;
			pklass->node_list = cn_cur;
			ohubLog(5, "%s: added class %d(0x%x) for node `%s'",
					me, klass, klass, node_name);
		} else {
			/* remove the class-to-node descriptor */
			if (NULL == cn_cur) {
				ohubLog(3, "%s: class %d(0x%x) not with node `%s'",
						me, klass, klass, node_name);
				return ERROR;
			}
			if (NULL == cn_prev)
				pklass->node_list = cn_cur->next;
			else
				cn_prev->next = cn_cur->next;
			oxfree(cn_cur);
			if (NULL == pklass->node_list) {
				if (NULL == pkprev)
					ohub_msg_classes = pklass->next;
				else
					pkprev->next = pklass->next;
				oxfree(pklass);
			}
			if (pnode->klass_num > 0) {
				pnode->klass_num --;
			}
			ohubLog(5, "%s: removed class %d(0x%x) from node `%s'",
					me, klass, klass, node_name);
		}
	}

	return OK;
}


oret_t
ohubHandleMsgWaterMark(struct OhubNetWatch_st * pnwatch,
					OhubAgent_t * pagent,	/* FIXME: asymmetric ! */
					int kind, int type, int len, char *buf,
					const char *url)
{
	static const char *me = "handleMsgWaterMark";
	OhubMsgNode_t *pnode, *pnprev;

	if ((pnwatch && pagent) || (!pnwatch && !pagent)) {
		ohubLog(1, "%s: invalid references for %s", me, url);
		return ERROR;
	}

	if (len != 0) {
		ohubLog(5, "%s: invalid watermark from %s len=%d",
				me, url, len);
		return ERROR;
	}

	/* find a node to act upon */
	for (pnode = ohub_msg_nodes, pnprev = NULL; NULL != pnode;
						pnprev = pnode, pnode = pnode->next) {
		if (((NULL != pnode->subject_p && pnode->subject_p->pagent == pagent)
			|| (NULL == pnode->subject_p && NULL == pagent))
			&& pnode->watch_p == pnwatch) {
			break;
		}
	}
	if (NULL == pnode) {
		ohubLog(3, "%s: watermark node not found for %s", me, url);
		return ERROR;
	}

	pnode->is_full = (type == OLANG_MSG_HIMARK);
	ohubLog(5, "%s: node %s is %s",
			me, pnode->name, pnode->is_full ? "HI" : "LO");
	return OK;
}


oret_t
ohubHandleMsgSend(struct OhubNetWatch_st * pnwatch,
				OhubAgent_t * pagent,	/* FIXME: asymmetric ! */
				int kind, int type, int len, char *buf,
				const char *url)
{
	static const char *me = "handleMsgSend";
	ulong_t ulv;
	ushort_t usv;
	ulong_t m_op;
	int s_len, d_len, m_len;
	char *m_dest;
	OhubMsgNode_t *pnode = NULL;
	OhubMsgClassNode_t *pcn;
	OhubMsgClass_t *pclass;
	ulong_t reply[2];
	int m_klass, m_type;
	oret_t rc;
	int err = OK;

	if ((pnwatch && pagent) || (!pnwatch && !pagent)) {
		ohubLog(1, "%s: invalid references", me);
		return ERROR;
	}

	if (len < 18) {
		ohubLog(5, "%s: too short message len=%d from %s", me, len, url);
		return ERROR;
	}

	ulv = *(ulong_t *)(buf + 0);
	m_op = OUNTOHL(ulv);
	usv = *(ushort_t *)(buf + 8);
	m_len = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 10);
	s_len = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 12);
	d_len = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 14);
	m_klass = OUNTOHS(usv);
	usv = *(ushort_t *)(buf + 16);
	m_type = OUNTOHS(usv);

	if (len != 18 + s_len + d_len + m_len) {
		ohubLog(5, "%s: invalid message len=%d from %s", me, len, url);
		return ERROR;
	}

	if (d_len > 0) {
		/* targeted message */
		m_dest = buf + 18 + s_len;
		for (pnode = ohub_msg_nodes; NULL != pnode; pnode = pnode->next) {
			if (0 == strncmp(pnode->name, m_dest, d_len)
					&& 0 == pnode->name[d_len]) {
				break;
			}
		}
		if (NULL == pnode) {
			if (ohub_verb > 4) {
				char *dest = oxnew(char, d_len);
				oxbcopy(m_dest, dest, d_len);
				dest[d_len] = '\0';
				olog("%s: no target `%s' for msg(slen=%d,dlen=%d) from `%s'",
					me, dest, s_len, d_len, url);
				oxfree(dest);
			}
			err = OLANG_ERR_RCVR_OFF;
		} else if (pnode->is_full) {
			err = OLANG_ERR_RCVR_FULL;
		} else {
			if (NULL != pnode->watch_p)
				rc = ohubWatchSendPacket(
								pnode->watch_p,
								OLANG_DATA, OLANG_MSG_RECV, len, buf);
			else
				rc = ohubSubjSendPacket(
								pnode->subject_p->pagent->nsubj_p,
								OLANG_DATA, OLANG_MSG_RECV, len, buf);
			err = (rc == OK ? OK : OLANG_ERR_RCVR_DOWN);
			if (ohub_verb > 5) {
				char *dest = oxnew(char, d_len+1);
				oxbcopy(buf+18+s_len, dest, d_len);
				dest[d_len] = '\0';
				olog("%s: unicast (op=%lxh,cls=%d/%xh,typ=%d,len=%d,dst=%s)"
					" %s => %s rc=%d", me, m_op, m_klass, m_klass, m_type,
					m_len, dest, url, pnode ? pnode->name : "?", rc);
					oxfree(dest);
			}
		}
	} else if (0 == m_klass) {
		ohubLog(3, "%s: broadcast message without class !", me);
		err = OLANG_ERR_RCVR_DOWN; /* FIXME: need correct error code */
	} else {
		/* broadcast message */
		err = OLANG_ERR_RCVR_DOWN;
		for (pclass = ohub_msg_classes; pclass; pclass = pclass->next) {
			if (pclass->klass != m_klass)
				continue;
			for (pcn = pclass->node_list; pcn; pcn = pcn->next) {
				if (NULL == (pnode = pcn->node))
					continue;
				if (((NULL != pnode->subject_p
						&& pnode->subject_p->pagent == pagent)
					|| (NULL == pnode->subject_p && NULL == pagent))
						&& pnode->watch_p == pnwatch) {
					continue;	/* broadcast for everybody but himself */
				}
				if (pnode->is_full)
					continue;
				if (NULL != pnode->watch_p)
					rc = ohubWatchSendPacket(
									pnode->watch_p,
									OLANG_DATA, OLANG_MSG_RECV, len, buf);
				else
					rc = ohubSubjSendPacket(
									pnode->subject_p->pagent->nsubj_p,
									OLANG_DATA, OLANG_MSG_RECV, len, buf);
				if (ohub_verb > 5) {
					char *dest;
					dest = oxnew(char, d_len+1);
					oxbcopy(buf+18+s_len, dest, d_len);
					dest[d_len] = '\0';
					olog("%s: broadcast (op=%lxh,cls=%d/%xh,typ=%d,len=%d,dst=%s)"
						" %s => %s rc=%d", me, m_op, m_klass, m_klass, m_type,
						m_len, dest, url, pnode ? pnode->name : "?", rc);
						oxfree(dest);
				}
				if (rc == OK)
					err = OK;	/* if at least one send is success */
			}
		}
	}

	if (m_op != 0) {
		reply[0] = OUHTONL(m_op);
		reply[1] = OHTONL(err);
		if (pnwatch)
			rc = ohubWatchSendPacket(pnwatch, OLANG_DATA,
									OLANG_MSG_SEND_REPLY, 8, reply);
		else
			rc = ohubSubjSendPacket(pagent->nsubj_p, OLANG_DATA,
									OLANG_MSG_SEND_REPLY, 8, reply);
		ohubLog(7, "%s: replied to `%s' op=0x%lx err=0x%x", me, url, m_op, err);
	}

	return (OK == err ? OK : ERROR);
}
