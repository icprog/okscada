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

  Miscellaneous low level functions.

*/

#ifndef OPTIKUS_UTIL_H
#define OPTIKUS_UTIL_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


/*					strings						*/

bool_t  strMask(const char *str, const char *reg);
char   *strLeftTrim(char *str);
char   *strRightTrim(char *str);
char   *strTrim(char *str);

/*			pretty printing of tables			*/

oret_t  prettyHeader(const char *format, ...);
oret_t  prettyFooter(const char *format, ...);
oret_t  prettyData(const char *format, ...);

/*				file name expansion				*/

char   *expandSetup(const char *path);
char   *expandPerform(const char *partial_name, char *full_name_buffer);

/*					clocks					*/

long    osMsSleep(long mseconds);
long    osUsSleep(long useconds);
long    osMsClock(void);
long    osUsClock(void);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_UTIL_H */
