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
#  Perl bindings for Optikus.
#

package Optikus::Watch;

use strict;
use Carp;
use Gtk2;
use Time::HiRes qw(setitimer ITIMER_REAL);
use Optikus::Optikus;
use Optikus::Log ':all';

BEGIN
{
  use Exporter;
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  $VERSION = 0.1;
  @ISA  = qw(Exporter DynaLoader);
  @EXPORT  = qw(
               );
  %EXPORT_TAGS = (all => [ qw(
                              CC TC
                              begin_test end_test
                              tget tput
                              waits wait_for
                              send_ctlmsg send_immed count_acked
                              sample_script shell_script
                              form_exec form_script
                              send_uv send_packet
                             ) ] );
  @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
}


my (%monitors, %callbacks, %aliases);
my ($data_sub, $alive_sub, $idle_sub);
my ($gtk_mode, $root_dir, $test_name);
my $periodic;
my $def_timeout = -1;
my $DEBUG = 0;
my $inited = 0;


# ============== initialization & shutdown ================


sub init
{
  my (%a) = @_;	# hash of arguments

  for my $key (keys %a) {
    my $dkey = $key;
    $dkey =~ s/_/\-/g;
    $a{$dkey} = $a{$key};
    $dkey =~ s/\-/_/g;
    $a{$dkey} = $a{$key};
  }

  unless (exists($a{client_name}) && length($a{client_name}) > 0) {
    carp "client_name not supplied";
    return 0;
  }

  $root_dir = $ENV{OPTIKUS_HOME};
  $root_dir = $ENV{HOME} unless defined($root_dir) && length($root_dir) > 0;
  $gtk_mode = 0;
  if (defined(Gtk2::Gdk::DisplayManager->get->get_default_display)) {
    $gtk_mode = 1;
  }

  unless (exists($a{server}) && length($a{server}) > 0) {
    $a{server} = "localhost";
  }
  unless (exists($a{def_timeout}) && $a{def_timeout}) {
    $a{def_timeout} = 2000;
  }
  if (exists($a{verbosity})) {
    debug($a{verbosity});
  }

  %monitors = ();
  %callbacks = ();
  %aliases = ();

  $data_sub = $a{data_handler};
  $alive_sub = $a{alive_handler};
  setIdleHandler($a{idle_handler});
  $a{data_handler} = \&_data_handler;
  $a{alive_handler} = \&_alive_handler;
  $a{idle_handler} = \&_idle_handler;

  my $rc = watchHashInit(\%a);
  if ($rc == 0) {
    carp "cannot initialize package";
    exit();
    return 0;
  }

  undef $periodic if defined $periodic;
  if (defined($a{period}) && $a{period} > 0) {
    if ($gtk_mode) {
      $periodic = Glib::Timeout->add($a{period}, \&idle);
    } else {
      $SIG{ALRM} = \&idle;
      setitimer(ITIMER_REAL, $a{period}/1000.0, $a{period}/1000.0);
    }
  }

  $inited = 1;
  return 1;
}


sub exit
{
  if ($gtk_mode) {
    Glib::Source->remove($periodic) if defined $periodic;
  } else {
    delete($SIG{ALRM}) if exists($SIG{ALRM});
    setitimer(ITIMER_REAL, 0, 0);
  }
  undef $periodic;
  %monitors = ();
  undef $idle_sub;
  undef $data_sub;
  undef $alive_sub;
  watchExit();
  $inited = 0;
  return 1;
}


sub debug
{
  my $level = shift;
  $level = 2 unless defined $level;
  $DEBUG = $level < 0 ? 1 : 0;
  $level *= -1 if $level < 0;
  watchSetDebugging($level);
  return $level;
}


# ================= aliases ===================

sub unalias
{
  my $name = shift;
  my $alias = $aliases{$name};
  return $name unless defined $alias;
  olog "unaliased $name --> $alias";
  return $alias;
}

sub readAliasFile
{
  my $file = shift;
  open(ALIASES, "< $file") or return 0;
  my $count = 0;
  while (<ALIASES>) {
    chomp;
    next if /^\s*\#/;
    next if /^\s*$/;
    unless (/^\s*(\S+)\s+(\S+)\s*$/) {
      olog "invalid alias in file $file: [$_]";
      next;
    }
    $aliases{$1} = $2;
    $count++;
  }
  close(ALIASES);
  return $count;
}


# ================= read & write ===============


sub write
{
  my ($name, $value, $timeout) = @_;
  return unless $inited;
  $timeout = $def_timeout unless defined $timeout;
  watchLock();
  my $rc = watchWriteAsString($name, $value, $timeout);
  watchUnlock();
  return $rc;
}


sub read
{
  my ($name, $timeout) = @_;
  return unless $inited;
  $timeout = $def_timeout unless defined $timeout;
  watchLock();
  my $rc = watchReadByName($name, $timeout);
  watchUnlock();
  return $rc;
}


# ================ monitoring ===================


sub enable
{
  my $enable = shift;
  return unless $inited;
  watchLock();
  my $rc = watchEnableMonitoring($enable);
  watchUnlock();
  return $rc;
}


sub monitor
{
  my ($name, $func, $data, $timeout) = @_;
  return unless $inited;
  $data = 0 unless defined $data;
  $timeout = $def_timeout unless defined $timeout;
  my $ooid = info($name);
  unless ($ooid) {
    olog "cannot monitor name=[$name] ooid=[$ooid] data=[$data]\n"
         if $DEBUG;
    return 0;
  }
  unless (defined $monitors{$ooid}) {
    watchLock();
    my $rc = watchMonitor($ooid, $data, $timeout);
    watchUnlock();
    unless($rc) {
      olog "cannot monitor name=[$name] ooid=[$ooid] data=[$data]\n"
           if $DEBUG;
      return 0;
    }
    $monitors{$ooid} = ();
    $monitors{$ooid}->{name} = $name;
    $monitors{$ooid}->{funcs} = ();
    $monitors{$ooid}->{count} = 1;
  } else {
    olog "already monitored [$name]\n" if $DEBUG;
    watchLock();
    my $rc = watchRenewMonitor($ooid, $timeout);
    watchUnlock();
    $monitors{$ooid}->{count} = $monitors{$ooid}->{count} + 1;
  }
  if (defined $func) {
    push @{$monitors{$ooid}->{funcs}}, { func => $func, data => $data };
  }
  return $ooid;
}


sub unmonitor
{
  my ($name, $func, $data) = @_;
  return unless $inited;
  if ($name eq "*") {
    watchUnmonitor(0);
    return 1;
  }
  my $ooid = info($name);
  my $mon = $monitors{$ooid};
  unless ($ooid > 0 and defined $mon) {
    olog "cannot unmonitor name=[$name] ooid=[$ooid]\n"
         if $DEBUG;
    return 0;
  }
  my $count = 0;
  if (defined $func) {
    for my $i (0 .. $#{$mon->{funcs}})  {
      next if $mon->{funcs}->[$i]->{func} ne $func;
      next if $mon->{funcs}->[$i]->{data} ne $data and defined $data;
      splice(@{$mon->{funcs}}, $i, 1);
      $count++;
      $monitors{$ooid}->{count} = $monitors{$ooid}->{count} - 1;
    }
  } else {
    $count += $#{$mon->{funcs}};
    $mon->{funcs} = ();
    $monitors{$ooid}->{count} = $monitors{$ooid}->{count} - 1;
  }
  if ($monitors{$ooid}->{count} <= 0) {
    watchLock();
    my $rc = watchUnmonitor($ooid);
    watchUnlock();
    $count++ if $rc;
    undef $monitors{$ooid};
    olog "deleting monitor [$name]\n" if $DEBUG;
  }
  return $count;
}


sub _data_handler
{
  my ($data, $value, $undef, $ooid, $name, $type, $len) = @_;
  undef $value if $undef;
  olog "data_handler: ".
      "ooid=$ooid name=\"$name\" value=\"$value\" undef=$undef ".
      "type='$type' len=$len data=$data data_sub=$data_sub\n"
      if $DEBUG;
  foreach my $mon (@{$monitors{$ooid}->{funcs}}) {
    my $func = $mon->{func};
    next unless ref($func) eq "CODE";
    &$func ($mon->{data}, $value, $ooid, $name, $type, $len);
  }
  if ($data_sub) {
    &$data_sub ($data, $value, $ooid, $name, $type, $len);
  }
}


# ================= information ===================

sub info
{
  my ($xname, $timeout) = @_;
  $timeout = $def_timeout unless defined $timeout;
  my $name = unalias($xname);
  if (wantarray) {
    return unless $inited;
    watchLock();
    my @ret = watchGetInfo($name, $timeout);
    watchUnlock();
    return @ret;
  }
  return 0 unless $inited;
  watchLock();
  my ($ooid,@unused) = watchGetInfo($name, $timeout);
  watchUnlock();
  return defined $ooid ? $ooid : 0;
}


# ================= subject state ===================


sub _alive_handler
{
  my ($subject, $state) = @_;
  olog "alive_handler: [$subject] is [$state] alive_sub=$alive_sub\n"
      if $DEBUG;
  if ($alive_sub) {
    &$alive_sub ($subject, $state);
  }
}


sub subject
{
  my $subject = shift;
  return unless $inited;
  watchLock();
  my $rc = watchGetSubjectInfo($subject ? $subject : '');
  watchUnlock();
  return $rc;
}


# ================= idle ===================


sub _idle_handler
{
  &$idle_sub() if defined $idle_sub;
}


sub setIdleHandler
{
  return unless $inited;
  my $prev = $idle_sub;
  $idle_sub = shift;
  watchIdleEnable(defined($idle_sub) ? 1 : 0);
  return $prev;
}


sub idle
{
  my $timeout = shift;
  return unless $inited;
  $timeout = 0 unless $timeout;
  $timeout = 0 if $timeout eq "ALRM";
  watchLock();
  watchIdle($timeout);
  watchUnlock();
  watchLazyHandleEvents(1);
  return 1;
}


sub waitSec
{
  my $sec = shift;
  return 0 unless $inited;
  $sec = 0 unless $sec;
  return watchWaitSec($sec);
}


# ================= tied scalars  ===================


sub TIESCALAR($$)
{
  my ($class, $desc, $func, $data) = @_;
  return unless $inited;
  return undef unless defined $desc;
  my $self = ();
  $self->{desc} = $desc;
  $self->{ooid} = info($desc);
  unless ($self->{ooid}) {
    #carp "TIESCALAR: unknown desc [$desc]\n";
    return undef;
  }
  if (defined $func) {
    if (ref($func) eq 'CODE') {
      monitor($desc, $func, $data, $def_timeout);
      $self->{monitored} = 1;
      $self->{func} = $func;
      $self->{data} = $data;
      olog "monitoring [$desc] tied with func [$func]\n" if $DEBUG;
    } elsif ($func) {
      monitor($desc, undef, undef, $def_timeout);
      $self->{monitored} = 1;
      olog "monitoring [$desc] tied\n" if $DEBUG;
    }
  }
  bless($self, $class);
  return $self;
}


sub FETCH($)
{
  my $self = shift;
  return unless $inited;
  watchLock();
  my $val = watchReadByName($self->{ooid}, $def_timeout);
  watchUnlock();
  return $val;
}


sub STORE($$)
{
  my ($self, $val) = @_;
  return unless $inited;
  watchLock();
  my $rc = watchWriteAsString($self->{ooid}, $val, $def_timeout);
  watchUnlock();
  return $rc;
}


sub DESTROY($)
{
  my $self = shift;
  return unless $inited;
  if ($self->{monitored}) {
    olog "unmonitoring [$self->{desc}]\n" if $DEBUG;
    unmonitor($self->{desc}, $self->{func}, $self->{data});
    $self->{monitored} = 0;
    undef $self->{func};
    undef $self->{data};
  }
}


# =============== messages ================


sub sendImmedPacket
{
  my ($data, $nbytes) = @_;
  return 0 unless $inited;
  my $rc = owatchSendPacket($data, $nbytes);
  return $rc == 0 ? 1 : 0;
}


sub ackedPackets
{
  my $flush = shift;
  $flush = 0 unless $flush;
  return owatchGetAckCount($flush);
}


sub prepareCtlMsgFormat
{
  my $fmt = shift;
  $fmt = "" unless defined $fmt;
  $fmt =~ tr/\=\-\_/\%\%\%/;
  $fmt =~ s/(\%\w+)\((\d+)\)/$1 x $2/ge;
  return $fmt;
}


sub sendCtlMsg
{
  my ($dest, $klass, $type, $format, @args) = @_;
  return 0 unless $inited;
  # FIXME: dest is ignored
  $dest = "";
  $format = prepareCtlMsgFormat($format);
  return watchSendCtlMsgAsArray($dest, $klass, $type, $format, @args);
}


sub sendStrCtlMsg
{
  my ($dest, $klass, $type, $format, $all_args) = @_;
  return 0 unless $inited;
  # FIXME: dest is ignored
  $dest = "";
  $format = prepareCtlMsgFormat($format);
  $all_args = "" unless defined $all_args;
  return watchSendCtlMsgAsString($dest, $klass, $type, $format, $all_args);
}


# ================= waiting ===============


my @core_ops = ("sin","cos","abs","atan2","exp","hex",
                "int","log","oct","rand","sqrt",
                "rand","srand");
my %core_ops = ();
foreach (@core_ops) { $core_ops{$_} = 1; }


sub waitFor
{
  my ($cond, $timeout, $msg_ok, $msg_err) = @_;
  my $expr = $cond;
  my @names = ();
  my @vars = ();
  my @vals = ();
  my @offs = ();
  $expr =~ s/\s//g;
  study $expr;
  while ($expr =~ /^(.*?)((?<![\$\@\%])(?:[_a-zA-Z]\w*\@)?[_a-zA-Z][\w\[\]\.]*)(.*?)$/) {
    my ($left, $elem, $right) = ($1, $2, $3);
    $expr = $left.'#'.$right;
    push @offs, length($left);
    push @names, $elem;
    push @vals, 0;
    push @vars, 0;
    if (defined $core_ops{$elem}) {
      $names[$#names] = "";
      $vals[$#vals] = $elem;
      next;
    }
    my $rc = tie $vars[$#vars], "Optikus::Watch", $elem;
    unless ($rc) {
      olog "ERROR: cannot find variable [$elem] in condition [$cond]";
      $names[$#names] = "";
      #return 0;
    }
  }
  my @parts = ();
  if ($#offs < 0) {
    push @parts, $expr;
  } else {
    push @parts, substr($expr,0,$offs[0]);
    for (my $i=0; $i <= $#offs; $i++) {
      if ($i < $#offs) {
        push @parts, substr($expr, $offs[$i]+1, $offs[$i+1]-$offs[$i]-1);
      } else {
        push @parts, substr($expr, $offs[$i]+1);
      }
    }
  }
  unless (defined($timeout) && $timeout > 0) {
    $timeout = 1;
  }
  $msg_ok = "OK for \"$cond\"" unless defined $msg_ok;
  $msg_err = "ERROR for \"$cond\"" unless defined $msg_err;
  #print ("expr=[$expr] names=[".join(',',@names)."] vals=[".join(',',@vals)."] offs=[".join(',',@offs)."]\n");
  olog "waiting for \"$cond\" within $timeout sec";
  my $cur_time = Time::HiRes::time();
  my $end_time = $cur_time + $timeout;
  my $ok = 0;
  my ($msg, $result);
  while ($cur_time <= $end_time) {
    my $calc = $parts[0];
    for (my $i=0; $i <= $#offs; $i++) {
      $vals[$i] = $vars[$i] if defined($names[$i]) && $names[$i] ne '';
      $calc .= $vals[$i];
      $calc .= $parts[$i+1];
    }
    $msg = '';
    {
      local $SIG{__WARN__} = sub { $msg = $_[0]; };
      $result = eval($calc);
    }
    if ($msg ne '') {
      olog "ERROR: invalid syntax \"$cond\": $msg";
      @vars = ();
      return;
    }
    #print "calc=[$calc] result=$result\n";
    if ($result) {
      $ok = 1;
      last;
    }
    waits(0.100);
    $cur_time = Time::HiRes::time();
  }
  @vars = ();
  olog ($ok ? $msg_ok : $msg_err);
  return $ok;
}


# ================ scripts ==================


sub runSampleScript
{
  my $name = shift;
  return 0 unless $inited;
  my $sample_script = "$root_dir/host/bin/sample-script";
  unless (-x $sample_script) {
    olog "cannot find the $sample_script executable";
    return 0;
  }
  my $script = "$root_dir/share/script/$name.oss";
  unless (-r $script) {
    olog "cannot find the $script sample script";
    return 0;
  }
  olog "run sample script $name";
  system ("$sample_script $name &");
  return 1;
}


sub pformsVariable
{
  my $name = shift;
  my $s = $name;
  my $ooid;
  while ($s =~ /(.*?\[)((?:[_a-zA-Z]\w*\@)?[_a-zA-Z][\w\[\]\.]*)(\].*?)$/) {
    my ($left, $ind, $right) = ($1, $2, $3);
    $ooid = Optikus::Watch::info($ind);
    unless ($ooid) {
      olog "pformsVariable: index [$ind] in [$name] not found";
      return 0;
    }
    my $val = Optikus::Watch::read($ind);
    unless (defined($val)) {
      olog "pformsVariable: cannot read index [$ind] in [$name]";
    }
    $s = "$left$val$right";
  }
  olog "pformsVariable: [$name] --> [$s]";
  $ooid = Optikus::Watch::info($s);
  return $ooid;
}


sub pformsWrite
{
  my $line = shift;
  return 0 unless $inited;
  chomp $line;
  unless ($line =~ /^\s*(.*?)\s*=\s*(.*?)\s*$/) {
    olog "pformsWrite: invalid syntax: [$line]";
    return 0;
  }
  my ($left, $right) = ($1, $2);
  my $ooid = pformsVariable($left);
  unless ($ooid) {
    olog "pformsWrite: variable [$left] not found: [$line]";
    return 0;
  }
  $ooid = pformsVariable($right) if  ($right =~ m/.*?\@.*?/);
  my $val = $ooid ? Optikus::Watch::read($right) : eval($right);
  unless (defined $val) {
    olog "pformsWrite: undefined right value: [$line]";
    return 0;
  }
  my $rc = Optikus::Watch::write($left, $val);
  return $rc;
}


sub runFormScript
{
  my $name = shift;
  return 0 unless $inited;
  my $script = "$root_dir/test/pforms/$name";
  unless (-r $script) {
    olog "cannot find pforms script $script";
    return 0;
  }
  olog "run pforms script $script";
  open (SCRIPT, "< $script");
  my @lines = (<SCRIPT>);
  close (SCRIPT);
  for (@lines) {
    chomp;
    next if /^\s*\#/;
    next if /^\s*$/;
    pformsWrite ($_);
  }
  # FIXME...
  return 1;  
}


sub runShellScript
{
  my $cmd = join(' ',@_);
  olog "shell command: $cmd";
  system ("$cmd &");
  return 1;
}


# ================= test control ===============


sub begin_test
{
  $test_name = shift;
  my $no_stdout = shift;
  unless (defined $test_name) {
    $test_name = $0;
    $test_name =~ s/^.*\///;
    $test_name =~ s/\.\S+$//;
  }
  $test_name = "test" unless defined $test_name;
  $root_dir = $ENV{OPTIKUS_HOME};
  $root_dir = $ENV{HOME} unless defined($root_dir) && length($root_dir) > 0;
  my $log_file = (-d "$root_dir/var/log") ? "$root_dir/var/log/host.log"
                                          : "./host.log";
  my $client_name = "OTEST";
  Optikus::Log::open($client_name, $log_file, 4096);
  Optikus::Log::mode(Optikus::Log::mode() | OLOG_STDOUT | OLOG_FFLUSH)
    unless $no_stdout;
  Optikus::Watch::init(client_name => $client_name,
                       verbosity => 2,
                       conn_timeout => 0,
                       def_timeout => 1000,
                      )
      or croak "cannnot start test \"$test_name\"";
  olog "*** BEGIN TEST \"$test_name\" ***";
  return 1;
}


sub end_test
{
  return unless $inited;
  olog "*** END TEST \"$test_name\" ***";
  Optikus::Watch::exit();
  Optikus::Log::close();
  return 1;
}


# ================= exported names ===============


sub CC             { return "cc"; }
sub TC             { return "tc"; }
sub tget($)        { Optikus::Watch::read(@_); }
sub tput($$)       { Optikus::Watch::write(@_); }
sub send_ctlmsg    { sendCtlMsg(@_); }
sub send_immed     { sendImmedPacket(@_); }
sub count_acked    { ackedPackets(@_); }
sub waits($)       { waitSec(@_); }
sub wait_for       { waitFor(@_); }
sub sample_script($) { runSampleScript(@_); }
sub shell_script   { runShellScript(@_); }
sub form_script    { runFormScript(@_); }
sub form_exec      { pformsWrite(@_); }


# =============== compatibility ==================


sub send_uv        { send_ctlmsg(@_); }
sub send_packet
{
  my $no = shift;
  olog "OBSOLETE: SEND PACKET no. $no";
  return 1;
}


# ================================================


END
{
  Optikus::Watch::exit;
  olog "Optikus::Watch::END\n" if $DEBUG;
}

1;

__END__

=head1 NAME

Optikus::Watch - Perl bindings for Optikus.

=head1 SYNOPSIS

  use Optikus::Watch;
  blah blah blah

=head1 ABSTRACT

This module contains Perl bindings for Optikus.

=head1 DESCRIPTION

Stub documentation for Watch.

=head2 EXPORT

None by default.

=head1 SEE ALSO

Mention other useful documentation.

=head1 AUTHOR

vit, E<lt>vitkinet@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2006-2007, vitki.net. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.1 or,
at your option, any later version of Perl 5 you may have available.

=cut
