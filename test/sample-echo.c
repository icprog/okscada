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

  Test utility that echoes sent messages.

  TO BE REMOVED.

*/

#include <optikus/watch.h>
#include <optikus/lang.h>
#include <optikus/log.h>
#include <optikus/getopt.h>
#include <optikus/util.h>		/* for strTrim,osMsClock */
#include <optikus/conf-mem.h>	/* for oxazero */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>			/* for FIONREAD */


static void    echoExit(int unused);
static bool_t  echoLoop(void);
static void    echoInput(void);
static void    echoReportState(void);

#define SERVER		"localhost"
#define SUBJ		"sample"
#define TIMEOUT		1000

#define PROMPT    "echo>"
#define EQ(a,b)   (0 == strcmp((a),(b)))

static char   *echo_serv = SERVER;
static char   *echo_subj = SUBJ;
static bool_t  echo_online;

static char    echo_last_input[80];

static int     echo_interval = 100;
static long    echo_cur;
static long    echo_next;
static int     echo_prompt;

/* Help message */
static char   *echo_help_str =
	"\n"
	"help: \n"
	"help|h|?          - get this help \n"
	".                 - repeat last input\n"
	"quit|exit|q|e     - quit the program\n"
	"stat              - print status of subjects\n"
	"exec <params>     - execute unix command\n"
	"v <number>        - set verbosity level\n"
	"info|i            - program information\n"
	;


/*
 * Test if user hit a key on standard input.
 */
bool_t
userHitKey(void)
{
	int   nchars = 0;
	ioctl(0, FIONREAD, &nchars);
	return (nchars != 0);
}


/*
 * Parse user input.
 */
static void
echoParseLine(char *s)
{
	static char c[80], b[80];
	static char p1[80], p2[80], p3[80], p4[80], p5[80];
	static char p6[80], p7[80], p8[80], p9[80], p10[80], p11[80], p12[80];
	int     good_cmd = 1;

	oxazero(c);
	oxazero(b);
	oxazero(p1);
	oxazero(p2);
	oxazero(p3);
	oxazero(p4);
	oxazero(p5);
	oxazero(p6);
	oxazero(p7);
	oxazero(p8);
	oxazero(p9);
	oxazero(p10);
	oxazero(p11);
	oxazero(p12);

	sscanf(s, " %s ", c);
	if (EQ(c, "."))
		strcpy(s, echo_last_input), printf(".>%s\n", s);
	oxazero(c);
	sscanf(s, " %s %s %s %s %s %s %s %s %s %s %s %s %s ",
		   c, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
	if (!strcspn(s, " \t\r\n"))
		return;

	if (EQ(c, "help") || EQ(c, "?") || EQ(c, "h")) {
		printf(echo_help_str);
	}
	else if (EQ(c, "quit") || EQ(c, "exit") || EQ(c, "q") || EQ(c, "e")) {
		echoExit(0);
	}
	else if (EQ(c, "stat")) {
		echoReportState();
	}
	else if (EQ(c, "info") || EQ(c, "i")) {
		printf("%-16s: %s\n", "state", echo_online ? "online" : "offline");
	}
	else if (EQ(c, "v")) {
		owatch_verb = atoi(p1);
	}
	else if (EQ(c, "exec")) {
		sprintf(b, "%s %s %s %s %s %s %s %s %s %s %s %s",
				p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12);
		strTrim(b);
		olog("execute: '%s'", b);
		system(b);
	}
	else {
		good_cmd = 0;
		printf("wrong command, use ? for help\n");
	}
	if (good_cmd)
		strcpy(echo_last_input, s);
}


/*
 * Handle echo packets.
 */
static oret_t
echoMsgHandler(ooid_t od, const char *src, const char *dest,
				int klass, int type, void *data, int len, long param)
{
	if (type == OLANG_MSG_TYPE_ECHO)
		olog("ECHO: %s", (char *) data);
	else
		olog("invalid message %d", type);
	echo_prompt = 1;
	return OK;
}


/*
 * Initialize the program.
 */
#define LONG_OPTIONS "hs:p:"

struct ooption echo_cmd_options[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "server", no_argument, NULL, 's' },
	{ "peer", no_argument, NULL, 'p' },
	{ 0, 0, 0, 0 }
};

char   *echo_usage_string =
	"sample-echo\n"
	"usage: %s \n"
	"\t [-h|--help]              - get this help \n"
	;


int
main(int argc, char **argv)
{
	int     c, err;

	for (;;) {
		c = ogetopt_long(argc, argv, LONG_OPTIONS,
						 echo_cmd_options, (int *) NULL);
		if (c == EOF)
			break;
		switch (c) {
		case 0:
			break;		/* getopt_long() returns 0 for some options. */
		case 's':
			echo_serv = ooptarg;
			if (NULL == echo_serv || '\0' == *echo_serv)
				goto USAGE;
			break;
		case 'p':
			echo_subj = ooptarg;
			if (NULL == echo_subj || '\0' == *echo_subj)
				goto USAGE;
			break;
		case 'h':
			goto USAGE;
		default:
		  USAGE:
			fprintf(stderr, echo_usage_string, argv[0]);
			return 1;
		}
	}

	printf("echo starts\n");

	ologOpen("echo", NULL, 0);
	ologFlags(OLOG_TIME | OLOG_MSEC | OLOG_STDOUT | OLOG_FLUSH);

	if (owatchSetupConnection(echo_serv, "echo", 0, &err) == ERROR
			&& err != OWATCH_ERR_PENDING) {
		olog("initialization failed");
		return 1;
	}

	if (owatchAddMsgHandler(OLANG_MSG_CLASS_ECHO, echoMsgHandler, 0) != OK) {
		olog("cannot register message handler");
		return 1;
	}

	signal(SIGINT, echoExit);
	signal(SIGQUIT, echoExit);
	signal(SIGTERM, echoExit);
	signal(SIGPIPE, SIG_IGN);

	echo_prompt = 1;
	echo_next = echo_cur = osMsClock();

	while (echoLoop()) {
		if (echo_prompt) {
			printf(PROMPT);
			fflush(stdout);
			echo_prompt = 0;
		}
		if (userHitKey()) {
			echoInput();
		}
	}

	echoExit(0);
	return 0;
}


/*
 * Print state of subjects.
 */
static void
echoReportState()
{
	char all[1024];		/* FIXME: magic size */
	char *s, *e;

	if (owatchGetSubjectList(all, sizeof(all)) == ERROR) {
		olog("too many subjects");
		return;
	}
	s = e = all;
	while (*s) {
		while (*e && *e != ' ')
			e++;
		*e = 0;
		printf("%-16s - %s\n", s, owatchGetAlive(s) ? "OK" : "Offline");
		s = e+1;
	}
	echo_prompt = 1;
}


/*
 * Read input string from user.
 */
static void
echoInput(void)
{
	char line[256] = {};

	fgets(line, sizeof(line)-1, stdin);
	strTrim(line);
	echoParseLine(line);
	echo_prompt = 1;
}


/*
 * The main control loop of the program.
 * Returns TRUE, if execution continues.
 * Returns FALSE, if program should be terminated.
 */
static bool_t
echoLoop(void)
{
	echo_cur = osMsClock();
	while (echo_next - echo_cur <= 0)
		echo_next += echo_interval;
	owatchWork(echo_next - echo_cur);

	if (echo_online && !owatchGetAlive(echo_subj)) {
		olog("offline");
		echo_prompt = 1;
		echo_online = FALSE;
	}
	else if (!echo_online && owatchGetAlive(echo_subj)) {
		olog("online");
		echo_prompt = 1;
		echo_online = TRUE;
	}
	return TRUE;
}


/*
 * Exit the program.
 * The argument is only needed to match signal handler prototype.
 */
static void
echoExit(int unused)
{
	puts("exit echo");
	exit(0);
}
