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
package Optikus::Forms::Widget::Anim;

use strict;
use Gtk2;
use Optikus::Forms::Util;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Forms::WidgetActive);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  for my $name (split(/\s*,\s*/, $arg{Image})) {
    push @{$self->{PIX}}, create_pic($name);
  }
  $self->{current} = 0;
  $self->{WIDGET} = new Gtk2::Image->new_from_pixbuf($self->{PIX}->[0]);
  $self->{WIDGET}->show;
  bless ($self, $class);
  $self->test_refresh (300, \&refresh_widget);
  return $self;
}

sub refresh_widget
{
  my $self = shift;
  my $k;
  if ($Optikus::Forms::Widget::TEST) {
    $k = int(make_test_value(0,$#{$self->{PIX}}+0.8));
  } else {
    $k = $self->{VAR}->[0];
  }
  $k = 0 if $k < 0;
  $k = $#{$self->{PIX}} if $k > $#{$self->{PIX}};
  $self->{current} = $k;
  $self->{WIDGET}->set_from_pixbuf($self->{PIX}->[$k]);
  return 1;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr  => 'Image',
            type  => 'images',
          }
         );
}

1;

