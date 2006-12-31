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
package Optikus::Forms::Widget::MultiString;

use strict;
use Gtk2;

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
  my $self = $class->SUPER::new(%arg);
  my $label = new Gtk::Label("N/A");
  $self->{WIDGET} = $label;
  $self->{txthash} = ();
  for (split "\n", $arg{vlist}) {
    if ( /(\d+)\s*=>\s*(.*)/ ) {
      $self->{txthash}->{$1} = $2;
      print "$1 : $2\n";
    }
  }
  bless ($self, $class);
  return $self;
}

sub refresh_widget
{
  my $self = shift;
  my $string;
  my $nv = $self->{VAR}->[0];
  my $txt = $self->{txthash}->{$nv} || "No text";
  $self->{WIDGET}->set_text($txt) if defined $self->{WIDGET}; 
}

sub set_value
{
  my $widget = shift;
  my $self = shift;
  print "Illegal call: set_value() for Optikus::Forms::Widget::MultiString\n";
  return;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "vlist",
            type => "mstring",
          }
         );
}

1;

