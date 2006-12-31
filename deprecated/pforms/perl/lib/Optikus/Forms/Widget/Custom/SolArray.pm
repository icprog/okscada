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
package Optikus::Forms::Widget::Custom::SolArray;

use strict;
use Gtk2;
use Optikus::Forms::Util;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Forms::Widget::Custom Optikus::Forms::WidgetActive);
  %EXPORT_TAGS = ( );
}

use constant PI => 3.14159265359;

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  my $number = $arg{number};
  return undef unless ($number == 2 or $number == 4);
  $self->{n} = $arg{number};
  $self->{PBG}  = create_pic("png/sb_${number}.png");
  $self->{SUN} = create_pic("png/sb_sun.png");
  my ($w, $h) = ($self->{PBG}->get_width, $self->{PBG}->get_height);
  die "sun battery should be 128x128\n" unless ($w == 128 and $h == 128);
  $self->{WIDGET} = create_image($self->{PBG});
  $self->{WIDGET}->show;
  $self->varbind('sim@UGOL_'.$number, 
                 'tc@GTIFV_ESB[0]',
                 'tc@GTIFV_ESB[1]',
                 'MIIO_M7.B._10',
                 'sim@TRANS_UPOR'.$number,
                 'sim@TRANS_SB'.$number.'0',
                 'sim@TRANS_SB'.$number.'112'
                );
  bless ($self, $class);
  $self->test_refresh (200, \&refresh_widget);
  ($self->{PIXMAP}, $self->{BITMAP}) = $self->{PBG}->render_pixmap_and_mask(1);
  $self->redraw_pixmap (0, 0, 0, 0, 0, 0);
  $self->{lazy_draw_id} = Glib::Timeout->add(2000, sub {$self->actual_redraw_pixmap;});
  return $self;
}

sub redraw_pixmap
{
  my $self = shift;
  $self->{NEW_DATA} = [ @_ ];
}

sub actual_redraw_pixmap
{
  my $self = shift;
  return unless defined $self->{NEW_DATA};
  my ($batt, $s1, $s2, $night, $upor, $zone) = @{$self->{NEW_DATA}};
  $self->{NEW_DATA} = undef;
  undef $self->{NEW_DATA};
  #print "actually redrawing\n";
  my ($pixmap, $bitmap) = ($self->{PIXMAP}, $self->{BITMAP});
  my $gc = Gtk2::Gdk::GC->new($pixmap);
  my @color;
  for ("#ffffff",
       "#ffffff",
       "#d64e4e",
       "#009cea",
       "#004e9c",
       "#00fd00",
       "#ffff00"
       )
  {
    my $c = Gtk2::Gdk::Color->parse($_);
    $gc->get_colormap->alloc_color ($c, 0, 0);
    push @color, $c;
  }
  my %color = ( red    => "#d64e4e",
                aqua   => "#009cea",
                blue   => "#004e9c",
                grass  => "#00fd00",
                yellow => "#ffff00",
              );
  for my $c (keys %color) {
    $color{$c} = Gtk2::Gdk::Color->parse($color{$c});
    $gc->get_colormap->alloc_color ($color{$c}, 0, 0);
  }
  my $angle    =  -$batt * (2 * PI) + PI/2;
  $angle += 2*PI while $angle < 0;
  $angle -= 2*PI while $angle > 2 * PI;
  my $nz =  int(($angle + PI/16) / PI * 8); 
  my $z1 = $nz / 16 * 2 * PI - PI / 16;
  my $z2 = $nz / 16 * 2 * PI + PI / 16;
  $s1 *= -1 if $self->{n} == 4;
  $s1 = 64 - 8  + $s1 * 40;
  $s2 = 64 - 8  - $s2 * 40;
  $gc->set_line_attributes (3, "solid", "round", "miter");
  $pixmap->draw_pixbuf ($gc, $self->{PBG},
                        0, 0,  0, 0, 128, 128,
                        "none", 0, 0);
  $gc->set_foreground ($color{$zone ? 'yellow' : 'aqua'});
  $pixmap->draw_polygon($gc, 1,
                        63, 63,
                        63 + cos($z2)*43, 63 + sin($z2) * 43,
                        63 + cos($z1)*43, 63 + sin($z1) * 43);
  $gc->set_foreground ($color{blue});
  $pixmap->draw_line($gc,
                     63 + cos($z1)*62, 63 + sin($z1) * 62,
                     63, 63);
  $pixmap->draw_line($gc,
                     63 + cos($z2)*62, 63 + sin($z2) * 62,
                     63, 63);
  $pixmap->draw_pixbuf($gc, $self->{SUN},
                       0, 0,  $s1, $s2,  16, 16,
                       "none", 0, 0)
      unless $night;
  $gc->set_foreground ($color{grass});
  $pixmap->draw_line($gc,
                     63 + cos($angle)*62, 63 + sin($angle) * 62,
                     63, 63);
  $gc->set_foreground ($color{red});
  $pixmap->draw_polygon ($gc, 1,
                         63, 63,
                         53, 125,
                         42, 121)
      if $upor;
  $gc->get_colormap->free_colors (values %color);
  $self->{WIDGET}->set_from_pixmap ($pixmap, $bitmap);
  return 1;
}

sub refresh_widget
{
  my $self = shift;
  return 0 if exists $self->{'exiting'};
  my ($batt, $s1, $s2, $night, $upor, $z1, $z2, $zone);
  if ($Optikus::Forms::Widget::TEST) {
    $batt = int(make_test_value (0, 360, 31));
    $s1 = make_test_value (0, 1, 37);
    $s2 = make_test_value (0, 1, 39);
    $night = make_test_value (0, 9, 14) < 1 ? 1 : 0;
    $upor = int(make_test_value (0, 1.9, 35));
    $z1 = int(make_test_value (0, 2, 36));
    $z2 = int(make_test_value (0, 2, 37));
  } else {
    ($batt, $s1, $s2, $night, $upor, $z1, $z2) = @{$self->{VAR}};
  }
  $zone = $z1 || $z2;
  #print "sosb_$self->{number} batt=$batt s1=".sprintf("%.2f",$s1)." s2="
  #      .sprintf("%.2f",$s2)." night=$night upor=$upor z1_SB0=$z1 z2_SB112=$z2 zone=$zone\n";
  $self->redraw_pixmap($batt / 360.0, $s1, $s2, $night, $upor, $zone);
  return 1;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr  => 'number',
            type  => 'integer',
          }
         );
}

1;

