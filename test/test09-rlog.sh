#!/bin/sh

#set -x

TOP_SRCDIR=${TOP_SRCDIR:=".."}
source "$TOP_SRCDIR/test/test-common.sh"

start_subject=no
result="FAILED"
setup
test $? = 0 && $domdir/test09-rlog && result="PASSED"
stop
echo $result
test $result = "PASSED" && exit 0 || exit 1
