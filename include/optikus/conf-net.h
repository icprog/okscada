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

  Network code portability.

*/

#ifndef OPTIKUS_CONF_NET_H
#define OPTIKUS_CONF_NETL_H

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


/* maximum of two file descriptors (for select) */
#define OMAX(a,b)		((a)>(b)?(a):(b))


/*					byte order					*/

#define OROTS(x)	(short)((((ushort_t)(x) & 0xff) << 8) \
							| (((ushort_t)(x) >> 8) & 0xff))
#define OROTL(x)	(long)(((((ulong_t)(x)) & 0xff) << 24) \
							| ((((ulong_t)(x) >> 8) & 0xff) << 16) \
							| ((((ulong_t)(x) >> 16) & 0xff) << 8) \
							| (((ulong_t)(x) >> 24) & 0xff))

#if defined(WORDS_BIGENDIAN)
#define ONTOHS(x)	((short)(x))
#define OHTONS(x)	((short)(x))
#define ONTOHL(x)	((long)(x))
#define OHTONL(x)	((long)(x))
#else /* !WORDS_BIGENDIAN */
#define ONTOHS(x)	OROTS(x)
#define OHTONS(x)	OROTS(x)
#define ONTOHL(x)	OROTL(x)
#define OHTONL(x)	OROTL(x)
#endif /* WORDS_BIGENDIAN */

#define OUNTOHS(x)	((ushort_t)ONTOHS(x))
#define OUHTONS(x)	((ushort_t)OHTONS(x))
#define OUNTOHL(x)	((ulong_t)ONTOHL(x))
#define OUHTONL(x)	((ulong_t)OHTONL(x))


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_CONF_NET_H */
