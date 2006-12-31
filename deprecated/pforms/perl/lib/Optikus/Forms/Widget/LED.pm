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
package Optikus::Forms::Widget::LED;

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

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my %valid_class = (
                     'd' => undef,
                     'b' => undef,
                     'G' => undef,
                     'cntr' => undef,
                    );
  my $self = $class->SUPER::new(%arg);
  my $subclass = $arg{subclass};
  $subclass = 'd' unless exists $valid_class{$subclass};
  for my $letter (0..9, "dot", "dash") {
    push @{$self->{digit}}, create_pic("png/${subclass}_${letter}.png");
  }
  my $hbox = new Gtk2::HBox(0, 1);
  for (1 .. length(sprintf $arg{format}, 0)) {
    my $pixmap = new Gtk2::Image->new_from_pixbuf($self->{digit}->[0]);
    push @{$self->{pixmap}}, $pixmap;
    $hbox->add($pixmap);
  }
  $hbox->show_all;
  $self->{WIDGET} = $hbox;
  bless ($self, $class);
  $self->test_refresh (500, \&refresh_widget);
  return $self;
}

sub refresh_widget
{
  my $self = shift;
  my $val;
  if ($Optikus::Forms::Widget::TEST) {
    $val = make_test_value(0,10**length(sprintf($self->{format},0))-1);
  } else {
    $val = $self->{VAR}->[0];
  }
  my @dig = split("", sprintf $self->{format}, $val);
  for my $k (0..$#dig) {
    my $n = $dig[$k];
    $n = 10 if $n eq ".";
    $n = 11 if $n eq "-";
    $n = 11 unless defined $n;
    $self->{pixmap}->[$k]->set_from_pixbuf($self->{digit}->[$n])
	if defined $self->{pixmap}->[$k];
  }
  return 1;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "format",
            type => "string",
          },
          {
            attr => "subclass",
            type => "string",
          }
         );
}

1;
