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

  Convert between different time formats.

*/

#include <optikus/time.h>
#include <optikus/conf-mem.h>	/* for oxbcopy,oxvzero */
#include <time.h>
#include <sys/time.h>
#include <stdio.h>				/* for sprintf,sscanf */

#if defined(HAVE_CONFIG_H)
#include <config.h>				/* for VXWORKS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS)
#include <timers.h>
#endif /* VXWORKS */


#define GENERATE_CONSTANTS  0

/*
 * number of seconds between
 * GPS epoch (00:00:00 January 6, 1980)
 * and UTC epoch (00:00:00 January 1, 1970)
 */
static long gps_epoch_since_utc_epoch = 315954000;

static long year1990_epoch_seconds[2039 - 1990] = {
	/* 1990 */ 315187200, 346723200, 378262800, 409881600, 441417600,
	/* 1995 */ 472953600, 504489600, 536112000, 567648000, 599184000,
	/* 2000 */ 630720000, 662342400, 693878400, 725414400, 756950400,
	/* 2005 */ 788572800, 820108800, 851644800, 883180800, 914803200,
	/* 2010 */ 946339200, 977875200, 1009411200, 1041033600, 1072569600,
	/* 2015 */ 1104105600, 1135641600, 1167264000, 1198800000, 1230336000,
	/* 2020 */ 1261872000, 1293494400, 1325030400, 1356566400, 1388102400,
	/* 2025 */ 1419724800, 1451260800, 1482796800, 1514332800, 1545955200,
	/* 2030 */ 1577491200, 1609027200, 1640563200, 1672185600, 1703721600,
	/* 2035 */ 1735257600, 1766793600, 1798416000, 1829952000
};
static short year1990_leap_flag[2039 - 1990] = {
	/* 1990 */ 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	/* 2000 */ 1, 0, 0, 0, 1, 0, 0, 0, 1, 0,
	/* 2010 */ 0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
	/* 2020 */ 1, 0, 0, 0, 1, 0, 0, 0, 1, 0,
	/* 2030 */ 0, 0, 1, 0, 0, 0, 1, 0, 1
};
static long leap_month_seconds[12] = {
	/* Jan */ 0, 2678400, 5184000,
	/* Apr */ 7862400, 10454400, 13132800,
	/* Jul */ 15724800, 18403200, 21081600,
	/* Oct */ 23673600, 26352000, 28944000
};
static long normal_month_seconds[12] = {
	/* Jan */ 0, 2678400, 5097600,
	/* Apr */ 7776000, 10368000, 13046400,
	/* Jul */ 15638400, 18316800, 20995200,
	/* Oct */ 23587200, 26265600, 28857600
};


int     otime_StandardYear = 2001;
int     otime_daylight_saving = 0;


#define BCD2BIN(x)		((((uint_t)(x)&0xF0)>>4)*10+((uint_t)(x)&0x0F))
#define BIN2BCD(x)		((((uint_t)(x)/10)<<4)|((uint_t)(x)%10))

#define WBYTE4(w)		(((ushort_t)(w) & 0xF000) >> 12)
#define WBYTE3(w)		(((ushort_t)(w) & 0x0F00) >> 8 )
#define WBYTE2(w)		(((ushort_t)(w) & 0x00F0) >> 4 )
#define WBYTE1(w)		(((ushort_t)(w) & 0x000F)      )



/*
 * .
 */
long
otimeDelta(ulong_t cur, ulong_t prev, ulong_t ring)
{
	long long r = (long long) ring / 2, v = (long long) cur - (long long) prev;
	while (v < -r)
		v += r + r;
	while (v > r)
		v -= r + r;
	return (long) v;
}


/*
 * .
 */

#if defined(VXWORKS)

oret_t
otimeCurUnsegFine(TimeUnsegFine_t * to)
{
	struct timespec tspec;

	clock_gettime(CLOCK_REALTIME, &tspec);
	to->sec = tspec.tv_sec - gps_epoch_since_utc_epoch;
	to->usec = tspec.tv_nsec / 1000;
	return OK;
}

oret_t
otimeCurSegFine(TimeSegFine_t * to)
{
	TimeUnsegFine_t uft;

	otimeCurUnsegFine(&uft);
	return otimeUnsegFine2SegFine(&uft, to);
}

#else /* !VXWORKS, i.e. UNIX */

oret_t
otimeCurUnsegFine(TimeUnsegFine_t * to)
{
	static int tz_set = 0;
	struct timeval tv_time;

	if (!tz_set) {
		/* daylight saving correction */
		time_t  tt;
		struct tm stm, *ptm;
		TimeUnsegFine_t uft;
		TimeSegFine_t sft;

		tz_set = 1;
		tzset();
		time(&tt);
		ptm = localtime_r(&tt, &stm);
		otimeCurUnsegFine(&uft);
		otimeUnsegFine2SegFine(&uft, &sft);
		otime_daylight_saving = 3600 * (ptm->tm_hour - sft.hour);
	}
	gettimeofday(&tv_time, NULL);
	to->sec = tv_time.tv_sec - gps_epoch_since_utc_epoch + otime_daylight_saving;
	to->usec = tv_time.tv_usec;
	return OK;
}

oret_t
otimeCurSegFine(TimeSegFine_t * to)
{
	static int tz_set = 0;
	time_t  tt;
	struct timeval tv;
	struct tm stm, *ptm;

	if (!tz_set) {
		tz_set = 1;
		tzset();
	}
	if (!to)
		return ERROR;
	oxvzero(to);
	if (time(&tt) == (time_t) - 1)
		return ERROR;
	if (gettimeofday(&tv, 0) != 0)
		return ERROR;
	ptm = localtime_r(&tt, &stm);
	if (ptm == 0)
		return ERROR;
	to->year = ptm->tm_year + 1900;
	to->month = ptm->tm_mon + 1;
	to->day = ptm->tm_mday;
	to->hour = ptm->tm_hour;
	to->min = ptm->tm_min;
	to->sec = ptm->tm_sec;
	to->usec = tv.tv_usec;
	return OK;
}

#endif /* VXWORKS vs UNIX */


/*
 * .
 */
oret_t
otimeAddUnsegFine(TimeUnsegFine_t * res, TimeUnsegFine_t * off)
{
	res->sec += (long) off->sec;
	res->usec += (long) off->usec;
	while ((long) res->usec >= 1000000L) {
		res->usec -= 1000000L;
		res->sec++;
	}
	while ((long) res->usec < 0) {
		res->usec += 1000000L;
		res->sec--;
	}
	return OK;
}


/*
 * .
 */
oret_t
otimeSubUnsegFine(TimeUnsegFine_t * result, TimeUnsegFine_t * offset)
{
	result->sec -= (long) offset->sec;
	result->usec -= (long) offset->usec;
	while ((long) result->usec >= 1000000L) {
		result->usec -= 1000000L;
		result->sec++;
	}
	while ((long) result->usec < 0) {
		result->usec += 1000000L;
		result->sec--;
	}
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegFine2UnsegUS(TimeUnsegFine_t * from, TimeUnsegUS_t * to)
{
	to->coarse = from->sec;
	to->fine = (ushort_t) ((from->usec << 8) / 1000000L);
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegUS2UnsegFine(TimeUnsegUS_t * from, TimeUnsegFine_t * to)
{
	to->sec = from->coarse;
	to->usec = ((ulong_t) from->fine * 1000000L) >> 8;
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegFine2USRM(TimeUnsegFine_t * from, TimeUSRM_t * to)
{
	to->coarse_msw = (float) (((ulong_t) from->sec & 0xFFFF0000) >> 16);
	to->coarse_lsw = (float) ((ulong_t) from->sec & 0x0000FFFF);
	to->fine = (float) ((ulong_t) ((from->usec << 8) / 1000000L) << 8);
	return OK;
}


/*
 * .
 */
oret_t
otimeUSRM2UnsegFine(TimeUSRM_t * from, TimeUnsegFine_t * to)
{
	if (from->coarse_msw < 0.0 || from->coarse_msw > 65535.0 ||
		from->coarse_lsw < 0.0 || from->coarse_lsw > 65535.0 ||
		from->fine < 0.0 || from->fine > 65535.0) {
		to->sec = 0;
		to->usec = 0;
		return ERROR;
	}
	to->sec = ((((ulong_t) from->coarse_msw & 0x0FFFF) << 16) |
			   ((ulong_t) from->coarse_lsw & 0x0FFFF));
	to->usec = (((ulong_t) from->fine >> 8) * 1000000L) >> 8;
	return OK;
}


/*
 * .
 */
oret_t
otimeIrigBC2UnsegFine(TimeIrigBC_t * from, TimeUnsegFine_t * to)
{
	ushort_t *wp = (ushort_t *) from;
	ushort_t w;
	int     yday, year, hour, min, sec;
	long    msec, usec;
	oret_t ret_cod;

	year = from->unused_year;
	w = wp[1];
	yday = 100 * (w & 0x0F);
	yday--;
	w = wp[2];
	yday += WBYTE4(w) * 10 + WBYTE3(w);
	hour = WBYTE2(w) * 10 + WBYTE1(w);
	w = wp[3];
	min = WBYTE4(w) * 10 + WBYTE3(w);
	sec = WBYTE2(w) * 10 + WBYTE1(w);
	w = wp[4];
	msec = WBYTE4(w) * 100 + WBYTE3(w) * 10 + WBYTE2(w);
	usec = WBYTE1(w) * 100;
	w = wp[5];
	usec += WBYTE4(w) * 10 + WBYTE3(w);
	if (year < 1990 || year >= 2038) {
		ret_cod = ERROR;
		to->sec = year1990_epoch_seconds[otime_StandardYear - 1990];
	} else {
		ret_cod = OK;
		to->sec = year1990_epoch_seconds[year - 1990];
	}
	to->sec += sec + min * 60L + hour * 3600L + yday * 86400L;
	to->usec = msec * 1000L + usec;
	return ret_cod;
}


/*
 * .
 */
oret_t
otimeUnsegFine2IrigBC(TimeUnsegFine_t * from, TimeIrigBC_t * to)
{
	ulong_t sec, usec;
	oret_t ret_code;
	int     year;

	sec = from->sec;
	usec = from->usec;
	if ((long) sec < year1990_epoch_seconds[1990 - 1990] ||
		(long) sec >= year1990_epoch_seconds[2039 - 1 - 1990]) {
		year = 0;
		sec %= (86400L * 366L);
		ret_code = ERROR;
	} else {
		for (year = 0; year < 2039 - 1990 - 1; year++)
			if ((long) sec < year1990_epoch_seconds[year + 1])
				break;
		sec -= year1990_epoch_seconds[year];
		year += 1990;
		ret_code = OK;
	}
	to->unused1 = 0;
	to->unused_year = year;
	/* micro seconds */
	to->usec_lo_bcd = BIN2BCD(usec % 100);
	usec /= 100;
	to->usec_me_bcd = BIN2BCD(usec % 100);
	usec /= 100;
	to->usec_hi_bcd = BIN2BCD(usec % 100);
	/* seconds */
	to->sec_bcd = BIN2BCD(sec % 60);
	sec /= 60;
	/* minutes */
	to->min_bcd = BIN2BCD(sec % 60);
	sec /= 60;
	/* hours */
	to->hour_bcd = BIN2BCD(sec % 24);
	sec /= 24;
	/* units of days , caution: IRIG-B days begin with 1, not 0 */
	sec++;
	to->day_lsb_bcd = BIN2BCD(sec % 100);
	sec /= 100;
	/* hundreds of days */
	to->day_msb_bcd = BIN2BCD(sec % 10);
	/* reserved */
	to->reserved1 = 0;
	to->reserved2 = 0;

	return ret_code;
}


/*
 * .
 */
oret_t
otimeIrigGenFreeze2UnsegFine(TimeIrigGenFreeze_t * from, TimeUnsegFine_t * to)
{
	int     yday, year, hour, min, sec;
	long    msec, usec;
	ushort_t w;
	oret_t ret_cod;

	w = from->gen_freeze[5];
	year = WBYTE4(w) * 1000 + WBYTE3(w) * 100 + WBYTE2(w) * 10 + WBYTE1(w);
	w = from->gen_freeze[4];
	yday = WBYTE3(w) * 100 + WBYTE2(w) * 10 + WBYTE1(w);
	w = from->gen_freeze[3];
	hour = WBYTE4(w) * 10 + WBYTE3(w);
	min = WBYTE2(w) * 10 + WBYTE1(w);
	w = from->gen_freeze[2];
	sec = WBYTE4(w) * 10 + WBYTE3(w);
	msec = WBYTE2(w) * 100 + WBYTE1(w) * 10;
	w = from->gen_freeze[1];
	msec += WBYTE4(w);
	usec = WBYTE3(w) * 100 + WBYTE2(w) * 10 + WBYTE1(w);
	if (year < 1990 || year >= 2038) {
		ret_cod = ERROR;
		to->sec = year1990_epoch_seconds[otime_StandardYear - 1990];
	} else {
		ret_cod = OK;
		to->sec = year1990_epoch_seconds[year - 1990];
	}
	to->sec += sec + min * 60L + hour * 3600L + (yday - 1) * 86400L;
	to->usec = msec * 1000L + usec;
	return ret_cod;
}


/*
 * .
 */
oret_t
otimeBT2SegFine(const ushort_t * from, TimeSegFine_t * to)
{
	const uchar_t *b = (const uchar_t *) from;

	to->year = BCD2BIN(b[1]) * 100 + BCD2BIN(b[2]);
	to->month = BCD2BIN(b[3]);
	to->day = BCD2BIN(b[4]);
	to->hour = BCD2BIN(b[5]);
	to->min = BCD2BIN(b[6]);
	to->sec = BCD2BIN(b[7]);
	to->usec = 0;				/* FIXME */
	to->yday = 0;				/* FIXME */
	return OK;
}


/*
 * .
 */
oret_t
otimeBT2UnsegFine(const ushort_t * from, TimeUnsegFine_t * to)
{
	TimeSegFine_t sft;

	otimeBT2SegFine(from, &sft);
	return otimeSegFine2UnsegFine(&sft, to);
}


/*
 * .
 */
oret_t
otimeSegFine2BT(TimeSegFine_t * from, ushort_t * to)
{
	uchar_t *s = (uchar_t *) to;

	s[0] = 0x50;
	s[1] = BIN2BCD(from->year / 100);
	s[2] = BIN2BCD(from->year % 100);
	s[3] = BIN2BCD(from->month);
	s[4] = BIN2BCD(from->day);
	s[5] = BIN2BCD(from->hour);
	s[6] = BIN2BCD(from->min);
	s[7] = BIN2BCD(from->sec);
	s[8] = 0x00;
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegFine2BT(TimeUnsegFine_t * from, ushort_t * to)
{
	TimeSegFine_t sft;

	if (otimeUnsegFine2SegFine(from, &sft))
		return ERROR;
	return otimeSegFine2BT(&sft, to);
}


/*
 * .
 */
oret_t
otimeUnsegFine2IrigGenFreeze(TimeUnsegFine_t * from, TimeIrigGenFreeze_t * to)
{
	static void *p;

	p = from;
	p = to;
	return ERROR;
}


/*
 * .
 */
oret_t
otimeIrigGenPreset2UnsegFine(TimeIrigGenPreset_t * from, TimeUnsegFine_t * to)
{
	static void *p;

	p = from;
	p = to;
	return ERROR;
}


/*
 * .
 */
oret_t
otimeUnsegFine2IrigGenPreset(TimeUnsegFine_t * from, TimeIrigGenPreset_t * to)
{
	ulong_t sec, usec;
	oret_t ret_code;
	int     year;
	ushort_t *gp = to->gen_preset;

	sec = from->sec;
	usec = from->usec;
	if ((long) sec < year1990_epoch_seconds[1990 - 1990] ||
		(long) sec >= year1990_epoch_seconds[2039 - 1 - 1990]) {
		year = 0;
		sec %= (86400L * 366L);
		ret_code = ERROR;
	} else {
		for (year = 0; year < 2039 - 1990 - 1; year++)
			if ((long) sec < year1990_epoch_seconds[year + 1])
				break;
		sec -= year1990_epoch_seconds[year];
		year += 1990;
		ret_code = OK;
	}
	/* year */
	gp[9] = BIN2BCD(year / 100);
	gp[8] = BIN2BCD(year % 100);
	/* micro seconds */
	usec /= 1000;
	gp[1] = (ushort_t) (usec % 10);
	usec /= 10;
	gp[2] = BIN2BCD(usec);
	/* seconds */
	gp[3] = BIN2BCD(sec % 60);
	sec /= 60;
	/* minutes */
	gp[4] = BIN2BCD(sec % 60);
	sec /= 60;
	/* hours */
	gp[5] = BIN2BCD(sec % 24);
	sec /= 24;
	/* units of days , caution: IRIG-B days begin with 1, not 0 */
	sec++;
	gp[6] = BIN2BCD(sec % 100);
	sec /= 100;
	/* hundreds of days */
	gp[7] = BIN2BCD(sec % 10);
	/* reserved */
	gp[0] = 0;

	return ret_code;
}


/*
 * .
 */
oret_t
otimeSBS2UnsegFine(TimeSBS_t * from, TimeUnsegFine_t * to)
{
	ulong_t h_time, ml_time;
	ulong_t q, r;

	/* calculate high/middle-low 32 bits of sbs time */
	h_time = (ulong_t) from->time_high;
	ml_time = (ulong_t) from->time_low + ((ulong_t) from->time_med << 16);
	/* transform ml_time in seconds / micro seconds */
	to->sec = ml_time / 1000000L;
	to->usec = ml_time % 1000000L;
	/* calculate 48 bit usec into sec/usec:
	 *  the 33 bit: 0x100000000 / 1000000 (decimal) = 4294,967296 sec !
	 *  so for each h_time bit, we have 4295 seconds - 32704 usec.
	 *  both this values can be multiplied by a 16 bit word to obtain a
	 *  value inside a 32 bit range: no lost of precision
	 *  so we first calculate the seconds, and then the number of micro
	 *  seconds to substract
	 */
	/* calculate seconds to add */
	to->sec += h_time * 4295L;
	/* calculate usec to substract */
	h_time *= 32704;
	q = h_time / 1000000L;
	r = h_time % 1000000L;
	/* take seconds */
	to->sec -= q;
	/* take microseconds and check for borrow */
	if (r > (ulong_t) to->usec) {	/* borrow */
		to->sec--;
		to->usec += 1000000L;
	}
	to->usec -= r;
	/* set rest to default values */
	to->sec += year1990_epoch_seconds[otime_StandardYear - 1990];
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegFine2SBS(TimeUnsegFine_t * from, TimeSBS_t * to)
{
	static void *p;

	p = from;
	p = to;
	return ERROR;
}


/*
 * .
 */
oret_t
otimeUnsegFine2SegString(TimeUnsegFine_t * from, char *to)
{
	TimeSegFine_t smt;

	if (otimeUnsegFine2SegFine(from, &smt) == ERROR) {
		*to = 0;
		return ERROR;
	}
	sprintf(to, "%04d/%02d/%02d-%02d:%02d:%02d.%03d",
			smt.year, smt.month, smt.day,
			smt.hour, smt.min, smt.sec, (int) smt.usec / 1000);
	return OK;
}


/*
 * .
 */
oret_t
otimeSegString2UnsegFine(char *from, TimeUnsegFine_t * to)
{
	TimeSegFine_t smt;
	short   msec = 0;
	int     n;

	oxvzero(&smt);
	n = sscanf(from, "%hd/%hd/%hd-%hd:%hd:%hd.%hd",
			   &smt.year, &smt.month, &smt.day,
			   &smt.hour, &smt.min, &smt.sec, &msec);
	smt.usec = (long) msec *1000;

	if (smt.year < 100)
		smt.year += 1900;
	if (n != 7)
		return ERROR;
	if (otimeSegFine2UnsegFine(&smt, to) == ERROR)
		return ERROR;
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegFine2UnsegString(TimeUnsegFine_t * from, char *to)
{
	sprintf(to, "%08ld.%06ld", from->sec, from->usec);
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegString2UnsegFine(char *from, TimeUnsegFine_t * to)
{
	TimeSegFine_t dummy;
	int     n;

	n = sscanf(from, "%ld.%ld", &to->sec, &to->usec);
	if (n != 2)
		return ERROR;
	if (otimeUnsegFine2SegFine(to, &dummy) == ERROR)
		return ERROR;
	return OK;
}


/*
 * .
 */
oret_t
otimeUnsegFine2SegFine(TimeUnsegFine_t * from, TimeSegFine_t * to)
{
	long    sec_rest;
	long   *cur_month_seconds;
	int     year, month;

	if (from->sec < year1990_epoch_seconds[1990 - 1990] ||
		from->sec >= year1990_epoch_seconds[2039 - 1 - 1990])
		return ERROR;
	sec_rest = from->sec;
	for (year = 0; year < 2039 - 1990 - 1; year++)
		if (sec_rest < year1990_epoch_seconds[year + 1])
			break;
	sec_rest -= year1990_epoch_seconds[year];
	if (year1990_leap_flag[year])
		cur_month_seconds = leap_month_seconds;
	else
		cur_month_seconds = normal_month_seconds;
	to->yday = sec_rest / 86400L + 1;
	for (month = 1; month < 12; month++)
		if (sec_rest < cur_month_seconds[month + 1 - 1])
			break;
	sec_rest -= cur_month_seconds[month - 1];
	to->year = year + 1990;
	to->month = month;
	to->day = sec_rest / 86400L + 1;
	sec_rest %= 86400L;
	to->hour = sec_rest / 3600;
	sec_rest %= 3600;
	to->min = sec_rest / 60;
	to->sec = sec_rest % 60;
	to->usec = from->usec;
	return 0;
}


/*
 * .
 */
oret_t
otimeSegFine2UnsegFine(TimeSegFine_t * from, TimeUnsegFine_t * to)
{
	long   *cur_month_seconds;

	if ((from->year < 1990 || from->year >= 2038) ||
		(from->month < 1 || from->month > 12) ||
		(from->day < 1 || from->day > 31))
		return ERROR;

	if (year1990_leap_flag[from->year - 1990])
		cur_month_seconds = leap_month_seconds;
	else
		cur_month_seconds = normal_month_seconds;

	to->sec = (year1990_epoch_seconds[from->year - 1990] +
			   cur_month_seconds[from->month - 1] +
			   (from->day - 1) * 86400L +
			   from->hour * 3600L + from->min * 60L + from->sec);
	to->usec = from->usec;
	return 0;
}


/*
 * .
 */
oret_t
otimeConvert(TimeCode_t from_cod, TimeCode_t to_cod, void *from, void *to)
{
	oret_t ret_cod;
	TimeUnsegFine_t uf;

	if (from_cod == TC_UnsegFine) {
		switch (to_cod) {
		case TC_UnsegUS:
			ret_cod = otimeUnsegFine2UnsegUS(from, to);
			break;
		case TC_USRM:
			ret_cod = otimeUnsegFine2USRM(from, to);
			break;
		case TC_UnsegFine:
			oxbcopy(from, to, sizeof(TimeUnsegFine_t));
			ret_cod = OK;
			break;
		case TC_SegFine:
			ret_cod = otimeUnsegFine2SegFine(from, to);
			break;
		case TC_SBS:
			ret_cod = otimeUnsegFine2SBS(from, to);
			break;
		case TC_IrigBC:
			ret_cod = otimeUnsegFine2IrigBC(from, to);
			break;
		case TC_IrigGenFreeze:
			ret_cod = otimeUnsegFine2IrigGenFreeze(from, to);
			break;
		case TC_IrigGenPreset:
			ret_cod = otimeUnsegFine2IrigGenPreset(from, to);
			break;
		case TC_SegString:
			ret_cod = otimeUnsegFine2SegString(from, to);
			break;
		case TC_UnsegString:
			ret_cod = otimeUnsegFine2UnsegString(from, to);
			break;
		case TC_BT:
			ret_cod = otimeUnsegFine2BT(from, to);
			break;
		default:
			ret_cod = ERROR;
			break;
		}
	} else if (to_cod == TC_UnsegFine) {
		switch (from_cod) {
		case TC_CurUnsegFine:
			ret_cod = otimeCurUnsegFine(to);
			break;
		case TC_UnsegUS:
			ret_cod = otimeUnsegUS2UnsegFine(from, to);
			break;
		case TC_USRM:
			ret_cod = otimeUSRM2UnsegFine(from, to);
			break;
		case TC_UnsegFine:
			oxbcopy(from, to, sizeof(TimeUnsegFine_t));
			ret_cod = OK;
			break;
		case TC_SegFine:
			ret_cod = otimeSegFine2UnsegFine(from, to);
			break;
		case TC_SBS:
			ret_cod = otimeSBS2UnsegFine(from, to);
			break;
		case TC_IrigBC:
			ret_cod = otimeIrigBC2UnsegFine(from, to);
			break;
		case TC_IrigGenFreeze:
			ret_cod = otimeIrigGenFreeze2UnsegFine(from, to);
			break;
		case TC_IrigGenPreset:
			ret_cod = otimeIrigGenPreset2UnsegFine(from, to);
			break;
		case TC_SegString:
			ret_cod = otimeSegString2UnsegFine(from, to);
			break;
		case TC_UnsegString:
			ret_cod = otimeUnsegString2UnsegFine(from, to);
			break;
		case TC_BT:
			ret_cod = otimeBT2UnsegFine(from, to);
			break;
		default:
			ret_cod = ERROR;
			break;
		}
	} else {
		ret_cod = otimeConvert(from_cod, TC_UnsegFine, from, &uf);
		ret_cod |= otimeConvert(TC_UnsegFine, to_cod, &uf, to);
	}
	return ret_cod;
}


#if GENERATE_CONSTANTS
/*
 *  calculation of constants
 *  MOVE INTO SEPARATE SOURCE.
 */
void
_print_smt_constants()
{
	int     n, m;
	static char *mon_name[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	m = 2039 - 1990;
	printf("\n/* number of seconds between\n"
		   " * GPS epoch (00:00:00 January 6, 1980)\n"
		   " * and UTC epoch (00:00:00 January 1, 1970"
		   " */\n"
		   "static long gps_epoch_since_utc_epoch = %ld; \n",
		   gps_epoch_since_utc_epoch);
	printf("static long year1990_epoch_seconds[2039-1990] = {");
	for (n = 0; n < m; n++) {
		if (n % 5 == 0)
			printf("\n   /* %d */  ", n + 1990);
		printf("%10ld%c ", year1990_epoch_seconds[n], (n == m - 1) ? ' ' : ',');
	}
	printf("\n};\n");

	printf("static short  year1990_leap_flag[2039-1990] = {");
	for (n = 0; n < m; n++) {
		if (n % 10 == 0)
			printf("\n   /* %d */  ", n + 1990);
		printf("%2hd%c ", year1990_leap_flag[n], (n == m - 1) ? ' ' : ',');
	}
	printf("\n};\n");

	printf("static long   leap_month_seconds[12] = {");
	m = 12;
	for (n = 0; n < m; n++) {
		if (n % 3 == 0)
			printf("\n   /* %s */  ", mon_name[n]);
		printf("%8ld%c ", leap_month_seconds[n], (n == m - 1) ? ' ' : ',');
	}
	printf("\n};\n");

	printf("static long   normal_month_seconds[12] = {");
	m = 12;
	for (n = 0; n < m; n++) {
		if (n % 3 == 0)
			printf("\n   /* %s */  ", mon_name[n]);
		printf("%8ld%c ", normal_month_seconds[n], (n == m - 1) ? ' ' : ',');
	}
	printf("\n};\n");
}


void
_calculate_smt_constants()
{
	struct tm tm1, tm2;
	time_t  t1, t2;
	int     k, n;
	long    s;
	int     mdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	/* number of seconds since epoch to Jan 1 of the year */
	/* GPS epoch: "06 January 1980" */
	memset(&tm1, 0, sizeof(tm1));
	tm1.tm_year = 1980 - 1900;
	tm1.tm_mon = 0;
	tm1.tm_mday = 6;

	memset(&tm2, 0, sizeof(tm2));
	tm2.tm_year = 1990;
	tm2.tm_mon = 0;
	tm2.tm_mday = 1;

	t1 = mktime(&tm1);
	gps_epoch_since_utc_epoch = (long) t1;
	for (n = 0; n < 2039 - 1990; n++) {
		tm2.tm_year = n + (1990 - 1900);
		t2 = mktime(&tm2);
		year1990_epoch_seconds[n] = (long) difftime(t2, t1);
	}

	/* check for leap years */
	memset(&tm2, 0, sizeof(tm2));
	for (n = 0; n < 2039 - 1990; n++) {
		tm2.tm_year = n + (1990 - 1900);
		tm2.tm_mon = 1;
		tm2.tm_mday = 29;
		tm2.tm_hour = 1;
		mktime(&tm2);
		year1990_leap_flag[n] = (tm2.tm_mon == 1);
	}

	/* calculate seconds offsets for first dates of months since Jan,1 */
	/* leap year */
	/* normal year */
	mdays[1] = 28;
	for (n = 0; n < 12; n++) {
		for (s = k = 0; k < n; k++)
			s += mdays[k];
		normal_month_seconds[n] = s * 86400L;
	}
	mdays[1] = 29;
	for (n = 0; n < 12; n++) {
		for (s = k = 0; k < n; k++)
			s += mdays[k];
		leap_month_seconds[n] = s * 86400L;
	}
}
#endif /* GENERATE_CONSTANTS */
