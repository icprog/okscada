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
package Optikus::Canvas::Widget::Toggle::Switch;

use strict;
use Gtk2;
use Optikus::Canvas::Util;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::Widget::Toggle);
  @EXPORT  = ( );
  %EXPORT_TAGS = ( );
  @EXPORT_OK = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  $self->{IMAGE} = ();
  my @images = split(/\s*,\s*/, $arg{Image});
  for my $im (@images) {
    push @{$self->{IMAGE}}, create_pic($im);
  }
  $self->{PIXMAP} = Gtk2::Image->new_from_pixbuf($self->{IMAGE}->[0]);
  $self->{WIDGET}->add($self->{PIXMAP});
  bless($self, $class);
  $self->test_refresh(200, \&refresh_widget);
  return $self;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "Image",
            type => "images",
          }
         );
}

sub refresh_widget
{
  my $self = shift;
  my $toggle;
  if ($Optikus::Canvas::Widget::TEST) {
    $toggle = int(make_test_value(0,1.5));
  } else {
    $toggle = $self->{VAR}->[0];
  }
  print "in refresh_widget($self), toggle: $toggle\n" if $DEBUG;
  $toggle = 0 if $toggle < 0;
  $toggle = 1 if $toggle > 1;
  $self->{PIXMAP}->set_from_pixbuf($self->{IMAGE}->[$toggle]);
  $self->SUPER::refresh_widget();
}

1;
