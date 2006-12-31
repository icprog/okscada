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

  Logging facility.

*/

#include <optikus/log.h>
#include <optikus/util.h>		/* for osMsClock */
#include <optikus/time.h>		/* for TimeXxxFine_t */
#include <optikus/conf-mem.h>	/* for oxnew,oxfree,oxbzero */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if defined(HAVE_CONFIG_H)
#include <config.h>				/* for VXWORKS */
#endif /* HAVE_CONFIG_H */

#if defined(VXWORKS)
#include <logLib.h>
#else /* !VXWORKS */
#include <unistd.h>				/* for unlink() */
#include <stdlib.h>				/* for exit() */
#include <sys/stat.h>			/* for stat() */
#endif /* VXWORKS vs UNIX */

/*			internal variables and procedures			*/

#define MAX_CHUNK_FILE_NO	4
#define LOG_BUF_NUM			32
#define LOG_BUF_SIZE		4096
#define LOG_FILE_NUM		3
#define LOG_DEFAULT_FLAGS	(OLOG_TIME | OLOG_MSEC | OLOG_STDERR)
#define LOG_DEF_BUFFERING_KB 1024

typedef char logname_t[80];

static FILE *  _log_files[LOG_FILE_NUM];
static long    _log_lengths[LOG_FILE_NUM];
static long    _log_size_limit[LOG_FILE_NUM];
static long    _log_check_ms[LOG_FILE_NUM];
static bool_t  _log_working[LOG_FILE_NUM];
static int     _log_init_count;
static logname_t _log_names[LOG_FILE_NUM];
static int     _log_flags = LOG_DEFAULT_FLAGS;
static char    _log_client[32] = "NONAME";
static char *  _log_bufs[LOG_BUF_NUM];
static int     _log_cur_buf;

static int     _log_keep_mode;
static FILE *  _log_keep_file;
static char *  _log_keep_buf;
static int     _log_keep_len;
static int     _log_keep_offset;
static int     _log_keep_more;
static bool_t  _log_keep_call_handler;

static ologhandler_t _log_keep_handler_ptr;
static ologhandler_t _log_handler;
static ologtimer_t   _log_timer;


/*
 * .
 */
void
_logInit(void)
{
	int     i;

	if (_log_bufs[0])
		return;
	for (i = 0; i < LOG_BUF_NUM; i++)
		_log_bufs[i] = oxnew(char, LOG_BUF_SIZE);
}


/*
 * create formatted message in the buffer
 */
char   *
ologFormat(char *buffer, const char *format, void *var_args)
{
	TimeUnsegFine_t uft;
	TimeSegFine_t sft;
	char   *s, *p;
	char    time2[40];
	int     i;

	if (!buffer) {
		if (!_log_bufs[0])
			_logInit();
#if defined(VXWORKS)
		int il = osIntrLock(0);
#endif
		_log_cur_buf = ((i = _log_cur_buf) + 1) % LOG_BUF_NUM;
#if defined(VXWORKS)
		osIntrUnlock(il);
#endif
		buffer = _log_bufs[i];
		*buffer = 0;
	}
	if (NULL == _log_timer) {
		/* timer=0 - print local time from system clock */
		*time2 = 0;
		otimeCurSegFine(&sft);
	} else if (_log_flags & OLOG_USERTIME) {
		/* timer<>0,OLOG_USERTIME<>0 - print both local and model time */
		(*_log_timer) ((void *) time2);
		strcat(time2, " ");
		otimeCurSegFine(&sft);
	} else {
		/* timer<>0,OLOG_USERTIME=0 - print the model time only */
		*time2 = 0;
		(*_log_timer) ((void *) &uft);
		otimeUnsegFine2SegFine(&uft, &sft);
	}
	s = buffer;
	if (_log_flags & OLOG_DATE)
		s += sprintf(s, "%02d.%02d.%02d ", sft.year % 100, sft.month, sft.day);
	if (_log_flags & (OLOG_TIME | OLOG_MSEC | OLOG_USEC)) {
		s += sprintf(s, "%02d:%02d:%02d", sft.hour, sft.min, sft.sec);
		if (_log_flags & OLOG_MSEC)
			s += sprintf(s, ".%03d", (int) sft.usec / 1000);
		else if (_log_flags & OLOG_USEC)
			s += sprintf(s, ".%06ld", sft.usec);
	}
	strcat(s, " ");
	strcat(s, time2);
	strcat(s, _log_client);
	strcat(s, ": ");
	p = s + strlen(s);
	vsprintf(p, format, *(va_list *) var_args);
	strcat(s, "\n");
	return buffer;
}


/*
 * Print a message and abort the program.
 */
oret_t
ologAbort(const char *format, ...)
{
	va_list var_args;
	char    buffer[256];

	va_start(var_args, format);
	ologFormat(buffer, format, (void *) &var_args);
	va_end(var_args);
	_log_flags &= ~(OLOG_STDOUT | OLOG_STDERR);
	if (_log_files[0])
		fputs(buffer, _log_files[0]);
#if defined(VXWORKS)
	if (_log_flags & OLOG_LOGMSG)
		logMsg(buffer, 0, 0, 0, 0, 0, 0);
	else
#endif
	if (_log_flags & OLOG_STDERR)
		fputs(buffer, stderr);
	else
		fputs(buffer, stdout);
	exit(1);
	return ERROR;
}


/*
 * .
 */
bool_t
ologEnableOutput(int no, bool_t wrk)
{
	bool_t  old;

	if (no < 0 || no >= LOG_FILE_NUM)
		return FALSE;
	old = _log_working[no];
	if (wrk == TRUE || wrk == FALSE)
		_log_working[no] = wrk;
	return old;
}


/*
 * .
 */
oret_t
ologSetOutput(int no, const char *name, int kb_limit)
{
	if (no < 0 || no >= LOG_FILE_NUM)
		return ERROR;
	if (NULL == name) {
		/* close */
		if (_log_files[no])
			fclose(_log_files[no]);
		oxbzero(_log_names[no], sizeof(logname_t));
		_log_working[no] = FALSE;
		_log_lengths[no] = -1;
		_log_size_limit[no] = 0;
		return OK;
	}
	if (!*name) {
		/* stdout */
		strcpy(_log_names[no], "<stdout>");
		_log_files[no] = NULL;
		_log_working[no] = TRUE;
		_log_lengths[no] = -1;
		_log_size_limit[no] = 0;
		return OK;
	}
	/* open */
	expandPerform(name, _log_names[no]);
	if (!*_log_names[no])
		strcpy(_log_names[no], name);
	_log_files[no] = fopen(_log_names[no], "a");
	_log_lengths[no] = -1;
	_log_size_limit[no] = kb_limit * 1024u;
	if (_log_size_limit[no] < 0)
		_log_size_limit[no] = 0;
	_log_check_ms[no] = osMsClock();
	if (!_log_files[no]) {
		_log_files[no] = fopen(_log_names[no], "w");
		_log_lengths[no] = 0;
	}
#if !defined(VXWORKS)
	if (_log_files[no] && _log_lengths[no] < 0) {
		struct stat st;

		if (stat(_log_names[no], &st) == 0) {
			_log_lengths[no] = (long) st.st_size;
		}
	}
	chmod(_log_names[no], 0666);
#endif /* !VXWORKS */
	_log_working[no] = TRUE;
	return (_log_files[no] ? OK : ERROR);
}


/*
 * .
 */
char   *
ologOpen(const char *client_name, const char *log_file, int kb_limit)
{
	int  no;

	if (_log_init_count++)
		return _log_names[0];
	_logInit();
	if (NULL == client_name || '\0' == *client_name)
		client_name = "PROGRAM";
	if (NULL == log_file)
		log_file = "";
	strcpy(_log_client, client_name);
	/* If the log file is already open, close it */
	for (no = 0; no < LOG_FILE_NUM; no++)
		ologSetOutput(no, NULL, 0);
	if (ologSetOutput(0, log_file, kb_limit))
		ologAbort("cannot open log file %s", _log_names[0]);
	return _log_names[0];
}


/*
 * .
 */
oret_t
ologClose()
{
	if (_log_init_count <= 0)
		return OK;
	_log_init_count--;
	return OK;
}


/*
 * .
 */
oret_t
_logReopenFile(int no)
{
	int     j;

	fclose(_log_files[no]);
	for (j = 0; j < 10; j++) {
		_log_files[no] = fopen(_log_names[no], "a");
		if (_log_files[no])
			break;
	}
	if (!_log_files[no]) {
		FILE   *f = fopen(_log_names[no], "r");

		fclose(f);
		if (f == 0) {
			_log_files[no] = fopen(_log_names[no], "w");
			_log_lengths[no] = 0;
		}
	}
	if (!_log_files[no]) {
		fprintf(stderr, "logging lost file \"%s\"", _log_names[no]);
		return ERROR;
	}
	return OK;
}


int
ologWrite(const char *msg)
{
	int     no, j, rc, len;
	char   *buffer;

	if (NULL == msg) {
		return -1;
	}
	len = strlen(msg);

	if (!_log_bufs[0]) {
		_logInit();
	}

#if defined(VXWORKS)
	{
		int l = osIntrLock(0);
#endif /* VXWORKS */
		_log_cur_buf = ((j = _log_cur_buf) + 1) % LOG_BUF_NUM;
#if defined(VXWORKS)
		osIntrUnlock(l);
	}
#endif /* VXWORKS */

	buffer = _log_bufs[j];
	strncpy(buffer, msg, LOG_BUF_SIZE - 1);
	buffer[LOG_BUF_SIZE - 1] = '\0';

	for (no = 0; no < LOG_FILE_NUM; no++) {
		if (!_log_files[no] || !_log_working[no])
			continue;
		rc = fputs(msg, _log_files[no]);
		if (_log_flags & OLOG_FLUSH)
			fflush(_log_files[no]);

#if !defined(VXWORKS)
		if (rc < 0) {
			/* Probably, another application has moved or renamed the file. */
			if (_logReopenFile(no))
				continue;
			fputs(msg, _log_files[no]);
		}
		if (_log_lengths[no] >= 0 && _log_size_limit[no] > 0) {
			long    now = osMsClock();

			_log_lengths[no] += len;
			/* Other programs can also write to this file... */
			if (now - _log_check_ms[no] > 1000) {
				int     fd = fileno(_log_files[no]);
				struct stat st;

				if (fstat(fd, &st) == 0) {
					_log_lengths[no] = (long) st.st_size;
				}
				_log_check_ms[no] = now;
			}
			if (_log_lengths[no] > _log_size_limit[no]) {
				struct stat st;

				if (stat(_log_names[no], &st) == 0) {
					_log_lengths[no] = (long) st.st_size;
					if (_log_lengths[no] < _log_size_limit[no]) {
						/* Another application already truncated the file. */
						if (_logReopenFile(no) == OK)
							continue;
					}
				}
			}
			if (_log_lengths[no] > _log_size_limit[no]) {
				char   *name = _log_names[no];
				char    buf[300], buf2[300];
				int     max_no = MAX_CHUNK_FILE_NO;

				sprintf(buf, "%s.%d", name, max_no);
				unlink(buf);
				fputs("******** maximum size reached *******\n",
					  _log_files[no]);
				fclose(_log_files[no]);
				for (j = max_no - 1; j >= 0; j--) {
					sprintf(buf, "%s.%d", name, j);
					sprintf(buf2, "%s.%d", name, j + 1);
					rename(buf, buf2);
				}
				sprintf(buf, "%s.%d", name, 0);
				rename(name, buf);
				_log_files[no] = fopen(name, "w");
				_log_lengths[no] = 0;
				_log_check_ms[no] = now;
			}
		}
#endif /* !VXWORKS, i.e. UNIX */
	}

	if ((_log_flags & (OLOG_STDOUT | OLOG_STDERR | OLOG_LOGMSG))
		|| (!_log_files[0] && !_log_handler)) {
#if defined(VXWORKS)
		if (osIntrContext() || (_log_flags & OLOG_LOGMSG) != 0) {
			logMsg(buffer, 0, 0, 0, 0, 0, 0);
		}
		else
#endif /* VXWORKS */
		if (_log_flags & OLOG_STDERR) {
			fputs(msg, stderr);
		}
		else {
			fputs(msg, stdout);
			if (_log_flags & OLOG_FLUSH)
				fflush(stdout);
		}
	}

	if (NULL != _log_handler) {
		(*_log_handler) (msg);
	}

	return len;
}


/*
 * .
 */
#undef olog
oret_t
olog(const char *format, ...)
{
	va_list ap;
	char   *msg;

	va_start(ap, format);
	msg = ologFormat(NULL, format, (void *) &ap);
	va_end(ap);
	return ologWrite(msg);
}


/*
 * .
 */
#undef ologIf
oret_t
ologIf(bool_t guard, const char *format, ...)
{
	va_list ap;
	char   *msg;

	if (guard) {
		va_start(ap, format);
		msg = ologFormat(NULL, format, (void *) &ap);
		va_end(ap);
		return ologWrite(msg);
	}
	return 0;
}


/*
 * .
 */
ologhandler_t
ologSetHandler(ologhandler_t handler)
{
	ologhandler_t old = _log_handler;
	_log_handler = handler;
	return old;
}


/*
 *   .
 */
ologtimer_t
ologSetTimer(ologtimer_t timer)
{
	ologtimer_t old = _log_timer;
	_log_timer = timer;
	return old;
}


/*
 * .
 */
int
ologFlags(int flags)
{
	int old = _log_flags;
	if (flags != OLOG_GETMODE) {
		_log_flags = flags;
	}
	return old;
}


/*
 * .
 */
static int
_logKeepHandler(const char *msg)
{
	int  len = strlen(msg);

	if (_log_keep_buf) {
		if (len + _log_keep_offset + 10 < _log_keep_len) {
			strcpy(_log_keep_buf + _log_keep_offset, msg);
			_log_keep_offset += len;
		} else {
			strcpy(_log_keep_buf + _log_keep_offset, "\n**ofl**\n");
			_log_keep_more = 9;
		}
	}
	if (_log_keep_call_handler && _log_keep_handler_ptr)
		_log_keep_handler_ptr(msg);
	return 0;
}


oret_t
ologBuffering(const char *file, int kb_size)
{
	int     n;

	if (!file) {
		if (!_log_keep_file) {
			printf("please start keeping first\n");
			return ERROR;
		}
		_log_handler = _log_keep_handler_ptr;
		_log_keep_handler_ptr = NULL;
		_log_flags = _log_keep_mode;
		_log_keep_mode = 0;
		n = _log_keep_offset + _log_keep_more;
		fwrite(_log_keep_buf, 1, n, _log_keep_file);
		fclose(_log_keep_file);
		oxfree(_log_keep_buf);
		_log_keep_buf = NULL;
		_log_keep_file = NULL;
		_log_keep_len = _log_keep_more = _log_keep_offset = 0;
		return OK;
	}
	if (_log_keep_file) {
		printf("already active\n");
		return ERROR;
	}
	if (kb_size <= 0) {
		kb_size = LOG_DEF_BUFFERING_KB;
	}
	_log_keep_more = _log_keep_offset = 0;
	_log_keep_len = kb_size * 1024u;

	if (NULL == (_log_keep_buf = oxnew(char, _log_keep_len))) {
		_log_keep_len = 0;
		return ERROR;
	}

	if (NULL == (_log_keep_file = fopen(file, "w"))) {
		oxfree(_log_keep_buf);
		_log_keep_buf = 0;
		_log_keep_len = 0;
		printf("cannot open file\n");
		return ERROR;
	}

	_log_keep_mode = _log_flags;
	_log_keep_handler_ptr = _log_handler;
	_log_flags &= ~(OLOG_LOGMSG | OLOG_STDOUT | OLOG_STDERR | OLOG_FLUSH);
	_log_handler = _logKeepHandler;
	return OK;
}
