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

  TO BE REMOVED.

*/

#include <optikus/conf-os.h>
#include <stdio.h>

void
proc696(void)
{
	fprintf(stderr, "!");
}

int
main(int argc, char **argv)
{
	int     i;
	ushort_t *irig = (ushort_t *) osVme2Mem(VME_AM_SUP_A16, 0xC000, 0);

	printf("board_no=%d irig_base=%p\n", osGetBoardNo(), irig);
	printf("irig1=%04Xh irig2=%04Xh\n", irig[0], irig[1]);
	osSetVmeVect(92, (proc_t) proc696, 0);
	osSetVmeIrq(2, 1);
	for (i = 0; i < 6; i++) {
		osMsSleep(1000);
		fprintf(stderr, ".");
	}
	osSetVmeIrq(2, 0);
	return 0;
}
