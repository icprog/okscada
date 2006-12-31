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
#  Command line parser.
#

package Optikus::CmdLine;

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
  %EXPORT_TAGS = (all => [ qw(
                             ) ] );
  @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );
}


sub new
{
  my ($proto, $handler, $usage) = @_;
  my $class = ref($proto) || $proto;
  my $self = {};
  bless ($self, $class);
  $self->{handler} = $handler;
  $self->{usage} = $usage;
  @{$self->{args}} = ();
  %{$self->{actions}} = ();
  return $self;
}


sub action
{
  my ($self, $symbols, $handler) = @_;
  for my $symbol (split /\s+/,$symbols) {
    ${$self->{actions}}{$symbol} = $handler;
  }
}


sub actions
{
  my ($self, %hash) = @_;
  for my $symbols (keys %hash) {
    $self->action($symbols, $hash{$symbols});
  }
}

sub DESTROY
{
  my $self = shift;
  undef $self;
}


sub parse
{
  my $self = shift;
  @{$self->{args}} = ();
  foreach my $op (@_)  {
    if ($op =~ /^-\S$/ or $op =~ /^\-\-/ or $op !~ /^\-/)  {
      push @{$self->{args}}, $op;
      next;
    }
    for (my $i = 1; $i < length($op); $i++)  {
      push @{$self->{args}}, "-".substr($op,$i,1);
    }
  }
  for ($self->{pos} = 0;
       $self->{pos} <= $#{$self->{args}};
       $self->{pos}++) {
    $self->nextOption;
    undef $self->{opt};
  }
}


sub nextOption
{
  my $self = shift;
  my $op = ${$self->{args}}[$self->{pos}];
  if ($op =~ /^(\-[^=]+)=(.+)$/)  {
    ${$self->{args}}[$self->{pos}] = $op = $1;
    $self->{opt} = $2;
  }
  $self->usage("unknown option: $op") if $op !~ /^-/;
  if ($op eq "-h" || $op eq "--help")  {
    $self->usage;
  }
  if ($op eq "--options")  {
    my $cfg_file = $self->arg;
    open(CONFIG, $cfg_file)
      or croak "cannot open options file $cfg_file: $!";
    my @new_args;
    while (<CONFIG>) {
      chop; chomp;
      next if /^\s*#/ or /^\s*$/;
      my @tmp_args = split(' ');
      push(@new_args, @tmp_args);
    }
    close(CONFIG);
    my ($save_pos, @save_args) = ($self->{pos}, @{$self->{args}});
    $self->parse(@new_args);
    ($self->{pos}, @{$self->{args}}) = ($save_pos, @save_args);
    return;
  }
  my $handler = ${$self->{actions}}{$op};
  return &$handler ($self, $op) if defined $handler;
  if ($self->{handler}) {
    &{$self->{handler}} ($self, $op);
  } else {
    $self->usage("improper option $op");
  }
}


sub usage
{
  my $self = shift;
  if ($self->{usage}) {
    &{$self->{usage}} (@_);
  } else {
    my $msg = shift;
    if ($msg and $msg eq "short")  {
      print STDERR "no valid action specified. use -h to get help\n";
      exit(1);
    }
    print STDERR "$msg\n" if $msg;
  }
}


sub arg
{
  my $self = shift;
  my $op = ${$self->{args}}[$self->{pos}];
  my $arg = $self->{opt};
  $arg = ${$self->{args}}[++$self->{pos}] unless defined $arg;
  undef $self->{opt};
  $self->usage("option $op needs argument")
      if !defined($arg) or $arg =~ /^-/;
  return $arg;
}


1;

__END__

=head1 NAME

Optikus::CmdLine - Command line parser.

=head1 SYNOPSIS

  use Optikus::CmdLine;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for CmdLine.

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
