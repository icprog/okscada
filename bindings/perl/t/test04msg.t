#!/usr/bin/perl -w

use strict;
use FindBin qw"$Bin";
use lib "$Bin/../lib", "$Bin/../blib/arch";

use Test::More tests => 11;

BEGIN { use_ok 'Optikus::Log'; }
BEGIN { use_ok 'Optikus::Watch', ':all'; }

ok ( Optikus::Log::open("test4"),  "can open log" );
ok ( Optikus::Watch::init(
                        client_name => "test04msg",
                        server => "localhost",
                        msg_dest => "sample",
                        conn_timeout => 0,
                        def_timeout => 1000,
                        verbosity => 2,
                       )
   , "initialization" );

ok ( Optikus::Watch::waitSec(0.1)
   , "waiting" );

ok ( Optikus::Watch::sendCtlMsg("cc", 1, 1)
   , "control msg without parameters" );
ok ( Optikus::Watch::sendCtlMsg("cc", 3, 1, "%hx%hx%hx%hx", 0x5020, 0x0304, 0x1123, 0x3000)
   , "control msg with some paramaters" );
ok ( Optikus::Watch::sendStrCtlMsg("cc", 3, 1, "=hx%hx(3)", '5020 0304 1123 3000')
   , "control msg with repeating and string parameters" );
ok ( Optikus::Watch::sendCtlMsg("tc", 1, 42, "%f(13)", 215583.0, 38350052.0, 4331769.0,
                      56946.0, 4331769.0, 67991120.0, -305259.0, 56946.0,
                      -305259.0, 39000532.0, -26.96, 1.97, 0.09)
   , "control msg with lots of parameters" );

for (my $tact = 0; $tact < 10; $tact++) {
  last unless Optikus::Watch::ackedPackets() < 4;
  Optikus::Watch::waitSec(0.1);
}
is ( Optikus::Watch::ackedPackets(), 4
   , "all packets are acknowledged" );

ok ( Optikus::Watch::exit()
   , "shutdown" );

