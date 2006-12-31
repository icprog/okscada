#!/usr/bin/perl

use strict;
use FindBin qw"$Bin";
use lib "$Bin/../lib", "$Bin/../blib/arch";

use Test::More tests => 7;

BEGIN { use_ok 'Optikus::Watch'; }

my $debug = 0;

my %subjects;

ok(
  Optikus::Watch::init (
                      client_name => "test03subj",
                      server => "localhost",
                      msg_target => "sample",
                      conn_timeout => 0,
                      def_timeout => 1000,
                      verbosity => 2,
                      alive_handler => sub {
                          my ($subject, $state) = @_;
                          $subjects{$subject} = $state;
                          print "$subject --> $state\n" if $debug;
                        }
                     )
  , "initialization" );

ok ( Optikus::Watch::idle(10)
   , "idle run" );
is ( $subjects{"*"}, "*"
   , "hub is online" );
is ( $subjects{"sample"}, "sample"
   , "sample agent is online" );
is ( $subjects{"+"}, "+"
   , "domain is full" );
ok ( Optikus::Watch::exit
   , "finalization" );

