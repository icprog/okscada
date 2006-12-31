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

package Optikus::Log;

use strict;
use Carp;
use Optikus::Optikus;

BEGIN
{
  use Exporter;
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
  $VERSION = 0.1;
  @ISA  = qw(Exporter DynaLoader);
  @EXPORT  = qw(
               );
  %EXPORT_TAGS = (all => [ qw(
                              &olog
                              &OLOG_DATE &OLOG_TIME &OLOG_MSEC &OLOG_USEC
                              &OLOG_LOGMSG &OLOG_STDOUT &OLOG_STDERR
                              &OLOG_FFLUSH &OLOG_USERTIME
                              &OLOG_GETMODE
                             ) ] );
  @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
}

sub OLOG_DATE    (){ 0x001 }
sub OLOG_TIME    (){ 0x002 }
sub OLOG_MSEC    (){ 0x004 }
sub OLOG_USEC    (){ 0x008 }
sub OLOG_LOGMSG  (){ 0x010 }
sub OLOG_STDOUT  (){ 0x020 }
sub OLOG_STDERR  (){ 0x040 }
sub OLOG_FFLUSH  (){ 0x080 }
sub OLOG_USERTIME(){ 0x100 }
sub OLOG_GETMODE (){ 0     }


sub open
{
  my ($client, $file, $kb_limit, $server, $topic) = @_;
  $file = "" unless $file;
  $kb_limit = 0 unless $kb_limit;
  $server = "" unless $server;
  $topic = "" unless $topic;
  return logOpen($client, $file, $kb_limit, $server, $topic);
}

sub close
{
  return logClose();
}

sub log
{
  my $s = join(' ',@_);
  chomp $s;
  logLog($s);
  return 1;
}

sub olog
{
  my $s = join(' ',@_);
  chomp $s;
  logLog($s);
  return 1;
}

sub mode
{
  my $mode = shift;
  $mode = 0 unless defined $mode;
  return logFlags($mode);
}

sub channel
{
  my ($nom, $name) = @_;
  return logSetOutput($nom, $name);
}

sub enable
{
  my ($nom, $enable) = @_;
  return logEnableOutput($nom, $enable);
}

END
{
  logClose();
}


1;

__END__

=head1 NAME

Optikus::Log - Perl bindings for Optikus.

=head1 SYNOPSIS

  use Optikus::Log;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Log.

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
