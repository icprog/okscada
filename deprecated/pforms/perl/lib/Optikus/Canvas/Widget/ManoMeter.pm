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
package Optikus::Canvas::Widget::ManoMeter;

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

use constant PI => 3.14159265359;

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  my $subclass = $arg{subclass};
  $self->{PBG} = create_pic("png/mnmtr_${subclass}.png");
  $self->{WIDGET} = Gtk2::Image->new_from_pixbuf($self->{PBG});
  $self->{MAX} = $arg{MAX};
  $self->{MIN} = $arg{MIN};
  ($self->{PIXMAP}, $self->{BITMASK}) = $self->{PBG}->render_pixmap_and_mask(1);
  $self->redraw_pixmap(0);
  bless($self, $class);
  Glib::Timeout->add(200,sub{$self->redraw_pixmap();return 1;})
      if $Optikus::Canvas::Widget::TEST;
  return $self;
}

sub redraw_pixmap
{
  my ($self, $value) = @_;
  $value = make_test_value(0,1)
    if $Optikus::Canvas::Widget::TEST;
  my $angle = $value * (PI*3./2.) - (PI*5./4.);
  my $w   = $self->{PBG}->get_width();
  my $h   = $self->{PBG}->get_height();
  my $sz  = $w < $h ? $w : $h;
  my $outer = $sz / 2;
  my $inner = $sz / 14.3;
  my ($pixmap, $mask) = ($self->{PIXMAP}, $self->{BITMASK});
  my $gc = Gtk2::Gdk::GC->new($pixmap);
  $pixmap->draw_pixbuf($gc, $self->{PBG},
                       0, 0, 0, 0, $sz, $sz,
                       "none", 0, 0);
  my $color = Gtk2::Gdk::Color->parse("#003366");
  $gc->get_colormap->alloc_color($color, 0, 0);
  $gc->set_foreground($color);
  $gc->set_line_attributes(3, "solid", "round", "miter");
  $pixmap->draw_line($gc,
                     $outer + cos($angle) * ($outer - $inner),
                     $outer + sin($angle) * ($outer - $inner),
                     $outer - 1  + cos($angle) * $inner,
                     $outer - 1 + sin($angle) * $inner);
  $gc->get_colormap->free_colors($color);
  $self->{WIDGET}->set_from_pixmap($pixmap, $mask);
}

sub refresh_widget
{
  my $self = shift;
  my $value = (($self->{VAR}->[0] - $self->{MIN})
              / ($self->{MAX} - $self->{MIN}));
  $self->redraw_pixmap($value);
  return 1;
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
          {
            attr  => 'subclass',
            type  => 'string',
          },
         );
}

1;
