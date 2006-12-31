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

  Test program for configuration parser.

*/

#include <optikus/ini.h>
#include <stdio.h>
#include <string.h>


char   *a_passport =
 "\n"
 "[CONFIG_VARIABLES:describe] \n"
 "area_start      = integer unsigned hex !0 section=HARDWARE \n"
 "dev1_present    = bool default=1                    section=HARDWARE \n"
 "dev2_present    = bool default=1                    section=HARDWARE \n"
 "base_address    = integer unsigned hex default=0xA0000000 section=HARDWARE \n"
 "print_io_config = bool                              section=HARDWARE \n"
 "first_device    = string(32) default='/dev/defdev1'   section=HARDWARE \n"
 "second_device   = string(32) default='/dev/defdev2' section=HARDWARE \n"
 ";code_file      = string(32) !0    section=FIRMWARE \n"
 "code_type = enum section=FIRMWARE short !0 { char=0, uchar=1, short=2, ushort=3, "
 " int=4, uint=5, long=6, ulong=7, float=8, double=9, bool=10 } \n"
 "\n";

const char *a_new_pass =
 "[CONFIG_VARIABLES:describe] \n"
 "new_param  = integer unsigned hex default=0x42000 section=NEW_PARAMS \n"
 "[NEW_TABLE:describe] \n"
 "bus = integer (3) [0..31] !0 \n"
 "dev_no = integer (3) [-1..31] !0 \n"
 "\n";


int
test_a(const char *ini_file, bool_t pr)
{
	inival_t v;
	inidb_t db;
	int     rc;

	/* open parameter file */
	db = iniOpen(ini_file, a_passport, 0);
	if (!db) {
		printf("cannot open %s\n", ini_file);
		return -1;
	}

	/* read parameters */
	rc = iniParam(db, "area_start", &v);
	if (rc) {
		printf("error reading area_start\n");
		return -1;
	}
	if (pr)
		printf("area_start: rc=%d val=%xh\n", rc, v.v_int);

	rc = iniParam(db, "dev1_present", &v);
	if (rc) {
		printf("error reading dev1_present\n");
		return -1;
	}
	if (pr)
		printf("dev1_present: rc=%d val=%xh\n", rc, v.v_enum);

	rc = iniParam(db, "code_type", &v);
	if (rc) {
		printf("error reading code_type\n");
		return -1;
	}
	if (pr)
		printf("code_type: rc=%d val=%xh\n", rc, v.v_enum);

	rc = iniParam(db, "code_file", &v);
	if (rc == OK) {
		printf("this parameter must be absent\n");
		return -1;
	}
	printf("NOTE: the message above is OK\n");

	rc = iniPassport(db, a_new_pass);
	if (rc) {
		printf("augmenting another passport\n");
		return -1;
	}
	if (pr)
		printf("new_add:rc=%d\n", rc);

	rc = iniParam(db, "new_param", &v);
	if (rc) {
		printf("error reading new_param\n");
		return -1;
	}
	if (pr)
		printf("new_param=%x\n", v.v_int);

	/* close database */
	iniClose(db);
	return 0;
}


const char *b_passport =
 "[CONFIG_VARIABLES:describe]\n"
 "test_path = string (63) section=TEST_CONTROL \n"
 "list_path = string (63) section=TEST_CONTROL \n"
 "[DEVICES:describe] \n"
 "idx      = integer (2) [0..99] !0 \n"
 "bus      = integer (2) [1..32] !0 \n"
 "base_addr= integer unsigned hex !0 \n"
 "chan     = integer (1) [1..2] !0 \n"
 "ram_size = integer !0 \n"
 "vect     = integer (4) [0..255] !0 \n"
 "irq      = integer (2) [0..7] !0 \n"
 "vme_dev  = string (31) !0 \n"
 "irig     = bool !0 \n"
 "ifile    = string (95) !0 \n"
 "dfile    = string (95) !0 \n"
 "\n";

int
test_b(const char *ini_file, bool_t pr, int loops)
{
	inival_t v[12];
	inireq_t req;
	inidb_t db;
	int     i, rc, count, k;

	/* open parameter file */
	db = iniOpen(ini_file, b_passport, 0);
	if (!db) {
		printf("cannot open %s\n", ini_file);
		return -1;
	}
	if (pr)
		printf("opened %s\n", ini_file);

	for (k = 0; k < loops; k++) {
		req = iniAskRow(db, "DEVICES");
		count = iniCount(db, req);
		if (pr)
			printf("DEV: req=%d count=%d\n", req, count);

		for (i = 0; i < count; i++) {
			rc = iniGetRow(db, req, v + 1, v + 2, v + 3, v + 4, v + 5,
						   v + 6, v + 7, v + 8, v + 9, v + 10, v + 11);
			if (!rc) {
				if (pr)
					printf("DEVICES broken at row %d\n", i);
				return -1;
			}
			if (pr)
				printf("DEVICES: i=%d idx=%d bus=%d base=%xh chan=%d ram=%xh "
						"vec=%xh irq=%d dev=%s irig=%d iram=%s dram=%s\n",
						i, v[1].v_int, v[2].v_int, v[3].v_int, v[4].v_int,
						v[5].v_int, v[6].v_int, v[7].v_int, v[8].v_str, v[9].v_int,
				 		v[10].v_str, v[11].v_str);
			
			rc = iniParam(db, "list_path", v + 0);
			if (rc) {
				printf("cannot get list_path\n");
				return -1;
			}
			if (pr)
				printf("list_path=[%s] rc=%d\n", v[0].v_str, rc);
		}
	}
	iniClose(db);
	return 0;
}


int
main(int argc, char **argv)
{
	int     i;
	char   *a_ini, *b_ini;
	int     loops = 0;
	int     ret = 0;

	a_ini = argc > 1 ? argv[1] : "test02-a.ini";
	b_ini = argc > 2 ? argv[2] : "test02rb.ini";

	if (1) {
		for (i = 0; i < loops; i++)
			ret |= test_a(a_ini, FALSE);
		ret |= test_a(a_ini, FALSE);
	}
	if (1) {
		for (i = 0; i < loops; i++)
			ret |= test_b(b_ini, FALSE, 10000);
		ret |= test_b(b_ini, FALSE, 0);
	}
	puts(ret ? "ERR" : "OK");
	return (ret ? 1 : 0);
}
