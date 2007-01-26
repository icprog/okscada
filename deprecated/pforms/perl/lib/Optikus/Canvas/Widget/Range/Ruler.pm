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
package Optikus::Canvas::Widget::Range::Ruler;

use strict;
use Gtk2;
use Optikus::Canvas::Util;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::Widget::Range);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $param = shift;
  my $self = $class->SUPER::new(%arg);
  my $fvert = is_true($arg{vertical});
  my $scale = ($fvert ? Gtk2::VScale->new($self->{ADJ}):
                        Gtk2::HScale->new($self->{ADJ}) );
  my $w = $self->{width};
  my $h = $self->{height};
  $w = ($fvert ? 40 : 80) unless defined $w;
  $h = ($fvert ? 80 : 40) unless defined $h;
  $scale->set_size_request($w, $h);
  $self->{WIDGET} = $scale;
  $scale->signal_connect('button_release_event', \&set_value, $self);
  bless($self, $class);
  $self->test_refresh(200, sub {$self->SUPER::refresh_widget});
  return $self;
}

sub set_value
{
  my ($widget, $event, $self) = @_;
  return unless defined $self->{VAR};
  return unless defined $self->{VAR}->[0];
  #printf("in set_value(). oldval: %d, newval %d\n",
  #       $self->{VAR}->[0], $self->{ADJ}->value);
  return if $self->{VAR}->[0] == $self->{ADJ}->value;
  $self->{VAR}->[0] = $self->{ADJ}->value;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr    => "vertical",
            type    => "boolean",
          }
         );
}

1;
