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

  Path and string manipulation minutiae.

*/

#include "hub-priv.h"
#include <optikus/util.h>		/* for expandPerform */
#include <optikus/conf-mem.h>	/* for oxnew,oxstrdup */
#include <string.h>				/* for memmove,rindex, strcpy,strlen */
#include <stdlib.h>		/* for getenv */
#include <ctype.h>		/* for isalnum */
#include <limits.h>		/* for PATH_MAX */


#define PATH_SEP	'/'
#define ISNAME(c)	(isalnum((int)(c)) || (c)=='_')

char    ohub_common_root[OHUB_MAX_PATH];


/*
 * .
 */
char   *
ohubNameOfPath(const char *src, char *dst)
{
	char   *s;

	if (NULL == dst)
		dst = oxstrdup(src);
	if (dst != src)
		strcpy(dst, src);
	s = rindex(dst, PATH_SEP);
	if (s != NULL)
		strcpy(dst, s + 1);
	return dst;
}

char   *
ohubDirOfPath(const char *src, char *dst)
{
	char   *s;

	if (NULL == dst)
		dst = oxstrdup(src);
	if (dst != src)
		strcpy(dst, src);
	s = rindex(dst, PATH_SEP);
	if (s != NULL)
		*s = 0;
	return dst;
}


char   *
ohubSubstEnv(const char *src, char *dst)
{
	const char *s = src;
	const char *p;
	char    buf[80] = {0};
	char    tmp[OHUB_MAX_PATH] = {0};
    char   *d = tmp;
	char   *b;

	while (*s) {
		if (s[0] == '$' && s[1] == '$') {
			s++;
			s++;
			*d++ = '$';
		} else if (s[0] == '$' && s[1] == '{') {
			p = s + 2;
			b = buf;
			while (*p && *p != '}')
				*b++ = *p++;
			*b = 0;
			if (*p == '}') {
				s = p + 1;
				p = getenv(buf);
				if (p != NULL) {
					strcpy(d, p);
					d += strlen(p);
				}
			} else {
				*d++ = *s++;
				*d++ = *s++;
			}
		} else if (s[0] == '$' && ISNAME(s[1])) {
			p = s + 1;
			b = buf;
			while (*p && ISNAME(*p))
				*b++ = *p++;
			*b = 0;
			s = p;
			p = getenv(buf);
			if (p != NULL) {
				strcpy(d, p);
				d += strlen(p);
			}
		} else {
			*d++ = *s++;
		}
	}
	*d = 0;
	if (NULL == dst)
		dst = oxstrdup(tmp);
	else
		strcpy(dst, tmp);
	return dst;
}


char   *
ohubMergePath(const char *dir, const char *name, char *dst)
{
	if (dir == 0)
		dir = "";
	if (name == 0)
		name = "";
	if (dst == 0)
		dst = oxnew(char, strlen(dir) + strlen(name) + 2);

	if (*name == PATH_SEP || !*dir)
		strcpy(dst, name);
	else if (!*name)
		strcpy(dst, dir);
	else {
		int     n = strlen(dir);

		strcpy(dst, dir);
		if (dst[n - 1] != PATH_SEP) {
			dst[n++] = PATH_SEP;
			dst[n] = 0;
		}
		strcpy(dst + n, name);
	}
	return dst;
}


char   *
ohubUnrollPath(const char *src, char *dst)
{
	char    tmp[OHUB_MAX_PATH];

	if (!src || !*src) {
		dst[0] = 0;
		return dst;
	}
	ohubSubstEnv(src, tmp);
	dst = ohubMergePath(ohub_common_root, tmp, dst);
	dst = ohubSubstEnv(dst, dst);
	return dst;
}


char   *
ohubExpandPath(const char *path, const char *name, char *buf)
{
	char   *result;

	expandSetup(path);
	result = expandPerform(name, buf);
	return result;
}


char   *
ohubAbsolutePath(const char *src, char *dst)
{
	char   *s, buf[PATH_MAX];
	s = realpath(src, buf);
	strcpy(dst, s ? s : src);
	return dst;
}


char   *
ohubShortenPath(const char *src, char *buf, int limit)
{
	const char *s, *e;
	int     n = strlen(src);

	if (NULL == buf)
		buf = oxnew(char, n + 4);

	if (limit <= 0)
		limit = 20;
	e = (s = src) + ((n = strlen(src)) - limit);
	if (n >= limit)
		for (s = e; s > src && *s != PATH_SEP; s--);
	if (s == src) {
		if (buf != src)
			strcpy(buf, src);
	} else {
		n = (int) (src + n - s) + 1;
		memmove(buf + 3, s, n);
		buf[0] = buf[1] = buf[2] = '.';
	}
	return buf;
}


char   *
ohubTrimLeft(char *buf)
{
	char   *s = buf;

	for (;;) {
		switch (*s) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\b':
			s++;
			continue;
		}
		break;
	}
	if (s != buf)
		strcpy(buf, s);
	return buf;
}


char   *
ohubTrimRight(char *buf)
{
	/* trim trailing whitespace */
	int     k = strlen(buf) - 1;

	while (k >= 0) {
		switch (buf[k]) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\b':
			buf[k--] = 0;
			continue;
		}
		break;
	}
	return buf;
}


char   *
ohubTrim(char *buf)
{
	return ohubTrimLeft(ohubTrimRight(buf));
}


#define U8ROT(a,b)    u8=a,a=b,b=u8

oret_t
ohubRotate(bool_t enable, char type, short len, char *buf)
{
	uchar_t *pu8, u8;

	if (!enable || type == 's' || len == 1)
		return OK;
	pu8 = (uchar_t *) buf;
	switch (len) {
	case 2:
		U8ROT(pu8[0], pu8[1]);
		return OK;
	case 4:
		U8ROT(pu8[0], pu8[3]);
		U8ROT(pu8[1], pu8[2]);
		return OK;
	case 8:
		U8ROT(pu8[0], pu8[7]);
		U8ROT(pu8[1], pu8[6]);
		U8ROT(pu8[2], pu8[5]);
		U8ROT(pu8[3], pu8[4]);
		return OK;
	case 16:
		U8ROT(pu8[0], pu8[15]);
		U8ROT(pu8[1], pu8[14]);
		U8ROT(pu8[2], pu8[13]);
		U8ROT(pu8[3], pu8[12]);
		U8ROT(pu8[4], pu8[11]);
		U8ROT(pu8[5], pu8[10]);
		U8ROT(pu8[6], pu8[9]);
		U8ROT(pu8[7], pu8[8]);
		return OK;
	default:
		return OK;
	}
}
