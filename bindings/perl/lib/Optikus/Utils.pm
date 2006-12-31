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
#  Minutiae.
#

package Optikus::Utils;

use strict;
use Carp;
use Cwd 'abs_path';
use File::Spec::Functions;

BEGIN
{
  use Exporter;
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $TICK);
  $VERSION = 0.1;
  @ISA  = qw(Exporter);
  @EXPORT  = qw( );
  %EXPORT_TAGS = (all => [ qw(
                              &dirOf
                              &fileOf
                              &mergePath
                              &substEnv
                              &unrollPath
                              &setUnrollPathRoot
                              &pathDisplay
                              &num2hex
                              &needsMake
                              &timeNow
                              &checkProg
                             ) ] );
  @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
}

sub dirOf
{
  return (File::Spec->splitpath(shift))[1];
}

sub fileOf
{
  return (File::Spec->splitpath(shift))[2];
}

sub mergePath
{
  my ($dir, $file) = @_;
  return $file if !defined($dir) || ($dir =~ /^\s*$/);
  return (file_name_is_absolute($file) ? $file : catfile($dir, $file));
}

sub substEnv
{
  my $s = shift;
  return $s unless $s;
  $s =~ s/\$\{([^\}]+)\}/$ENV{$1}/eg;
  $s =~ s/\$(\w+)/$ENV{$1}/eg;
  return $s;
}

my $unroll_path_root;

sub setUnrollPathRoot
{
  $unroll_path_root = shift;
}

sub unrollPath
{
  my $path = shift;
  return $path unless $path;
  $path = substEnv($path);
  $path = mergePath($unroll_path_root, $path);
  $path = substEnv($path);
  my $tmp = abs_path($path);
  $path = $tmp if defined $tmp;
  return $path;
}

sub pathDisplay
{
  my ($s, $lim) = @_;
  $lim = 20 unless $lim and $lim > 0;
  return '"'.$s.'"' unless $s =~ m/\//;
  return '"'.$s.'"' if length($s) <= $lim;
  for (my $i = length($s) - $lim; $i > 0; $i--)  {
    return '"...'.substr($s,$i).'"' if substr($s,$i,1) eq "/";
  }
  return '"'.$s.'"';
}

sub num2hex
{
  my $num = shift;
  return ($num < 0 ? sprintf("-0x%x",(-$num)) : sprintf("0x%x",$num));
}

# acts like 'dest: source1 source2...' in makefile
# return true if destination needs and update
sub needsMake
{
  my $dbg_make = 0;
  my ($dest, @sources) = @_;
  unless (-e $dest)  {
    logMsg('info','make-make',"$dest does not exist - gonna remake it") if $dbg_make;
    return 1;
  }
  my $d_age = (-M $dest);
  foreach my $src (@sources)  {
    croak "ERROR: source file $src does not exist\n" unless -e $src;
    croak "ERROR: source $src must be a plain file\n" unless -f $src;
    if ($d_age >= (-M $src))  {
      logMsg('info','make-make',"$dest is older than $src") if $dbg_make;
      return 1;
    }
  }
  croak "ERROR: destination $dest must be a plain file\n" unless -f $dest;
  logMsg('info','make-make',"$dest is ok for ".join(',',@sources)) if $dbg_make;
  return 0;
}

# current time in printable format
sub timeNow
{
  my ($ss,$mi,$hh,$dd,$mo,$yy,$dow) = localtime(time);
  my $tm = sprintf("%02d-%02d-%02d,%02d:%02d:%02d",
                  $yy+1900-2000,$mo+1,$dd, $hh,$mi,$ss);
  return $tm;
}

sub checkProg
{
  my ($cmd, $path) = @_;
  $cmd =~ s{^\s+}{};
  $cmd =~ s{\s+$}{};
  $cmd = mergePath($path, $cmd) if $path;
  $cmd =~ s[\{FILE\}][\*]g;
  $cmd = substEnv($cmd);
  my $prog = $cmd;
  $prog = $1 if $prog =~ m/^\s*(\S+)\s+/;
  croak "ERROR: cannot find executable \"$prog\"\n" unless -x $prog;
  return $cmd;
}

1;

__END__

=head1 NAME

Optikus::Utils - Utility routines for the Optikus perl bindings.

=head1 SYNOPSIS

  use Optikus::Utils;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Utils.

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
