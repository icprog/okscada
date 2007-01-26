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
package Optikus::Canvas::Widget::Range::Dial;

use strict;
use Gtk2;
use Optikus::Canvas::Util;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::Widget::Range);
  %EXPORT_TAGS = ( );
}

use constant PI => 3.14159265359;

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $param = shift;
  my $self = $class->SUPER::new(%arg);
  $self->{want_bg} = 1;
  $self->{PBG} = create_pic("png/dial_bg.png");
  $self->{PBG0} = $self->{PBG}->copy;
  ($self->{PIXMAP}, $self->{BITMASK}) = $self->{PBG}->render_pixmap_and_mask(1);
  my $widget = Gtk2::DrawingArea->new;
  $widget->size($self->{PBG}->get_width, $self->{PBG}->get_height);
  $self->{WIDGET} = $widget;
  $widget->add_events([qw[button_press_mask pointer_motion_mask button_release_mask]]);
  $widget->signal_connect(expose_event => \&dial_expose_handler, $self);
  $widget->signal_connect(button_press_event => \&dial_press_handler, $self);
  $widget->signal_connect(button_release_event => \&dial_release_handler, $self);
  $widget->signal_connect(motion_notify_event => \&dial_motion_handler, $self);
  $self->{ADJ}->signal_connect(changed => sub{$self->redraw_pixmap;});
  $self->{ADJ}->signal_connect(value_changed => sub{$self->redraw_pixmap;});
  bless($self, $class);
  $self->resize($self->{width}, $self->{height});
  $self->redraw_pixmap;
  $self->test_refresh(300, \&refresh_widget);
  $self->{lazy_draw_id} = Glib::Timeout->add(300, sub {$self->actual_redraw_pixmap;});
  return $self;
}

sub dial_expose_handler
{
  my ($widget, $event, $self) = @_;
  $self->redraw_pixmap() unless defined $self->{PIXMAP};
  my $rect = $event->area;
  my $gc = Gtk2::Gdk::GC->new($widget->window);
  $gc->set_clip_mask($self->{BITMASK});
  $gc->set_clip_origin(0,0);
  $widget->window->draw_drawable($gc,
                                 $self->{PIXMAP},
                                 $rect->x, $rect->y,
                                 $rect->x, $rect->y,
                                 $rect->width, $rect->height
                                );
}

sub redraw_pixmap
{
  my $self = shift;
  $self->{NEW_DATA} = 1;
}

sub actual_redraw_pixmap
{
  my $self = shift;
  return 1 unless $self->{NEW_DATA};
  $self->{NEW_DATA} = 0;
  my $widget = $self->{WIDGET};
  my $adj = $self->{ADJ};
  my $value = $adj->value;
  $value = 0 unless defined $value;
  $value = $adj->lower if $value < $adj->lower;
  $value = $adj->upper if $value > $adj->upper;
  #print "da lower=".$adj->lower." upper=".$adj->upper." value=$value old=$self->{value}\n";
  return 1 if defined($self->{value}) and $self->{value} == $value;
  $self->{value} = $value;
  my $angle = (7. * PI / 6. - ($value - $adj->lower) * 4. * PI
                       / 3. / ($adj->upper - $adj->lower));
  $self->{angle} = $angle;
  my $w = $self->{PBG}->get_width;
  my $h = $self->{PBG}->get_height;
  my $xc = $w / 2;
  my $yc = $h / 2;
  my $radius = ($w < $h ? $w : $h) * 0.45;
  my $pointer_width = $radius / 5;
  my $s = sin($angle);
  my $c = cos($angle);
  my $increment = 100 * PI / ($radius * $radius);
  my $inc = $adj->upper - $adj->lower;
  #$inc = 1 if $inc == 0;
  return 1 if $inc <= 0;
  $inc *= 10. while $inc < 100;
  $inc /= 10. while $inc >= 1000;
  my ($pixmap, $bitmap) = ($self->{PIXMAP}, $self->{BITMASK});
  my $gc = Gtk2::Gdk::GC->new($pixmap);
  $pixmap->draw_pixbuf($gc, $self->{PBG},
                       0, 0,  0, 0, $w, $h,
                       "none", 0, 0);
  $gc->set_line_attributes(2, "solid", "round", "miter");
  my %color = ( tick1   => "#99ff99",
                tick2   => "#99cc99",
                pointer => "#ffcc33",
                contour => "#660000" );
  for my $c (keys %color) {
    $color{$c} = Gtk2::Gdk::Color->parse($color{$c});
    $gc->get_colormap->alloc_color($color{$c}, 0, 0);
  }
  my $last = -1;
  for (my $i = 0; $i <= $inc; $i++) {
    my $theta = $i * PI / (18. * $inc / 24.) - PI / 6.;
    next if $theta - $last < $increment;
    $last = $theta;
    my $tick_length;
    if (($i % int($inc/10)) == 0) {
      $tick_length = $pointer_width;
      $gc->set_foreground($color{tick1});
    } else {
      $tick_length = $pointer_width / 2;
      $gc->set_foreground($color{tick2});
    }
    $s = sin($theta);
    $c = cos($theta);
    $pixmap->draw_line($gc,
                       $xc + $c * ($radius - $tick_length),
                       $yc - $s * ($radius - $tick_length),
                       $xc + $c * $radius,
                       $yc - $s * $radius
                      );
  }
  $s = sin($angle);
  $c = cos($angle);
  my @points = ($xc + $s * $pointer_width / 2,
                $yc + $c * $pointer_width / 2,
                $xc + $c * $radius,
                $yc - $s * $radius,
                $xc - $s * $pointer_width / 2,
                $yc - $c * $pointer_width / 2,
                $xc - $c * $radius / 10,
                $yc + $s * $radius / 10,
                $xc + $s * $pointer_width / 2,
                $yc + $c * $pointer_width / 2
               );
  $gc->set_foreground($color{pointer});
  $pixmap->draw_polygon($gc, 1, @points);
  $gc->set_line_attributes(2, "solid", "round", "miter");
  $gc->set_foreground($color{contour});
  $pixmap->draw_polygon($gc, 0, @points);
  $gc->get_colormap->free_colors(values %color);
  set_value(undef, $self, $value);
  return 1 unless defined $widget->window;
  my $rect = Gtk2::Gdk::Rectangle->new(0,0,$w,$h);
  $widget->window->invalidate_rect($rect, 0);
  return 1;
}

sub update_mouse
{
  my ($self, $x, $y) = @_;
  my $widget = $self->{WIDGET};
  my $adj = $self->{ADJ};
  my $xc = $widget->allocation->width / 2;
  my $yc = $widget->allocation->height / 2;
  my $angle = atan2($yc-$y, $x-$xc);                                                                                        
  $angle += PI*2 if $angle < - PI/2.;
  $angle = - PI/6 if $angle < - PI/6;
  $angle = 7.*PI/6. if $angle > 7.*PI/6.;
  my $value = ($adj->lower + (7.*PI/6 - $angle)
               * ($adj->upper - $adj->lower) / (4.*PI/3.));
  $adj->set_value($value);
  $self->{NEW_DATA} = 1;
  $self->actual_redraw_pixmap;
}

sub dial_press_handler
{
  my ($widget, $event, $self) = @_;
  return 0 unless $event->button == 1;
  my $w = $widget->allocation->width;
  my $h = $widget->allocation->height;
  my $radius = ($w < $h ? $w : $h) * 0.45;
  my $pointer_width = $radius / 5;
  my $dx = $event->x - $w / 2;
  my $dy = $h / 2 - $event->y;
  my $angle = $self->{angle};
  my $s = sin($angle);
  my $c = cos($angle);
  my $d_parallel = $s * $dy + $c * $dx;
  my $d_perpendicular = abs($s * $dx - $c * $dy);
  if (not $self->{button} and
      $d_perpendicular < $pointer_width/2 and
      $d_parallel > - $pointer_width)
    {
      $self->{button} = $event->button;
      $self->update_mouse($event->x, $event->y);
    }
  return 0;
}

sub dial_release_handler
{
  my ($widget, $event, $self) = @_;
  return 0 unless $self->{button} and $self->{button} == $event->button;
  undef $self->{button};
  $self->update_mouse($event->x, $event->y);
}

sub dial_motion_handler
{
  my ($widget, $event, $self) = @_;
  return 0 unless $self->{button};
  $self->update_mouse($event->x, $event->y);
}

sub refresh_widget
{
  my $self = shift;
  my $val;
  if ($Optikus::Canvas::Widget::TEST) {
    $val = make_test_value($self->{ADJ}->lower, $self->{ADJ}->upper, 7);
  } else {
    $val = $self->{VAR}->[0];
  }
  $self->{ADJ}->set_value($val);
}

sub set_value
{
  my ($widget, $self, $val) = @_;
  return unless defined($self->{VAR}->[0]);
  return if $self->{VAR}->[0] == $self->{ADJ}->value;
  #printf ("dial:set_value(old=%d,new=%d)\n", $self->{VAR}->[0], $self->{ADJ}->value);
  $self->{VAR}->[0] = $self->{ADJ}->value;
}

sub resize
{
  my ($self, $w, $h) = @_;
  return 1 unless $w > 1 and $h > 1;
  my $pixbuf = $self->{PBG0};
  $self->{PBG} = $pixbuf->scale_simple($w, $h, "bilinear")
      unless $w == $pixbuf->get_width and $h == $pixbuf->get_height;
  ($self->{PIXMAP}, $self->{BITMASK}) = $self->{PBG}->render_pixmap_and_mask(1);
  $self->{WIDGET}->size($w, $h);
  undef $self->{value};
  $self->redraw_pixmap;
  $self->{width} = $w;
  $self->{height} = $h;
  return 1;
}

1;
