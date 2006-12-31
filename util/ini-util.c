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

  String manipulation minutiae for the configuration parser.

*/

#include "ini-priv.h"

#include <stdarg.h>
#include <ctype.h>		/* for tolower() */


/*
 * remove whitespace (spaces and tabs) from buffer.
 */
void
_iniRmSpaceTab(char *buf)
{
	register char *s, *d;

	for (s = d = buf; *s; s++)
		if (*s != '\t' && *s != ' ')
			*d++ = *s;
	*d = 0;
}


/*
 * print warning or error message prepended with parsing location.
 */
void
_iniMessage(ini_source_t * source, const char *message, ...)
{
	int     cur_line, cur_char = 1, tab_size = 8, offset, k, i;
	char   *b, *name, buffer[256], location[64];
	va_list ap;

	if (!source || !source->buffer || !*source->name)
		*location = 0;
	else {
		cur_line = source->first_line + 1;
		b = source->buffer;
		offset = (int) (source->p - b);
		k = 0;
		name = source->name;
		for (i = 0; i < source->chunk_qty; i++) {
			if (offset >= source->chunks[i].offset) {
				k = source->chunks[i].offset;
				name = source->chunks[i].name;
			}
		}
		for (; b[k] && k < offset; k++) {
			switch (b[k]) {
			case '\r':
				cur_char = 1;
				break;
			case '\n':
				cur_char = 1;
				cur_line++;
				break;
			case '\b':
				cur_char = cur_char == 1 ? 1 : cur_char - 1;
				break;
			case '\t':
				cur_char += tab_size - (cur_char - 1) % tab_size;
				break;
			default:
				cur_char++;
				break;
			}
		}
		if (!b[k] && k < offset)
			_iniMessage(0, "internal error: pointer out of section");
		if (!b[k] && k >= offset && cur_line > 1)
			cur_line--;
		sprintf(location, "%s[%d/%d]: ", name, cur_line, cur_char);
	}
	va_start(ap, message);
	vsprintf(buffer, message, ap);
	va_end(ap);
	fprintf(stderr, "%serror: %s\n", location, buffer);
}


/*
 * bring string to lower case.
 */
char   *
_iniLowerStr(char *buffer)
{
	register char *s;

	for (s = buffer; *s; s++)
		*s = tolower(*s);
	return buffer;
}
