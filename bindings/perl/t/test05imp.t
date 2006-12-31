#!/usr/bin/perl

use strict;
use FindBin qw"$Bin";
use lib "$Bin/../lib", "$Bin/../blib/arch";

use Test::More tests => 10;

BEGIN { use_ok 'Optikus::Watch', ':all'; }

count_acked(1);

ok ( begin_test
   , "starting the test" );
ok ( send_ctlmsg(CC, 1, 1)
   , "control msg without parameters"  );
ok ( send_ctlmsg(CC, 3, 1, "%hx(4)", 0x5020, 0x0304, 0x1123, 0x3000)
   , "control msg with repeating parameters" );
ok ( send_ctlmsg(TC, 1, 42, "%f(13)", 215583.0, 38350052.0, 4331769.0, 56946.0,
                             4331769.0, 67991120.0, -305259.0, 56946.0,
                             -305259.0, 39000532.0, -26.96, 1.97, 0.09)
   , "control msg with many floating parameters" );
ok ( waits(0.1)
   , "waiting" );
for (my $tact = 0; $tact < 10 && count_acked() < 3; $tact++) {
  waits 0.1;
}
is ( count_acked(), 3
   , "all 3 packets are acknowledged" );
ok ( tput('sample@arr1[1]', 1)
   , "writing a value" );
ok ( wait_for('sample@arr1[1]>20', 10)
   , "waiting for a value with timeout" );
ok ( end_test
   , "ending the test" );

