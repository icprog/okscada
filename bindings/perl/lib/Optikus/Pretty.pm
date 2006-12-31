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
#  Dumper of configuration data.
#

package Optikus::Pretty;

use strict;
use Carp;

BEGIN
{
  use Exporter;
  use vars qw($VERSION $DEBUG @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $TICK);
  $VERSION = 0.1;
  @ISA  = qw(Exporter);
  @EXPORT  = qw(
               );
  %EXPORT_TAGS = (all => [ qw( ) ] );
  @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
}

sub divider
{
  my $fmt = shift;
  my $p = $fmt;
  $p =~ s{\s*\;.*?$}{};
  $p =~ s{\%\%}{_}g;
  $p =~ s{\%\-?(\d+)\w}{'=' x ($1 < 1 ? 1 : $1)}eg;
  $p =~ s{[^\{\|\}]}{\-}g;
  $p =~ s{[\{\|\}]}{\+}g;
  $p =~ s{^.}{\;};
  return $p;
}

sub title
{
  my ($fmt, @hdr) = @_;
  my $p = divider($fmt);
  $p =~ s{\-}{\=}g;
  $p =~ s{\+}{\|}g;
  $p =~ s{[\|\;]}{\{};
  $p =~ s{\|(?=[^\|]*$)}{\}};
  $p =~ s{^.}{\;};
  my $i = 0;
  $p =~ s{(?<=[\;\{\|])(\=+)(?=[\|\}])} {
           my $n = length($1);
           my $s = $i <= $#hdr ? $hdr[$i++] : "?";
           my $k = length($s);
           if ($k > $n) {
             $s = $n == 1 ? substr($s,0,1) : substr($2,0,$n-1)."*";
           } elsif ($k < $n) {
             my $a = int(($n - $k) / 2);
             my $b = $n - $k - $a;
             $s = (' ' x $b) . $s . (' ' x $a);
           }
           $s
         }eg;
  return $p;
}

sub header
{
  my $t = title(@_);
  my $d = divider(@_);
  return "$d\n$t\n$d\n";
}

sub data
{
  my ($fmt, @data) = @_;
  return sprintf($fmt,@data)."\n";
}

sub footer
{
  my $d = divider(@_);
  return "$d\n";
}

1;

__END__

=head1 NAME

Optikus::Pretty - Dumper of configuration data.

=head1 SYNOPSIS

  use Optikus::Pretty;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Pretty.

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
