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

  Time representation conversions.

  TO BE FREEZED.

*/

#ifndef OPTIKUS_TIME_H
#define OPTIKUS_TIME_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


typedef enum
{
	TC_UnsegUS,
	TC_UnsegFine,
	TC_SegFine,
	TC_SBS,
	TC_IrigBC,
	TC_IrigGenFreeze,
	TC_IrigGenPreset,
	TC_SegString,
	TC_UnsegString,
	TC_CurUnsegFine,
	TC_USRM,
	TC_BT
} TimeCode_t;

typedef struct
{
	float   coarse_msw;
	float   coarse_lsw;
	float   fine;
} TimeUSRM_t;

typedef struct
{
	long    coarse;
	short   fine;
} TimeUnsegUS_t;

typedef struct
{
	long    sec;
	long    usec;
} TimeUnsegFine_t;

typedef struct
{
	short   year;			/* 1990..2038 */
	short   month;			/* 1..12 */
	short   day;			/* 1..31 */
	short   hour;			/* 0..23 */
	short   min;			/* 0..59 */
	short   sec;			/* 0..59 */
	long    usec;			/* 0..999999 */
	short   yday;			/* 1..366 - mostly unused */
} TimeSegFine_t;

typedef struct
{
	ushort_t time_high;
	ushort_t time_med;
	ushort_t time_low;
} TimeSBS_t;

typedef struct
{							/* string being read from IRIG Bancomm registers */
	uchar_t reserved1;
	uchar_t reserved2;
	uchar_t day_msb_bcd;
	uchar_t day_lsb_bcd;
	uchar_t hour_bcd;
	uchar_t min_bcd;
	uchar_t sec_bcd;
	uchar_t usec_hi_bcd;
	uchar_t usec_me_bcd;
	uchar_t usec_lo_bcd;
	ushort_t unused1;
	ushort_t unused_year;
} TimeIrigBC_t;

typedef struct
{							/* string being read from GEN_FREEZE registers of IRIG TrueTime */
	ushort_t gen_freeze[6];
	/* 0: unused */
	/* 1: Umsec Husec Tusec Uusec */
	/* 2: Tsec  Usec  Hmsec Tmsec */
	/* 3: Thour Uhour Tmin  Umin  */
	/* 4: stats Hday  Tday  Uday  */
	/* 5: THyr  Hyear Tyear Uyear */
} TimeIrigGenFreeze_t;

typedef struct
{
	ushort_t gen_preset[10];
	/* 0: unused */
	/* 1: unit_msec_bcd */
	/* 2: hd_ten_ms_bcd */
	/* 3: sec_bcd       */
	/* 4: min_bcd       */
	/* 5: hour_bcd      */
	/* 6: yday_lsb_bcd  */
	/* 7: yday_hdrd     */
	/* 8: year_lsb_bcd  */
	/* 9: year_msb_bcd  */
} TimeIrigGenPreset_t;


extern int otime_StandardYear;

#define smtDeltaUsec(cur,prev)   (((cur)->sec-(prev)->sec)*1000000+((cur)->usec-(prev)->usec))

#define HOUR_USEC  3600000000ul

long    otimeDelta(ulong_t cur, ulong_t prev, ulong_t ring);

oret_t  otimeAddUnsegFine(TimeUnsegFine_t * result, TimeUnsegFine_t * offset);
oret_t  otimeSubUnsegFine(TimeUnsegFine_t * result, TimeUnsegFine_t * offset);
oret_t  otimeConvert(TimeCode_t from_cod, TimeCode_t to_cod, void *from, void *to);

oret_t  otimeCurUnsegFine(TimeUnsegFine_t * to);
oret_t  otimeCurSegFine(TimeSegFine_t * to);

oret_t  otimeSegFine2UnsegFine(TimeSegFine_t * from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2SegFine(TimeUnsegFine_t * from, TimeSegFine_t * to);

oret_t  otimeUnsegUS2UnsegFine(TimeUnsegUS_t * from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2UnsegUS(TimeUnsegFine_t * from, TimeUnsegUS_t * to);

oret_t  otimeUSRM2UnsegFine(TimeUSRM_t * from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2USRM(TimeUnsegFine_t * from, TimeUSRM_t * to);

oret_t  otimeIrigBC2UnsegFine(TimeIrigBC_t * from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2IrigBC(TimeUnsegFine_t * from, TimeIrigBC_t * to);

oret_t  otimeSBS2UnsegFine(TimeSBS_t * from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2SBS(TimeUnsegFine_t * from, TimeSBS_t * to);

oret_t  otimeSegString2UnsegFine(char *from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2SegString(TimeUnsegFine_t * from, char *to);

oret_t  otimeUnsegString2UnsegFine(char *from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2UnsegString(TimeUnsegFine_t * from, char *to);

oret_t  otimeIrigGenFreeze2UnsegFine(TimeIrigGenFreeze_t * from,
									TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2IrigGenFreeze(TimeUnsegFine_t * from, TimeIrigGenFreeze_t * to);

oret_t  otimeIrigGenPreset2UnsegFine(TimeIrigGenPreset_t * from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2IrigGenPreset(TimeUnsegFine_t * from, TimeIrigGenPreset_t * to);

oret_t  otimeBT2SegFine(const ushort_t * from, TimeSegFine_t * to);
oret_t  otimeSegFine2BT(TimeSegFine_t * from, ushort_t * to);

oret_t  otimeBT2UnsegFine(const ushort_t * from, TimeUnsegFine_t * to);
oret_t  otimeUnsegFine2BT(TimeUnsegFine_t * from, ushort_t * to);


OPTIKUS_END_C_DECLS

#endif	/* OPTIKUS_TIME_H */
