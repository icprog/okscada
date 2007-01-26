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
package Optikus::Canvas::Widget::Control::Link;

use strict;
use Gtk2;
use Optikus::Canvas::Util;
use Optikus::Log ':all';

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::Widget::Control);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  my $button = create_hor_button($arg{Label});
  $self->{WIDGET} = $button;
  $button->signal_connect("clicked", \&invoke_screen, $self);
  bless($self, $class);
  $self->resize($self->{width}, $self->{height});
  return $self;
}

sub get_attr_list
{
  my $self = shift;
  my @attrl = 
      ($self->SUPER::get_attr_list,
       {
         attr => "Label",
         type => "glabel",
       },
       {
         attr => "link",
         type => "string",
       }
      );
  my $n = 0;
  for (@attrl) {
    if ($_->{attr} eq "var") {
      splice @attrl, $n, 1;
    }
    $n++;
  }
  return @attrl;
}

sub invoke_screen
{
  my ($button, $self) = @_;
  return if $Optikus::Canvas::Widget::SIMULATION;
  my $screen = $self->{link};
  if (!defined($screen) || length($screen) == 0) {
    olog "Invalid screen in $self";
    return;
  }
  olog "activate screen [$screen]";
  &::activate_screen($screen); 
}

1;
