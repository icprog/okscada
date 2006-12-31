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

  Configuration and setup of the hub.

*/

#include "hub-priv.h"
#include <optikus/ini.h>
#include <optikus/util.h>		/* for prettyData,...,strTrim */
#include <optikus/conf-mem.h>	/* for oxnew,oxazero */
#include <string.h>				/* for strcpy,strcat,strcmp */
#include <stdio.h>				/* for printf,sprintf */
#include <stdlib.h>				/* for getenv */


char    ohub_common_ini[OHUB_MAX_PATH];
char    ohub_log_file[OHUB_MAX_PATH];
int     ohub_log_limit;
char    ohub_remote_log_file[OHUB_MAX_PATH];
int     ohub_remote_log_limit;
char    ohub_pid_file[OHUB_MAX_PATH];
char    ohub_debug_file[OHUB_MAX_PATH];
char    ohub_cur_domain[OHUB_MAX_NAME];
char    ohub_ooid_cache_file[OHUB_MAX_PATH];
int     ohub_domain_qty;
int     ohub_server_port;
OhubDomain_t *ohub_all_domains;
OhubDomain_t *ohub_pdomain;

#define GETPARAM(N)                    \
    if (iniParam(ini,s=N,&v) != OK)    \
      goto NOPARAM

#define GETTABLE(N)                    \
    req = iniAskRow (ini, s=N);        \
    num = iniCount (ini, req);         \
    if (req == INIREQ_NULL || num == 0)  \
      goto NOTABLE;


/*
 *     Reading the common part of configuration.
 */

const char *ohub_common_passport = "\n"
	"[CONFIG_VARIABLES:describe]\n"
	"common_root = string(138) !0 section=DEFAULTS \n"
	"default_domain = string(31) !0 section=HUB_DEFAULTS\n"
	"verbosity = integer default=3 section=HUB_DEFAULTS\n"
	"log_file = string(138) !0 section=HUB_DEFAULTS\n"
	"log_limit = integer default=4096 section=HUB_DEFAULTS\n"
	"remote_log_file = string(138) section=HUB_DEFAULTS\n"
	"remote_log_limit = integer default=4096 section=HUB_DEFAULTS\n"
	"pid_file = string(138) default='' section=HUB_DEFAULTS\n"
	"debug_file = string(138) default='' section=HUB_DEFAULTS\n"
	"server_port = integer default=0 section=HUB_DEFAULTS\n"
	"\n"
	"[DOMAINS:describe]\n"
	"domain = string(31) !0\n" "config_file = string(140) !0\n" "\n";


oret_t
ohubReadCommonConfig(void)
{
	char    path[OHUB_MAX_PATH];
	OhubDomain_t *pdom;
	char   *s, *file;
	inival_t v, v1, v2;
	inireq_t req;
	inidb_t ini;
	int     i, num;

	/* guess OPTIKUS_HOME */
	if (getenv("OPTIKUS_HOME") == NULL) {
		strcpy(path, ohub_prog_dir);
		strcat(path, "/..");
		ohubAbsolutePath(path, path);
		setenv("OPTIKUS_HOME", path, 0);
		ohubLog(4, "guessed path: %s", path);
	}
	/* find common config file */
	if (!*ohub_common_ini) {
		sprintf(path, "$OPTIKUS_HOME/etc:%s", ohub_prog_dir);
		ohubExpandPath(path, "ocommon.ini", ohub_common_ini);
		if (!*ohub_common_ini)
			ologAbort("ERROR: cannot find \"ocommon.ini\" in \"%s\"\n", path);
	}
	/* read common config */
	ini = iniOpen(file = ohub_common_ini, ohub_common_passport, PMK_ERROR);
	if (!ini)
		ologAbort("ERROR: cannot open \"%s\"\n", ohub_common_ini);
	if (!*ohub_common_root) {
		GETPARAM("common_root");
		ohubSubstEnv(v.v_str, ohub_common_root);
#if 0
		if (!*ohub_common_root)
			ologAbort("ERROR: common root \"%s\" must exist\n",
					 ohub_common_root);
#endif
	}
	if (!*ohub_cur_domain) {
		GETPARAM("default_domain");
		strcpy(ohub_cur_domain, v.v_str);
	}
	if (!ohub_debug) {
		GETPARAM("verbosity");
		ohub_verb = v.v_int;
	}
	if (!*ohub_log_file) {
		GETPARAM("log_file");
		ohubUnrollPath(v.v_str, ohub_log_file);
	}
	if (ohub_log_limit <= 0) {
		GETPARAM("log_limit");
		ohub_log_limit = v.v_int;
	}
	if (!*ohub_remote_log_file) {
		GETPARAM("remote_log_file");
		ohubUnrollPath(v.v_str, ohub_remote_log_file);
	}
	if (ohub_remote_log_limit <= 0) {
		GETPARAM("remote_log_limit");
		ohub_remote_log_limit = v.v_int;
	}
	if (!*ohub_pid_file) {
		GETPARAM("pid_file");
		ohubUnrollPath(v.v_str, ohub_pid_file);
	}
	if (!*ohub_debug_file) {
		GETPARAM("debug_file");
		ohubUnrollPath(v.v_str, ohub_debug_file);
	}
	if (ohub_server_port == 0) {
		GETPARAM("server_port");
		ohub_server_port = v.v_int;
	}
	/* read domain map */
	GETTABLE("DOMAINS");
	ohub_all_domains = oxnew(OhubDomain_t, num);
	ohub_domain_qty = num;
	for (i = 0; i < num; i++) {
		pdom = &ohub_all_domains[i];
		iniGetRow(ini, req, &v1, &v2);
		pdom->ikey = i;
		strcpy(pdom->name, v1.v_str);
		ohubUnrollPath(v2.v_str, pdom->ini_file);
	}
	/* find current domain */
	ohub_pdomain = NULL;
	for (i = 0; i < ohub_domain_qty; i++) {
		if (!strcmp(ohub_cur_domain, ohub_all_domains[i].name)) {
			ohub_pdomain = &ohub_all_domains[i];
			break;
		}
	}
	if (!ohub_pdomain)
		ologAbort("ERROR[%s]: domain \"%s\" not found\n",
				 ohub_common_ini, ohub_cur_domain);
	/* done */
	if (!*ohub_pid_file && *ohub_log_file) {
		ohubDirOfPath(ohub_log_file, path);
		ohubMergePath(path, "ohub.pid", ohub_pid_file);
	}
	if (!*ohub_debug_file && *ohub_log_file) {
		ohubDirOfPath(ohub_log_file, path);
		ohubMergePath(path, "ohub.dbg", ohub_debug_file);
	}
	return OK;
  NOPARAM:
	ologAbort("ERROR[%s]: parameter %s not specified\n", file, s);
	return ERROR;
  NOTABLE:
	ologAbort("ERROR[%s]: section %s bad or absent\n", file, s);
	return ERROR;
}

/*
 *   Reading the domain specific configuration.
 */

const char *ohub_domain_passport = "\n"
	"[CONFIG_VARIABLES:describe]\n"
	"domain_name = string(31) !0 section=DEFAULTS \n"
	"module_root = string(138) !0 section=DEFAULTS \n"
	"ooid_cache = string(138) default='' section=DEFAULTS\n"
	"\n"
	"[AGENTS:describe]\n"
	"agent = string(15) !0 \n"
	"url = string(39) !0 \n"
	"enable = bool !0 \n"
	"\n"
	"[SUBJECTS:describe]\n"
	"subject = string(15) !0 \n"
	"agent = string(15) !0 \n"
	"arch = string(15) !0 \n"
	"symtable = string(15) !0 \n"
	"availability = enum short !0 { off=0, temporary=1, permanent=2 } \n"
	"\n"
	"[MODULES:describe]\n"
	"subject = string(15) !0 \n"
	"nickname = string(15) !0 \n"
	"unit# = integer unsigned !0 \n"
	"make = bool !0 \n"
	"mod_file = string(138) !0 \n" "quark_file = string(138) !0 \n" "\n";

oret_t
ohubReadDomainConfig(OhubDomain_t * pdom)
{
	OhubAgent_t *pagent;
	OhubSubject_t *psubject;
	OhubModule_t *pmod;
	char   *s, *file;
	inival_t v, v1, v2, v3, v4, v5, v6;
	inireq_t req;
	inidb_t ini;
	int     i, j, num;

	if (NULL == pdom) {
		pdom = ohub_pdomain;
		if (NULL == pdom)
			return ERROR;
	}

	/* read domain config file */
	ini = iniOpen(file = pdom->ini_file, ohub_domain_passport, PMK_ERROR);
	if (!ini)
		ologAbort("ERROR: cannot open \"%s\"\n", pdom->ini_file);
	GETPARAM("domain_name");
	strcpy(pdom->name, v.v_str);
	GETPARAM("module_root");
	ohubUnrollPath(v.v_str, pdom->mod_root);
	if (!*ohub_ooid_cache_file) {
		GETPARAM("ooid_cache");
		if (v.v_str && *v.v_str)
			ohubMergePath(pdom->mod_root, v.v_str, ohub_ooid_cache_file);
		else
			ohub_ooid_cache_file[0] = 0;
	}

	/* read agent map */
	GETTABLE("AGENTS");
	pdom->agents = oxnew(OhubAgent_t, num);
	pdom->agent_qty = num;
	for (i = 0; i < num; i++) {
		pagent = &pdom->agents[i];
		iniGetRow(ini, req, &v1, &v2, &v3, &v4, &v5);
		pagent->ikey = i;
		strcpy(pagent->agent_name, v1.v_str);
		strcpy(pagent->url, v2.v_str);
		strTrim(pagent->url);
		pagent->enable = v3.v_bool;
		if (0 == strncmp(pagent->url, "olang://", 8)) {
			pagent->proto = OHUB_PROTO_OLANG;
		} else {
			pagent->proto = OHUB_PROTO_MAX;
			pagent->enable = FALSE;
			ologAbort("ERROR[%s]: unknown protocol \"%s\" in agent \"%s\"",
					 file, pagent->url, pagent->agent_name);
		}
		pagent->nsubj_p = NULL;
	}

	/* read subject map */
	GETTABLE("SUBJECTS");
	pdom->subjects = oxnew(OhubSubject_t, num);
	pdom->subject_qty = num;
	for (i = 0; i < num; i++) {
		psubject = &pdom->subjects[i];
		iniGetRow(ini, req, &v1, &v2, &v3, &v4, &v5);
		psubject->ikey = i;
		strcpy(psubject->subject_name, v1.v_str);
		strcpy(psubject->agent_name, v2.v_str);
		strcpy(psubject->arch_name, v3.v_str);
		strcpy(psubject->symtable_name, v4.v_str);
		psubject->availability = v5.v_enum;
		psubject->subject_state[0] = 0;
		psubject->pagent = NULL;
		oxazero(psubject->waiters);
		psubject->waiters_qty = 0;
	}

	/* read module map */
	GETTABLE("MODULES");
	pdom->modules = oxnew(OhubModule_t, num);
	pdom->module_qty = num;
	for (i = 0; i < num; i++) {
		pmod = &pdom->modules[i];
		iniGetRow(ini, req, &v1, &v2, &v3, &v4, &v5, &v6);
		pmod->ikey = i;
		strcpy(pmod->subject_name, v1.v_str);
		strcpy(pmod->nick_name, v2.v_str);
		pmod->unit_no = v3.v_int;
		pmod->make_flag = v4.v_bool;
		ohubMergePath(pdom->mod_root, v5.v_str, pmod->mod_file);
		ohubMergePath(pdom->mod_root, v6.v_str, pmod->quark_file);
		pmod->record_qty = 0;
		pmod->records = NULL;
		pmod->glob_heap = NULL;
		pmod->glob_heap_len = pmod->glob_heap_size = 0;
		pmod->path_heap = NULL;
		pmod->path_heap_len = pmod->path_heap_size = 0;
		pmod->psubject = NULL;
		for (j = 0; j < SEG_MAX_NO; j++)
			pmod->segment_addr[j] = -1;
	}

	/* fix up references */
	for (i = 0; i < pdom->module_qty; i++) {
		pmod = &pdom->modules[i];
		pmod->psubject = NULL;
		for (j = 0; j < pdom->subject_qty; j++) {
			psubject = &pdom->subjects[j];
			if (!strcmp(pmod->subject_name, psubject->subject_name)) {
				pmod->psubject = psubject;
				break;
			}
		}
		if (NULL == pmod->psubject)
			ologAbort("ERROR[%s]: unknown subject \"%s\" in module \"%s\"",
					 file, pmod->subject_name, pmod->nick_name);
	}
	for (i = 0; i < pdom->subject_qty; i++) {
		psubject = &pdom->subjects[i];
		psubject->pagent = NULL;
		for (j = 0; j < pdom->agent_qty; j++) {
			pagent = &pdom->agents[j];
			if (!strcmp(psubject->agent_name, pagent->agent_name)) {
				psubject->pagent = pagent;
				break;
			}
		}
		if (NULL == psubject->pagent)
			ologAbort("ERROR[%s]: unknown agent \"%s\" in subject \"%s\"",
					 file, psubject->agent_name, psubject->subject_name);
	}
	for (i = 0; i < pdom->agent_qty; i++) {
		pagent = &pdom->agents[i];
		pagent->pdomain = pdom;
	}

	/* done */
	iniClose(ini);
	return OK;
  NOPARAM:
	ologAbort("ERROR[%s]: parameter %s not specified\n", file, s);
	return ERROR;
  NOTABLE:
	ologAbort("ERROR[%s]: section %s bad or absent\n", file, s);
	return ERROR;
}


/*
 *  Debugging dump of the configuration variables.
 */

#define YN(v)   ((v)?"yes":"no")

oret_t
ohubDumpConfig(OhubDomain_t * pdom, bool_t verbose)
{
	OhubAgent_t *pagent;
	OhubSubject_t *psubject;
	OhubModule_t *pmod;
	char   *fmt;
	int     i;

	/* common */
	printf("\n");
	printf("cfg: common_root     = \"%s\"\n", ohub_common_root);
	printf("cfg: common_ini      = \"%s\"\n", ohub_common_ini);
	printf("cfg: log_file        = \"%s\"\n", ohub_log_file);
	printf("cfg: cur_domain_ref  = \"%s\"\n", ohub_cur_domain);
	/* domain */
	if (pdom == NULL) {
		printf("cfg: domain_name     = null\n");
		return ERROR;
	}
	printf("cfg: domain_ini      = \"%s\"\n", pdom->ini_file);
	printf("cfg: domain_name     = \"%s\"\n", pdom->name);
	if (!verbose)
		return OK;
	/* agents */
	printf("[AGENTS(%s)]\n", pdom->name);
	fmt = "%-7s %-14s %-6s %-3s";
	prettyHeader(fmt, "agent", "url", "proto", "ena");
	for (i = 0, pagent = pdom->agents; i < pdom->agent_qty; i++, pagent++) {
		const char *proto;
		switch (pagent->proto) {
		case OHUB_PROTO_OLANG:	proto = "olang";	break;
		default:				proto = "?";		break;
		}
		prettyData(fmt, pagent->agent_name, pagent->url, proto, YN(pagent->enable));
	}
	prettyFooter(fmt);
	/* subjects */
	printf("[SUBJECTS(%s)]\n", pdom->name);
	fmt = "%-7s %-7s %-10s %-6s %-7s";
	prettyHeader(fmt, "subject", "agent", "arch", "symtab", "state");
	for (i = 0, psubject = pdom->subjects; i < pdom->subject_qty; i++, psubject++)
		prettyData(fmt, psubject->subject_name, psubject->agent_name,
				   psubject->arch_name, psubject->symtable_name,
				   psubject->subject_state);
	prettyFooter(fmt);
	/* modules */
	printf("[MODULES(%s)]\n", pdom->name);
	fmt = "%-7s %-8s %2d %-32s";
	prettyHeader(fmt, "subject", "nickname", "unit", "quark_file");
	for (i = 0, pmod = pdom->modules; i < pdom->module_qty; i++, pmod++)
		prettyData(fmt, pmod->subject_name, pmod->nick_name,
				   pmod->unit_no, pmod->quark_file);
	prettyFooter(fmt);
	return OK;
}
