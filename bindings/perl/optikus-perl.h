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

  Perl bindings for Optikus.

*/

#include <optikus/watch.h>

int  watchGetInfo(const char *name, owquark_t * info_p, int timeout);

#define PUSH_WATCH_VAL(pval)                                        \
  switch ((pval)->type) {                                           \
  case 'b': XPUSHs(sv_2mortal(newSViv((pval)->v.v_char)));   break; \
  case 'B': XPUSHs(sv_2mortal(newSViv((pval)->v.v_uchar)));  break; \
  case 'h': XPUSHs(sv_2mortal(newSViv((pval)->v.v_short)));  break; \
  case 'H': XPUSHs(sv_2mortal(newSViv((pval)->v.v_ushort))); break; \
  case 'i': XPUSHs(sv_2mortal(newSViv((pval)->v.v_int)));    break; \
  case 'I': XPUSHs(sv_2mortal(newSViv((pval)->v.v_uint)));   break; \
  case 'l': XPUSHs(sv_2mortal(newSViv((pval)->v.v_long)));   break; \
  case 'L': XPUSHs(sv_2mortal(newSViv((pval)->v.v_ulong)));  break; \
  case 'f': XPUSHs(sv_2mortal(newSVnv((pval)->v.v_float)));  break; \
  case 'd': XPUSHs(sv_2mortal(newSVnv((pval)->v.v_double))); break; \
  case 'E': XPUSHs(sv_2mortal(newSVnv((pval)->v.v_enum)));   break; \
  case 's': XPUSHs(sv_2mortal(newSVpv((pval)->v.v_str,0)));  break; \
  }

