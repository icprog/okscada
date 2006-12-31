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
package Optikus::Forms::WidgetActive;

use strict;

use Optikus::Forms::Widget::Toggle;
use Optikus::Forms::Widget::Range;
use Optikus::Forms::Widget::String;
use Optikus::Forms::Widget::MultiString;
use Optikus::Forms::Widget::LED;
use Optikus::Forms::Widget::Meter;
use Optikus::Forms::Widget::ManoMeter;
use Optikus::Forms::Widget::Anim;

use Optikus::Watch;

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

  $arg{var} =~ s/^\s+// if defined $arg{var};
  $arg{var} =~ s/\s+$// if defined $arg{var};
  $self->varbind(split(/[\s,]+/, $arg{var})) if defined($arg{var});

  return $self;
}

sub forget
{
  my $self = shift;
  $self->SUPER::forget();
  $self->unbind;
}

sub DESTROY
{
  my $self = shift;
  $self->{WIDGET}->destroy;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "readonly",
            type => "boolean",
          },
          {
             attr => "var",
             type => "string",
          }
         );
}

sub varbind
{
  my ($self, @param) = @_;
  $self->{VAR} = ();
  return if $Optikus::Forms::Widget::SIMULATION;
  my $func = $self->can('refresh_widget');
  for my $n (0 .. $#param) {
    my $name = $param[$n];
    $self->{VARNAME}->[$n] = $name;
    my $rc = tie $self->{VAR}->[$n], 'Optikus::Watch', $name, $func, $self; # CORRECT?
    print "monitor name=[$name] func=[$func] rc=[$rc]\n"
	if $DEBUG;
  }
}

sub unbind
{
  my $self = shift;
  return if $Optikus::Forms::Widget::SIMULATION;
  my $func = $self->can('refresh_widget');
  for my $n (0 .. $#{$self->{VAR}}) {
    my $name = $self->{VARNAME}->[$n];
    #undef $self->{VAR}->[$n];
    print "unmonitor name=[$name] self=[$self]\n"
	if $DEBUG;
  }
  undef $self->{VAR};
}

sub refresh_widget
{
  die "STUB method called";
}

1;

