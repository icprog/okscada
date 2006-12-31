#!/usr/bin/perl

use strict;
use FindBin qw"$Bin";
use lib "$Bin/../lib", "$Bin/../blib/arch";

use Test::More tests => 12;

BEGIN { use_ok 'Optikus::Log', ':all'; }

ok ( Optikus::Log::open("log2console")
   , "opening terminal log" );
ok ( olog("console-msg")
   , "writing message to terminal" );

my $logfile = "perl2file.log";
unlink($logfile);
ok ( Optikus::Log::open("log2file", $logfile)
   , "opening log file" );
ok ( -r $logfile
   , "the file was actually created" );
ok ( -z $logfile
   , "there are no messages in the file" );
my $pid = $$;
ok ( olog("file-msg $pid")
   , "writing message to file" );
ok ( open(LOG, $logfile)
   , "can open log file" );
my $count = 0;
my $msg = "";
while(<LOG>) {
  $count++;
  $msg = $_;
};
close(LOG);
is ( $count, 1
   , "file contains only one message" );
like( $msg, qr/^\d\d\.\d\d\.\d\d \d\d:\d\d:\d\d\.\d{3} log2file: file\-msg $pid$/
    , "the message format is correct" );

ok ( Optikus::Log::open("log2remote", "", 0, "localhost")
   , "opening remote log" );
ok ( olog("remote-msg $pid")
   , "sending remote message" );

