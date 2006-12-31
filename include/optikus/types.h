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

  Main header of Optikus

  Note: If you build for VxWorks, define the VXWORKS macro on the compiler
        command line.

*/

#ifndef OPTIKUS_TYPES_H
#define OPTIKUS_TYPES_H

#ifdef __cplusplus
#define OPTIKUS_BEGIN_C_DECLS extern "C" {
#define OPTIKUS_END_C_DECLS   }
#else
#define OPTIKUS_BEGIN_C_DECLS
#define OPTIKUS_END_C_DECLS
#endif

OPTIKUS_BEGIN_C_DECLS


/* ============ type definitions ======== */

#if defined(VXWORKS)
#include <vxWorks.h>
#endif /* VXWORKS */

#include <sys/types.h>

#if defined(__linux__)
typedef unsigned int uint_t;
typedef unsigned char uchar_t;
typedef unsigned short ushort_t;
typedef unsigned long ulong_t;
#endif /* __linux__ */

typedef unsigned long ooid_t;

typedef void (*proc_t) (void);

typedef int bool_t;				/* TRUE or FALSE */
typedef int oret_t;			/* OK or ERROR */

/*
	The C standard doesn't strictly define signedness of a common char type.
	Use sbyte_t to explicitely declare signed values in range the -128..127.
*/
typedef signed char sbyte_t;

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

#define TRUE  1
#define FALSE 0

#define OK    0

#ifdef ERROR
#undef ERROR
#endif
#define ERROR (-1)

OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_TYPES_H */
