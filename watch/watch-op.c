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

  Asynchronous operations.

*/

#include "watch-priv.h"
#include <optikus/util.h>		/* for osMsSleep,osMsClock */
#include <optikus/conf-mem.h>	/* for oxnew,oxfree,oxazero */
#include <stdio.h>				/* for sprinf */


/* FIXME: hidden flag of common use ! */
bool_t  owatch_handlers_are_lazy;

OwatchIdleHandler_t owatch_idle_handler[OWATCH_MAX_IDLE_HANDLERS];
int     owatch_idle_handler_qty;

typedef struct OwatchLazyCall_st
{
	int     type;
	long    param;
	struct OwatchLazyCall_st *next;
	struct OwatchLazyCall_st *prev;
} OwatchLazyCall_t;

OwatchLazyCall_t *owatch_first_lazy_call;
OwatchLazyCall_t *owatch_last_lazy_call;

int     owatch_async_timeout;

int     owatch_free_op_count;

typedef struct
{
	OwatchOpHandler_t phand;
	long    param;
} OwatchOpHandlerRecord_t;

OwatchOpHandlerRecord_t *owatch_op_handlers;

OwatchOpRecord_t *owatch_ops;

ulong_t owatch_next_op_id;

#define MAKE_OP_ID(result,no)                                        \
      if ((owatch_next_op_id & 0x0fff)==0)  owatch_next_op_id++;       \
      (result) = ((0x0fff & (no)) | ((owatch_next_op_id++ & 0x0fff) << 12));

#define NO_OF_OP(op)   ((int)((op) & 0x0fff))

#define IS_SAME_OP(op1,op2)    (NO_OF_OP(op1) == NO_OF_OP(op2))


char   *
owatchOpString(long op_type, char *buf)
{
	switch ((int) op_type) {
	case OWATCH_OPT_CONNECT:		return "conn";
	case OWATCH_OPT_DESC_INFO:		return "dscinf";
	case OWATCH_OPT_OOID_INFO:		return "ooidinf";
	case OWATCH_OPT_DESC_MON:		return "dscmon";
	case OWATCH_OPT_OOID_MON:		return "ooidmon";
	case OWATCH_OPT_COMBO_MON:		return "combomon";
	case OWATCH_OPT_WAIT_MON:		return "waitmon";
	case OWATCH_OPT_SEND:			return "send";
	case OWATCH_OPT_WRITE:			return "write";
	case OWATCH_OPT_READ:			return "read";
	case OWATCH_OPT_TRIGGER:		return "trigs";
	case OWATCH_OPT_MSG_SEND:		return "msgsend";
	case OWATCH_OPT_MSG_RECV:		return "msgrecv";
	case OWATCH_OPT_MSG_CLASS:		return "msgclas";
	default:
		sprintf(buf, "%lxh", op_type);
		return buf;
	}
}


const char *
owatchErrorString(int err_code)
{
	static char ermes[32];		/* FIXME: thread unsafe */

	switch (err_code) {
	case OWATCH_ERR_OK:				return "OK";
	case OWATCH_ERR_ERROR:			return "ERROR";
	case OWATCH_ERR_PENDING:		return "ERR_PENDING";
	case OWATCH_ERR_INTERNAL:		return "ERR_INTERNAL";
	case OWATCH_ERR_CANCELLED:		return "ERR_CANCELLED";
	case OWATCH_ERR_FREEOP:			return "ERR_FREEOP";
	case OWATCH_ERR_NETWORK:		return "ERR_NETWORK";
	case OWATCH_ERR_REFUSED:		return "ERR_REFUSED";
	case OWATCH_ERR_SCREWED:		return "ERR_SCREWED";
	case OWATCH_ERR_INVALID:		return "ERR_INVALID";
	case OWATCH_ERR_SMALL_DESC:		return "ERR_SMALL_DESC";
	case OWATCH_ERR_SMALL_DATA:		return "ERR_SMALL_DATA";
	case OWATCH_ERR_TOOLATE:		return "ERR_TOOLATE";
	case OWATCH_ERR_NOSPACE:		return "ERR_NOSPACE";
	case OWATCH_ERR_NODATA:			return "ERR_NODATA";
	case OWATCH_ERR_NOINFO:			return "ERR_NOINFO";
	case OWATCH_ERR_NOTFOUND:		return "ERR_NOTFOUND";
	case OWATCH_ERR_MSG_TRUNC:		return "ERR_MSG_TRUNC";
	case OWATCH_ERR_RCVR_OFF:		return "ERR_RCVR_OFF";
	case OWATCH_ERR_RCVR_FULL:		return "ERR_RCVR_FULL";
	default:
		if (err_code > 0)
			sprintf(ermes, "ERR_P_%d", err_code);
		else
			sprintf(ermes, "ERR_M_%d", -err_code);
		return ermes;
	}
}


/*
 *   add operation to the list
 *   FIXME: currently realized via inefficient
 *          sequential array (still easy to develop)
 */
owop_t
owatchAllocateOp(OwatchOpCanceller_t pcanc, long data1, long data2)
{
	OwatchOpRecord_t *oprec;
	int     no;

	if (!owatch_ops || owatch_free_op_count <= 0)
		return OWOP_ERROR;
	oprec = &owatch_ops[1];
	for (no = 1; no < OWATCH_MAX_OPS; no++, oprec++)
		if (oprec->op_state == OWATCH_OPS_FREE)
			break;
	if (no >= OWATCH_MAX_OPS) {
		owatchLog(10, "operations exhausted");
		return OWOP_ERROR;
	}
	MAKE_OP_ID(oprec->op, no);
	oprec->op_state = OWATCH_OPS_PENDING;
	oprec->err_code = 0;
	oprec->data1 = data1;
	oprec->data2 = data2;
	oprec->canceller = pcanc;
	oprec->m_op = oprec->s_op = OWOP_NULL;
	oprec->chainer = NULL;
	oprec->ch_param1 = oprec->ch_param2 = 0;
	oprec->loc_handler = NULL;
	oprec->loc_param = 0;
	owatchLog(11, "allocating op=%xh oprec=%ph", oprec->op, oprec);
	owatch_free_op_count--;
	return oprec->op;
}


/*
 *   returns operation descriptor given its Owop
 */
OwatchOpRecord_t *
owatchGetOpRecord(owop_t op)
{
	int     no = NO_OF_OP(op);
	OwatchOpRecord_t *oprec = &owatch_ops[no];

	if (no < 1 || no >= OWATCH_MAX_OPS || !owatch_ops)
		return NULL;
	if (oprec->op_state == OWATCH_OPS_FREE)
		return NULL;
	return oprec;
}


/*
 *  check that operation is valid
 */
bool_t
owatchIsValidOp(owop_t op)
{
	int     no = NO_OF_OP(op);
	OwatchOpRecord_t *oprec = &owatch_ops[no];

	if (no < 1 || no >= OWATCH_MAX_OPS || !owatch_ops) {
		owatchLog(10, "op %xh not valid: out of range no=%d", op, no);
		return FALSE;
	}
	if (oprec->op_state == OWATCH_OPS_FREE) {
		owatchLog(10, "op %xh not valid: not allocated", op);
		return FALSE;
	}
	if (oprec->op != op) {
		owatchLog(10, "op %xh not valid: mismatched %xh", op, oprec->op);
		return FALSE;
	}
	return TRUE;
}


/*
 *   free operation descriptor
 */
oret_t
owatchDeallocateOp(owop_t op)
{
	OwatchOpRecord_t *oprec = owatchGetOpRecord(op);

	owatchLog(11, "deallocating op=%xh oprec=%ph", op, oprec);
	if (!oprec)
		return ERROR;
	owatchLog(10, "deallocated op %xh", op);
	oprec->err_code = 0;
	oprec->data1 = 0;
	oprec->data2 = 0;
	oprec->canceller = NULL;
	oprec->op = NO_OF_OP(oprec->op);
	oprec->op_state = OWATCH_OPS_FREE;
	oprec->m_op = oprec->s_op = OWOP_NULL;
	oprec->chainer = NULL;
	oprec->ch_param1 = oprec->ch_param2 = 0;
	oprec->loc_handler = NULL;
	oprec->loc_param = 0;
	owatch_free_op_count++;
	return OK;
}


/*
 *   reset operation parameters
 */
owop_t
owatchReuseSlaveOp(owop_t op, OwatchOpCanceller_t pcanc, long data1, long data2)
{
	OwatchOpRecord_t *oprec = owatchGetOpRecord(op);

	if (!oprec || oprec->op != op) {
		ologAbort("invalid op %xh for reuse", op);
		return OWOP_ERROR;
	}
	if (oprec->s_op || oprec->m_op == OWOP_NULL) {
		ologAbort("should reuse only slave ops %xh", op);
		return OWOP_ERROR;
	}
	MAKE_OP_ID(oprec->op, oprec->op);
	oprec->op_state = OWATCH_OPS_PENDING;
	oprec->err_code = 0;
	oprec->data1 = data1;
	oprec->data2 = data2;
	oprec->canceller = pcanc;
	owatchLog(11, "reused old_op=%xh new_op=%xh", op, oprec->op);
	return oprec->op;
}


/*
 *  chain operations
 */
oret_t
owatchChainOps(owop_t m_op, owop_t s_op,
			  OwatchOpChainer_t chainer, long param1, long param2)
{
	OwatchOpRecord_t *m_oprec, *s_oprec;
	int     err_code;

	m_oprec = owatchGetOpRecord(m_op);
	if (NULL == m_oprec) {
		ologAbort("master %xh not found", m_op);
		return ERROR;
	}
	owatchLog(11, "chaining m_op=%xh s_op=%xh", m_op, s_op);
	if (m_oprec->s_op != OWOP_NULL) {
		ologAbort("master %xh already has slave %xh", m_op, m_oprec->s_op);
		return ERROR;
	}
	if (NULL == chainer) {
		ologAbort("null chainer");
		return ERROR;
	}
	m_oprec->chainer = chainer;
	m_oprec->ch_param1 = param1;
	m_oprec->ch_param2 = param2;
	switch (s_op) {
	case OWOP_OK:
	case OWOP_ERROR:
		m_oprec->s_op = s_op;
		err_code = s_op == OWOP_OK ? OK : ERROR;
		(*chainer) (m_op, s_op, err_code,
					m_oprec->ch_param1, m_oprec->ch_param2);
		if (m_oprec->s_op == s_op) {
			ologAbort("invalid chainer (1): master %xh slave %xh",
					 m_oprec->op, s_op);
			return ERROR;
		}
		break;
	case OWOP_SELF:
		m_oprec->s_op = s_op;
		break;
	case OWOP_ALL:
	case OWOP_NULL:
		ologAbort("invalid slave for master %xh", m_op);
		return ERROR;
	default:
		s_oprec = owatchGetOpRecord(s_op);
		if (!s_oprec || s_oprec->m_op != OWOP_NULL || s_oprec->op != s_op) {
			ologAbort("invalid slave %xh for master %xh", s_op, m_op);
			return ERROR;
		}
		m_oprec->s_op = s_op;
		s_oprec->m_op = m_op;
		break;
	}
	return OK;
}


oret_t
owatchUnchainOp(owop_t m_op)
{
	OwatchOpRecord_t *m_oprec, *s_oprec;
	owop_t  s_op;

	m_oprec = owatchGetOpRecord(m_op);
	if (NULL == m_oprec) {
		ologAbort("master %xh not found", m_op);
		return ERROR;
	}
	s_op = m_oprec->s_op;
	owatchLog(11, "unchaining m_op=%xh s_op=%xh", m_op, s_op);
	if (s_op == OWOP_NULL) {
		ologAbort("master %xh should have slave", m_op);
		return ERROR;
	}
	m_oprec->chainer = NULL;
	m_oprec->ch_param1 = m_oprec->ch_param2 = 0;
	m_oprec->s_op = OWOP_NULL;
	switch (s_op) {
	case OWOP_OK:
	case OWOP_ERROR:
	case OWOP_SELF:
		return OK;
	case OWOP_ALL:
	case OWOP_NULL:
		ologAbort("invalid slave for master %xh", m_op);
		return ERROR;
	}
	s_oprec = owatchGetOpRecord(s_op);
	if (NULL == s_oprec || !s_oprec->m_op || s_oprec->op != s_op) {
		ologAbort("invalid slave %xh for master %xh", s_op, m_op);
		return ERROR;
	}
	if (s_oprec->op_state != OWATCH_OPS_DONE) {
		ologAbort("invalid slave %xh state %d for master %xh",
				 s_op, s_oprec->op_state, m_op);
		return ERROR;
	}
	s_oprec->m_op = OWOP_NULL;
	owatchDeallocateOp(s_op);
	return OK;
}


/*
 *  finish operation and set its status
 */
oret_t
owatchFinalizeOp(owop_t op, int err_code)
{
	int     i;
	OwatchOpHandlerRecord_t *hrec;
	OwatchOpRecord_t *oprec, *m_oprec;
	OwatchOpChainer_t chainer;

	oprec = owatchGetOpRecord(op);
	if (NULL == oprec || oprec->op != op)
		return ERROR;
	if (oprec->op_state != OWATCH_OPS_PENDING) {
		ologAbort("cannot finalize non-pending op %xh", op);
		return ERROR;
	}
	if (err_code == OWATCH_ERR_PENDING) {
		ologAbort("cannot finalize with pending status op %xh", op);
		return ERROR;
	}
	if (oprec->s_op != OWOP_NULL && oprec->s_op != OWOP_SELF) {
		ologAbort("cannot finalize master %xh, slave %xh still undone",
				 op, oprec->s_op);
		return ERROR;
	}
	oprec->op_state = OWATCH_OPS_DONE;
	oprec->err_code = err_code;
	if (oprec->s_op == OWOP_SELF) {
		(*oprec->chainer) (op, OWOP_SELF, err_code,
						   oprec->ch_param1, oprec->ch_param2);
		if (oprec->s_op == OWOP_SELF) {
			ologAbort("invalid chainer %xh", op);
			return ERROR;
		}
		if (oprec->m_op == OWOP_NULL)
			goto END;
	}
	if (oprec->m_op != OWOP_NULL) {
		m_oprec = owatchGetOpRecord(oprec->m_op);
		if (!m_oprec || m_oprec->op != oprec->m_op || !m_oprec->chainer) {
			ologAbort("invalid master %xh in slave %xh", oprec->m_op, op);
			return ERROR;
		}
		chainer = m_oprec->chainer;
		(*chainer) (oprec->m_op, op, err_code,
					m_oprec->ch_param1, m_oprec->ch_param2);
		if (oprec->op == op || m_oprec->s_op == op) {
			ologAbort("invalid chainer (2): master %xh slave %xh",
					 oprec->m_op, op);
			return ERROR;
		}
		if (oprec->loc_handler)
			goto END;
	}
	hrec = &owatch_op_handlers[0];
	for (i = 0; i < OWATCH_MAX_OP_HANDLERS; i++, hrec++)
		if (hrec->phand != NULL)
			(*hrec->phand) (hrec->param, op, err_code);
  END:
	if (oprec->loc_handler) {
		owatchLog(11, "almost finalized op=%xh code=%d. calling local hdlr.",
					op, err_code);
		(*oprec->loc_handler) (oprec->loc_param, op, err_code);
		if (oprec->op == op)
			owatchDeallocateOp(op);
		else {
			owatchLog(11, "somebody else deallocated op=%xh. (new_op=%xh)",
						op, oprec->op);
		}
	}
	owatchLog(10, "finalized op=%xh code=%d", op, err_code);
	return OK;
}


/*
 *  cancel operation
 */
oret_t
owatchCancelOp(owop_t op)
{
	OwatchOpRecord_t *oprec;
	int     i;

	if (op == OWOP_OK || op == OWOP_ERROR || !owatch_ops)
		return ERROR;
	if (op == OWOP_ALL) {
		/* cancel all pending operations */
		oprec = &owatch_ops[1];
		for (i = 1; i < OWATCH_MAX_OPS; i++, oprec++)
			owatchCancelOp(oprec->op);
		return OK;
	}
	/* cancel a single operation */
	oprec = owatchGetOpRecord(op);
	if (NULL == oprec || oprec->op != op)
		return ERROR;
	switch (oprec->s_op) {
	case OWOP_OK:
	case OWOP_ERROR:
	case OWOP_NULL:
	case OWOP_ALL:
	case OWOP_SELF:
		break;
	default:
		/* cancel slave first: cancel chain downs to top */
		owatchCancelOp(oprec->s_op);
		break;
	}
	if (oprec->op_state == OWATCH_OPS_DONE) {
		owatchDeallocateOp(op);
		return ERROR;
	}
	if (oprec->op_state != OWATCH_OPS_PENDING)
		return ERROR;
	if (NULL != oprec->canceller) {
		(*oprec->canceller) (op, oprec->data1, oprec->data2);
	}
	owatchFinalizeOp(op, OWATCH_ERR_CANCELLED);
	owatchDeallocateOp(op);
	return OK;
}


/*
 *  wait for operation to complete
 */
oret_t
owatchWaitOp(owop_t op, int timeout, int *err_code)
{
	int     i, err, n_pend, n_ok, n_err, n_events;
	OwatchOpRecord_t *oprec;
	long    now, end;
	char    tmp_buf[16];

	if (!owatch_ops) {
		/* not initialized still */
		if (err_code)
			*err_code = OWATCH_ERR_ERROR;
		return ERROR;
	}
	if (op == OWOP_OK) {
		/* operation already OK */
		if (err_code)
			*err_code = OWATCH_ERR_OK;
		return OK;
	}
	if (op == OWOP_ERROR) {
		/* operation already failed */
		if (err_code)
			*err_code = OWATCH_ERR_ERROR;
		return ERROR;
	}
	if (op == OWOP_ALL) {
		/* wait for all pending operations */
		if (timeout < 0 && timeout != OWATCH_FOREVER && timeout != OWATCH_NOWAIT) {
			if (err_code)
				*err_code = OWATCH_ERR_INVALID;
			return ERROR;
		}
		now = osMsClock();
		end = now + timeout;
		for (;;) {
			/* count cases */
			n_pend = n_ok = n_err = 0;
			oprec = &owatch_ops[1];
			for (i = 1; i < OWATCH_MAX_OPS; i++, oprec++) {
				switch (oprec->op_state) {
				case OWATCH_OPS_PENDING:
					n_pend++;
					switch (oprec->data1) {

					}
					owatchLog(6, "pending op %d op=%xh data1=%s data2=%lxh",
								n_pend, oprec->op,
								owatchOpString(oprec->data1, tmp_buf),
								oprec->data2);
					break;
				case OWATCH_OPS_DONE:
					if (oprec->err_code == OWATCH_ERR_OK)
						n_ok++;
					else
						n_err++;
					break;
				case OWATCH_OPS_FREE:
					break;
				default:
					/* internal error */
					break;
				}
			}
			/* check results */
			if (!n_pend || timeout == OWATCH_NOWAIT)
				break;
			if (timeout == OWATCH_FOREVER) {
				do {
					n_events = owatchInternalPoll(OWATCH_POLL_INTERVAL_MS);
					owatchPerformLazyCalls();
					owatchLog(11, "poll event num=%d", n_events);
				} while (!n_events);
				if (n_events < 0) {
					owatchLog(3, "oops got poll ERROR");
					osMsSleep(OWATCH_POLL_INTERVAL_MS);
				}
				continue;
			}
			now = osMsClock();
			if (end - now < 0)
				break;
			owatchInternalPoll(end - now);
			owatchPerformLazyCalls();
			owatchCallIdleHandler();
		}
		/* there were pending operations */
		if (timeout != OWATCH_NOWAIT) {
			oprec = &owatch_ops[1];
			for (i = 1; i < OWATCH_MAX_OPS; i++, oprec++)
				if (oprec->op_state == OWATCH_OPS_DONE)
					owatchDeallocateOp(oprec->op);
		}
		/* return results */
		if (!n_pend) {
			if (err_code)
				*err_code = n_err ? OWATCH_ERR_ERROR : OWATCH_ERR_OK;
			owatchLog(10, "end wait all op no pending");
			return OK;
		}
		if (err_code)
			*err_code = n_err ? OWATCH_ERR_ERROR : OWATCH_ERR_PENDING;
		owatchLog(10, "end wait all op had pending");
		return ERROR;
	}
	oprec = owatchGetOpRecord(op);
	if (!oprec || oprec->op != op) {
		/* invalid operation parameter */
		if (err_code)
			*err_code = OWATCH_ERR_INVALID;
		return ERROR;
	}
	/* wait for single operation */
	if (timeout == OWATCH_NOWAIT) {
		if (oprec->op_state == OWATCH_OPS_PENDING) {
			if (err_code)
				*err_code = OWATCH_ERR_PENDING;
			return ERROR;
		}
	} else if (timeout == OWATCH_FOREVER) {
		while (oprec->op_state == OWATCH_OPS_PENDING) {
			n_events = owatchInternalPoll(OWATCH_POLL_INTERVAL_MS);
			owatchPerformLazyCalls();
			owatchCallIdleHandler();
			if (n_events < 0) {
				olog("oops2 got poll error");
				osMsSleep(OWATCH_POLL_INTERVAL_MS);
			}
		}
	} else if (timeout > 0) {
		now = osMsClock();
		end = now + timeout;
		while (oprec->op_state == OWATCH_OPS_PENDING && end - now > 0) {
			owatchInternalPoll(end - now);
			owatchPerformLazyCalls();
			owatchCallIdleHandler();
			now = osMsClock();
		}
		if (oprec->op_state == OWATCH_OPS_PENDING) {
			if (err_code)
				*err_code = OWATCH_ERR_PENDING;
			return ERROR;
		}
	} else {
		/* invalid timeout parameter */
		if (err_code)
			*err_code = OWATCH_ERR_INVALID;
		return ERROR;
	}
	if (oprec->op_state != OWATCH_OPS_DONE) {
		/* invalid operation state */
		if (err_code) {
			if (oprec->op_state == OWATCH_OPS_FREE) {
				/* operation was prematurely deallocated */
				*err_code = OWATCH_ERR_FREEOP;
			} else {
				/* another kind of invalid operation state */
				*err_code = OWATCH_ERR_INTERNAL;
			}
		}
		return ERROR;
	}
	/* operation is finished */
	err = oprec->err_code;
	if (err == 1)
		err = OWATCH_ERR_INTERNAL;
	owatchDeallocateOp(op);
	if (err_code)
		*err_code = err;
	return err == OWATCH_ERR_OK ? OK : ERROR;
}


/*
 *  wait for operation completion
 *  to be used within other procedure calls
 */
owop_t
owatchInternalWaitOp(owop_t op)
{
	oret_t rc;
	int     err_code;

	if (!owatch_async_timeout || op == OWOP_OK || op == OWOP_ERROR)
		return op;
	rc = owatchWaitOp(op, owatch_async_timeout, &err_code);
	switch (err_code) {
	case 0:
		return OWOP_OK;
	case 1:
		return op;
	}
	return OWOP_ERROR;
}


/*
 *  associates user data with operation
 */
oret_t
owatchSetOpData(owop_t op, long data1, long data2)
{
	OwatchOpRecord_t *oprec = owatchGetOpRecord(op);

	if (!oprec || oprec->op != op)
		return ERROR;
	oprec->data1 = data1;
	oprec->data2 = data2;
	return OK;
}


/*
 *  returns user data associated with operation
 */
oret_t
owatchGetOpData(owop_t op, long *pdata1, long *pdata2)
{
	OwatchOpRecord_t *oprec = owatchGetOpRecord(op);

	if (!oprec || oprec->op != op) {
		if (pdata1)
			*pdata1 = 0;
		if (pdata2)
			*pdata2 = 0;
		return ERROR;
	}
	if (pdata1)
		*pdata1 = oprec->data1;
	if (pdata2)
		*pdata2 = oprec->data2;
	return OK;
}


/*
 *  add new handle of operations completion
 */
oret_t
owatchAddOpHandler(const char *filter, OwatchOpHandler_t phand, long param)
{
	int     i;

	if (!owatch_op_handlers)
		return ERROR;
	for (i = 0; i < OWATCH_MAX_OP_HANDLERS; i++) {
		if (!owatch_op_handlers[i].phand) {
			owatch_op_handlers[i].param = param;
			owatch_op_handlers[i].phand = phand;
			return OK;
		}
	}
	return ERROR;
}


/*
 *  remove a handler
 */
oret_t
owatchRemoveOpHandler(OwatchOpHandler_t phand)
{
	int     i, n;

	if (!owatch_op_handlers || !phand)
		return ERROR;
	for (i = n = 0; i < OWATCH_MAX_OP_HANDLERS; i++) {
		if (owatch_op_handlers[i].phand == phand) {
			owatch_op_handlers[i].phand = NULL;
			owatch_op_handlers[i].param = 0;
			n++;
		}
	}
	return n ? OK : ERROR;
}


/*
 *   instant handler
 */
oret_t
owatchLocalOpHandler(owop_t op, OwatchOpHandler_t phand, long param)
{
	OwatchOpRecord_t *oprec;
	int     err_code;

	switch (op) {
	case OWOP_ALL:
	case OWOP_NULL:
	case OWOP_SELF:
		return ERROR;
	case OWOP_OK:
		if (!phand)
			return ERROR;
		(*phand) (param, op, OWATCH_ERR_OK);
		return OK;
	case OWOP_ERROR:
		if (!phand)
			return ERROR;
		(*phand) (param, op, OWATCH_ERR_ERROR);
		return OK;
	default:
		break;
	}
	oprec = owatchGetOpRecord(op);
	if (!oprec || !owatchIsValidOp(op))
		return ERROR;
	if (!phand) {
		/* FIXME: what if operation is already done ? */
		if (!oprec->loc_handler)
			return ERROR;
		oprec->loc_handler = NULL;
		oprec->loc_param = 0;
		return OK;
	}
	if (oprec->loc_handler)
		return ERROR;
	if (oprec->op_state == OWATCH_OPS_DONE) {
		owatchWaitOp(op, OWATCH_NOWAIT, &err_code);
		(*phand) (param, op, err_code);
		return OK;
	}
	oprec->loc_param = param;
	oprec->loc_handler = phand;
	return OK;
}


/*
 * Detach operation.
 */
static int
_owatchOpDetacher(long param, owop_t op, int err_code)
{
	return 0;
}

oret_t
owatchDetachOp(owop_t op)
{
	return owatchLocalOpHandler(op, _owatchOpDetacher, 0);
}


/*
 *  get library parameter 'async_timeout'
 */
int
owatchGetAsyncTimeout()
{
	return owatch_async_timeout;
}


oret_t
owatchSetAsyncTimeout(int timeout)
{
	if (timeout == OWATCH_NOWAIT || timeout == OWATCH_FOREVER || timeout > 0) {
		owatch_async_timeout = timeout;
		return OK;
	}
	return ERROR;
}


/*
 *  initialize operations structures
 */
oret_t
owatchInitOperations(void)
{
	owatch_next_op_id = osMsClock();
	owatch_next_op_id = 0x0ff8;
	owatch_op_handlers = oxnew(OwatchOpHandlerRecord_t, OWATCH_MAX_OP_HANDLERS);
	if (!owatch_op_handlers)
		return ERROR;
	if (NULL == (owatch_ops = oxnew(OwatchOpRecord_t, OWATCH_MAX_OPS)))
		return ERROR;
	owatch_free_op_count = OWATCH_MAX_OPS;
	return OK;
}


/*
 *  clear operations structures
 */
oret_t
owatchExitOperations(void)
{
	int     i;
	OwatchLazyCall_t *next, *curr;

	oxazero(owatch_idle_handler);
	owatch_idle_handler_qty = 0;

	next = owatch_first_lazy_call;
	while (next) {
		curr = next;
		next = curr->next;
		oxfree(curr);
	}
	owatch_first_lazy_call = owatch_last_lazy_call = NULL;

	if (NULL != owatch_op_handlers) {
		for (i = 0; i < OWATCH_MAX_OP_HANDLERS; i++)
			owatch_op_handlers[i].phand = NULL;
	}
	oxfree(owatch_op_handlers);
	owatch_op_handlers = NULL;

	if (NULL != owatch_ops) {
		for (i = 0; i < OWATCH_MAX_OPS; i++)
			owatch_ops[i].op = 0;
	}
	oxfree(owatch_ops);
	owatch_ops = NULL;

	owatch_free_op_count = 0;

	return OK;
}


int
owatchFreeOpCount()
{
	return owatch_free_op_count;
}


oret_t
owatchRecordLazyCall(int type, long param)
{
	OwatchLazyCall_t *call = oxnew(OwatchLazyCall_t, 1);

	call->type = type;
	call->param = param;
	call->next = NULL;
	call->prev = owatch_last_lazy_call;
	if (owatch_last_lazy_call)
		owatch_last_lazy_call->next = call;
	owatch_last_lazy_call = call;
	if (!owatch_first_lazy_call)
		owatch_first_lazy_call = call;
	return OK;
}


int
owatchPerformLazyCalls()
{
	OwatchLazyCall_t *next = owatch_first_lazy_call;
	OwatchLazyCall_t *curr;
	int     count = 0;

	owatch_first_lazy_call = owatch_last_lazy_call = NULL;
	while (next) {
		curr = next;
		next = curr->next;
		switch (curr->type) {
		case 1:
			owatchCallDataHandlers(curr->param);
			break;
		case 2:
			owatchCallAliveHandlers(curr->param);
			break;
		}
		oxfree(curr);
		count++;
	}
	return count;
}


oret_t
owatchCallIdleHandler(void)
{
	int     i;

	for (i = 0; i < owatch_idle_handler_qty; i++)
		if (owatch_idle_handler[i])
			(*(owatch_idle_handler[i])) ();
	return OK;
}


oret_t
owatchRegisterIdleHandler(OwatchIdleHandler_t handler, bool_t enable)
{
	int     i, j;
	oret_t rc;

	if (enable) {
		if (!handler)
			return ERROR;
		for (i = 0; i < OWATCH_MAX_IDLE_HANDLERS; i++) {
			if (owatch_idle_handler[i] == handler)
				return OK;
		}
		for (i = 0; i < OWATCH_MAX_IDLE_HANDLERS; i++) {
			if (owatch_idle_handler[i] == 0) {
				owatch_idle_handler[i] = handler;
				if (i >= owatch_idle_handler_qty)
					owatch_idle_handler_qty = i + 1;
				return OK;
			}
		}
		return ERROR;
	}
	if (!handler) {
		oxazero(owatch_idle_handler);
		owatch_idle_handler_qty = 0;
		return OK;
	}
	rc = ERROR;
	for (i = 0; i < OWATCH_MAX_IDLE_HANDLERS; i++) {
		if (owatch_idle_handler[i] == handler) {
			for (j = i; j < OWATCH_MAX_IDLE_HANDLERS - 1; j++)
				owatch_idle_handler[j] = owatch_idle_handler[j + 1];
			owatch_idle_handler[OWATCH_MAX_IDLE_HANDLERS - 1] = 0;
			owatch_idle_handler_qty--;
			rc = OK;
		}
	}
	return rc;
}
