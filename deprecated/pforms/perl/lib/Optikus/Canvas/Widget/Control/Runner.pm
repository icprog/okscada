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
package Optikus::Canvas::Widget::Control::Runner;

use strict;
use Gtk2;
use Optikus::Canvas::Util;
use Optikus::Log ':all';
use Optikus::Watch ':all';

BEGIN 
{
  use Exporter ();
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  @ISA  = qw(Optikus::Canvas::Widget::Control);
  %EXPORT_TAGS = ( );
}

sub new
{
  my ($proto, %arg) = @_;
  my $class = ref($proto) || $proto;
  my $self = $class->SUPER::new(%arg);
  my $button = create_hor_button($arg{Label});
  $self->{WIDGET} = $button;
  $button->signal_connect("clicked", \&do_run, $self);
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
         type => "glabel",
       },
       {
         attr => "Command",
         type => "string",
       }
      );
  my $n = 0;
  for my $a (@attrl) {
    if ($a->{attr} eq "var") { splice @attrl, $n, 1; }
    $n++;
  }
  return @attrl;
}

sub do_run
{
  my ($button, $self) = @_;
  my $cmd = $self->{Command};
  if ($Optikus::Canvas::Widget::SIMULATION) {
    print "Execute [$cmd]\n";
    return;
  }
  if (not defined($cmd) or length($cmd) == 0) {
    print "Invalid command in $self\n";
    return;
  }
  my $msg = "";
  local $SIG{__WARN__} = sub { $msg = $_[0]; };
  my $rc = eval($cmd);
  if ($msg eq "") {
    olog "execute [$cmd] ".($rc?"OK":"ERROR");
  } else {
    olog "execute [$cmd] FAILURE [$msg]";
  }
}

1;
