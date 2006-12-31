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
package Optikus::Forms::Widget::Label;

use strict;
use Gtk2;

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
  $arg{Label} =~ s/\\n/\n/g if defined $arg{Label};
  my $label = new Gtk2::Label($arg{Label});
  $label->set_justify("center");
  $self->{WIDGET} = $label;
  $self->update_appearance;
  bless ($self, $class);
  return $self;
}

sub get_attr_list
{
  my $self = shift;
  return ($self->SUPER::get_attr_list,
          {
            attr => "Label",
            type => "string",
          },
          {
            attr => "font",
            type => "font",
          },
          {
            attr => "color",
            type => "color",
          }
         );
}

sub update_appearance
{
  my ($self, $force, %attr) = @_;
  if ($force) {
    for my $a (keys %attr) { $self->{$a} = $attr{$a}; }
  }
  my $widget = $self->{WIDGET};
  return unless defined $widget;
  my $parent = $widget->parent;
  $attr{font} = $self->{font} if length($self->{font}) > 0;
  if (length($attr{font}) > 0) {
    $widget->modify_font (Gtk2::Pango::FontDescription->from_string($attr{font}));
  } elsif (defined $parent) {
    $widget->modify_font ($parent->style->font_desc);
  }
  $attr{color} = $self->{color} if length($self->{color}) > 0;
  if (length($attr{color}) > 0) {
    $widget->modify_fg ("normal", Gtk2::Gdk::Color->parse($attr{color}));
  } elsif (defined $parent) {
    $widget->modify_fg ("normal", $parent->style->fg("normal"));
  }
  return 1;
}

1;

