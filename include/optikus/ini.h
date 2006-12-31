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

  Parser of configuration (.ini) files.

*/

#ifndef OPTIKUS_INI_H
#define OPTIKUS_INI_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


typedef union					/* generalized data type */
{
	int     v_int;
	long    v_long;
	float   v_float;
	double  v_double;
	short   v_short;
	char    v_char;
	uchar_t v_uchar;
	ushort_t v_ushort;
	uint_t  v_uint;
	ulong_t v_ulong;
	int     v_bool;
	int     v_enum;
	char   *v_str;
	void   *v_ptr;
} inival_t;

#define PMK_QUIET    0
#define PMK_WARNING  0
#define PMK_ERROR    0
#define PMK_SEVERE   0

typedef struct _ini_db_struc *inidb_t;	/* ini file handle */

typedef int inireq_t;			/* request handle */

#define INIREQ_NULL  0

#define DEF_SECTION  0

inidb_t iniOpen(const char *ini_file, const char *param_desc,
				int unused_msg_kind);
int     iniCache(const char *ini_file, char *buffer);
oret_t  iniFlush(void);
void    iniClose(inidb_t db);
void    iniClean(inidb_t db);
int     iniParam(inidb_t db, const char *entry, inival_t * value);
int     iniCount(inidb_t db, inireq_t req);
inireq_t iniAskRow(inidb_t db, const char *table_name);
bool_t  iniGetRow(inidb_t db, inireq_t req, ...);
oret_t  iniAugment(inidb_t db, const char *describing, const char *name);
oret_t  iniPassport(inidb_t db, const char *describing);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_INIT_H */
