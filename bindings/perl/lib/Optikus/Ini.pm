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
#  Configuration file parser.
#

package Optikus::Ini;

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

my $last_ini_section;

# open and pre-parse configuration file in .ini format

sub new
{
  my $proto = shift;
  my $file = shift;
  my $class = ref($proto) || $proto;
  my $self = {};
  bless ($self, $class);
  open(INI, "< $file") or croak "cannot read config from $file\n";
  $self->{'*file*'} = $file;
  my $sec;
  my $lineno = 0;
  while(<INI>)  {
    chop; chomp;
    $lineno++;
    # trim whitespace
    s{^\s+}{};
    s{\s+$}{};
    # strip off end-of-line comments
    # FIXME: this code sucks up if unescaped strings
    #        contain pound signs or whatsoever... :)
    s{^(?:\/\/|\#|\;).*$}{};
    s{^(\s*\{.*?\})\s*(?:\/\/|\#|\;).*$}{$1};
    # suppress inline comments (loosy again)
    s{\s*\/\*.*?\*\/\s*}{}g;
    next if $_ eq '';
    # start of section
    if ( m/^\[\s*([\w]+)\s*\]$/ )  {
      $sec = {};
      $self->{$1} = $sec;
      next;
    }
    confess "ERROR[$file:$lineno]: no open section\n" unless $sec;
    if ( m/^(\w+)\s*\=\s*(.*?)$/ )  {
      # parameter definition
      $sec->{$1} = "$lineno:$2";
      next;
    }
    if ( m/^\s*\{\s*(.*?)\s*\}\s*$/ )  {
      # table row
      my $tbl = $sec->{'*table*'};
      unless (defined($tbl))  {
        $tbl = [];
        $sec->{'*table*'} = $tbl;
      }
      # collapse redundant whitespace
      # FIXME: again, what if unescaped strings contain pipes... ;)
      push @{$tbl}, "$lineno:".join('|',split(/\s*\|\s*/,$1));
      next;
    }
    confess "ERROR[$file:$lineno]: unrecognized line format\n";
  }
  close(INI);
  return $self;
}

# free up memory
sub DESTROY
{
  my $self = shift;
  delete @{$self}{keys %{$self}};
  undef %{$self};
  undef $self;
}

# convert text description of data format into internal form
sub parseFormat
{
  my ($loc, $fmt) = @_;
  # we check for enumeration first because the enumeration items
  # themselves may have names coinsiding with reserved words.
  # FIXME: this check sucks because format descriptions are allowed
  #        to contain 'default=blahblah' tokens which in turn
  #        can contain reserved words.
  if ($fmt =~ /\benum(eration)?\b/) {
    confess "ERROR[$loc]: wrong enumeration definition\"$fmt\"\n"
      unless $fmt =~ m/\{\s*(.*?)\s*\}/;
    my $enum = $1;
    my $toks = {};
    foreach my $tok (split(/\s*,\s*/,$enum))  {
      confess "ERROR[$loc]: wrong enumeration item \"$tok\"\n"
        unless $tok =~ m/([\w\d]+)\s*\=\s*(\-?\d+)/;
      $toks->{lc($1)} = $2;
    }
    $toks->{'*desc*'} = $enum;
    return $toks;
  }
  return 'b' if $fmt =~ /\bbool(ean)?\b/;
  return 'n' if $fmt =~ /\bnum(ber)?\b/;
  return 'i' if $fmt =~ /\bint(eger)?\b/;
  return 's' if $fmt =~ /\bstr(ing)?\b/;
  confess "ERROR[$loc]: unknown format \"$fmt\"\n";
  return '';
}

# parse textual line into data value using given format
sub parseValue
{
  my ($s, $loc, $f, $def) = @_;
  unless (defined($s))  {
    return $def unless defined($def) and $def eq "!*MUST*!";
    confess "ERROR[$loc]: value must be supplied\n";
  }
  #$s = $1 if $s =~ m/^\s*(.*?)\s*$/;
  if ($f eq 's') {
    my $c = substr($s,0,1);
    return $s if $c ne '"' and $c ne "'";
    if ( $s =~ m/^\"(.*?)\"/o or $s =~ m/\'(.*?)\'/o )  {
      $s = $1;
      $s =~ s{\\n}{\n}go;
      $s =~ s{\\t}{\t}go;
      $s =~ s{\\([0-7]+)}{oct($1)}ego;
      $s =~ s{\\x([0-9a-fA-F]+)}{hex($1)}ego;
    }
    return $s;
  }
  if ($f eq 'i') {
    my $c = substr($s,0,1);
    my $p;
    if ($c eq '-')  {
      $p = substr($s,1);
    } else {
      $c = ''; $p = $s;
    }
    if (substr($p,0,1) eq '0') {
      return hex($s) if $p =~ /^0x[0-9a-fA-F]+$/o;
      return int($s) if $p =~ /^\d+$/o;
      return oct($s) if $p =~ /^0b[01]+$/o;
      return oct($c.'0'.$1) if $p =~ /^0o([0-7]+)$/o;
    } else {
      return int($s) if $p =~ /^\d+$/o;
    }
    confess "ERROR[$loc]: unrecognized integer \"$s\"";
  }
  if ($f eq 'b') {
    $s = lc($s);
    return 1 if $s eq '1' or $s eq 't' or $s eq 'true'
                or $s eq 'yes' or $s eq 'y' or $s eq 'on';
    return 0 if $s eq '0' or $s eq 'f' or $s eq 'false'
                or $s eq 'no' or $s eq 'n' or $s eq 'off';
    confess "ERROR[$loc]: unrecognized boolean \"$s\"\n";
  }
  if ($f eq 'n') {
    return int($s) if $s =~ /^\-?\d+$/o;
    return ($s + 0) if $s =~ /^\-?\d+\.\d*$/o or $s =~ /^\d*\.\d+$/o;
    confess "ERROR[$loc]: unrecognized number \"$s\"\n";
  }
  my $v = $f->{lc($s)};
  confess "ERROR[$loc]: wrong token \"$s\" in liew of \"".$f->{'*desc*'}."\"\n"
    if !defined($v) or $s eq '*desc*';
  return $v;
}

# read and possibly reformat a single parameter
sub param
{
  my ($self, $sec, $name, $fmt, $def) = @_;
  my $loc = $self->{'*file*'};
  $fmt = 'string' unless defined($fmt);
  if ($fmt =~ m/^\s*([\w\d]+)\s*\=\s*/)  {
    $name = $1;
    $fmt =~ s{^\s*\w+\s*\=\s*}{};
  }
  confess "ERROR[$loc]: wrong request for parameter \"$sec\"/\"$name\"\n"
    unless (defined($sec) and $sec ne ''
            and defined($name) and $name ne '');
  if ($sec eq '*')  {
    if ($fmt =~ /\bsection\s*=\s*(\w+)\b/)  {
      $sec = $1;
      $fmt =~ s{\bsection\s*=\s*\w+\b}{};
    } else {
      undef $sec;
      foreach my $p (keys %{$self})  {
        if ( $p !~ /^\*/ and exists($self->{$p}->{$name}) )  {
          confess "ERROR[$loc]: parameter \"$name\" both at \"$sec\" and \"$p\"\n"
            if defined($sec);
          $sec = $p;
        }
      }
      $sec = '*nosuchsec*' unless defined($sec);
    }
  }
  $last_ini_section = $sec;
  my $s = $self->{$sec}->{$name};
  if ($fmt =~ m/\bdef(?:ault)?\s*\=\s*(\S+)\s*/)  {
    $def = $1;
    $fmt =~ s{\bdef(?:ault)?\s*\=\s*(\S+)\s*}{};
  }
  if ($fmt =~ m/\s*\/\s*(\S+)\s*$/)  {
    $def = $1;
    $fmt =~ s{\s*\/\s*(\S+)\s*$}{};
  }
  if ($fmt =~ m/\!0/)  {
    $def = "!*MUST*!";
    $fmt =~ s{\!0}{};
  }
  if (defined($s))  {
    confess "internal error\n" unless $s =~ m/^(\d+)\:/;
    $loc = "$loc:$1";
    $s =~ s{^\d+\:}{};
  } else {
    $loc = "$loc:$sec/$name";
  }
  my $f = parseFormat($loc, $fmt);
  my $v = parseValue($s,$loc,$f,$def);
  return $v;
}

# parse a whole table
sub table
{
  my ($self, $res_table, $sec, @format_descs) = @_;
  my $file = $self->{'*file*'};
  my @names = ();
  my @formats = ();
  my @defs = ();
  foreach my $fmt (@format_descs)  {
    confess "ERROR[$file]: undefined column name in format \"$fmt\"\n"
      unless $fmt =~ m/^\s*([\w\d]+)\s*\=\s*/;
    push @names, $1;
    $fmt =~ s{^\s*\w+\s*\=\s*}{};
    my $def;
    if ($fmt =~ m/\bdef(?:ault)?\s*\=\s*(\S+)\s*/)  {
      $def = $1;
      $fmt =~ s{\bdef(?:ault)?\s*\=\s*(\S+)\s*}{};
    }
    if ($fmt =~ m/\s*\/\s*(\S+)\s*$/)  {
      $def = $1;
      $fmt =~ s{\s*\/\s*(\S+)\s*$}{};
    }
    if ($fmt =~ m/\!0/)  {
      $def = "!*MUST*!";
      $fmt =~ s{\!0}{};
    }
    push @defs, $def;
    push @formats, parseFormat($file,$fmt);
  }
  my $raw_table = $self->{$sec}->{'*table*'};
  return 0 unless defined($raw_table);
  my $rno = 0;
  foreach my $row_str (@{$raw_table})  {
    $rno++;
    confess "internal error\n" unless $row_str =~ m/^(\d+)\:/;
    my $loc = "$file:$1";
    $row_str =~ s{^\d+\:}{};
    my @cols = split('\|',$row_str);
    confess "ERROR[$loc]: must have $#names columns in row\n"
      if $#names != $#cols;
    my $row_val = {};
    for my $i (0 .. $#names)  {
      my $col = $cols[$i];
      my $val = parseValue($col, $loc, $formats[$i], $defs[$i]);
      $row_val->{$names[$i]} = $val;
    }
    push @{$res_table}, $row_val;
  }
  return 1;
}

1;

__END__

=head1 NAME

Optikus::Ini - Configuration file parser.

=head1 SYNOPSIS

  use Optikus::Ini;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Ini.

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
