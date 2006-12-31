#!/bin/sh

#set -x
#GDB="gdb"
#hub_verb=5
#subject_verb=5
#watch_verb=3
#print_all=1
#wait_echo=8

TOP_SRCDIR=${TOP_SRCDIR:-".."}
source "$TOP_SRCDIR/test/test-common.sh"

result="FAILED"
setup
if test $? = 0
then
  if test x$wait_echo != x
  then
    echo "waiting $wait_echo seconds for sample-echo ..."
    sleep $wait_echo
  fi
  $GDB $domdir/test08-subjmsg $watch_verb $print_all && result="PASSED"
fi
stop
echo $result
test $result = "PASSED" && exit 0 || exit 1
