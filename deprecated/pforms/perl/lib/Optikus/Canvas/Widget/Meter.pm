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
package Optikus::Canvas::Widget::Meter;

use strict;
use Gtk2;
use Optikus::Canvas::Util;
use Optikus::Canvas::Widget::Meter::BigTank;
use Optikus::Canvas::Widget::Meter::SmallTank;
use Optikus::Canvas::Widget::Meter::RoundTank;
use Optikus::Canvas::Widget::Meter::Volume;
use Optikus::Canvas::Widget::Meter::ThermoMeter;
use Optikus::Canvas::Widget::ManoMeter;


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
  my $subclass = $arg{subclass};
  my $color = $arg{color};
  $self->{PXEMPTY} = create_pic("png/${subclass}_empty.png");
  $self->{PXFILL} = create_pic("png/${subclass}_${color}.png");
  $self->{color} = $arg{color};
  my ($w, $h) = ($self->{PXEMPTY}->get_width, $self->{PXEMPTY}->get_height);
  $self->{WIDGET} = Gtk2::Image->new;
  ($self->{PIXMAP}, $self->{BITMASK})
                  = $self->{PXEMPTY}->render_pixmap_and_mask(1);
  $self->redraw_pixmap($self->{MIN});
  $self->{yMIN} = $arg{yMIN};
  $self->{yMAX} = $arg{yMAX} < $arg{yMIN} ? $arg{yMAX} : 1;
  $self->{MIN} = $arg{MIN};
  $self->{MAX} = $arg{MAX} > $arg{MIN} ? $arg{MAX} : $arg{MIN} + 2;
  $self->{W} = $w;
  $self->{H} = $h;
  $self->{C} = ($self->{yMIN} - $self->{yMAX}) / ($self->{MAX} - $self->{MIN});
  bless($self, $class);
  $self->test_refresh(100, \&redraw_pixmap, 0);
  return $self;
}

sub get_y
{
  my ($self, $val) = @_;
  return $self->{yMIN} if $val < $self->{MIN};
  return $self->{yMAX} if $val > $self->{MAX};
  return $self->{yMIN} - ($val - $self->{MIN}) * $self->{C};
}

sub redraw_pixmap
{
  my ($self, $current) = @_;
  my $y = $self->get_y($current);
  $y = make_test_value(0, $self->{H}) if $Optikus::Canvas::Widget::TEST;
  my ($pixmap, $mask) = ($self->{PIXMAP}, $self->{BITMASK});
  my $gc = Gtk2::Gdk::GC->new($pixmap);
  my $h_y;
  $h_y = $self->{H} - $y;
  $h_y = 0 if ( $h_y < 0 );
  $pixmap->draw_pixbuf($gc,
                       $self->{PXEMPTY},
                       0, 0,
                       0, 0, $self->{W}, $y,
                       "none", 0, 0);
  $pixmap->draw_pixbuf($gc,
                       $self->{PXFILL},
                       0, $y,
                       0, $y, $self->{W}, $h_y,
                       "none", 0, 0);
  $self->{WIDGET}->set_from_pixmap($pixmap, $mask);
}

sub refresh_widget
{
  my $self = shift;
  $self->redraw_pixmap($self->{VAR}->[0]);
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          { 
            attr  => 'MIN',
            type  => 'double',
          },
          {
            attr  => 'MAX',
            type  => 'double',
          },
          { attr  => 'color',
            type  => 'color',
          }
         );
}

1;
