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
#  Utilities for the PForms widget library.
#
package Optikus::Forms::Util;

use strict;
use utf8;
use open ':utf8';
use Carp;
use Gtk2;
use Time::HiRes;
use Optikus::Log;

our ($root_dir, $pforms_home, $pic_home, $fmt_home, $etc_home);

BEGIN
{
  use Exporter;
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $TICK);
  $VERSION = 0.1;
  @ISA  = qw(Exporter);
  @EXPORT  = qw(
				&pforms_gtk_init
				&create_pic
				&create_image
				&create_tool
				&create_hor_label
				&create_hor_button
				&new_tool
				&find_widget_by_xy
				&recalc_xy
				&run_dialog
				&create_main_popup
				&close_extra_popups
				&parse_simple_xml
				&get_time_of_day
				&make_test_value
				&create_file_sel
				&create_font_sel
				&create_color_sel
				&is_true
				$root_dir
				$pforms_home
				$pic_home
				$fmt_home
				$etc_home
			);
  @EXPORT_OK = qw( );
}

$root_dir = $ENV{OPTIKUS_HOME};
$pforms_home = $root_dir;
$pic_home = "$pforms_home/share/pic";
$fmt_home = "$pforms_home/share/forms";
$etc_home = "$pforms_home/etc";

my $debug = 0;

sub pforms_log_file
{
  my $log_home = "$root_dir/var/log";
  my $log_file = ((-d $log_home) ? "$log_home/pforms.log" : "");
  return $log_file;
}

sub pforms_gtk_init
{
  my $client = shift;
  $client = "PFORMS" unless defined $client;
  Optikus::Log::open($client, pforms_log_file(), 4096);
  Gtk2->init;
  my $theme_home = "$pforms_home/share/themes";
  my $gtkrc;
  $gtkrc = "$theme_home/default/gtkrc" unless -r $gtkrc;
  $gtkrc = "$theme_home/simple/gtkrc" unless -r $gtkrc;
  die "cannot find gtkrc [$gtkrc]\n" unless -r $gtkrc;
  Gtk2::Rc->parse($gtkrc);
}

# =========================================
#
#    images
#
#

my %pic_cache;

sub create_pic
{
  my $file = shift;
  my $path = "$pic_home/$file";
  unless (-r $path) {
    print "pforms picture not found [$path]\n";
    $path = "$pic_home/tools/red_ex.png";
    die "substitute not found [$path]\n" unless -r $path;
  }
  $pic_cache{$path} = Gtk2::Gdk::Pixbuf->new_from_file($path)
      unless $pic_cache{$path};
  return $pic_cache{$path};
}

sub create_image
{
  return Gtk2::Image->new_from_pixbuf(create_pic(shift));
}

sub create_tool
{
  my ($tools, $tip, $pic, $func) = @_;
  $tools->append_item(undef,$tip,undef, create_image($pic), $func);
}

sub create_hor_label
{
  my $desc = shift;
  my ($hbox, $label, $lp, $rp, $lpic, $rpic);
  $hbox = Gtk2::HBox->new(0,1);
  if ($desc =~ m/^\s*pic\(\s*(.+?)\s*\)\s?(.*)$/) {
    ($lp, $desc) = ($1, $2);
    $lpic = Gtk2::Image->new_from_pixbuf(create_pic($lp));
  }
  if ($desc =~ m/^(.*?)\s?pic\(\s*(.+?)\s*\)\s*$/) {
    ($desc, $rp) = ($1, $2);
    $rpic = Gtk2::Image->new_from_pixbuf(create_pic($rp));
  }
  $desc =~ s/\\n/\n/g if defined $desc;
  $label = Gtk2::Label->new($desc);
  $label->set_justify("center");
  $hbox->pack_start($lpic, 1, 0, 1) if defined $lpic;
  $hbox->pack_start($label, 1, 0, 1) if defined $label;
  $hbox->pack_end($rpic, 1, 0, 1) if defined $rpic;
  return $hbox;
}

sub create_hor_button
{
  my $desc = shift;
  my ($button, $hbox, $vbox, $label, $lp, $rp, $mp, $lpic, $rpic, $mpic);
  if ($desc =~ m/^\s*pic\(\s*(.+?)\s*\)\s?(.*)$/) {
    ($lp, $desc) = ($1, $2);
    $lpic = create_image($lp);
  }
  if ($desc =~ m/^(.*?)\s?pic\(\s*(.+?)\s*\)\s*$/) {
    ($desc, $rp) = ($1, $2);
    $rpic = create_image($rp);
  }
  if (defined($lpic) || defined($rpic)) {
    $hbox = Gtk2::HBox->new(0,2);
  }
  $rp = $desc;
  while ($rp =~ m/^(.*?)\s?pic\(\s*(.+?)\s*\)\s?(.*?)$/)
  {
    $vbox = Gtk2::VBox->new(0,2) unless defined $vbox;
    ($lp, $mp, $rp) = ($1, $2, $3);
    $lp = "" if $lp eq ".";
    if (length($lp) > 0) {
      $lp =~ s/\\n/\n/g;
      $label = Gtk2::Label->new($lp);
      $label->set_justify("center");
      $vbox->pack_start($label, 1, 0, 2);
    }
    $mpic = create_image($mp);
    $vbox->pack_start($mpic, 1, 0, 2) if defined $mpic;
    $rp = "" if $rp eq ".";
  }
  undef $label;
  if (length($rp) > 0)
  {
    $rp =~ s/\\n/\n/g;
    $rp = " " if $rp eq ".";
    $label = Gtk2::Label->new($rp);
    $label->set_justify("center");
    if (defined $vbox) {
      $vbox->pack_end($label, 1, 0, 2);
      undef $label;
    }
  }
  if (defined $hbox)
  {
    $hbox->pack_start($lpic, 1, 0, 2) if defined $lpic;
    undef $lpic;
    $hbox->pack_start($label, 1, 0, 2) if defined $label;
    undef $label;
    $hbox->pack_end($vbox, 1, 0, 4) if defined $vbox;
    undef $vbox;
    $hbox->pack_end($rpic, 1, 0, 2) if defined $rpic;
    undef $rpic;
  }
  $button = new Gtk2::Button;
  $button->add($hbox) if defined $hbox;
  $button->add($vbox) if defined $vbox;
  $button->add($label) if defined $label;
  return $button;
}

sub set_window_icon
{
  my ($win, $pic) = @_;
  croak "uname not found\n" unless -x "/bin/uname";
  my $os = `/bin/uname -s 2>/dev/null`;
  return if ($os =~ /sunos/i);
  $win->window->set_icon(undef, create_pic($pic)->render_pixmap_and_mask(1));
}

# =========================================
#
#    dialogs
#
#

sub run_dialog
{
  my ($title, $message, $func, @buttons) = @_;
  return unless defined $func;
  my ($x, $y, $dialog, $label, $frame,
      $hbox, $vbox0, $vbox, $button, $pic);
  $dialog = new Gtk2::Window("toplevel");
  $title = "Confirmation" unless defined $title;
  $dialog->set_title($title);
  $dialog->set_position("mouse");
  $vbox0 = new Gtk2::VBox(0,0);
  $dialog->add($vbox0);
  $frame = new Gtk2::Frame("");
  $vbox0->add($frame);
  $vbox = new Gtk2::VBox(0,0);
  $frame->add($vbox);
  $hbox = new Gtk2::HBox(0,0);
  $vbox->pack_start($hbox, 0, 1, 2);
  $hbox->pack_start(Gtk2::Label->new("    "), 0, 0, 0);
  $hbox->pack_start(Gtk2::Image->new_from_pixbuf(
                                    create_pic("tools/help.png")), 0, 0, 0);
  $hbox->pack_start(new Gtk2::Label("    "), 0, 0, 0);
  $label = new Gtk2::Label("\n$message\n");
  $label->set_alignment(1, 0);
  $hbox->pack_start($label, 1, 1, 0);
  $hbox->pack_end(Gtk2::Label->new("    "), 0, 0, 0);
  $hbox = new Gtk2::HBox(0, 0);
  $vbox0->pack_end($hbox, 0, 1, 1);
  $hbox->pack_start(Gtk2::Label->new(""), 1, 1, 1);
  if ($#buttons < 0)  {
    push @buttons, "+pic(kpic/button_ok.png)   Yes  ";
    push @buttons, "-pic(kpic/button_cancel.png)   No   ";
  }
  for my $k (0 .. $#buttons) {
    my $desc = $buttons[$k];
    my $is_func = 0;
    if ($desc =~ /^\+(.*)/) {
      $desc = $1;
      $is_func = 1;
    }
    my $is_grab = 0;
    if ($desc =~ /^\-(.*)/) {
      $desc = $1;
      $is_grab = 1;
    }
    $button = create_hor_button($desc);
    $hbox->pack_start($button, 0, 1, 3);
    if ($is_func) {
      $button->signal_connect("clicked", sub {
          $dialog->destroy();
          &$func();
      });
    } else {
      $button->signal_connect("clicked", sub { $dialog->destroy(); } );
    }
    if ($is_grab) {
      $button->can_default(1);
      $button->grab_default();
    }
  }
  $hbox->pack_end(Gtk2::Label->new(""), 1, 1, 1);
  $dialog->show_all;
}


# =========================================
#
#    selection dialogs
#
#

my %fs_data;

sub file_selection_ok
{
  my $id = shift;
  my $fs = $fs_data{$id}{dialog};
  my $file = $fs->get_filename();
  $fs->destroy();
  return unless defined($file) and length($file) > 0;
  $fs_data{$id}{file} = $file;
  my $func = $fs_data{$id}{func};
  &$func ($file, $fs_data{$id}{home});
}

sub create_file_sel
{
  my ($id, $title, $file, $func) = @_;
  my $fs = new Gtk2::FileSelection($title);
  $fs_data{$id}{dialog} = $fs;
  $fs_data{$id}{func} = $func;
  $fs_data{$id}{home} = $file;
  my $prev_file = $fs_data{$id}{file};
  $file = $prev_file if defined $prev_file;
  $file .= "/" if -d $file;
  $fs->set_filename($file) if defined $file;
  $fs->set_position("mouse");
  $fs->signal_connect("destroy", sub { destroy $fs; } );
  $fs->signal_connect("delete_event", sub { destroy $fs; } );
  $fs->cancel_button->signal_connect("clicked", sub { destroy $fs; });
  $fs->ok_button->signal_connect("clicked", sub { file_selection_ok($id); } );
  $fs->set_modal(1);
  $fs->show_all;
}

sub create_font_sel
{
  my ($title, $font, $func) = @_;
  $title = "Choose Font..." unless defined $title;
  my $fsd = Gtk2::FontSelectionDialog->new($title);
  $fsd->set_position("mouse");
  $fsd->set_font_name($font) if length($font) > 0;
  $fsd->signal_connect('response' => sub {
            my (undef, $response) = @_;
            my $font = $fsd->get_font_name;
            $fsd->destroy;
            undef $font if $response ne 'ok';
            &$func ($font);
      });
  $fsd->show;
}

sub create_color_sel
{
  my ($title, $color, $func) = @_;
  $title = "Choose Color..." unless defined $title;
  my $csd = Gtk2::ColorSelectionDialog->new($title);
  $csd->set_position("mouse");
  my $cs = $csd->colorsel;
  if (length($color) > 0) {
    my $c = Gtk2::Gdk::Color->parse($color);
    $cs->set_previous_color($c);
    $cs->set_current_color($c);
  }
  $cs->set_has_palette(1);
  $csd->signal_connect('response' => sub {
        my (undef, $response) = @_;
        my $c = $cs->get_current_color;
        $csd->destroy;
        my $color = sprintf("#%02x%02x%02x",
                            $c->red >> 8, $c->green >> 8, $c->blue >> 8);
        undef $color unless $response eq 'ok';
        &$func ($color);
    });
  $csd->show;
}

# =========================================
#
#    coordinates
#
#

sub recalc_xy
{
  my ($x, $y, $widget, $area) = @_;
  while (defined($widget) and $widget ne $area)  {
    #print ">> append to ($x x $y) $widget (".$widget->allocation->x
    #      ." x ".$widget->allocation->y.")\n";
    $x += $widget->allocation->x;
    $y += $widget->allocation->y;
    $widget = $widget->parent;
  }
  #print "> got ($x x $y) $widget\n";
  return ($x, $y);
}

sub find_widget_by_xy
{
  my ($x, $y, $widget, $area, @widget_list) = @_;
  ($x, $y) = recalc_xy($x, $y, $widget, $area);
  for my $widget_record (@widget_list) {
    $widget = $widget_record->{WIDGET};
    if ($widget->allocation->x <= $x
        && $x < $widget->allocation->x + $widget->allocation->width
        && $widget->allocation->y <= $y
        && $y <= $widget->allocation->y + $widget->allocation->height)
    {
      return $widget_record;
    }
  }
  return undef;
}

# =========================================
#
#    trivial XML parser
#
#

sub parse_simple_xml
{
  my ($file, $param, $handler) = @_;
  my ($text, $elem, %attr);
  unless (open F, "<$file") {
    print "cannot open xml file [$file]\n";
    return 0;
  }
  while (<F>) {
    next if /^\s*\#/;
    while (!/^$/){
      if (s/^<(.*?)>//) {
        my $tag = $1;
        if ($tag =~ s/^\///) {
          $attr{$elem} = $text
            if defined($text) && length($text) > 0 && !defined($attr{$elem});
          undef $text;
          %attr = ()
              if &$handler ($file, $param, $tag, \%attr);
        } else {
          undef $text;
          $elem = $tag;
        }
      }
      if (s/^([^<]*)//) {
        $text .= $1;
      }
    }
  }
  close F;
  return 1;
}

# =========================================
#
#    miscellaneous
#
#

sub get_time_of_day
{
   return Time::HiRes::time();
}

sub make_test_value
{
  my ($min, $max, $speed) = @_;
  unless (defined $max) {
    $max = $min;
    $min = 0;
  }
  $speed = 2 unless defined $speed;
  return (sin(get_time_of_day() / $speed) + 1) * ($max - $min) / 2 + $min;
}

sub is_true
{
  my $var = shift;
  if ( defined($var)
       && ( $var eq "1" || $var =~ /^true$/i
            || $var =~ /^on$/i || $var =~ /^yes$/i))
  {
    return 1;
  }
  return 0;
}

sub plist
{
  my $plist = `/bin/ps -efa 2>/dev/null`;
  return $plist;
}

sub pgrep
{
  my ($app, $plist) = @_;
  croak "uname not found\n" unless -x "/bin/uname";
  my $os = `/bin/uname -s 2>/dev/null`;
  my @pids = ();
  if ($os =~ /linux/i) {
    croak "pgrep not found\n" unless -x "/usr/bin/pgrep";
    my $pids = `/usr/bin/pgrep $app 2>/dev/null`;
    $pids =~ s/^\s+//;
    $pids =~ s/\s+$//;
    @pids = split(/\s+/,$pids);
  } elsif ($os =~ /sunos/i) {
    unless (length($plist) > 0) {
      $plist = plist();
    }
    for (split(/\n/, $plist)) {
      chomp;
      my $prog = substr($_,47);
      $prog =~ s/^\s+//;
      $prog =~ s/^\S+\///;
      if ($prog =~ /^perl\s/) {
        $prog =~ s/^perl\s+//;
        $prog =~ s/^\S+\///;
      }
      $prog =~ s/\s+.*$//;
      /^\s+\S+\s+(\d+)\s+/;
      my $pid = $1;
      next unless $prog =~ /$app/;
      #print "line=[$_] pid=[$pid] prog=[$prog]\n";
      push @pids, $pid;
    }
  } else {
    croak "OS $os not supported\n";
  }
  if (wantarray) {
    return $#pids >= 0 ? @pids : undef;
  } else {
    return $#pids >= 0 ? 1 : 0;
  }
}

sub pkill
{
  my ($app, $sig) = @_;
  $sig = "term" unless defined($sig);
  $sig =~ s/^\-+//;
  $sig = uc($sig);
  croak "uname not found\n" unless -x "/bin/uname";
  my $os = `/bin/uname -s 2>/dev/null`;
  if ($os =~ /linux/i) {
    croak "pkill not found\n" unless -x "/usr/bin/pkill";
    my $out = `/usr/bin/pkill -$sig $app 2>/dev/null`;
    return ($? >> 8) == 0;
  } elsif ($os =~ /sunos/i) {
    my $found = 0;
    for (`/bin/ps -efa 2>/dev/null`) {
      chomp;
      my $prog = substr($_,47);
      $prog =~ s/^\s+//;
      $prog =~ s/^\S+\///;
      if ($prog =~ /^perl\s/) {
        $prog =~ s/^perl\s+//;
        $prog =~ s/^\S+\///;
      }
      /^\s+\S+\s+(\d+)\s+/;
      my $pid = $1;
      next unless $prog =~ /$app/;
      #print "line=[$_] pid=[$pid] prog=[$prog]\n";
      kill $sig, $pid;
      $found = 1;
    }
    return $found;
  } else {
    croak "OS $os not supported\n";
  }
}

1;

