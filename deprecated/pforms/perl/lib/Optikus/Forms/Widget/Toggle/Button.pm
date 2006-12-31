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
package Optikus::Forms::Widget::Toggle::Button;

use strict;
use Gtk2;
use Optikus::Forms::Util;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Forms::Widget::Toggle);
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  $self->{LABEL} = create_hor_label($arg{Label});
  $self->{WIDGET}->add($self->{LABEL});
  bless ($self, $class);
  $self->resize ($self->{width}, $self->{height});
  return $self;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "Label",
            type => "glabel",
          }
         );
}

sub resize
{
  my ($self, $w, $h) = @_;
  return 1 unless $w > 1 and $h > 1;
  $self->{WIDGET}->set_size_request ($w, $h);
  $self->{width} = $w;
  $self->{height} = $h;
  return 1;
}

1;

