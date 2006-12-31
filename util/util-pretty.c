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

  Pretty printing of tables (in-memory arrays of structures).

*/

#include <optikus/util.h>
#include <optikus/conf-mem.h>	/* for oxazero */
#include <unistd.h>		/* for write */
#include <stdlib.h>		/* for strtol */
#include <stdio.h>		/* for vsprintf */
#include <string.h>		/* for strtok_r,strcpy,strlen */
#include <stdarg.h>


static char *sep = " ";
static char *sep2 = "\377";


static int
fieldSize(const char *fmt)
{
	char   *s;
	int     size;

	size = (int) strtol(fmt + 1, &s, 10);
	if (size < 0)
		size = -size;
	if (size == 0) {
		size = (*s == 'c' ? 1 : 4);
	}
	if (*s == 'l' || *s == 'L')
		s++;
	if (*s != 0)
		s++;
	while (*s != 0)
		s++, size++;
	size += 2;
	return size;
}

/*
 * Print table header.
 */
oret_t
prettyHeader(const char *format, ...)
{
	char   *cur;

	char   *sav;
	char    buf[256], fmt[256], field[64], *arg, *p;
	int     size, len, gap, tail, i;
	va_list ap;

	prettyFooter(format);
	p = buf;
	strcpy(fmt, format);
	va_start(ap, format);
	for (cur = strtok_r(fmt, sep, &sav); cur; cur = strtok_r(NULL, sep, &sav)) {
		*p++ = '|';
		size = fieldSize(cur);
		arg = (char *) va_arg(ap, char *);

		strcpy(field, arg);
		len = strlen(field);
		gap = tail = 0;
		if (len > size) {
			field[size - 1] = '*';
			field[size] = 0;
			len = size;
		} else if (len < size) {
			gap = (size - len) / 2;
			tail = size - (gap + len);
		}
		while (gap-- > 0)
			*p++ = ' ';
		for (i = 0; i < len; i++)
			*p++ = field[i];
		while (tail-- > 0)
			*p++ = ' ';
	}
	va_end(ap);
	*p++ = '|';
	*p++ = '\n';
	*p++ = 0;
	write(2, buf, strlen(buf));
	prettyFooter(format);
	return OK;
}


/*
 * Print table footer or divider.
 */
oret_t
prettyFooter(const char *format, ...)
{
	char   *cur;

	char   *sav;
	char    buf[256], fmt[256], *p;
	int     size;

	p = buf;
	strcpy(fmt, format);
	for (cur = strtok_r(fmt, sep, &sav); cur; cur = strtok_r(NULL, sep, &sav)) {
		*p++ = '+';
		size = fieldSize(cur);
		while (size-- > 0)
			*p++ = '-';
	}
	*p++ = '+';
	*p++ = '\n';
	*p++ = 0;
	write(2, buf, strlen(buf));
	return OK;
}


/*
 * Print table row.
 */
oret_t
prettyData(const char *format, ...)
{
	char   *cur;

	char   *sav, *sav2;
	char    buf[256], fmt[256], field[64], str[256], *arg, *p;
	int     size, len, gap, tail, i;
	va_list ap;

	strcpy(fmt, format);
	for (p = fmt; *p; p++)
		if (strchr(sep, *p))
			*p = *sep2;
	va_start(ap, format);
	vsprintf(str, fmt, ap);
	va_end(ap);
	oxazero(buf);
	p = buf;
	cur = strtok_r(fmt, sep2, &sav);
	arg = strtok_r(str, sep2, &sav2);
	while (cur && arg) {
		*p++ = '|';
		size = fieldSize(cur);
		strcpy(field, arg);
		len = strlen(field);
		gap = tail = 0;
		if (len > size) {
			field[size - 1] = '*';
			field[size] = 0;
			len = size;
		} else if (len < size) {
			gap = (size - len) / 2;
			tail = size - (gap + len);
		}
		while (gap-- > 0)
			*p++ = ' ';
		for (i = 0; i < len; i++)
			*p++ = field[i];
		while (tail-- > 0)
			*p++ = ' ';
		cur = strtok_r(NULL, sep2, &sav);
		arg = strtok_r(NULL, sep2, &sav2);
	}
	*p++ = '|';
	*p++ = '\n';
	*p++ = 0;
	write(2, buf, strlen(buf));
	return OK;
}
