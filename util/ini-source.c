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

  TO BE MERGED INTO THE FILE WHERE LOCATION UTILITIES LIVE.

*/

#include "ini-priv.h"
#include <optikus/util.h>		/* for strMask (FIXME: fnmatch?) */
#include <optikus/conf-mem.h>	/* for oxfree,oxvzero */
#include <string.h>				/* for strchr */
#include <ctype.h>				/* for isalnum */


/*
 * .
 */
int
_iniFindSection(ini_source_t * source, const char *section, bool_t quiet)
{
	char   *f = source->p;
	char   *start;
	char    buffer[256];
	int     c, k, len;
	bool_t  first = TRUE;

	/* look for the section with the supplied name. */
	for (;;) {
		/* skip until start of a section */
		for (;;) {
			if ((c = *f++) == 0) {
				if (quiet || !section)
					return -1;
				_iniMessage(source, "section [%s] not found", section);
				return -2;
			}
			if (c == '\r' || c == '\n' || first) {
				first = FALSE;
				while (c == '\r' || c == '\n')
					c = *f++;
				while (c == ' ' || c == '\t')
					c = *f++;
				if (c == '[')
					break;
				else
					f--;
			}
		}
		/* read name of the section */
		for (k = 0; k < 250; k++) {
			if ((c = *f++) == 0) {
				_iniMessage(source, "wrong format");
				return -3;
			}
			if (isalnum(c) || strchr("_+-*/;:@#$%^&()", c) != 0)
				buffer[k] = c;
			else if (c == ']')
				break;
			else {
				_iniMessage(source, "wrong section name");
				return -4;
			}
		}
		if (k > 200) {
			_iniMessage(source, "section name too long");
			return -5;
		}
		buffer[k] = 0;
		/* check whether it is wanted section */
		if (section && strMask(section, buffer))
			break;
	}
	/* read the section */
	for (c = *f++; c != '\r' && c != '\n' && c; c = *f++);
	source->p = start = f;
	for (;;) {
		if ((c = *f++) == 0)
			break;
		if (c == '\r' || c == '\n') {
			while (c == '\r' || c == '\n')
				c = *f++;
			while (c == ' ' || c == '\t')
				c = *f++;
			if (c == '[') {
				f--;
				break;
			}
			if (c == 0)
				break;
		}
	}
	len = (int) (f - start);
	return len;
}


/*
 * .
 */
void
_iniDelSource(ini_source_t * source)
{
	if (NULL != source) {
		oxfree(source->chunks);
		oxfree(source->buffer);
		oxvzero(source);
	}
}
