# -*-perl-*-    vi: set ts=4 sw=4 :
#
#  Copyright (C) 2006-2007, vitki.net. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#  $Date$
#  $Revision$
#  $Source$
#
#  Optikus data widget library.
#
package Optikus::Canvas::Widget::Meter::ThermoMeter;

use strict;
use Gtk2;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::Widget::Meter);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $param = shift;
  $arg{color} = 'red';
  my $self = $class->SUPER::new(%arg,
                                'subclass' => 'thermometer',
                                'yMIN'  => 119,
                                'yMAX'  => 9);
}

1;
