#!/bin/sh

#set -x
param="$1"

TOP_SRCDIR=${TOP_SRCDIR:-".."}
source "$TOP_SRCDIR/test/test-common.sh"

result="PASSED"
setup
if test $? = 0 ; then
  echo
  echo '==========================================================================='
  echo 'This is interactive test. It is not automatically run for "make check".'
  echo 'We are unable to verify if it passes or fails without user interaction'
  echo 'and "make check" assumes it passed if "hub" and "agent" were able to run.'
  echo "Manually run this test as \"$0 run\"."
  echo 'The "Optikus See" window pops up. You are free to monitor or modify'
  echo 'any data you like with this tool.'
  echo 'It is advisable to start from monitoring "param1", "param2" and "param3".'
  echo 'Then try to change "strinc", "strinh", "strini", "strinf", "strind"'
  echo 'and watch the value of "strout" changing.'
  echo 'When finished, choose "File->Exit" in the main menu.'
  echo '==========================================================================='
  echo
  test -n "$param" && $TOP_BUILDDIR/see/osee1 || sleep 2
fi
stop
echo $result
test $result = "PASSED" && exit 0 || exit 1
