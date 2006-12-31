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

  Reading and sleeping on OS clock.

*/

#include <optikus/util.h>
#include <time.h>
#include <sys/time.h>

#if defined(HAVE_CONFIG_H)
#include <config.h>				/* for VXWORKS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS)
#include <timers.h>
#endif /* VXWORKS */


#if defined(VXWORKS)

long
osMsSleep(long msec)
{
	struct timespec tobe, real;

	if (msec <= 0)
		return 0;
	tobe.tv_sec = msec / 1000L;
	tobe.tv_nsec = (msec % 1000L) * 1000000L;
	if (nanosleep(&tobe, &real) == OK)
		return msec;
	else
		return (real.tv_sec * 1000L + real.tv_nsec / 1000000L);
}


long
osUsSleep(long usec)
{
	struct timespec tobe, real;

	if (usec <= 0)
		return 0;
	tobe.tv_sec = usec / 1000000L;
	tobe.tv_nsec = (usec % 1000000L) * 1000L;
	if (nanosleep(&tobe, &real) == OK)
		return usec;
	else
		return (real.tv_sec * 1000000L + real.tv_nsec / 1000L);
}


long
osMsClock()
{
	struct timespec tspec;
	clock_gettime(CLOCK_REALTIME, &tspec);
	return (tspec.tv_sec * 1000L + (tspec.tv_nsec + 500000L) / 1000000L);
}


long
osUsClock(void)
{
	struct timespec tspec;
	clock_gettime(CLOCK_REALTIME, &tspec);
	return (tspec.tv_sec * 1000000L + tspec.tv_nsec / 1000L);
}

#else /* UNIX */

long
osMsSleep(long msec)
{
	struct timespec tobe, real;

	if (msec <= 0)
		return 0;
	tobe.tv_sec = msec / 1000L;
	tobe.tv_nsec = (msec % 1000L) * 1000000L;
	if (nanosleep(&tobe, &real) == OK)
		return msec;
	else
		return (real.tv_sec * 1000L + real.tv_nsec / 1000000L);
}


long
osUsSleep(long usec)
{
	struct timespec tobe, real;

	if (usec <= 0)
		return 0;
	tobe.tv_sec = usec / 1000000L;
	tobe.tv_nsec = (usec % 1000000L) * 1000L;
	if (nanosleep(&tobe, &real) == OK)
		return usec;
	else
		return (real.tv_sec * 1000000L + real.tv_nsec / 1000L);
}


long
osMsClock()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000L + (tv.tv_usec + 500L) / 1000L);
}


long
osUsClock(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000000L + tv.tv_usec);
}

#endif /* VXWORKS vs UNIX */
