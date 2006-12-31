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

  Internal declarations for the hub.

*/

#ifndef OPTIKUS_HUB_PRIV_H
#define OPTIKUS_HUB_PRIV_H

#include <optikus/log.h>
#include <optikus/tree.h>
#include <optikus/lang.h>

OPTIKUS_BEGIN_C_DECLS


#define OHUB_MAX_PATH 140
#define OHUB_MAX_NAME 40

/*					Main					*/

extern char *ohub_cmd_ver;
extern char *ohub_cmd_help;
extern char ohub_prog_name[];
extern char ohub_prog_dir[];

struct OhubDomain_st;

void   ohubInteractiveResolutionTest(struct OhubDomain_st *pdom);

extern int ohub_verb;
extern int ohub_debug;

#define ohubLog(verb,args...)	(ohub_verb >= (verb) ? olog(args) : -1)


/*			Network interface with watchers			*/

struct OhubNetWatch_st;
struct OhubMonAccum_st;
struct OhubAgent_st;

extern bool_t ohub_watchers_enable;

oret_t  ohubWatchInitNetwork(int serv_port);
oret_t  ohubWatchersShutdownNetwork(void);
oret_t  ohubWatchSendPacket(struct OhubNetWatch_st *pnwatch, int kind,
							int type, int len, void *data);
oret_t  ohubWatchHandleProtocol(struct OhubNetWatch_st *pnwatch, int kind,
								int type, int len, char *buf);
void    ohubWatchBackgroundTask(void);
int     ohubWatchPrepareEvents(fd_set * set, int nfds);
int     ohubWatchHandleEvents(fd_set * set, int num);
void    ohubWatchLazyShutdown(void);
oret_t  ohubWatchGetById(ushort_t id, struct OhubNetWatch_st **nwatch_pptr,
						 const char *who);
bool_t  ohubWatchNeedsRotor(struct OhubNetWatch_st *pnwatch);
bool_t  ohubWatchAnnounceBlocked(struct OhubNetWatch_st *pnwatch);
int     ohubWatchSendQueueGap(struct OhubNetWatch_st *pnwatch);
struct  OhubMonAccum_st *ohubWatchGetAccum(struct OhubNetWatch_st *pnwatch);
int     ohubHandleDataMonReply(struct OhubAgent_st *pagent, int kind,
								int type, int len, char *buf);

/*				Subject network				*/

extern bool_t ohub_subjs_enable;

struct OhubNetSubj_st;
struct OhubAgent_st;
struct OhubUnregAccum_st *ohubSubjGetUnregAccum(struct OhubNetSubj_st *pnsubj);

oret_t  ohubSubjInitNetwork(void);
oret_t  ohubSubjShutdownNetwork(void);
struct OhubNetSubj_st *ohubSubjOpen(const char *subj_url,
									struct OhubAgent_st *pagent);
oret_t  ohubSubjDisconnect(struct OhubNetSubj_st *pnsubj);
oret_t  ohubSubjClose(struct OhubNetSubj_st *pnsubj);
oret_t  ohubSubjSendPacket(struct OhubNetSubj_st *pnsubj, int kind,
							int type, int len, void *data);
oret_t  ohubSubjHandleProtocol(struct OhubNetSubj_st *pnsubj, int kind,
								int type, int len, char *buf);
void    ohubSubjBackgroundTask(void);
int     ohubSubjPrepareEvents(fd_set * set, int nfds);
int     ohubSubjHandleEvents(fd_set * set, int num);
void    ohubSubjLazyShutdown(void);
bool_t  ohubSubjNeedsRotor(struct OhubNetSubj_st *pnsubj);
oret_t  ohubSubjTrigger(struct OhubNetSubj_st *pnsubj);
int     ohubSubjSendQueueGap(struct OhubNetSubj_st *pnsubj);

/*					Quark operations						*/

#define OHUB_MAX_MUL   4192
#define OHUB_MAX_KEY   16384

struct OhubModule_st;
struct OhubDomain_st;
struct OhubAgent_st;
struct OhubSubject_st;

typedef struct OhubQuark_st
{
	int     ukey;
	int     ikey;
	long    adr;
	long    off;
	long    off0;
	int     len;
	char    ptr;
	char    type;
	char    seg;
	char    spare1;
	int     obj_ikey;
	int     bit_off;
	int     bit_len;
	int     dim[4];
	int     coef[4];
	int     glob_ikey;
	long    glob_adr;
	int     ind[4];
	ooid_t   ooid;
	long    phys_addr;
	int     next_no;
	char    glob_name[OHUB_MAX_PATH];
	char    path[OHUB_MAX_PATH];
} OhubQuark_t;

typedef struct OhubRecord_st
{
	int     ukey;
	int     ikey;
	long    adr;
	long    off;
	int     len;
	char    ptr;
	char    type;
	char    seg;
	char    spare1;
	int     obj_ikey;
	int     bit_off;
	int     bit_len;
	short   dim[4];
	int     coef[4];
	int     glob_ikey;
	long    glob_adr;
	int     next_no;
	int     glob_off;
	int     path_off;
} OhubRecord_t;

oret_t  ohubLoadModuleQuarks(struct OhubModule_st *pmod);
oret_t  ohubLoadDomainQuarks(struct OhubDomain_st *pdom);
int     ohubFindQuarkByDesc(struct OhubDomain_st *pdom,
							 const char *spath,
							 OhubQuark_t * presult, char *sresult);
bool_t  ohubRefineQuarkAddress(struct OhubDomain_st *pdom, OhubQuark_t * quark_p);
int     ohubType2Len(char type);
char   *ohubQuarkToString(OhubQuark_t * pch, char *pbuf);
oret_t  ohubRecordToQuark(struct OhubDomain_st *pdom, OhubRecord_t * prec,
						OhubQuark_t * quark_p);

/*					Serving data info requests					*/

oret_t  ohubFlushInfoCache(void);
oret_t  ohubFindCachedInfoByDesc(const char *desc, OhubQuark_t * quark_p);
oret_t  ohubFindCachedInfoByOoid(ooid_t ooid, OhubQuark_t * quark_p);
oret_t  ohubGetInfoByDesc(const char *desc, OhubQuark_t * quark_p,
							char *sresult);
oret_t  ohubGetInfoByOoid(ooid_t ooid, OhubQuark_t * quark_p, char *sresult);
oret_t  ohubHandleInfoReq(struct OhubNetWatch_st *pnwatch, int kind, int type,
							int len, char *buf);
oret_t  ohubHandleMonGroupReq(struct OhubNetWatch_st *pnwatch, const char *url,
								int kind, int type, int len, char *buf);
int     ohubPackInfoReply(char *reply, int p, oret_t rc, OhubQuark_t * quark_p,
							char *sresult);

/*					Node state					*/

#define SEG_NAME_2_NO(c)    ((int)(char)(c)-32)
#define SEG_MAX_NO          (128-32)
#define SEG_IS_VALID(c)     ((char)(c)>=(char)32 && (char)(c)<(char)127)

struct OhubAgent_st;

oret_t  ohubSendSubjects(struct OhubNetWatch_st *pnwatch, bool_t initial,
						   const char *url);
oret_t  ohubHandleSegments(struct OhubAgent_st *pagent, int kind,
							 int type, int len, char *buf);
oret_t  ohubSetAgentState(struct OhubAgent_st *pagent, const char *state);
oret_t  ohubHandleTriggerSubject(struct OhubNetWatch_st *pnwatch,
									ushort_t watch_id, const char *url,
									int kind, int type, int len, char *buf);
oret_t  ohubHandleTriggerTimeouts(void);
oret_t  ohubClearSubjectWaiters(struct OhubNetWatch_st *pnwatch, const char *url);

/*					Configuration					*/

typedef enum
{
	OHUB_PROTO_OLANG  = 1,
	OHUB_PROTO_MAX
} OhubProtocol_t;

typedef struct OhubAgent_st
{
	int     ikey;
	char    agent_name[OHUB_MAX_NAME];
	char    agent_state[OHUB_MAX_NAME];
	char    url[OHUB_MAX_NAME];
	OhubProtocol_t proto;
	bool_t  enable;
	struct OhubDomain_st *pdomain;
	struct OhubNetSubj_st *nsubj_p;
	int     cur_reg_count;
	int     cur_write_count;
} OhubAgent_t;

typedef enum
{
	OHUB_AVAIL_OFF = 0,
	OHUB_AVAIL_TEMP = 1,
	OHUB_AVAIL_PERM = 2,
	OHUB_AVAIL_MAX
} OhubAvailability_t;

#define OHUB_MAX_SUBJECT_WAITERS  8

typedef struct OhubSubjectWaiter_st
{
	struct OhubNetWatch_st *pnwatch;
	ulong_t op;
	long    watchdog;
} OhubSubjectWaiter_t;

typedef struct OhubSubject_st
{
	int     ikey;
	char    subject_name[OHUB_MAX_NAME];
	char    agent_name[OHUB_MAX_NAME];
	char    arch_name[OHUB_MAX_NAME];
	char    symtable_name[OHUB_MAX_NAME];
	char    subject_state[OHUB_MAX_NAME];
	OhubAvailability_t availability;
	OhubAgent_t *pagent;
	OhubSubjectWaiter_t waiters[OHUB_MAX_SUBJECT_WAITERS];
	int     waiters_qty;
} OhubSubject_t;

typedef struct OhubModule_st
{
	int     ikey;
	char    subject_name[OHUB_MAX_NAME];
	char    nick_name[OHUB_MAX_NAME];
	int     unit_no;
	bool_t  make_flag;
	char    mod_file[OHUB_MAX_PATH];
	char    quark_file[OHUB_MAX_PATH];
	int     record_qty;
	OhubRecord_t *records;
	char   *glob_heap;
	int     glob_heap_len;
	int     glob_heap_size;
	char   *path_heap;
	int     path_heap_len;
	int     path_heap_size;
	tree_t  quarks_byname;
	tree_t  globals_byname;
	long    segment_addr[SEG_MAX_NO];
	OhubSubject_t *psubject;
} OhubModule_t;

typedef struct OhubDomain_st
{
	int     ikey;
	char    name[OHUB_MAX_NAME];
	char    ini_file[OHUB_MAX_PATH];
	char    mod_root[OHUB_MAX_PATH];
	int     module_qty;
	OhubModule_t *modules;
	int     subject_qty;
	OhubSubject_t *subjects;
	int     agent_qty;
	OhubAgent_t *agents;
} OhubDomain_t;

extern int ohub_domain_qty;
extern OhubDomain_t *ohub_all_domains;
extern OhubDomain_t *ohub_pdomain;

extern char ohub_common_ini[];
extern char ohub_log_file[];
extern int  ohub_log_limit;
extern char ohub_remote_log_file[];
extern int  ohub_remote_log_limit;
extern char ohub_cur_domain[];
extern char ohub_ooid_cache_file[];
extern char ohub_pid_file[];
extern char ohub_debug_file[];

extern int ohub_server_port;

oret_t  ohubReadCommonConfig(void);
oret_t  ohubReadDomainConfig(OhubDomain_t * pdomain);
oret_t  ohubDumpConfig(OhubDomain_t * pdomain, bool_t verbose);

/*					OOIDs					*/

oret_t  ohubInitOoidFactory(const char *file_name);
oret_t  ohubExitOoidFactory(const char *file_name);
ooid_t ohubFindOoidByName(const char *path, bool_t can_add);
const char *ohubFindNameByOoid(ooid_t ooid, char *path_buf);
ooid_t ohubMakeStaticOoid(OhubDomain_t * pdom, OhubQuark_t * pch);

/*					Utilities					*/

extern char ohub_path_sep;
extern char ohub_common_root[];

char  * ohubDirOfPath(const char *src, char *dst);
char  * ohubNameOfPath(const char *src, char *dst);
char  * ohubSubstEnv(const char *src, char *dst);
char  * ohubAbsolutePath(const char *src, char *dst);
char  * ohubMergePath(const char *dir, const char *name, char *dst);
char  * ohubUnrollPath(const char *src, char *dst);
char  * ohubExpandPath(const char *path, const char *name, char *buf);
char  * ohubShortenPath(const char *src, char *buf, int limit);
char  * ohubTrimLeft(char *buf);
char  * ohubTrimRight(char *buf);
char  * ohubTrim(char *buf);
oret_t  ohubRotate(bool_t enable, char type, short len, char *buf);

/*					Proper					*/

extern int ohub_spin;
extern int ohub_dbg[];
extern char ohub_hub_desc[];

oret_t  ohubInit(void);
void    ohubExit(int spare);
void    ohubDaemon(void);
void    ohubMarkReload(int spare);
void    ohubUpdateDebugging(int spare);
oret_t  ohubAgentHandleIncoming(OhubAgent_t * pagent,
								int kind, int type, int len, char *buf);
oret_t  ohubWatchHandleIncoming(struct OhubNetWatch_st *pnwatch,
								ushort_t watch_id, const char *watch_url,
								int kind, int type, int len, char *buf);

/*					Monitoring					*/

typedef union
{
	int     v_int;
	long    v_long;
	float   v_float;
	double  v_double;
	short   v_short;
	char    v_char;
	uchar_t v_uchar;
	ushort_t v_ushort;
	uint_t  v_uint;
	ulong_t v_ulong;
	int     v_bool;
	int     v_enum;
	char   *v_str;
	void   *v_ptr;
	ulong_t v_proc;
} ovariant_t;

typedef struct
{
	char    type;
	char    undef;
	short   len;
	long    time;
	ovariant_t v;
} oval_t;

#define OHUB_MON_ACCUM_SIZE  2800
#define OHUB_MON_ACCUM_TTL   10

typedef struct OhubMonAccum_st
{
	char    buf[OHUB_MON_ACCUM_SIZE];
	int     off;
	int     qty;
	long    start_ms;
} OhubMonAccum_t;

#define OHUB_UNREG_ACCUM_SIZE  200
#define OHUB_UNREG_ACCUM_TTL   10

typedef struct OhubUnregAccum_st
{
	long    buf[OHUB_UNREG_ACCUM_SIZE + 2];
	int     qty;
	long    start_ms;
} OhubUnregAccum_t;

oret_t  ohubInitMonitoring(void);
oret_t  ohubExitMonitoring(void);
oret_t  ohubHandleMonRegistration(struct OhubNetWatch_st *pnwatch,
									const char *url, int kind,
									int type, int len, char *buf);
int     ohubMonRemoveWatch(struct OhubNetWatch_st *pnwatch, const char *url);
int     ohubUpdateMonAddresses(OhubDomain_t * pdom, int *mods, int mod_qty);
int     ohubSecureMoreMonitors(OhubDomain_t * pdom);
oret_t  ohubHandleMonitoring(int kind, int type, int len, char *buf,
							   OhubAgent_t * pagent);
oret_t  ohubHandleMonComboRequest(struct OhubNetWatch_st *pnwatch,
									const char *url, int kind,
									int type, int len, char *buf);
oret_t  ohubMonGetValue(ooid_t ooid, oval_t * pval);
int     ohubMonRenewWatch(struct OhubNetWatch_st *pnwatch, const char *url);
oret_t  ohubMonFlushMonAccum(struct OhubNetWatch_st *pnwatch,
								OhubMonAccum_t * pacc);
oret_t  ohubMonFlushUnregAccum(struct OhubNetSubj_st *pnsubj,
								OhubUnregAccum_t * pacc);

/*					Data R/W					*/

oret_t  ohubHandleOoidWrite(struct OhubNetWatch_st *pnwatch,
							ushort_t watch_id, const char *watch_url,
							int kind, int type, int len, char *buf);
oret_t  ohubHandleDataWrite(OhubAgent_t * pagent,
							int kind, int type, int len, char *buf);
oret_t  ohubHandleOoidRead(struct OhubNetWatch_st *pnwatch,
							ushort_t watch_id, const char *watch_url,
							int kind, int type, int len, char *buf);
oret_t  ohubHandleDataRead(OhubAgent_t * pagent,
							int kind, int type, int len, char *buf);
int     ohubCancelAgentData(OhubAgent_t * pagent);
int     ohubCancelWatchData(struct OhubNetWatch_st *pnwatch, const char *url);
int     ohubUpdateDataAddresses(OhubDomain_t * pdom, int *mods, int mod_qty);
int     ohubSecureMoreWrites(OhubDomain_t * pdom);
oret_t  ohubInitData(void);
oret_t  ohubExitData(void);


/*					Messages					*/

typedef enum {
	OHUB_MSGNODE_ADD	= 1,
	OHUB_MSGNODE_REMOVE	= 0,
	OHUB_MSGNODE_FLUSH	= 2
} OhubNodeAction;

oret_t  ohubInitMessages(void);
oret_t  ohubCleanupMessages(void);
oret_t  ohubRegisterMsgNode(struct OhubNetWatch_st *pnwatch,
							OhubSubject_t *psubject, OhubNodeAction action,
							ushort_t node_id, const char *node_name);
oret_t  ohubHandleMsgClassAct(struct OhubNetWatch_st * pnwatch,
							OhubAgent_t * pagent,	/* FIXME: asymmetric ! */
							int kind, int type, int len, char *buf,
							const char *url);
oret_t  ohubHandleMsgSend(struct OhubNetWatch_st * pnwatch,
							OhubAgent_t * pagent,	/* FIXME: asymmetric ! */
							int kind, int type, int len, char *buf,
							const char *url);
oret_t  ohubHandleMsgWaterMark(struct OhubNetWatch_st * pnwatch,
							OhubAgent_t * pagent,	/* FIXME: asymmetric ! */
							int kind, int type, int len, char *buf,
							const char *url);


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_HUB_PRIV_H */
