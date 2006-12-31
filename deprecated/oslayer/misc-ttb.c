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

  True Time Board API for IRIG Bancomm 63x/VME.

*/

#include <optikus/ttb.h>
#include <optikus/util.h>		/* for osMsSleep() */
#include <optikus/conf-net.h>	/* for ONTOHS,... */
#include <optikus/conf-os.h>
#include <stdio.h>				/* for printf() */


#define BIN_UNITS(x)     (((x) % 10) + '0')
#define BIN_TENS(x)      ((((x) / 10) % 10) + '0')
#define BIN_HUNDREDS(x)  ((((x) / 100) % 10) + '0')

#define WB4(w)  (((ushort_t)(w) & 0xF000) >> 12)
#define WB3(w)  (((ushort_t)(w) & 0x0F00) >> 8 )
#define WB2(w)  (((ushort_t)(w) & 0x00F0) >> 4 )
#define WB1(w)  (((ushort_t)(w) & 0x000F)      )

#define TTB_PROBE_ID     (TTB_MAX_NUM-1)

#define ttbPrintf(verb,args...)	(ttb_verb >= (verb) ? printf(args) : -1)

int     ttb_verb = 2;

int     ttb_error;
ushort_t *ttb_base[TTB_MAX_NUM];
int     ttb_type[TTB_MAX_NUM];
bool_t  ttb_auto_year[TTB_MAX_NUM];
bool_t  ttb_auto_stamp[TTB_MAX_NUM];
int     ttb_prev_year[TTB_MAX_NUM];
int     ttb_irq[TTB_MAX_NUM];
int     ttb_vector[TTB_MAX_NUM];
TtbIntr_t ttb_proc[TTB_MAX_NUM][TTB_INTR_MAX_NUM];
int     ttb_param[TTB_MAX_NUM][TTB_INTR_MAX_NUM];
int     ttb_intr_qty[TTB_MAX_NUM][TTB_INTR_MAX_NUM];
int     ttb_intr_state[TTB_MAX_NUM];
bool_t  ttb_intr_on[TTB_MAX_NUM];
bool_t  ttb_strict_probe = TRUE;

int     ttb_fifo_phase;
int     ttb_fifo_count;

int     ttb_fifo_pause = 2;

int     ttb_burn_gap_ms = 500;

#define tmp_i  __ttb_tmp_count
static volatile int tmp_i;


static int ttbInterruptRoutine(int idx);


/*
 * Scan the VME A16 address space for a time board.
 * start_a16 - first scan address (by default 0).
 * end_a16 - end scan address (by default 0xFFC0).
 * Always returns OK.
 * Prints addresses and types of any boards found.
 */
oret_t
ttbFind(int start_a16, int end_a16)
{
	oret_t rc;
	int     adr_a16;
	char   *type_str;
	int     save_verb = ttb_verb;

	ttb_verb = 1;
	if (start_a16 < 0)
		start_a16 = 0;
	if (end_a16 < 0)
		end_a16 = 0;
	if (end_a16 == 0)
		end_a16 = 0xFFC0;
	for (adr_a16 = start_a16; adr_a16 < end_a16; adr_a16 += 0x40) {
		rc = ttbInit(TTB_PROBE_ID, adr_a16, TTB_TYPE_Auto, 0, 0);
		if (rc)
			continue;
		switch (ttb_type[TTB_PROBE_ID]) {
		case TTB_TYPE_BC635:
			type_str = "Bancomm635vme";
			break;
		case TTB_TYPE_TrueTime:
			type_str = "TrueTime";
			break;
		default:
			type_str = "Unknown";
			break;
		}
		printf("Board \"%s\" at address %04Xh\n", type_str, adr_a16);
		ttbExit(TTB_PROBE_ID);
	}
	ttb_verb = save_verb;
	return OK;
}


/*
 * Probe the type of a time board located at the VME A16 address
 * base_a16 and print its state. If the board is not found or has
 * invalid or unsupported type, returns -1.
 */
oret_t
ttbPrint(int base_a16)
{
	char    buf[40];
	TimeUnsegFine_t t;
	volatile ushort_t *p;
	oret_t rc;
	bool_t  tmp_bool;
	int     idx;

	if (base_a16 == 0) {
		idx = 0;
		if (!ttb_type[idx])
			return ERROR;
	} else {
		idx = TTB_PROBE_ID;
		rc = ttbInit(idx, base_a16, TTB_TYPE_Auto, 0, 0);
		if (rc)
			return ERROR;
	}
	p = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		printf("TYPE: Bancomm635vme\n");
		printf("BASE=%08Xh\n", (unsigned) p);
		printf("ID=%04Xh DEV=%04Xh STS=%04Xh VEC=%04Xh IRQ=%d\n",
			   OUNTOHS(p[TTB_BC635_REG_ID]),
			   OUNTOHS(p[TTB_BC635_REG_DEVICE]),
			   OUNTOHS(p[TTB_BC635_REG_STATUS]),
			   OUNTOHS(p[22]), OUNTOHS(p[23]));
		tmp_i = p[TTB_BC635_REG_TIMEREQ];
		printf("RAW_TIME=%04X.%04X.%04X.%04X.%04X\n",
			   OUNTOHS(p[6]),
			   OUNTOHS(p[7]), OUNTOHS(p[8]), OUNTOHS(p[9]), OUNTOHS(p[10]));
		tmp_bool = ttbAutoYear(idx, TRUE);
		rc = ttbGetTime(idx, &t);
		ttbAutoYear(idx, tmp_bool);
		otimeUnsegFine2SegString(&t, buf);
		printf("FULL_TIME=%s\n", buf);
		break;
	case TTB_TYPE_TrueTime:
		printf("TYPE: TrueTime\n");
		break;
	default:
		printf("TYPE: Unknown\n");
		break;
	}
	if (base_a16)
		ttbExit(idx);
	return OK;
}


/*
 * Initialize the time board probing for its type, if needed.
 * If the board is not found or has unsupported/invalid type,
 * returns -1.
 */
int
ttbInit(int idx, int base_a16, int a_type, int irq, int vector)
{
	static const char *me = "ttbInit";
	TimeUnsegFine_t time_val;
	volatile char *cp;
	int     local, type, i;
	volatile ushort_t *sp;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		ttbPrintf(2, "%s: index %d out of range\n", me, idx);
		return ERROR;
	}
	if (base_a16 < 0 || base_a16 > 0xffff) {
		ttbPrintf(2, "%s: address %04Xh out of range\n", me, base_a16);
		return ERROR;
	}
	if (ttb_type[idx]) {
		ttbPrintf(2, "%s: board %d already active\n", me, idx);
		ttbExit(idx);
	}
	local = osVme2Mem(VME_AM_SUP_A16, base_a16, 0);
	if (local == -1) {
		ttbPrintf(2, "%s: address %04Xh out of range\n", me, base_a16);
		return ERROR;
	}
	cp = (volatile char *) local;
	sp = (volatile ushort_t *) local;
	/* Find the board time. */
	/* NOTE: TrueTime is not yet supported. */
	if (osMemProbe((char *) cp + 0x00, 2, FALSE) == ERROR ||
		osMemProbe((char *) cp + 0x02, 2, FALSE) == ERROR ||
		osMemProbe((char *) cp + 0x2E, 2, FALSE) == ERROR) {
		ttbPrintf(2, "%s: address %04Xh is empty\n", me, base_a16);
		return ERROR;
	}
	type = a_type;
	if (type == TTB_TYPE_Auto || ttb_strict_probe) {
		if (OUNTOHS(sp[0]) == 0xfef4 && OUNTOHS(sp[1]) == 0xf350) {
			type = TTB_TYPE_BC635;
		} else {
			ttbPrintf(2, "%s: cannot recognize board at %04Xh, got %04Xh/%04Xh\n",
					me, base_a16, OUNTOHS(sp[0]), OUNTOHS(sp[1]));
			return ERROR;
		}
		if (a_type != TTB_TYPE_Auto && a_type != type) {
			ttbPrintf(2, "%s: type at address %04Xh is not %d\n",
					me, base_a16, a_type);
			return ERROR;
		}
		a_type = type;
	}
	/* Initialize parameters. */
	ttb_base[idx] = (ushort_t *) sp;
	ttb_type[idx] = type;
	ttb_auto_year[idx] = FALSE;
	ttb_auto_stamp[idx] = FALSE;
	ttb_irq[idx] = irq;
	ttb_vector[idx] = vector;
	ttb_prev_year[idx] = TTB_DEF_YEAR;
	for (i = 0; i < TTB_INTR_MAX_NUM; i++) {
		ttb_proc[idx][i] = NULL;
		ttb_param[idx][i] = 0;
	}
	/* Set up the hardware. */
	switch (type) {
	case TTB_TYPE_BC635:
		sp[TTB_BC635_REG_CONTROL] = OUHTONS(0x0001);
		sp[TTB_BC635_REG_MASK] = OUHTONS(0x0000);
		sp[TTB_BC635_REG_VECTOR] = OUHTONS(0);
		sp[TTB_BC635_REG_LEVEL] = OUHTONS(0);
		break;
	default:
		break;
	}
	/* Try reading the time. */
	ttbGetTime(idx, &time_val);
	return OK;
}


/*
 * Enable/disable interrupts.
 */
oret_t
ttbEnableIntr(int idx, bool_t on)
{
	volatile ushort_t *sp;

	/* Validate arguments. */
	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	if (!ttb_type[idx]) {
		return ERROR;
	}
	if (ttb_irq[idx] == 0 || ttb_vector[idx] == 0) {
		return ERROR;
	}
	/* Validate the invocation nesting. */
	if (ttb_intr_state[idx] < 0)
		ttb_intr_state[idx] = 0;
	if (on) {
		if (ttb_intr_state[idx]++ > 0)
			return OK;
	} else {
		if (--ttb_intr_state[idx] > 0)
			return OK;
	}
	/* Enable interrupts from the board. */
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		if (on) {
			sp[TTB_BC635_REG_VECTOR] = OUHTONS(ttb_vector[idx]);
			sp[TTB_BC635_REG_LEVEL] = OUHTONS(ttb_irq[idx]);
		} else {
			sp[TTB_BC635_REG_VECTOR] = OUHTONS(0);
			sp[TTB_BC635_REG_LEVEL] = OUHTONS(0);
		}
		break;
	default:
		return ERROR;
	}
	/* Connect to the VME interrupt. */
	if (on) {
		osSetVmeVect(ttb_vector[idx], (proc_t) ttbInterruptRoutine, idx);
		osSetVmeIrq(ttb_irq[idx], TRUE);
		ttb_intr_on[idx] = TRUE;
	} else {
		osSetVmeVect(ttb_vector[idx], (proc_t) NULL, 0);
		osSetVmeIrq(ttb_irq[idx], FALSE);
		ttb_intr_on[idx] = FALSE;
	}
	return OK;
}


/*
 * Shut down board operations.
 */
oret_t
ttbExit(int idx)
{
	volatile ushort_t *sp;
	int     i;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	if (!ttb_type[idx]) {
		return ERROR;
	}
	sp = ttb_base[idx];

	/* Disconnect from the interrupt. */
	if (ttb_intr_state[idx] > 0) {
		ttb_intr_state[idx] = 1;
		ttbEnableIntr(idx, FALSE);
	}
	ttb_intr_state[idx] = 0;
	ttb_intr_on[idx] = FALSE;

	/* Reset the hardware. */
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		sp[TTB_BC635_REG_CONTROL] = OUHTONS(0x0001);
		sp[TTB_BC635_REG_MASK] = OUHTONS(0x0000);
		sp[TTB_BC635_REG_VECTOR] = OUHTONS(0);
		sp[TTB_BC635_REG_LEVEL] = OUHTONS(0);
		break;
	default:
		break;
	}

	/* Reset the parameters. */
	ttb_base[idx] = NULL;
	ttb_type[idx] = 0;
	ttb_auto_year[idx] = FALSE;
	ttb_irq[idx] = 0;
	ttb_vector[idx] = 0;
	for (i = 0; i < TTB_INTR_MAX_NUM; i++) {
		ttb_proc[idx][i] = NULL;
		ttb_param[idx][i] = 0;
	}

	return OK;
}


/*
 * Low-level interrupt service routine for time boards.
 */
static int
ttbInterruptRoutine(int idx)
{
	TimeIrigBC_t *bc_time_p;
	TimeUnsegFine_t uf_time = { 0 };
	ushort_t buf[10], dev_mask, w1, w2, w3;
	int     year, type;
	ulong_t stamp = 0;
	volatile ushort_t *sp;
	TtbIntr_t func;
	bool_t  tmp_bool;
	char    usr_mask[TTB_INTR_MAX_NUM] = { FALSE };

	if (idx < 0 || idx >= TTB_MAX_NUM)
		return 0;
	sp = ttb_base[idx];

	/* Request the event time stamp and find the type of the interrupt. */
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		if (ttb_auto_stamp[idx]) {
			buf[0] = sp[TTB_BC635_REG_TIMEREQ];
			buf[1] = sp[TTB_BC635_REG_TIME0];
			buf[2] = sp[TTB_BC635_REG_TIME1];
			w1 = buf[3] = sp[TTB_BC635_REG_TIME2];
			w1 = OUNTOHS(w1);
			w2 = buf[4] = sp[TTB_BC635_REG_TIME3];
			w2 = OUNTOHS(w2);
			w3 = buf[5] = sp[TTB_BC635_REG_TIME4];
			w3 = OUNTOHS(w3);
			stamp = (WB4(w1) * 600000000uL + WB3(w1) * 60000000uL +
					 WB2(w1) * 10000000uL + WB1(w1) * 1000000uL +
					 WB4(w2) * 100000uL + WB3(w2) * 10000uL + WB2(w2) * 1000uL +
					 WB1(w2) * 100uL + WB4(w3) * 10uL + (ulong_t) WB3(w3)
				);
		}
		dev_mask = sp[TTB_BC635_REG_INTSTAT];
		dev_mask = OUNTOHS(dev_mask);
		if (dev_mask & TTB_BC635_MSK_EXT) {
			sp[TTB_BC635_REG_INTSTAT] = OUHTONS(TTB_BC635_MSK_EXT);
			usr_mask[TTB_INTR_EXT] = 1;
		}
		if (dev_mask & TTB_BC635_MSK_PERIODIC) {
			sp[TTB_BC635_REG_INTSTAT] = OUHTONS(TTB_BC635_MSK_PERIODIC);
			usr_mask[TTB_INTR_PERIODIC] = 1;
		}
		if (dev_mask & TTB_BC635_MSK_TIME) {
			sp[TTB_BC635_REG_INTSTAT] = OUHTONS(TTB_BC635_MSK_TIME);
			usr_mask[TTB_INTR_TIME] = 1;
		}
		if (dev_mask & TTB_BC635_MSK_PPS) {
			sp[TTB_BC635_REG_INTSTAT] = OUHTONS(TTB_BC635_MSK_PPS);
			usr_mask[TTB_INTR_PPS] = 1;
		}
		if (dev_mask & TTB_BC635_MSK_FIFO) {
			sp[TTB_BC635_REG_INTSTAT] = OUHTONS(TTB_BC635_MSK_FIFO);
			usr_mask[TTB_INTR_FIFO] = 1;
		}
		if (ttb_auto_stamp[idx]) {
			tmp_bool = ttb_auto_year[idx];
			ttb_auto_year[idx] = FALSE;
			ttbGetYear(idx, &year);
			ttb_auto_year[idx] = tmp_bool;
			bc_time_p = (TimeIrigBC_t *) buf;
			bc_time_p->unused_year = year;
			otimeIrigBC2UnsegFine(bc_time_p, &uf_time);
		}
		break;
	case TTB_TYPE_TrueTime:
	default:
		return 0;
	}
	for (type = 0; type < TTB_INTR_MAX_NUM; type++) {
		if (usr_mask[type]) {
			ttb_intr_qty[idx][type]++;
			func = ttb_proc[idx][type];
			if (func != NULL)
				func(idx, type, ttb_param[idx][type], stamp, &uf_time);
		}
	}
	return 0;
}


/*
 * Hook up a user interrupt handler.
 */
oret_t
ttbHandler(int idx, int intr_type, TtbIntr_t handler, int param)
{
	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	if (!ttb_type[idx]) {
		return ERROR;
	}
	if (!ttb_vector[idx] && !ttb_irq[idx]) {
		return ERROR;
	}
	if (intr_type < 0 || intr_type >= TTB_INTR_MAX_NUM) {
		return ERROR;
	}
	ttb_proc[idx][intr_type] = NULL;
	ttb_param[idx][intr_type] = param;
	ttb_proc[idx][intr_type] = handler;
	return OK;
}


/*
 * Enable or disable interrupts by type.
 */
oret_t
ttbSetIntr(int idx, int intr_type, bool_t enable)
{
	ushort_t *mp, *sp;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	if (!ttb_type[idx]) {
		return ERROR;
	}
	if (!ttb_vector[idx] && !ttb_irq[idx]) {
		return ERROR;
	}
	if (intr_type < 0 || intr_type >= TTB_INTR_MAX_NUM) {
		return ERROR;
	}
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		mp = &ttb_base[idx][TTB_BC635_REG_MASK];
		sp = &ttb_base[idx][TTB_BC635_REG_INTSTAT];
		switch (intr_type) {
		case TTB_INTR_EXT:
			*sp = OUHTONS(TTB_BC635_MSK_EXT);
			break;
		case TTB_INTR_PERIODIC:
			*sp = OUHTONS(TTB_BC635_MSK_PERIODIC);
			break;
		case TTB_INTR_TIME:
			*sp = OUHTONS(TTB_BC635_MSK_TIME);
			break;
		case TTB_INTR_PPS:
			*sp = OUHTONS(TTB_BC635_MSK_PPS);
			break;
		case TTB_INTR_FIFO:
			*sp = OUHTONS(TTB_BC635_MSK_FIFO);
			break;
		default:
			break;
		}
		if (enable) {
			switch (intr_type) {
			case TTB_INTR_EXT:
				*mp |= OUHTONS(TTB_BC635_MSK_EXT);
				break;
			case TTB_INTR_PERIODIC:
				*mp |= OUHTONS(TTB_BC635_MSK_PERIODIC);
				break;
			case TTB_INTR_TIME:
				*mp |= OUHTONS(TTB_BC635_MSK_TIME);
				break;
			case TTB_INTR_PPS:
				*mp |= OUHTONS(TTB_BC635_MSK_PPS);
				break;
			case TTB_INTR_FIFO:
				*mp |= OUHTONS(TTB_BC635_MSK_FIFO);
				break;
			default:
				break;
			}
		} else {
			switch (intr_type) {
			case TTB_INTR_EXT:
				*mp &= ~OUHTONS(TTB_BC635_MSK_EXT);
				break;
			case TTB_INTR_PERIODIC:
				*mp &= ~OUHTONS(TTB_BC635_MSK_PERIODIC);
				break;
			case TTB_INTR_TIME:
				*mp &= ~OUHTONS(TTB_BC635_MSK_TIME);
				break;
			case TTB_INTR_PPS:
				*mp &= ~OUHTONS(TTB_BC635_MSK_PPS);
				break;
			case TTB_INTR_FIFO:
				*mp &= ~OUHTONS(TTB_BC635_MSK_FIFO);
				break;
			default:
				break;
			}
		}
		break;
	case TTB_TYPE_TrueTime:
	default:
		return ERROR;
	}
	return ttbEnableIntr(idx, enable);
}


/*
 * Example of interrupt handler, for testing.
 */
int
ttbInterruptTestHandler(int idx, int type, int param,
						ulong_t stamp, TimeUnsegFine_t * tp)
{
	char   *intr_str, time_str[64];

	switch (type) {
	case TTB_INTR_EXT:
		intr_str = "EXT";
		break;
	case TTB_INTR_PERIODIC:
		intr_str = "PERIOD";
		break;
	case TTB_INTR_TIME:
		intr_str = "TIME";
		break;
	case TTB_INTR_PPS:
		intr_str = "PPS";
		break;
	case TTB_INTR_FIFO:
		intr_str = "FIFO";
		break;
	default:
		intr_str = "UNKNOWN";
		break;
	}
	otimeUnsegFine2SegString(tp, time_str);
	printf("IRIG: %-2d INTR: %-3d TYPE: %-8s TIME: %-17s STAMP: %-7lu\n",
			idx, ttb_intr_qty[idx][type], intr_str, time_str, stamp);
	return 0;
}


/*
 * Test interrupts.
 */
oret_t
ttbTestIntr(int idx, int intr_type, bool_t start)
{
	oret_t rc;

	if (start) {
		rc = ttbHandler(idx, intr_type, ttbInterruptTestHandler, 0);
		rc |= ttbAutoStamp(idx, TRUE);
		rc |= ttbSetIntr(idx, intr_type, TRUE);
	} else {
		rc = ttbSetIntr(idx, intr_type, FALSE);
		rc |= ttbAutoStamp(idx, FALSE);
		rc |= ttbHandler(idx, intr_type, NULL, 0);
	}
	return rc;
}


/*
 * Write a string in FIFO (if it is present on the board).
 */
oret_t
ttbFifoWrite(int idx, const char *data)
{
	static const char *me = "ttbFifoWrite";
	volatile ushort_t *sp;
	volatile const uchar_t *dp = (volatile const uchar_t *) data;
	int     count = 100000;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		ttbPrintf(4, "%s: index %d out of range\n", me, idx);
		return ERROR;
	}
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		sp[TTB_BC635_REG_ACK] = OUHTONS(0x04);	/* remove ack. of output packet */
		sp[TTB_BC635_REG_ACK] = OUHTONS(0x08);	/* clear output fifo */
		sp[TTB_BC635_REG_FIFO] = OUHTONS(0x01);	/* begin packet */
		while (*dp) {
			ushort_t w = *dp++;

			sp[TTB_BC635_REG_FIFO] = OUHTONS(w);
		}
		sp[TTB_BC635_REG_FIFO] = OUHTONS(0x17);	/* end */
		sp[TTB_BC635_REG_ACK] = OUHTONS(0x81);
		while ((sp[TTB_BC635_REG_ACK] & OUHTONS(0x01)) == 0 && count-- > 0);
		if (count <= 0) {
			ttbPrintf(4, "%s: no FIFO acknowledge (idx=%d)\n", me, idx);
			return ERROR;
		}
		break;
	default:
		ttbPrintf(4, "%s: board undefined (idx=%d)\n", me, idx);
		return ERROR;
	}
	return OK;
}


/*
 * Read a string from FIFO (if the board has it).
 */
oret_t
ttbFifoRead(int idx, char *result)
{
	static const char *me = "ttbFifoRead";
	uchar_t *dp = (uchar_t *) result;
	ushort_t *sp;
	int     i, count;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		ttbPrintf(4, "%s: index %d out of range\n", me, idx);
		return ERROR;
	}
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		sp = ttb_base[idx];
		count = 10000;
		ttb_fifo_phase = 1;
		while ((sp[TTB_BC635_REG_ACK] & OUHTONS(0x04)) == 0 && count-- > 0) {
			ttb_fifo_count = count;
			for (i = 0; i < ttb_fifo_pause; i++);
		}
		ttb_fifo_phase = 2;
		if (count <= 0) {
			dp[0] = 0;
			ttb_fifo_phase = 5;
			ttbPrintf(4, "%s: FIFO not ready (idx=%d)\n", me, idx);
			return ERROR;
		}
		for (count = 0; count < 63; count++) {
			ttb_fifo_count = count;
			ttb_fifo_phase = 3;
			dp[count] = (uchar_t) (ushort_t) OUNTOHS(sp[TTB_BC635_REG_FIFO]);
			if (dp[count] == 0x17) {
				count++;
				break;
			}
			ttb_fifo_phase = 4;
			for (i = 0; i < ttb_fifo_pause; i++);
		}
		if (count == 64) {
			dp[0] = 0;
			ttb_fifo_phase = 6;
			ttbPrintf(4, "%s: too long FIFO responce (idx=%d)\n", me, idx);
			return ERROR;
		}
		ttb_fifo_phase = 0;
		dp[count] = 0;
		break;
	default:
		ttbPrintf(4, "%s: board undefined (idx=%d)\n", me, idx);
		return ERROR;
	}
	return OK;
}


/*
 * Read the microsecond time stamp [0..3600*10^6-1].
 */
ulong_t
ttbGetStamp(int idx)
{
	ushort_t w1, w2, w3, *sp;
	ulong_t r;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return 0;
	}
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		tmp_i = sp[TTB_BC635_REG_TIMEREQ];
		w1 = sp[TTB_BC635_REG_TIME2];
		w1 = OUNTOHS(w1);
		w2 = sp[TTB_BC635_REG_TIME3];
		w2 = OUNTOHS(w2);
		w3 = sp[TTB_BC635_REG_TIME4];
		w3 = OUNTOHS(w3);
		r = (WB4(w1) * 600000000uL + WB3(w1) * 60000000uL +
			 WB2(w1) * 10000000uL + WB1(w1) * 1000000uL +
			 WB4(w2) * 100000uL + WB3(w2) * 10000uL + WB2(w2) * 1000uL +
			 WB1(w2) * 100uL + WB4(w3) * 10uL + (ulong_t) WB3(w3)
			);
		break;
	default:
		r = 0;
		break;
	}
	return r;
}


/*
 * Enable or disable the automatic year update.
 */
bool_t
ttbAutoYear(int idx, bool_t auto_year)
{
	bool_t  prev_val;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return FALSE;
	}
	prev_val = ttb_auto_year[idx];
	ttb_auto_year[idx] = auto_year;
	return prev_val;
}


/*
 * Enable or disable time stamping of interrupts.
 */
bool_t
ttbAutoStamp(int idx, bool_t auto_stamp)
{
	bool_t  prev_val;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return FALSE;
	}
	prev_val = ttb_auto_stamp[idx];
	ttb_auto_stamp[idx] = auto_stamp;
	return prev_val;
}


/*
 * .
 */
oret_t
ttbSetYear(int idx, int def_year)
{
	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	ttb_prev_year[idx] = def_year;
	return OK;
}


/*
 * .
 */
oret_t
ttbGetYear(int idx, int *ptr_year)
{
	static const char *me = "ttbGetYear";
	uchar_t data[64];
	int     year;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		ttbPrintf(4, "%s: index %d out of range\n", me, idx);
		return ERROR;
	}
	*ptr_year = ttb_prev_year[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		if (ttb_prev_year[idx] != 0 && !ttb_auto_year[idx])
			return OK;
		if (ttbFifoWrite(idx, "O0") != OK) {
			ttbPrintf(4, "%s: cannot write to FIFO (idx=%d)\n", me, idx);
			return ERROR;
		}
		if (ttbFifoRead(idx, (char *) data) != OK) {
			ttbPrintf(4, "%s: cannot read from FIFO (idx=%d)\n", me,
				  idx);
			return ERROR;
		}
		if (data[0] != 0x01 || data[1] != 'o' ||
			data[2] != '0' || data[15] != 0x17) {
			ttbPrintf(4, "%s: FIFO format is wrong (idx=%d)\n", me, idx);
			return ERROR;
		}
		year = (data[3] - '0') * 10 + (data[4] - '0');
		if (year >= 50 && year < 100)
			year += 1900;
		else if (year >= 0 && year < 50)
			year += 2000;
		if (year < 0 || year > 3000) {
			ttbPrintf(4, "%s: FIFO year is out of range (idx=%d)\n", me, idx);
			return ERROR;
		}
		break;
	default:
		ttbPrintf(4, "%s: board undefined (idx=%d)\n", me, idx);
		return ERROR;
	}
	*ptr_year = ttb_prev_year[idx] = year;
	return OK;
}


/*
 * .
 */
oret_t
ttbGetTimeWithYear(int idx, int year, TimeUnsegFine_t * result)
{
	TimeIrigBC_t *time_ptr;
	oret_t rc;
	ushort_t buf[10], *sp, w1, w2, w3;
	ulong_t stamp;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		buf[0] = sp[TTB_BC635_REG_TIMEREQ];
		buf[1] = sp[TTB_BC635_REG_TIME0];
		buf[2] = sp[TTB_BC635_REG_TIME1];
		w1 = buf[3] = sp[TTB_BC635_REG_TIME2];
		w1 = OUNTOHS(w1);
		w2 = buf[4] = sp[TTB_BC635_REG_TIME3];
		w2 = OUNTOHS(w2);
		w3 = buf[5] = sp[TTB_BC635_REG_TIME4];
		w3 = OUNTOHS(w3);
		stamp = (WB4(w1) * 600000000uL + WB3(w1) * 60000000uL +
				 WB2(w1) * 10000000uL + WB1(w1) * 1000000uL +
				 WB4(w2) * 100000uL + WB3(w2) * 10000uL + WB2(w2) * 1000uL +
				 WB1(w2) * 100uL + WB4(w3) * 10uL + (ulong_t) WB3(w3)
			);
		rc = year != 0 ? OK : ttbGetYear(idx, &year);
		time_ptr = (TimeIrigBC_t *) buf;
		time_ptr->unused_year = year;
		rc |= otimeIrigBC2UnsegFine(time_ptr, result);
		break;
	default:
		return ERROR;
	}
	return rc;
}


/*
 * .
 */
oret_t
ttbGetTime(int idx, TimeUnsegFine_t * result)
{
	return ttbGetTimeWithYear(idx, 0, result);
}


/*
 * .
 */
oret_t
ttbSetTime(int idx, TimeUnsegFine_t * time_ptr)
{
	static const char *me = "ttbSetTime";
	oret_t rc;
	char    pb[16], pl[16];
	TimeSegFine_t sft;
	ushort_t *sp;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		ttbPrintf(4, "%s: index %d out of range\n", me, idx);
		return ERROR;
	}
	rc = otimeUnsegFine2SegFine(time_ptr, &sft);
	if (rc != OK) {
		return ERROR;
	}
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		pl[0] = 'L';
		pl[1] = BIN_TENS(sft.year);
		pl[2] = BIN_UNITS(sft.year);
		pl[3] = BIN_TENS(sft.month);
		pl[4] = BIN_UNITS(sft.month);
		pl[5] = BIN_TENS(sft.day);
		pl[6] = BIN_UNITS(sft.day);
		pl[7] = BIN_TENS(sft.hour);
		pl[8] = BIN_UNITS(sft.hour);
		pl[9] = BIN_TENS(sft.min);
		pl[10] = BIN_UNITS(sft.min);
		pl[11] = BIN_TENS(sft.sec);
		pl[12] = BIN_UNITS(sft.sec);
		pl[13] = 0;
		pb[0] = 'B';
		pb[1] = BIN_HUNDREDS(sft.yday);
		pb[2] = BIN_TENS(sft.yday);
		pb[3] = BIN_UNITS(sft.yday);
		pb[4] = pl[7];
		pb[5] = pl[8];
		pb[6] = pl[9];
		pb[7] = pl[10];
		pb[8] = pl[11];
		pb[9] = pl[12];
		pb[10] = 0;
		if (ttb_auto_year[idx])
			rc = ttbFifoWrite(idx, pl);
		else
			rc = ttbFifoWrite(idx, pb);
		ttb_prev_year[idx] = sft.year;
		if (rc != OK) {
			ttbPrintf(4, "%s: FIFO failed (idx=%d)\n", me, idx);
		}
		break;
	default:
		ttbPrintf(4, "%s: board undefined (idx=%d)\n", me, idx);
		return ERROR;
	}
	return rc;
}


/*
 * .
 */
oret_t
ttbSetOffset(int idx, long usec)
{
	char    pg[12];
	long    msec;
	ushort_t *sp;
	oret_t rc;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		pg[0] = 'G';
		pg[1] = (usec < 0) ? '-' : '+';
		if (usec < 0)
			usec = -usec;
		msec = usec / 1000;
		usec %= 1000;
		pg[2] = BIN_HUNDREDS(msec);
		pg[3] = BIN_TENS(msec);
		pg[4] = BIN_UNITS(msec);
		pg[5] = BIN_HUNDREDS(usec);
		pg[6] = BIN_TENS(usec);
		pg[7] = BIN_UNITS(usec);
		pg[8] = '0';
		pg[9] = 0;
		rc = ttbFifoWrite(idx, pg);
		break;
	default:
		return ERROR;
	}
	return rc;
}


/*
 * .
 */
oret_t
ttbSetStrTime(int idx, const char *str)
{
	static const char *me = "ttbSetStrTime";
	oret_t rc;
	TimeUnsegFine_t uf_time;

	if (str == 0 || *str == 0)
		rc = otimeCurUnsegFine(&uf_time);
	else
		rc = otimeSegString2UnsegFine((char *) str, &uf_time);
	if (rc != OK) {
		ttbPrintf(4, "%s: time %s not recognized (idx=%d)\n",
				me, (str == NULL ? "CURRENT" : str), idx);
		return rc;
	}
	rc = ttbSetTime(idx, &uf_time);
	if (rc != OK) {
		ttbPrintf(4, "%s: cannot set time (idx=%d)\n", me, idx);
	}
	return rc;
}


/*
 * .
 */
oret_t
ttbEventCapture(int idx, bool_t enable, bool_t rising, bool_t lock_ena)
{
	ushort_t *sp, val;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		val = sp[TTB_BC635_REG_CMD];
		val = OUNTOHS(val);
		if (enable)
			val |= (1 << 3);
		else
			val &= ~(1 << 3);
		if (rising)
			val &= ~(1 << 2);
		else
			val |= (1 << 2);
		if (lock_ena)
			val |= (1 << 0);
		else
			val &= ~(1 << 0);
		sp[TTB_BC635_REG_CMD] = val;
		return OK;
	default:
		return ERROR;
	}
}


/*
 * .
 */
oret_t
ttbEventUnlock(int idx)
{
	ushort_t *sp;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		tmp_i = sp[TTB_BC635_REG_UNLOCK];
		return OK;
	default:
		return ERROR;
	}
}


/*
 * .
 */
long
ttbStampDelta(ulong_t cur_stamp, ulong_t prev_stamp)
{
	long long h = 1800000000l,
		r = (long long) cur_stamp - (long long) prev_stamp;
	while (r < -h)
		r += h + h;
	while (r > h)
		r -= h + h;
	return (long) r;
}


/*
 * Set up the board to work as a time master.
 */
oret_t
ttbBurnSetup(int idx)
{
	oret_t rc = OK;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		rc |= ttbFifoWrite(idx, "A0");
		osMsSleep(ttb_burn_gap_ms);
		rc |= ttbFifoWrite(idx, "HBM");
		osMsSleep(ttb_burn_gap_ms);
		rc |= ttbFifoWrite(idx, "KB");
		osMsSleep(ttb_burn_gap_ms);
		rc |= ttbFifoWrite(idx, "P02");
		return rc;
	default:
		return ERROR;
	}
}


/*
 * .
 */
oret_t
ttbDelayedBurn(int idx, int ms)
{
	if (idx <= 0)
		idx = 1;
	if (ms <= 0)
		ms = 2;
	ms *= 1000;
	osMsSleep(ms);
	ttbBurnSetup(idx);
	return OK;
}


/*
 * .
 */
oret_t
ttbSetPeriodic(int idx, int n1, int n2, bool_t sync)
{
	char    pg[16];
	ushort_t *sp;
	oret_t rc = OK;

	if (idx < 0 || idx >= TTB_MAX_NUM) {
		return ERROR;
	}
	if (n1 < 2 || n1 > 65535)
		return ERROR;
	if (n2 < 2 || n2 > 65535)
		return ERROR;
	sp = ttb_base[idx];
	switch (ttb_type[idx]) {
	case TTB_TYPE_BC635:
		pg[0] = 'F';
		pg[1] = sync ? '5' : '2';
		sprintf(&pg[2], "%04X", n1);
		sprintf(&pg[6], "%04X", n2);
		pg[10] = 0;
		rc = ttbFifoWrite(idx, pg);
		break;
	default:
		return ERROR;
	}
	return rc;
}
