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

  Search for a file in a directory list and fully expand the file name.

*/

#include <optikus/util.h>
#include <optikus/log.h>
#include <optikus/conf-mem.h>	/* for oxnew */
#include <string.h>		/* for strcspn,strncpy,strcat,... */
#include <stdlib.h>		/* for getenv */
#include <sys/stat.h>	/* for stat */


#define  SLASH_CHR   '/'
#define  SLASH_STR   "/"

static char expand_path[320];		/* FIXME: magic sizes */
static char expand_path2[320];

int  expand_verb;

#define expandLog(verb,args...)		(expand_verb >= (verb) ? olog(args) : -1)


/*
 *  saves paths for trials
 */
char   *
expandSetup(const char *path_str)
{
	strcpy(expand_path2, expand_path);
	strcpy(expand_path, path_str);
	return expand_path2;
}


/*
 *  try to expand file name in several paths
 */
char   *
expandPerform(const char *part_src, char *buf)
{
	static const char *me = "expandPerform";
	struct stat st;
	char   *cur, *part1;

	char   *last;
	char   *delim = ":";
	char    var[82], part2[132], part[160];		/* FIXME: magic sizes */
	int     n;
	bool_t  anyway = FALSE;

	anyway = (NULL != buf && 0 == strncmp(buf, "ANYWAY", 6));	/* FIXME: hackish */
	if (NULL == buf) {
		if (NULL == (buf = oxnew(char, 128))) {	/* FIXME: magic size */
			return NULL;
		}
	}

	*buf = 0;
	if (NULL == part_src || *part_src == '\0') {
		return buf;
	}

	strcpy(part, part_src);
	if (*part == '$') {
		n = strcspn(part + 1, "/");
		if (n > 80)
			n = 80;
		strncpy(var, part + 1, n);
		var[n] = 0;
		strcpy(part2, part + 1 + n);
		part1 = getenv(var);
		if (!part1) {
			expandLog(2, "%s: environment variable '%s' not found", me, var);
			part1 = "";
		}
		strcpy(part, part1);
		strcat(part, part2);
		expandLog(5, "%s: source part1='%s', part2='%s', part='%s'",
					me, part1, part2, part);
	}
	strcpy(expand_path2, expand_path);
	for (cur = strtok_r(expand_path, delim, &last);
		 cur != NULL; cur = strtok_r(0, delim, &last)) {
		if (cur[0] != '$') {
			part1 = cur;
			part2[0] = 0;
		} else {
			cur++;
			n = strcspn(cur, "/");
			if (n > 80)
				n = 80;
			strncpy(var, cur, n);
			var[n] = 0;
			strcpy(part2, cur + n);
			part1 = getenv(var);
			if (!part1) {
				expandLog(2, "%s: environment variable '%s' not found", me, var);
				continue;
			}
		}
		if (part[0] == SLASH_CHR || part1[0] == 0) {
			strcpy(buf, part);
		} else {
			strcpy(buf, part1);
			if (part2[0] != 0)
				strcat(buf, part2);
			n = strlen(buf);
			if (n > 0 && buf[n - 1] != SLASH_CHR)
				strcat(buf, SLASH_STR);
			strcat(buf, part);
			expandLog(5, "%s: part1='%s' part2='%s' part3='%s' buf='%s'",
						me, part1, part2, part, buf);
		}
		if (0 == stat(buf, &st))
			break;
		expandLog(3, "%s: search as '%s' failed", me, buf);
		buf[0] = 0;
	}
	strcpy(expand_path, expand_path2);
	if (!*buf) {
		expandLog(2, "%s: file '%s' not found in path '%s'",
					me, part, expand_path);
	}
	expandLog(4, "%s: search as '%s' OK", me, buf);
	if (buf[0] == 0 && anyway)
		strcpy(buf, part);
	return buf;
}
