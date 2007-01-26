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
package Optikus::Canvas::Widget::String;

use strict;
use Gtk2;
use Optikus::Canvas::Util;

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
  my $string = sprintf $self->{format}, 0;
  $string =~ s/\\n/\n/g;
  my $label = new Gtk2::Label($string);
  $self->{WIDGET} = $label;
  bless($self, $class);
  $self->update_appearance;
  $self->test_refresh(300, \&refresh_widget);
  return $self;
}

sub refresh_widget
{
  my $self = shift;
  my ($string, @val);
  if ($Optikus::Canvas::Widget::TEST) {
    for (1 .. 10) {
      push @val, int(make_test_value(0,10));
    }
  } else {
    @val = @{$self->{VAR}};
  }
  $string = sprintf $self->{format}, @val;
  $string =~ s/\\n/\n/g;
  $self->{WIDGET}->set_text($string) if defined $self->{WIDGET}; 
}

sub set_value
{
  my ($widget, $self) = @_;
  print "Illegal call: set_value() for Optikus::Canvas::Widget::String\n";
  return;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "format",
            type => "string",
          },
          {
            attr => "font",
            type => "font",
          },
          {
            attr => "color",
            type => "color",
          }
         );
}

sub update_appearance
{
  Optikus::Canvas::Widget::Label::update_appearance(@_);
}

1;
