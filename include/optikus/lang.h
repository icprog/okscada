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

  Optikus: network protocol constants.

*/

#ifndef OPTIKUS_LANG_H
#define OPTIKUS_LANG_H

#include <optikus/types.h>

OPTIKUS_BEGIN_C_DECLS


/*			Magic Numbers			*/

#define OLANG_VERSION      0x0102		/* 1.2 */

#define OLANG_MAGIC        0x4F4E5750UL	/* 'ONWP', Optikus NetWork Protocol */

/*			Feature Flags			*/

#define OLANG_USE_CHECKSUMS    0

/*
	FIXME:	Should investigate more on the definition below !
			It worked in later version of protocol but
			somehow broke earlier revisions.
*/
#define OLANG_PREFER_TCP_NODELAY	1

#define OLANG_PREFER_THROTTLING		1


/*				Limits				*/

#define OLANG_MAX_PACKET		8192
#define OLANG_SMALL_PACKET		800
#define OLANG_HEAD_LEN			6
#define OLANG_TAIL_LEN			2
#define OLANG_CONNECT_QTY		4
#define OLANG_MAX_MSG_KLASS		32767
#define OLANG_MAX_MSG_TYPE		32767

/*				Ports				*/

#define OLANG_HUB_PORT     3210
#define OLANG_SUBJ_PORT    3215

/*			Kinds of Message 		*/

#define OLANG_DATA         1	/* data message (TCP stream) */
#define OLANG_COMMAND      2	/* command message (UDP high priority) */
#define OLANG_CONTROL      3	/* control message (UDP low priority) */

/*			Timeouts				*/

#define OLANG_DEAD_MS      40000
#define OLANG_ALIVE_MS     8000
#define OLANG_STAT_MS      10000

#define OLANG_PING_MIN_MS  64
#define OLANG_PING_MAX_MS  10000
#define OLANG_PING_COEF0   1
#define OLANG_PING_COEF1   4
#define OLANG_PING_COEF2   5

/*			Error Codes				*/

#define OLANG_ERR_OK			0x0000
#define OLANG_ERR_RCVR_OFF		0xf001
#define OLANG_ERR_RCVR_FULL		0xf002
#define OLANG_ERR_RCVR_DOWN		0xf003

/*			Packet Types			*/

#define OLANG_PING_REQ            0x2001
#define OLANG_PING_ACK            0x2002
#define OLANG_PING_NAK            0x2003
#define OLANG_CONN_REQ            0x2004
#define OLANG_CONN_ACK            0x2005
#define OLANG_CONN_NAK            0x2006
#define OLANG_ALIVE               0x2007
#define OLANG_SUBJECTS            0x2008
#define OLANG_RESERVED_01         0x2009
#define OLANG_RESERVED_02         0x200a
#define OLANG_SEGMENTS            0x200b
#define OLANG_INFO_BY_DESC_REQ    0x200c
#define OLANG_INFO_BY_OOID_REQ    0x200d
#define OLANG_INFO_BY_ANY_REPLY   0x200e
#define OLANG_MON_REG_REQ         0x200f
#define OLANG_MON_REG_ACK         0x2010
#define OLANG_MON_REG_NAK         0x2011
#define OLANG_MON_UNREG_REQ       0x2012
#define OLANG_MON_UNREG_ACK       0x2013
#define OLANG_MON_UNREG_NAK       0x2014
#define OLANG_MONITORING          0x2015
#define OLANG_DATA_REG_REQ        0x2016
#define OLANG_DATA_REG_ACK        0x2017
#define OLANG_DATA_REG_NAK        0x2018
#define OLANG_DATA_UNREG_REQ      0x2019
#define OLANG_DATA_UNREG_ACK      0x201a
#define OLANG_DATA_UNREG_NAK      0x201b
#define OLANG_OOID_WRITE_REQ      0x201c
#define OLANG_OOID_WRITE_REPLY    0x201d
#define OLANG_DATA_WRITE_REQ      0x201e
#define OLANG_DATA_WRITE_REPLY    0x201f
#define OLANG_OOID_READ_REQ       0x2020
#define OLANG_OOID_READ_REPLY     0x2021
#define OLANG_DATA_READ_REQ       0x2022
#define OLANG_DATA_READ_REPLY     0x2023
#define OLANG_TRIGGER_REQUEST     0x2024
#define OLANG_TRIGGER_REPLY       0x2025
#define OLANG_BLOCK_ANNOUNCE      0x2026
#define OLANG_MON_GROUP_REQ       0x2028
#define OLANG_MON_GROUP_REPLY     0x2029
#define OLANG_MON_COMBO_REQ       0x202a
#define OLANG_MON_COMBO_REPLY     0x202b
#define OLANG_DATA_MON_REPLY      0x202c
#define OLANG_DGROUP_UNREG_REQ    0x202d

/*			Message Packets					*/

#define OLANG_MSG_SEND_REQ			0x202e
#define OLANG_MSG_SEND_REPLY		0x202f
#define OLANG_MSG_RECV				0x2030
#define OLANG_MSG_HIMARK			0x2031
#define OLANG_MSG_LOMARK			0x2032
#define OLANG_MSG_CLASS_ACT			0x2033

/*			Miscellaneous Packets			*/

#define OLANG_REMOTE_LOG			0x2034
#define OLANG_SEL_MONITORING		0xEEEE

/*			Control Messages				*/

#define OLANG_MSG_CLASS_TEST		0x7010
#define OLANG_MSG_CLASS_THRASH		0x7011

#define OLANG_MSG_CLASS_SEND		0x7020
#define OLANG_MSG_TYPE_SEND			0x7021
#define OLANG_MSG_CLASS_ACK			0x7030
#define OLANG_MSG_TYPE_ACK			0x7031
#define OLANG_MSG_CLASS_ECHO		0x7040
#define OLANG_MSG_TYPE_ECHO			0x7041

#define OLANG_CMSG_PREFIX			0x7172
#define OLANG_CMSG_SUFFIX			0x7374

typedef struct
{
	ushort_t	prefix;
	ushort_t	klass;
	ushort_t	type;
	ushort_t	narg;
	/* variable length data follows, terminated by suffix */
} OlangCtlMsg_t;
#define OLANG_CMSG_HEADER_LEN		sizeof(OlangCtlMsg_t)
#define OLANG_CMSG_DATA(cmsg)		((char *)(cmsg) + OLANG_CMSG_HEADER_LEN)


OPTIKUS_END_C_DECLS

#endif /* OPTIKUS_LANG_H */
