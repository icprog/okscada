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

  Logging in back ends.

*/

#include "subj-priv.h"
#include <stdarg.h>		/* for va_list */
#include <string.h>		/* for strlen */

#if defined(HAVE_CONFIG_H)
#include <config.h>		/* for VXWORKS */
#endif /* HAVE_CONFIG_H */


bool_t  osubj_remote_log;


#undef osubjLog
int
osubjLog(const char *fmt, ...)
{
#if OSUBJ_LOGGING != 0

	va_list ap;
	char    buf[1024];		/* FIXME: magic size */

	/* format the message */
	va_start(ap, fmt);
#if OSUBJ_LOGGING == 1
	ologFormat(buf, fmt, ap);
#else /* format ourselves */
	strcpy(buf, "osubj: ");
	vsprintf(buf + 7, fmt, ap);
#endif /* LOGGING == 1 */
	va_end(ap);

	/* send to stream */
#if OSUBJ_LOGGING == 1
	ologWrite(buf);
#elif OSUBJ_LOGGING == 2
	strcat(buf, "\n");
	fputs(buf, stdout);
#else /* LOGGING == 3 */
	if (osubj_remote_log && osubjIsConnected()) {
		osubjSendPacket(OK, OLANG_DATA, OLANG_REMOTE_LOG, strlen(buf)+1, buf);
	}
	else { /* print locally */
#if defined(VXWORKS)
		logMsg(buf);
#else /* !VXWORKS */
		strcat(buf, "\n");
		fprintf(stderr, buf);
#endif /* VXWORKS vx UNIX */
	}
#endif /* LOGGING */

#endif /* LOGGING != 0 */
	return 0;
}


#if OSUBJ_LOGGING == 1
static ologhandler_t osubj_saved_log_handler;
static bool_t osubj_in_remote_log;		/* FIXME: must be atomic */

static int
osubjRemoteLogHandler(const char *msg)
{
	if (osubj_initialized && !osubj_aborted
			&& osubj_remote_log && osubjIsConnected()) {
		if (!osubj_in_remote_log) {
			osubj_in_remote_log = TRUE;
			osubjSendPacket(OK, OLANG_DATA, OLANG_REMOTE_LOG,
							strlen(msg)+1, (char *) msg);
			osubj_in_remote_log = 0;
		}
	}
	return 0;
}
#endif /* LOGGING == 1 */


bool_t
osubjRemoteLog(bool_t enable)
{
	bool_t old_enable = osubj_remote_log;

	if (enable == TRUE || enable == FALSE) {
#if OSUBJ_LOGGING == 1
		if (osubj_remote_log != enable) {
			if (enable) {
				osubj_saved_log_handler = ologSetHandler(osubjRemoteLogHandler);
			} else {
				ologSetHandler(osubj_saved_log_handler);
				osubj_saved_log_handler = NULL;
			}
		}
#endif /* LOGGING == 1 */
		osubj_remote_log = enable;
	}
	return old_enable;
}
