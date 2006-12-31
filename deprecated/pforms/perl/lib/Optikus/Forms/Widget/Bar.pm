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
package Optikus::Forms::Widget::Bar;

use strict;
use Gtk2;
use Optikus::Forms::Util;

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
  my $image = $self->{WIDGET} = Gtk2::Image->new;
  bless ($self, $class);
  $self->resize;
  return $self;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "color",
            type => "color",
          }
         );
}

sub resize
{
  my ($self, $w, $h) = @_;
  my $win = $self->{_window}->window;
  return unless defined $win;
  $w = $self->{width} unless $w > 0;
  $h = $self->{height} unless $h > 0;
  $w = $h = 40 unless $w > 0 and $h > 0;
  $w = 1 unless $w > 0;
  $h = 1 unless $h > 0;
  my $color = $self->{color};
  $color = "#000000" unless length($color) > 0;
  my $c = Gtk2::Gdk::Color->parse($color);
  $c = Gtk2::Gdk::Color->new(0,0,0) unless defined $c;
  my $p = Gtk2::Gdk::Pixmap->new($win,$w,$h,-1);
  my $gc = Gtk2::Gdk::GC->new($p);
  $gc->get_colormap->alloc_color($c,0,0);
  $gc->set_foreground($c);
  $p->draw_rectangle($gc,1,0,0,$w,$h);
  $gc->get_colormap->free_colors($c);
  $self->{WIDGET}->set_from_pixmap($p,undef);
  $self->{width} = $w;
  $self->{height} = $h;
  return 1;
}

1;

