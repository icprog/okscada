#!/bin/sh

#set -x
#GDB=gdb
numvals=150
nummons=10
deadline=90
printall=0
#hub_verb=5

TOP_SRCDIR=${TOP_SRCDIR:-".."}
source "$TOP_SRCDIR/test/test-common.sh"

result="FAILED"
setup
test $? = 0 \
    && $GDB $domdir/test10-stress1 $numvals $nummons $deadline $printall \
    && result="PASSED"
stop
echo $result
test $result = "PASSED" && exit 0 || exit 1
