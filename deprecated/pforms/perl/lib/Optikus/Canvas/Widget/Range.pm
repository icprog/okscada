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
package Optikus::Canvas::Widget::Range;

use strict;
use Gtk2;
use Optikus::Canvas::Util;
use Optikus::Canvas::Widget::Range::Ruler;
use Optikus::Canvas::Widget::Range::Dial;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::DataWidget);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $param = shift;
  my $self = $class->SUPER::new(%arg);
  if ($arg{min} > $arg{max}) {
    my $t = $arg{min};
    $arg{min} = $arg{max};
    $arg{max} = $t;
  };
  my $adj = new Gtk2::Adjustment(0.0, $arg{min}, $arg{max}, 0.1, 1.0, 1.0);
  $self->{ADJ} = $adj;
  bless($self, $class);
  return $self;
}

sub refresh_widget
{
  my $self = shift;
  my $val;
  if ($Optikus::Canvas::Widget::TEST) {
    $val = int(make_test_value($self->{min},$self->{max}));    
  } else {
    $val = $self->{VAR}->[0];
  }
  #print "in refresh_widget($self), newval=[$val]\n";
  $self->{ADJ}->set_value($val);
}

sub set_value
{
  my ($widget, $self) = @_;
  printf("in set_value(). oldval: %d, newval %d\n",
         $self->{VAR}->[0], $self->{ADJ}->value);
  return if $self->{VAR}->[0] == $self->{ADJ}->value;
  $self->{VAR}->[0] = $self->{ADJ}->value;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "min",
            type => "double",
          },
          {
            attr => "max",
            type => "double",
          }
         );
}

1;
