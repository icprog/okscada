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

  Logging.

*/

#ifndef OPTIKUS_LOG_H
#define OPTIKUS_LOG_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS

/* Set to 1, and olog() will be aliased to printf(), for a smart
 * compiler to validate that formats match argument types. */
#define __OLOG_VERIFICATION	0

#define OLOG_DATE		0x001
#define OLOG_TIME		0x002
#define OLOG_MSEC		0x004
#define OLOG_USEC		0x008
#define OLOG_LOGMSG		0x010
#define OLOG_STDOUT		0x020
#define OLOG_STDERR		0x040
#define OLOG_FLUSH		0x080
#define OLOG_USERTIME	0x100
#define OLOG_GETMODE	0

typedef int  (*ologhandler_t) (const char *);
typedef void (*ologtimer_t) (void *);

ologhandler_t ologSetHandler(ologhandler_t handler);
ologtimer_t   ologSetTimer(ologtimer_t timer);
char   *ologOpen(const char *client, const char *log_file, int kb_limit);
oret_t  ologClose(void);
oret_t  ologAbort(const char *format, ...);
int     ologWrite(const char *message);
char   *ologFormat(char *buffer, const char *format, void *var_args);
int     ologFlags(int flags);
oret_t  ologBuffering(const char *file, int kb_size);
oret_t  ologSetOutput(int no, const char *name, int kb_limit);
bool_t  ologEnableOutput(int no, bool_t do_work);

#if __OLOG_VERIFICATION
#include <stdio.h>
#define olog	printf
#else
int     olog(const char *format, ...);
#endif /* __OLOG_VERIFICATION */

# if defined(__GNUC__) && __GNUC__ >= 3
#  define ologIf(guard,args...)		((guard) ? olog(args) : -1)
# elif defined(__GNUC__) && __GNUC__ >= 2
#  define ologIf(guard,args...)		((guard) ? olog(## args) : -1)
# else
int     ologIf(bool_t guard, const char *format, ...);
# endif /* __GNUC__ */


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_LOG_H */
