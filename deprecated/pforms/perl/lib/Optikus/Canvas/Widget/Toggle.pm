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
package Optikus::Canvas::Widget::Toggle;

use strict;
use Gtk2;
use Optikus::Canvas::Util;

use Optikus::Canvas::Widget::Toggle::Button;
use Optikus::Canvas::Widget::Toggle::Switch;

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
  my $self = $class->SUPER::new(%arg);
  my $button = new Gtk2::ToggleButton;
  $button->signal_connect(clicked => \&set_value, $self);
  if (is_true($arg{no_border})) {
    $button->set_relief('none');
  }
  $self->{WIDGET} = $button;
  bless($self, $class);
  $self->set_tip($arg{tooltip}, "");
  $self->{refreshing} = 0;
  $self->test_refresh(200, \&refresh_widget);
  return $self;
}

sub refresh_widget
{
  my $self = shift;
  $self->{refreshing} = 1;
  my $toggle;
  if ($Optikus::Canvas::Widget::TEST) {
    $toggle = int(make_test_value(0,1.5));    
  } else {
    $toggle = $self->{VAR}->[0];
  }
  $self->{WIDGET}->set_active($toggle)
      if defined $self->{WIDGET};
  $self->{refreshing} = 0;
}

sub set_value
{
  my ($widget, $self) = @_;
  if ($Optikus::Canvas::Widget::SIMULATION) {
    $widget->set_state("normal");
    return;
  }
  if ($self->{refreshing}) {
    #print "refreshing in set_value($widget)\n";
    $self->{refreshing} = 0;
    return;
  }
  if (is_true($self->{readonly})) {
    print "READ-ONLY\n" if $DEBUG;
    $self->{WIDGET}->set_active(! !$self->{VAR}->[0])
      if defined $self->{WIDGET};
    return;
  }
  print ("set_value ".
         " name=[".$self->{VARNAME}->[0]."]".
         " old=[".$self->{VAR}->[0]."]".
         " new=[".$widget->get_active."]\n")
    if $DEBUG;
  return if $self->{VAR}->[0] == $widget->get_active;
  $self->{VAR}->[0] = $self->{WIDGET}->get_active ? 1 : 0;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "no_border",
            type => "boolean",
          }
         );
}

1;
