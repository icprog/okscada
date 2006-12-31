#!/bin/sh

#set -x
#GDB="gdb"
#hub_verb=5
TOP_SRCDIR=${TOP_SRCDIR:-".."}
source "$TOP_SRCDIR/test/test-common.sh"

result="FAILED"
setup
test $? = 0 && $GDB $domdir/test07-watchmsg && result="PASSED"
stop
echo $result
test $result = "PASSED" && exit 0 || exit 1
