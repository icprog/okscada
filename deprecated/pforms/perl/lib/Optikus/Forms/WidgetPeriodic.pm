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
#  PForms widget library.
#
package Optikus::Forms::WidgetPeriodic;

use strict;
use Gtk2;
use Optikus::Forms::Widget::Curve;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Forms::Widget);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  $self->time_bind($arg{freq}, $arg{when});
  return $self;
}

sub DESTROY
{
  my $self = shift;
  #print "DESTROING $self\n";
  $self->{WIDGET}->destroy;
}

sub forget
{
  my $self = shift;
  $self->SUPER::forget();
  $self->time_unbind;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "freq",
            type => "int",
          },
          {
            attr => "when",
            type => "int",
          }
         );
}

sub time_bind
{
  my ($self, $freq, $when) = @_;
  return if $Optikus::Forms::Widget::SIMULATION;
  $freq = 10 unless defined $freq;
  $when = 0 unless defined $when;
  #Optikus::Forms::Ex::add_periodic($self->can('refresh_widget'), $self, $freq, $when);
}

sub time_unbind
{
  my $self = shift;
  return if $Optikus::Forms::Widget::SIMULATION;
  #Optikus::Forms::Ex::remove_periodic($self->can('refresh_widget'));
}

sub refresh_widget
{
  die "STUB method called";
}

1;

