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

  Perl binding for Optikus.

*/

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#if PERL_IS_5_8_PLUS
#include "ppport.h"
#endif

#include <optikus-perl.h>
#include <optikus/util.h>
#include <optikus/log.h>
#include <optikus/watch.h>
#include <optikus/conf-mem.h>	/* for oxnew,oxfree */


#define LOG_SERVER_TIMEOUT	500

static bool_t _remote_log;
static bool_t _saved_remote_log;
static bool_t _init_watch;
static bool_t _connect_watch;


/*
 * .
 */
static int
_logClose (void)
{
	if (_remote_log) {
		if (_connect_watch)
			owatchDisconnect();
		if (_init_watch)
			owatchExit();
		owatchRemoteLog(_saved_remote_log);
	}
	_remote_log = _saved_remote_log = _init_watch = _connect_watch = FALSE;
	return (ologClose() == OK);
}


/* =================== MODULE: Log ====================== */

MODULE = Optikus::Log		PACKAGE = Optikus::Log


int
logOpen (client_name, log_file, kb_limit, server, topic)
	const char *client_name
	const char *log_file
	int kb_limit
	const char *server
	const char *topic
  INIT:
	oret_t rc;
	owop_t op;
	int err;
  CODE:
	_logClose();
	if (NULL == client_name || '\0' == *client_name)
		client_name = "Optikus";
	if (NULL == log_file || '\0' == *log_file)
		log_file = NULL;
	ologFlags(OLOG_DATE | OLOG_TIME | OLOG_MSEC | OLOG_FLUSH);
	ologOpen(client_name, log_file, kb_limit);
	rc = OK;
	if (NULL != server && *server != '\0') {
		_remote_log = TRUE;
		if (!owatchGetAlive("?")) {
			rc = owatchInit(client_name);
			if (rc == OK)
				_init_watch = TRUE;
		}
		if (rc == OK && !owatchGetAlive("*")) {
			op = owatchConnect(server);
			rc = owatchWaitOp(op, LOG_SERVER_TIMEOUT, &err);
			if (rc == OK)
				_connect_watch = TRUE;
			if (rc == ERROR && err == OWATCH_ERR_PENDING)
				owatchCancelOp(op);
		}
		_saved_remote_log = owatchRemoteLog(TRUE);
	}
	topic++;  /* just to avoid warning */
	RETVAL = (rc == OK);
  OUTPUT:
	RETVAL


int
logClose ()
  CODE:
	RETVAL = _logClose();
  OUTPUT:
	RETVAL


void
logLog (msg)
	const char *msg
  INIT:
	const char *s;
	char *d, *b;
  CODE:
	if (NULL == msg)
		msg = "";
	b = oxnew (char, strlen(msg)*2+1);
	for (s = msg, d = b; *s; *d++ = *s++) {
		if (*s == '%')
			*d++ = *s;
	}
	*d = 0;
	olog(b);
	oxfree(b);


int
logFlags (mode)
	int mode
  CODE:
	RETVAL = ologFlags(mode);
  OUTPUT:
	RETVAL


int
logSetOutput (no, name, limit_kb)
	int no
	const char *name
	int limit_kb
  CODE:
	if (!*name)
		name = NULL;
	RETVAL = (ologSetOutput(no, name, limit_kb) == OK);
  OUTPUT:
	RETVAL


int
logEnableOutput (no, do_work)
	int no
	int do_work
  CODE:
	RETVAL = (ologEnableOutput(no, do_work != 0) == OK);
  OUTPUT:
	RETVAL

