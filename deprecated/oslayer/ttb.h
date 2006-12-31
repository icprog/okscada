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

  Declarations w.r.t. the IRIG "True Time Boards", in particular
  the IRIG Bancomm bc635 / VME from SBS Datum Inc.

  TO BE FREEZED.

*/

#ifndef OPTIKUS_TTB_H
#define OPTIKUS_TTB_H

#include <optikus/time.h>

OPTIKUS_BEGIN_C_DECLS


#define TTB_BC635_MSK_EXT       0x01
#define TTB_BC635_MSK_PERIODIC  0x02
#define TTB_BC635_MSK_TIME      0x04
#define TTB_BC635_MSK_PPS       0x08
#define TTB_BC635_MSK_FIFO      0x10

#define TTB_BC635_REG_ID        (0x00/2)
#define TTB_BC635_REG_DEVICE    (0x02/2)
#define TTB_BC635_REG_STATUS    (0x04/2)
#define TTB_BC635_REG_CONTROL   (0x04/2)
#define TTB_BC635_REG_TIMEREQ   (0x0A/2)
#define TTB_BC635_REG_TIME0     (0x0C/2)
#define TTB_BC635_REG_TIME1     (0x0E/2)
#define TTB_BC635_REG_TIME2     (0x10/2)
#define TTB_BC635_REG_TIME3     (0x12/2)
#define TTB_BC635_REG_TIME4     (0x14/2)
#define TTB_BC635_REG_EVENT0    (0x16/2)
#define TTB_BC635_REG_EVENT1    (0x18/2)
#define TTB_BC635_REG_EVENT2    (0x1A/2)
#define TTB_BC635_REG_EVENT3    (0x1C/2)
#define TTB_BC635_REG_EVENT4    (0x1E/2)
#define TTB_BC635_REG_UNLOCK    (0x20/2)
#define TTB_BC635_REG_ACK       (0x22/2)
#define TTB_BC635_REG_CMD       (0x24/2)
#define TTB_BC635_REG_FIFO      (0x26/2)
#define TTB_BC635_REG_MASK      (0x28/2)
#define TTB_BC635_REG_INTSTAT   (0x2A/2)
#define TTB_BC635_REG_VECTOR    (0x2C/2)
#define TTB_BC635_REG_LEVEL     (0x2E/2)

#define TTB_OK                      0
#define TTB_ERROR                   -1
#define TTB_ERR_FifoWriteTimeout    1
#define TTB_ERR_FifoReadTimeout     2
#define TTB_ERR_FifoTooLong         3
#define TTB_ERR_FifoWrongFormat     4
#define TTB_ERR_FifoYearWrong       5
#define TTB_ERR_NoSense             6
#define TTB_ERR_WrongId             7
#define TTB_ERR_NotAppropriate      8

#define TTB_DEF_YEAR       2000
#define TTB_DEF_IRQ        4
#define TTB_DEF_VECTOR     77

#define TTB_MAX_NUM        4

#define TTB_INTR_EXT       0
#define TTB_INTR_PERIODIC  1
#define TTB_INTR_TIME      2
#define TTB_INTR_PPS       3
#define TTB_INTR_FIFO      4
#define TTB_INTR_MAX_NUM   5

#define TTB_TYPE_Auto      0
#define TTB_TYPE_BC635     1
#define TTB_TYPE_TrueTime  2

typedef int (*TtbIntr_t) (int idx, int type, int param,
						  ulong_t stamp, TimeUnsegFine_t * time_ptr);

extern int ttb_error;
extern ushort_t *ttb_base[TTB_MAX_NUM];
extern int ttb_type[TTB_MAX_NUM];
extern int ttb_irq[TTB_MAX_NUM];
extern int ttb_vector[TTB_MAX_NUM];
extern TtbIntr_t ttb_proc[TTB_MAX_NUM][TTB_INTR_MAX_NUM];
extern int ttb_param[TTB_MAX_NUM][TTB_INTR_MAX_NUM];
extern int ttb_intr_qty[TTB_MAX_NUM][TTB_INTR_MAX_NUM];
extern bool_t ttb_auto_year[TTB_MAX_NUM];
extern bool_t ttb_auto_stamp[TTB_MAX_NUM];
extern bool_t ttb_strict_probe;

oret_t  ttbFind(int start_a16, int end_a16);
oret_t  ttbPrint(int base_a16);
oret_t  ttbInit(int idx, int base_a16, int ttb_type, int irq, int vector);
oret_t  ttbExit(int idx);
oret_t  ttbHandler(int idx, int intr_type, TtbIntr_t handler, int param);
oret_t  ttbSetIntr(int idx, int intr_type, bool_t enable);
oret_t  ttbTestIntr(int idx, int intr_type, bool_t start);
oret_t  ttbFifoWrite(int idx, const char *data);
oret_t  ttbFifoRead(int idx, char *result);
ulong_t ttbGetStamp(int idx);
bool_t  ttbAutoYear(int idx, bool_t auto_year);
bool_t  ttbAutoStamp(int idx, bool_t auto_stamp);
oret_t  ttbSetYear(int idx, int def_year);
oret_t  ttbGetYear(int idx, int *ptr_year);
oret_t  ttbGetTime(int idx, TimeUnsegFine_t * result);
oret_t  ttbGetTimeWithYear(int idx, int year, TimeUnsegFine_t * result);
oret_t  ttbSetTime(int idx, TimeUnsegFine_t * time);
oret_t  ttbSetOffset(int idx, long usec);
oret_t  ttbSetStrTime(int idx, const char *str);
oret_t  ttbEventCapture(int idx,
						 bool_t enable, bool_t rising, bool_t lock_ena);
oret_t  ttbEventUnlock(int idx);
long    ttbStampDelta(ulong_t cur_stamp, ulong_t prev_stamp);
oret_t  ttbBurnSetup(int idx);
oret_t  ttbDelayedBurn(int idx, int ms);
oret_t  ttbSetPeriodic(int idx, int n1, int n2, bool_t sync);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_TTB_H */
