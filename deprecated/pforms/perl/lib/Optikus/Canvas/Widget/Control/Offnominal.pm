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
package Optikus::Canvas::Widget::Control::Offnominal;

use strict;
use utf8;
use open ':utf8';
use Gtk2;
use Optikus::Canvas::Util;
use Optikus::Watch ':all';
use Optikus::Log ':all';

BEGIN
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::Widget::Control);
  %EXPORT_TAGS = ( );
}

my $offnominals_file = "$root_home/etc/offnominals.lst";
my %offnom_descriptions = ();
my $got_descriptions = 0;

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  my $button = Gtk2::ToggleButton->new;
  my $hbox = Gtk2::HBox->new(0,5);
  $button->set_size_request(270, 40);
  $self->{IMAGE_OFF} = create_pic("png/offnom_idle.png");
  $self->{IMAGE_ON} = create_pic("png/offnom_run.png");
  $self->{PIXMAP} = Gtk2::Image->new_from_pixbuf($self->{IMAGE_OFF});
  $hbox->pack_start($self->{PIXMAP}, 1, 1, 1);
  my $label = create_hor_label($arg{Label});
  $hbox->pack_start($label, 1, 1, 4);
  $hbox->show_all;
  $button->add($hbox);
  $self->{WIDGET} = $button;
  my $descr = $self->{descr};
  unless (length($descr) > 0) {
    unless ($got_descriptions) {
      my $count = 0;
      if (open(OFFNOM_DESCR, $offnominals_file)) {
        while (<OFFNOM_DESCR>) {
          next unless /^\s*(.*?)\s*==>\s*(.*)\s*$/;
          $offnom_descriptions{$1} = $2;
          $count++;
        }
      }
      close(OFFNOM_DESCR);
      olog "parsed $count offnominal descriptions";
      $got_descriptions = 1;
    }
    $descr = $offnom_descriptions{$self->{name}};
  }
  $descr =~ s/\s+/ /g;
  $descr =~ s/\\n/\n/g;
  $self->set_tip($descr) if length($descr) > 0;
  $button->signal_connect("clicked", \&switch_ns, $self);
  bless($self, $class);
  $self->resize($self->{width}, $self->{height});
  return $self;
}

sub get_attr_list
{
  my $self = shift;
  my @attrl = 
      ($self->SUPER::get_attr_list,
       {
         attr => "Label",
         type => "string",
       },
       {
         attr => "name",
         type => "string",
       },
       {
         attr => "descr",
         type => "string",
       }
      );
  my $n = 0;
  for (@attrl) {
    if ($_->{attr} eq "var") {
      splice @attrl, $n, 1;
    }
    $n++;
  }
  return @attrl;
}

sub switch_ns
{
  my ($button, $self) = @_;
  if ($Optikus::Canvas::Widget::SIMULATION) {
    $button->set_state("normal");
    return;
  }
  my $on = $button->get_active;
  $self->{PIXMAP}->set_from_pixbuf($self->{$on?"IMAGE_ON":"IMAGE_OFF"});
  return if $Optikus::Canvas::Widget::SIMULATION;
  my $file = $self->{name};
  $file =~ s/^\s+//;
  $file =~ s/\s+$//;
  $file =~ tr/[a-z][A-Z][0-9]\+\-/ /cs;
  $file =~ s/\s+/_/g;
  my $state = $on ? "on" : "off";
  $file = "${file}.ofn_${state}";
  olog "offnominal $state: $self->{name}";
  sample_script($file);
}


1;
