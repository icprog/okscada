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

  Internal declarations for configuration parser.

*/

#ifndef OPTIKUS_INI_PRIV_H
#define OPTIKUS_INI_PRIV_H

#include <optikus/ini.h>
#include <stdio.h>			/* for FILE */

OPTIKUS_BEGIN_C_DECLS


#undef IGNORE
#define IGNORE      (-32765)	/* denotes unlimited string length */

#define SECTION_LENGTH  128		/* maximum section name length */
#define ENTRY_LENGTH    128		/* maximum parameter name length */
#define LINE_LENGTH     512		/* maximum input line length */

/* field types */
typedef enum
{
	FPD_NONE = 0,				/* field descriptor is uninitialized */
	FPD_STRING = 1,				/* the field is string */
	FPD_INTEGER = 2,			/* the field is integer */
	FPD_ENUM = 3,				/* the field is enumeration */
	FPD_FLOAT = 4				/* the field is float */
} field_parse_type_t;

/* string token descriptor */
typedef struct
{
	int     max_len;			/* IGNORE = unlimited length */
	char   *def;
} string_desc_t;

/* integer field descriptor */
typedef struct
{
	int     max_digits;	/* maximum number of digits, IGNORE = unconstrained */
	int     lo_limit;	/* minimum integer value, IGNORE = unconstrained */
	int     hi_limit;	/* maximum integer value, IGNORE = unconstrained */
	int     def_val;	/* default integer value, IGNORE = not specified */
	int     out_base;	/* base (dec, oct, bin, hex) for output formatting */
	bool_t  unsign;		/* FALSE=signed, TRUE=unsigned */
} integer_desc_t;

/* enumeration slot descriptor */
typedef struct
{
	char    str[32];		/* string representation of the slot */
	int     chars_to_cmp;	/* number of characters to compare, -1 = all */
	bool_t  ignore_case;	/* string comparison: TRUE = ignore case, FALSE = strict */
	int     value;			/* numeric slot value */
} enum_slot_desc_t;

/* enumeration descriptor */
typedef struct
{
	enum_slot_desc_t *enums;/* pointer to array of slot descriptors */
	int     enum_qty;		/* quantity of slot descriptors */
	bool_t  shorting;		/* TRUE = use the minimum distinctive part for comparison */
	bool_t  ignore_case;	/* TRUE = ignore case, FALSE = strict comparison */
	int     def_val;		/* default value for this field */
	int     out_base;		/* output base */
	int     max_len;		/* maximum lebgth */
} enum_desc_t;

/* general field descriptor */
typedef struct
{
	field_parse_type_t type;	/* field type */
	char    name[64];			/* string with the field name */
	char    section[64];		/* default section for the field */
	bool_t  not_null;			/* TRUE=field must not be null, FALSE=field may be null */
	union
	{
		string_desc_t d_str;	/* as a string */
		integer_desc_t d_int;	/* as an integer */
		enum_desc_t d_enum;		/* as an enumeration */
	} d;
} ini_field_t;

/* descriptor of sources */
typedef struct
{
	char    name[80];
	int     offset;
} ini_chunk_t;

typedef struct
{
	char    name[80];		/* name of the file (NULL = string in the program) */
	int     first_line;
	int     chunk_qty;
	ini_chunk_t *chunks;
	char   *buffer;			/* buffer containing the source */
	char   *p;				/* pointer to the current symbol being */
} ini_source_t;


#undef  SCP
#define SCP  (source->p)

#define ISNAME		"_+-*/:@$%^&()0123456789"\
					"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

/* fetch descriptor */
typedef struct
{
	char    name[64];		/* name of the section containing the table */
	ini_field_t *cols;		/* descriptors of fields */
	int     col_qty;
	int     row_qty;		/* number of rows */
	int     cur_row;		/* current row */
	inival_t *row;
	ini_source_t source;
} ini_fetch_t;

/* initialization file descriptor */
typedef struct _ini_db_struc
{
	ini_source_t source;
	ini_fetch_t *fetches;		/* pointer to array of fetch descriptors */
	int     fetch_qty;			/* quantity of fetches in the array */
} ini_database_t;


/* create fetch descriptor */
void    _iniCreateFetch(ini_fetch_t * fetch);

/* remove fetch descriptor and its slots */
void    _iniDelFetch(ini_fetch_t * fetch);

/* find and read the string */
int     _iniGetEntry(ini_source_t * source,
					 const char *section, const char *entry, bool_t with_name);
/* skip blanks */
int     _iniSkipBlanks(ini_source_t * source);

/* skip characters until the end of line */
int     _iniSkipUntilEol(ini_source_t * source);

/* print message (warning or error) with location information */
void    _iniMessage(ini_source_t * source, const char *message, ...);

/* bring string to lower case */
char   *_iniLowerStr(char *buffer);

/* check whether we have a given string in the stream */
bool_t  _iniExpectStr(char *str_to_expect, ini_source_t * source,
					  bool_t is_quiet);

/* read different kinds of fields */
int     _iniReadInteger(int max_digits, int lo_limit, int hi_limit,
						bool_t unsign, ini_source_t * source, int *error_code);
char   *_iniReadStr(bool_t skip_before, const char *delimiters, int max_len,
					ini_source_t * source, char *buff);
int     _iniParseFieldDesc(ini_field_t * desc, ini_source_t * source);
void    _iniDelFieldDesc(ini_field_t * field);
int     _iniReadField(ini_field_t * desc, ini_source_t * source,
					  inival_t * value, const char *delim);

/* find the default value for the field */
int     _iniDefaultValue(ini_field_t * field, inival_t * value);

/* make source from the ini file section */
int     _iniSectionFromIni(FILE *file, const char *ini_file,
						   const char *section, ini_source_t * source);
/* make source from the internal string (with subsections) */
int     _iniSectionFromString(const char *string, const char *section,
							  ini_source_t * source);
/* make source from the internal character string (simple) */
int     _iniSectionFromSimpleString(const char *string,
									const char *virtual_file_name,
									ini_source_t * source);

/* delete descriptor of the source and free memory */
void    _iniDelSource(ini_source_t * source);

/* find a section in a file */
int     _iniLocateSection(ini_source_t * source, const char *section);

/* find entry in the section */
int     _iniLocateEntry(ini_source_t * source, const char *entry,
						bool_t with_name);
/* remove spaces and tabs from the buffer */
void    _iniRmSpaceTab(char *buf);


OPTIKUS_END_C_DECLS

#endif /*  OPTIKUS_INI_PRIV_H  */
