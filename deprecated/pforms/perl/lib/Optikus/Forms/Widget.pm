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
package Optikus::Forms::Widget;

use strict;

use Gtk2;
use Optikus::Forms::Widget::Label;
use Optikus::Forms::Widget::Pixmap;
use Optikus::Forms::Widget::Bar;
use Optikus::Forms::Widget::Control;
use Optikus::Forms::Widget::Custom;
use Optikus::Forms::WidgetActive;
use Optikus::Forms::WidgetPeriodic;
use Optikus::Forms::Util;
use Optikus::Watch;

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS %XPM);
  @ISA  = qw(Exporter);
  @EXPORT  = qw(&forget);
  %EXPORT_TAGS = ( );
}

our $SIMULATION = 0;
our $TEST = 0;

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = {};
  $arg{posx} = 0 if not defined $arg{posx};
  $arg{posy} = 0 if not defined $arg{posy};
  for my $a (keys %arg) { $self->{$a} = $arg{$a}; }
  bless ($self, $class);
  return $self;
}

sub forget
{
  my $self = shift;
  # print "forgetting $self\n";
  $self->{exiting} = 1;
  Glib::Source->remove($self->{timeout_id}) if $self->{timeout_id};
  undef $self->{timeout_id};
  Glib::Source->remove($self->{lazy_draw_id}) if $self->{lazy_draw_id};
  undef $self->{lazy_draw_id};
}

sub DESTROY
{
  my $self = shift;
  $self->{WIDGET}->destroy;
}

sub test_refresh
{
  my ($self, $timeout, $func, $param) = @_;
  return unless $Optikus::Forms::Widget::TEST;
  $timeout = 200 unless $timeout > 0;
  $self->{timeout_id} = Glib::Timeout->add ($timeout,
                          sub {
                            &$func ($self, $param);
                            return 1;
                          });
}

sub get_attr_list
{
  return (
          {
            attr => "posx",
            type => "int",
          },
          {
            attr => "posy",
            type => "int",
          },
          {
            attr => "width",
            type => "int",
          },
          {
            attr => "height",
            type => "int",
          },
          {
            attr => "tooltip",
            type => "string",
          }
         );
}

sub refresh_widget
{
  die "STUB method called\n";
}

sub set_tip
{
  my ($self, $tip) = @_;
  $self->{_window}->{tooltips}->set_tip($self->{WIDGET}, $tip)
      if defined $tip;
}

sub write_byname
{
  my ($self, $name, $value) = @_;
  return if $Optikus::Forms::Widget::SIMULATION;
  Optikus::Watch::write($name, $value);
}

sub log
{
  my $self = shift;
  print join(' ',@_)."\n";
}

sub resize
{
  my ($self, $width, $height, $x, $y) = @_;
  return 0;
}

sub update_appearance
{
}

1;

