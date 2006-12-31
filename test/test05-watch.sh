#!/bin/sh

#set -x
watch_verb=1

TOP_SRCDIR=${TOP_SRCDIR:=".."}
source "$TOP_SRCDIR/test/test-common.sh"

result="FAILED"
setup
test $? = 0 && $domdir/test05-watch $watch_verb && result="PASSED"
stop
echo $result
test $result = "PASSED" && exit 0 || exit 1
