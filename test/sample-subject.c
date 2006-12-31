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

  An example demostration of the watching API in action.

*/

#include <optikus/subj.h>
#include <optikus/lang.h>
#include <optikus/time.h>
#include <optikus/log.h>
#include <optikus/util.h>
#include <optikus/conf-net.h>	/* for OUHTONS,... */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>		/* for sin */
#include <time.h>
#include <sys/time.h>	/* for gettimeofday */


#define SAMPLE_SUBJECT	"sample"
#define SAMPLE_SYMTABLE	"."
#define SAMPLE_MODULE	SAMPLE_SYMTABLE

#define MAX_SHOW_MSG_WORD	16

int max_show_msg_word = MAX_SHOW_MSG_WORD;

int print_all;
int msg2_count;

extern long osubj_sel_address;
extern int osubj_alloc_id_qty[];
extern int osubj_mon_buf_size;

signed char v_s8[1024];
unsigned char v_u8[1024];
signed short v_s16[1024];
unsigned short v_u16[1024];
signed int v_s32[1024];
unsigned int v_u32[1024];

char    v_char[1024];
uchar_t v_uchar[1024];
short   v_short[1024];
ushort_t v_ushort[1024];
int     v_int[1024];
uint_t  v_uint[1024];
long    v_long[1024];
ulong_t v_ulong[1024];
float   v_float[1024];
double  v_double[1024];
bool_t  v_bool[1024];

int     stress_test1;
int     st1_verb;

char    c_char[1024];
uchar_t c_uchar[1024];
short   c_short[1024];
ushort_t c_ushort[1024];
int     c_int[1024];
uint_t  c_uint[1024];
long    c_long[1024];
ulong_t c_ulong[1024];
float   c_float[1024];
double  c_double[1024];
bool_t  c_bool[1024];
char    c_string[1024][8];

int n_val;
char    n_char[1024];
uchar_t n_uchar[1024];
short   n_short[1024];
ushort_t n_ushort[1024];
int     n_int[1024];
uint_t  n_uint[1024];
long    n_long[1024];
ulong_t n_ulong[1024];
float   n_float[1024];
double  n_double[1024];
bool_t  n_bool[1024];
char    n_string[1024][8];

int     param1;
char    param2;
char    param3[40];
float   param4;
double  param5;
int     param6 = 1;
short   param7 = 2;
char    param8[40] = "nothing";

int     arr1[8];
short   arr2[8];
int     arr3[100];

float   num1 = 0.3;
double  num2 = 0.4;

char	strinc;
short	strinh;
int		strini;
float	strinf;
double	strind;
char	strins[16];
char	strout[64];

struct
{
	char    b3 : 3;
	char    b5 : 5;
} bs8[8];

int     pend_cmd;
int     paused;
int		mainloop_i;
int		test_counter;
int		show_stats;
long	stat_time;

time_t	unix_time;
struct tm local_time, gm_time;


/*
 * for test displays.
 */

int off_nominal_1;
int chan_a_on;
int bus_no;
int data_counter;
int chan_enable;
int job_enable;
int chan_a_status;
int chan_a_power;
int chan_a_restart;
int prev_chan_a_restart;
int disp_loop_cnt;

void
display_loop()
{
	if (++disp_loop_cnt < 5)
		return;
	if (chan_a_on) {
		disp_loop_cnt = 0;
		bus_no = (data_counter / 10) % 10;
		data_counter = (data_counter + 1) % 1000;
	}
	chan_a_on = job_enable && chan_enable;
	if (prev_chan_a_restart != chan_a_restart) {
		prev_chan_a_restart = chan_a_restart;
		chan_a_power = chan_a_restart;
	}
	chan_a_status = chan_a_power;
}


/*
 * for map.
 */

double	map_x;			/* latitude		-180..+180	*/
double	map_y;			/* longtitude	-90..+90	*/
double	map_h;			/* height		100..500	*/
time_t	map_time;

int		map_on      = 1;
double	map_x_min	= -180;
double	map_x_max	= 180;
double	map_x_speed = 14;
double	map_y_min	= -50;
double	map_y_max	= 80;
double	map_y_speed = 120;
double	map_angle   = 45;
double	map_a_min	= 30;
double	map_a_max	= 60;
double	map_a_speed	= 80;
int		map_sinsin	= 1;
double	map_h_min   = 100;
double	map_h_max   = 1000;
double	map_h_speed = 1;
int		map_year0, map_mon0, map_day0;
time_t	map_epoch;

#define PI		3.1415926
#define G2R(g)	((g)*(PI/180.))


void
map_loop()
{
	double s, v, a;
	struct timeval tv;

	if (!map_on)
		return;
	if (!map_epoch) {
		struct tm tm0 = {};
		if (map_year0 <= 0 || map_mon0 <= 0 || map_day0 <= 0) {
			map_year0 = 2000;
			map_mon0 = 1;
			map_day0 = 1;
		}
		tm0.tm_year = map_year0 - 1900;
		tm0.tm_mon = map_mon0 - 1;
		tm0.tm_mday = map_day0;
		map_epoch = mktime(&tm0);
	}
	time(&map_time);
	s = map_time - map_epoch;
	gettimeofday(&tv, 0);
	s += tv.tv_usec * 1.e-6;

	map_h = (map_h_min + map_h_max) / 2
			+ (map_h_max - map_h_min) / 2 * sin(G2R(map_h_speed) * s);

	if (map_a_speed > 0) {
		map_angle = (map_a_min + map_a_max) / 2
					+ (map_a_max - map_a_min) / 2 * sin(map_a_speed * 1e-6 * s);
	}
	a = G2R(map_angle);

	v = map_x_speed * 1e-3;
	map_x = map_x_min + fmod(v * s * sin(a), map_x_max - map_x_min);

	v = map_y_speed * 1e-6 * s;
	map_y = (map_y_min + map_y_max) / 2
			+ (map_y_max - map_y_min) / 2
				* (map_sinsin ? sin : cos)(v * cos(a));
}


/*
 * for stress test.
 */
#define INCRC(x)										\
		if (x[i]) {										\
			x[i+1] = x[i] + 1;							\
			if (st1_verb)								\
				fprintf(stderr, "%s[%d]=%d\n",#x,i,(int)x[i]);	\
		}

void
stress1_loop()
{
	int i, val;

	if (stress_test1 >= 2) {
		n_val++;
		for (i = 0; i < 1024; i++) {
			n_char[i] = n_val;
			n_uchar[i] = n_val;
			n_short[i] = n_val;
			n_ushort[i] = n_val;
			n_int[i] = n_val;
			n_uint[i] = n_val;
			n_long[i] = n_val;
			n_ulong[i] = n_val;
			n_float[i] = n_val;
			n_double[i] = n_val;
			n_bool[i] = n_val;
			sprintf(n_string[i], "%d", n_val);
		}
	}

	if (stress_test1 >= 1) {
		for (i = 1; i < 1024-2; i += 2) {
			INCRC(c_char);
			INCRC(c_uchar);
			INCRC(c_short);
			INCRC(c_ushort);
			INCRC(c_int);
			INCRC(c_uint);
			INCRC(c_long);
			INCRC(c_ulong);
			INCRC(c_float);
			INCRC(c_double);
			INCRC(c_bool);
			if (c_string[i][0] != '\0'
					&& sscanf(c_string[i], "%d", &val) == 1
					&& val != 0)
				sprintf(c_string[i+1], "%d", val+1);
		}
	}
}


int
main_loop(void)
{
	int     j;
	int     i = mainloop_i++;

	switch (pend_cmd) {
	case 1:
		paused = 0;
		break;
	case 2:
		paused = 1;
		break;
	}
	pend_cmd = 0;
	if (paused) {
		i--;
		return 0;
	}

	time(&unix_time);
	localtime_r(&unix_time, &local_time);
	local_time.tm_year += 1900;
	local_time.tm_mon += 1;
	gmtime_r(&unix_time, &gm_time);
	gm_time.tm_year += 1900;
	gm_time.tm_mon += 1;

	test_counter = i;
	arr1[0] = arr2[0] = (char) i;
	if (arr1[1])
		arr1[1]++;
	if (arr2[1])
		arr2[1]++;
	for (j = 0; j < 8; j++)
		bs8[j].b5 = bs8[j].b3 + 1;
	for (j = 0; j < 100; j++) {
		if (++arr3[j] < 10)
			break;
		arr3[j] = 0;
	}

	param1 = i;
	param2 = -i / 2;
	param5 = sin(param1 / 100.0);
	param4 = (float) (param5 * param5 * 100.0);

	sprintf(param3, "DATA{%d + %d = %d}", param1, param2, param1 + param2);
	sprintf(strout, "c=%c h=%d i=%d f=%.2f d=%.2f",
			strinc, strinh, strini, strinf, strind);

	if ((i % 10) == 0 && show_stats) {
		fprintf(stderr, ".");
	}

	if (!stat_time || osMsClock() - stat_time > 10000) {
		int    *p = osubj_alloc_id_qty;
		stat_time = osMsClock();
		if (show_stats) {
			fprintf(stderr, "\n"
					"[qnet=%d,rget=%d,rput=%d,udp=%d]"
					"{mon=%d,dread=%d,obj=%d,q=%d}\n",
					p[3], p[4], p[5], p[6], p[7], p[8], p[1], p[2]);
		}
	}

	return 0;
}


void test_exit(int sig)
{
	puts("subject exits");
	osubjExit();
	exit(0);
}


/*			Messaging			*/

void
handle_message(OlangCtlMsg_t *cmsg, int no, int len)
{
	ushort_t *data;
	char msg[256];
	int arg0, i, n, narg;
	ushort_t suffix;
	bool_t is_control = FALSE;

	if (len >= OLANG_CMSG_HEADER_LEN + sizeof(suffix)
			&& cmsg->prefix == OUHTONS(OLANG_CMSG_PREFIX)) {
		memcpy((char *) &suffix, (char *)cmsg + len - 2, sizeof(suffix));
		if (suffix == OUHTONS(OLANG_CMSG_SUFFIX)) {
			is_control = TRUE;
			data = (ushort_t *) OLANG_CMSG_DATA(cmsg);
			len -= OLANG_CMSG_HEADER_LEN + sizeof(suffix);
			arg0 = 0;
			if (len >= sizeof(int) && cmsg->narg > 0)
				arg0 = ONTOHL(*(int *)data);
			narg = OUNTOHS(cmsg->narg);
			n = sprintf(msg, "control message: "
					"no=%d len=%d class=%d type=%d narg=%d",
					no, len, OUNTOHS(cmsg->klass), OUNTOHS(cmsg->type), narg);
			if (narg > 0) {
				n += sprintf(msg + n, " [");
				for (i = 0; i < narg && i < max_show_msg_word
							&& n < sizeof(msg)-32; i++)
					n += sprintf(msg + n, "%hxh,", OUNTOHS(data[i]));
				if (i > 0)
					n--;	/* roll back last comma */
				strcpy(msg + n, i < narg ? " ...]" : "]");
			}
		}
	}
	if (!is_control) {
		sprintf(msg, "raw message: no=%d len=%d", no, len);
	}
	ologIf (osubj_verb >= 3, msg);
	/* broadcast the report */
	osubjSendMessage(0, NULL, OLANG_MSG_CLASS_ECHO, OLANG_MSG_TYPE_ECHO,
					msg, strlen(msg)+1);
}


oret_t
msg_handler(ooid_t id, const char *src, const char *dest,
			int klass, int type, void *data, int len)
{
	oret_t rc;

	switch (klass) {
	case OLANG_MSG_CLASS_SEND:
		switch (type) {
		case OLANG_MSG_TYPE_SEND:
			if (osubjSendMessage(id, src,
								OLANG_MSG_CLASS_ACK, OLANG_MSG_TYPE_ACK,
								NULL, 0)) {
				olog("cannot acknowledge message");
			}
			handle_message(data, id, len);
			break;
		default:
			olog("got unknown type %d len %d", type, len);
			break;
		}
		break;

	case OLANG_MSG_CLASS_TEST:
		switch (type) {
		case 1:
			msg2_count = 0;
			rc = osubjSendMessage(0, src, OLANG_MSG_CLASS_TEST, 21, data, len);
			ologIf(print_all, "got string \"%s\"\n", (char *)data);
			if (rc == ERROR)
				printf("cannot send back string echo\n");
			break;
		case 2:
			msg2_count++;
			if (print_all)
				fprintf(stderr, ".");
			break;
		case 3:
			rc = osubjSendMessage(0, src, OLANG_MSG_CLASS_TEST, 23,
									&msg2_count, sizeof(msg2_count));
			ologIf(print_all, "sending count of %d\n", msg2_count);
			if (rc == ERROR) {
				olog("cannot send back message count\n");
			}
			break;
		default:
			olog("got unknown test_suite type %d\n", type);
			break;
		}
		break;

	case OLANG_MSG_CLASS_THRASH:
		/* supposed to fill up my queues if I had some */
		if (print_all)
			fprintf(stderr, "#");
		break;

	default:
		olog("unknown klass %d\n", klass);
		break;
	}

	return OK;
}


int
main(int argc, char *argv[])
{
	int     i, j;
	int     mon_buf;
	bool_t  remote_log = FALSE;

	osubj_sel_address = (long) &test_counter;
	osubj_verb = argc > 1 ? atoi(argv[1]) : 0;
	if (0 == osubj_verb) {
		osubj_verb = 1;
	} else if (osubj_verb < 0) {
		remote_log = TRUE;
		osubj_verb = -osubj_verb;
	}
	show_stats = argc > 2 ? atoi(argv[2]) : 0;
	mon_buf = argc > 3 ? atoi(argv[3]) : 1024;
	osubj_mon_buf_size = mon_buf;
	srand48(osUsClock());
	signal(SIGINT, test_exit);
	signal(SIGTERM, test_exit);
	ologOpen("sample", NULL, 0);

	osubjInit(0, OSUBJ_WORK_ALL, 0, 0, NULL, msg_handler);
	osubjAddSubject(SAMPLE_SUBJECT, SAMPLE_SYMTABLE);
	osubjEnableMsgClass(OLANG_MSG_CLASS_SEND, TRUE);
	osubjEnableMsgClass(OLANG_MSG_CLASS_TEST, TRUE);
	osubjRemoteLog(remote_log);

	for (j = 0; j < 8; j++)
		bs8[j].b3 = j;
	for (j = 0; j < 8; j++)
		arr1[j] = arr2[j] = j;
	for (i = 0; 1; i++) {
		if (main_loop() != 0)
			break;
		map_loop();
		display_loop();
		for (j = 0; j < 10; j++) {
			stress1_loop();
			osubjSleep(10);
		}
	}
	test_exit(0);
	return 0;
}
