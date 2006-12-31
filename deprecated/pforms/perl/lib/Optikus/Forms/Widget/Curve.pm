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
package Optikus::Forms::Widget::Curve;

use strict;
use Gtk2;
use Optikus::Forms::Util;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Forms::WidgetPeriodic);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $param = shift;
  my $self = $class->SUPER::new(%arg);
  $arg{min} = 0 if $arg{min} == 0;
  $arg{max} = 100 if $arg{max} == 0;
  $arg{width} = 300 unless $arg{width} > 0;
  $arg{height} = 150 unless $arg{height} > 0;

  $self->{width} = $arg{width};
  $self->{height} = $arg{height};
  my $curve = $self->{WIDGET} = Gtk2::Curve->new;
  $curve->set_range(0, $arg{width}, 0, $arg{height});
  my ($i, @vec);
  for (1 .. $arg{width}) { push @vec, 0; }
  $curve->set_vector(@vec);
  $self->{VEC} = [@vec];
  $self->{WIDGET} = $curve;
  bless ($self, $class);
  #tie $self->{VAR}->[0], "Optikus::Forms::Ex", $arg{var}; FIXME
  $self->test_refresh(100,\&refresh_widget);
  return $self;
}

sub forget
{
  my $self = shift;
  $self->SUPER::forget();
  #untie $self->{VAR}->[0];  FIXME
}

sub refresh_widget
{
  my $self = shift;
  return unless defined $self->{VEC};
  return unless defined $self->{WIDGET};
  my ($span, $val, $nv);
  $self->{WIDGET}->set_curve_type('spline');
  shift @{$self->{VEC}};
  $span = $self->{max} - $self->{min};
  $span = 1 if $span == 0;
  #$val = $self->{VAR}->[0];
  $val = make_test_value ($self->{min}, $self->{max}, 5)
      if $Optikus::Forms::Widget::TEST;
  $nv = ($val - $self->{min}) / $span * $self->{height};
  push @{$self->{VEC}}, $nv;
  print "curve: span=$span val=$val nv=$nv\n" if $DEBUG;
  $self->{WIDGET}->set_vector(@{$self->{VEC}});
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr    => "min",
            type    => "double",
          },
          {
            attr    => "max",
            type    => "double",
          },
          {
            attr => "var",
            type => "string",
          }
         );
}

1;

