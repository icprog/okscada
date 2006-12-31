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

  Perl binding for Optikus.

*/

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#if PERL_IS_5_8_PLUS
#include "ppport.h"
#endif

static void
_call_XS (pTHX_ void (*subaddr) (pTHX_ CV *), CV * cv, SV ** mark)
{
	dSP;
	PUSHMARK (mark);
	(*subaddr) (aTHX_ cv);
	PUTBACK;
}

#define CALL_BOOT(name) { extern XS(name); _call_XS (aTHX_ name, cv, mark); }

#define CALL_OPTIKUS_BOOT(name)		CALL_BOOT(boot_Optikus__##name)

/* =================== MODULE: Optikus ====================== */

MODULE = Optikus::Optikus	PACKAGE = Optikus::Optikus

BOOT :
	CALL_OPTIKUS_BOOT(Log);
	CALL_OPTIKUS_BOOT(Watch);

