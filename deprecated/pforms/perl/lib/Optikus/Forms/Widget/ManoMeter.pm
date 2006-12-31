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
package Optikus::Forms::Widget::ManoMeter;

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

use constant PI => 3.14159265359;

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  my $subclass = $arg{subclass};
  my $color = $arg{color};
  $self->{PBG} = create_pic("png/mnmtr_${subclass}.png");
  $self->{WIDGET} = Gtk2::Image->new_from_pixbuf($self->{PBG});
  $self->{MAX} = $arg{MAX};
  $self->{MIN} = $arg{MIN};
  $self->{SZ} = $self->{PBG}->get_width;
  ($self->{PIXMAP}, $self->{BITMASK}) = $self->{PBG}->render_pixmap_and_mask(1);
  $self->redraw_pixmap(0);
  bless ($self, $class);
  Glib::Timeout->add(200,sub{$self->redraw_pixmap();return 1;})
      if $Optikus::Forms::Widget::TEST;
  return $self;
}

sub redraw_pixmap
{
  my ($self, $p) = @_;
  $p = make_test_value(0,1) if $PForms::UI::TEST;
  my $angle = - (PI + PI/4) + $p * (PI * 3 / 2);
  my $sz  = $self->{SZ}; 
  my $sz2 = $sz/2;
  my $sz3 = $sz/14.3;
  my ($pixmap, $mask) = ($self->{PIXMAP},$self->{BITMASK});
  my $gc = Gtk2::Gdk::GC->new($pixmap);
  $pixmap->draw_pixbuf($gc, $self->{PBG},
                       0, 0, 0, 0, $sz, $sz,
                      "none", 0, 0);
  my $clr = Gtk2::Gdk::Color->parse("#003366");
  $gc->get_colormap->alloc_color($clr,0,0);
  $gc->set_foreground($clr);
  $gc->set_line_attributes(3,"solid","round","miter");
  $pixmap->draw_line($gc,
                     $sz2 + cos($angle)*($sz2 - $sz3),
                     $sz2 + sin($angle)*($sz2 - $sz3),
                     $sz2 -1  + cos($angle)*$sz3,
                     $sz2 -1 + sin($angle)*$sz3);
  $gc->get_colormap->free_colors($clr);
  $self->{WIDGET}->set_from_pixmap($pixmap, $mask);
}

sub refresh_widget
{
  my $self = shift;
  $self->{i}++;
  $self->{i} = 0 if $self->{i} > 200;
  my $p = (($self->{VAR}->[0] - $self->{MIN})
               / ($self->{MAX} - $self->{MIN}));
  $self->redraw_pixmap($p);
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

