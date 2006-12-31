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

  The main routine of the hub.

*/

#include "hub-priv.h"
#include <optikus/getopt.h>
#include <optikus/ini.h>
#include <config.h>		/* for package requisites */

#include <stdio.h>
#include <stdlib.h>		/* for atoi */
#include <signal.h>
#include <string.h>

#if defined(HAVE_CONFIG_H)
#include <config.h>		/* for LINUX */
#endif /* HAVE_CONFIG_H */

#if defined(LINUX)
/*
	FIXME: It seems that readline causes memory corruption if we use
		oxrenew(x,x,old_array) i.e. calloc !
		To avoid it, we preallocate quark structures and calculate the number
		of lines in PSV files. Actually the problem is in oxrecalloc().
	FIXME: detect readline in configure.ac !
*/
#define USE_READLINE 1
#else
#define USE_READLINE 0
#endif

#if USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif /* USE_READLINE */


int     ohub_verb = 1;

char    ohub_prog_name[OHUB_MAX_NAME];
char    ohub_prog_dir[OHUB_MAX_PATH];
int     ohub_debug;

static bool_t ohub_test_mode;


/*
 *  parse command line options
 */
#define OHUB_CMD_OPTIONS "hc:d:vDp:g:O:l:"

struct ooption ohub_cmd_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"config", no_argument, NULL, 'c'},
	{"domain", no_argument, NULL, 'd'},
	{"version", no_argument, NULL, 'v'},
	{"logfile", no_argument, NULL, 'l'},
	{"pidfile", no_argument, NULL, 'P'},
	{"test-pforms", no_argument, NULL, 'D'},
	{"port", no_argument, NULL, 'p'},
	{"debug", no_argument, NULL, 'g'},
	{0, 0, 0, 0}
};

char   *ohub_cmd_ver =
	"%s - Optikus hub daemon  " PACKAGE_VERSION "\n" PACKAGE_COPYRIGHT "\n";

char   *ohub_cmd_help =
	"%s - optikus monitoring and control daemon\n"
	"\n"
	"Usage: %s [OPTIONS]...\n"
	"\n"
	"  -h / --help              - this help \n"
	"  -c / --config ini_file   - location of common config file \n"
	"                             (default: $OPTIKUS_HOME/etc/ocommon.ini or from \n"
	"                             where the program was started) \n"
	"  -l / --logfile log_file  - log to given file (default: from config) \n"
	"  -d / --domain domain     - current domain to work on (default: from config) \n"
	"  -P / --pidfile pid_file  - save pid to given file (default: from config) \n"
	"  -D / --test-pforms       - test address resolution (pforms paths) \n"
	"  -p / --port <port>       - listen at given port (default: from config or 3217) \n"
	"  -v / --version           - print version information and exit \n"
	"  -g / --debug <level>     - print debugging output \n"
	"\n"
	"Report bugs to <" PACKAGE_BUGREPORT ">\n";

oret_t
ohubParseOptions(int argc, char **argv)
{
	int     c;

	ohubNameOfPath(argv[0], ohub_prog_name);
	ohubDirOfPath(argv[0], ohub_prog_dir);
	for (;;) {
		c = ogetopt_long(argc, argv, OHUB_CMD_OPTIONS,
						 ohub_cmd_options, (int *) NULL);
		if (c == EOF)
			break;
		switch (c) {
		case 0:
			break;			/* getopt_long() returns 0 for some options */
		case 'c':
			strcpy(ohub_common_ini, ooptarg);
			break;
		case 'h':
		default:
		  USAGE:
			fprintf(stdout, ohub_cmd_help, argv[0], argv[0]);
			exit(1);
			/* NOT REACHED */
		case 'v':
			fprintf(stdout, ohub_cmd_ver, ohub_prog_name);
			ohubReadCommonConfig();
			ohubReadDomainConfig(NULL);
			ohubDumpConfig(ohub_pdomain, TRUE);
			exit(0);
			/* NOT REACHED */
		case 'd':
			strcpy(ohub_cur_domain, ooptarg);
			break;
		case 'l':
			strcpy(ohub_log_file, ooptarg);
			break;
		case 'P':
			strcpy(ohub_pid_file, ooptarg);
			break;
		case 'p':
			ohub_server_port = atoi(ooptarg);
			if (ohub_server_port <= 0)
				goto USAGE;
			break;
		case 'g':
			ohub_debug = 1;
			ohub_verb = atoi(ooptarg);
			if (ohub_verb <= 0)
				goto USAGE;
			break;
		case 'D':
			ohub_test_mode = TRUE;
			break;
		}
	}
	return OK;
}


/*
 *   The main routine.
 */
int
main(int argc, char **argv)
{
	ologFlags(OLOG_DATE | OLOG_TIME | OLOG_STDOUT | OLOG_FLUSH);
	ologOpen("ohub", NULL, 0);
	ohubParseOptions(argc, argv);
	ohubReadCommonConfig();
	ohubReadDomainConfig(NULL);

	signal(SIGINT, ohubExit);
	signal(SIGQUIT, ohubExit);
	signal(SIGTERM, ohubExit);

	if (ohub_test_mode) {
		printf("Optikus Hub " PACKAGE_VERSION " " PACKAGE_COPYRIGHT "\n");
	}
	ohubLoadDomainQuarks(ohub_pdomain);

	if (ohub_test_mode) {
		/* test address resolution */
		ohubInteractiveResolutionTest(ohub_pdomain);
	} else {
		/* daemon mode */
		signal(SIGHUP, ohubMarkReload);
		signal(SIGUSR1, ohubUpdateDebugging);
		signal(SIGPIPE, SIG_IGN);
		ohubDaemon();
	}
	ohubExit(0);
	return 0;
}


/*
 *  Interactive test.
 */
void
ohubInteractiveResolutionTest(OhubDomain_t * pdom)
{
	OhubQuark_t quark;
	OhubModule_t *pmod;
	char    path[OHUB_MAX_PATH + 4];
	char    result[OHUB_MAX_NAME + 4];

	printf("enter 'q' to exit.\n");
	for (;;) {
		/* user input */
#if USE_READLINE
		{
			char   *s = readline("enter>");
			if (NULL == s)
				break;
			if ('\0' == *s)
				continue;
			if (strlen(s) + 2 > sizeof(path)) {
				printf("line too long\n");
				continue;
			}
			strcpy(path, s);
			add_history(s);
		}
#else /* !USE_READLINE */
		printf("enter>");
		fflush(stdout);
		fgets(path, OHUB_MAX_PATH, stdin);
#endif /* USE_READLINE */
		ohubTrim(path);
		if (!*path)
			continue;
		if (0 == strcmp(path, "q"))
			break;
		/* resolve the address */
		if (ohubFindQuarkByDesc(pdom, path, &quark, result)) {
			printf("-  cannot resolve \"%s\": %s\n", path, result);
			continue;
		}
		pmod = &pdom->modules[quark.obj_ikey];
		printf("+  resolved \"%s\": ooid=%lu subj=%s mod=%s\n", path,
				quark.ooid, pmod->subject_name, pmod->nick_name);
		printf("++ %s\n", ohubQuarkToString(&quark, NULL));
	}
	printf("good bye.\n");
}
