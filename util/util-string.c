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

  String manipulation wrappers and replacements.

*/

#include <optikus/util.h>
#include <string.h>
#include <ctype.h>


/*
 * .
 */
bool_t
strMask(const char *str, const char *reg)
{
	register const char *s = str;
	register const char *r = reg;
	register int  l;

	while(1)
	{
		if (0 == strcmp(r, "*"))
			return TRUE;
		if (NULL == strchr(r, '*'))
			return (0 == strcmp(r, s));
		l = (int) (strchr(r, '*') - r);
		if (l > 0) {
			if (strncmp(r, s, l))
				return FALSE;
			if (r[l] == 0)
				return TRUE;
			r += l;
			s += l;
		}
		while (*r == '*')
			r++;
		if (*r == '\0')
			return TRUE;
		if (NULL == strchr(r, '*'))
			l = strlen(r);
		else
			l = (int) (strchr(r, '*') - r);
		while (*s != '\0') {
			if (0 == strncmp(r, s, l))
				break;
			else
				s++;
		}
		if (*s == '\0')
			return FALSE;
		r += l;
		s += l;
	}
}


#define BLANK(c)	((c)==' ' || (c)=='\t' || (c)=='\n' || (c)=='\r')

/*
 * .
 */
char   *
strLeftTrim(char *s)
{
	register char *p = s;
	while (BLANK(*p))  p++;
	if (p != s)
		strcpy(s, p);
	return s;
}


/*
 * .
 */
char *
strRightTrim(char *s)
{
	register char *p = s;
	while (*p)  p++;
	while (p != s && BLANK(*(p-1)))  p--;
	*p = '\0';
	return s;
}


/*
 * .
 */
char *
strTrim(char *s)
{
	register char *p = s;
	while (BLANK(*p))  p++;
	if (p != s)
		strcpy(s, p);
	while (*p)  p++;
	while (p != s && BLANK(*(p-1)))  p--;
	*p = '\0';
	return s;
}
