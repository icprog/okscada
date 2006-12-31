#!/bin/sh

# setup test environment
TOP_SRCDIR=${TOP_SRCDIR:-"../.."}
TOP_BUILDDIR=${TOP_BUILDDIR:-"../.."}
MAKE=${MAKE:-"make"}
source "$TOP_SRCDIR/test/test-common.sh"
OPTIKUS_HOME=$TOP_SRCDIR
export OPTIKUS_HOME

my_pkill()
{
  sig=$1
  pat=$2
  pids=`ps -efa 2>/dev/null | \
        grep -v vim | grep -v grep | grep -v tail | grep -v ocontrol | \
        grep $pat | awk '{print $2}'`
  test -z "$pids" && return 0
  #echo "sig=$sig pat=$pat pids=$pids."
  kill $sig $pids >/dev/null 2>&1
  return 0
}

case "$1" in
start)
  setup && echo OK || echo FAIL
  ;;
stop)
  stop
  pkill_dir="/bin"
  test -x "$pkill_dir/pkill" || pkill_dir="/usr/bin"
  test -x "$pkill_dir/pkill" || pkill_dir="/usr/local/bin"
  test -x "$pkill_dir/pkill" || pkill_dur=""
  if test -z "$pkill_dir"
  then
    $pkill_dir/pkill -TERM sample-subject > /dev/null 2>&1
  else
    my_pkill -TERM sample-subject
  fi
  test $? = 0 && echo OK || echo "please stop sample-subject manually"
  ;;
test)
  $MAKE test
  ;;
all)
  setup
  if test $? = 0
  then
    $MAKE test
  else
    echo ""
    echo "**************************************"
    echo "******** CANNOT SETUP TESTS ! ********"
    echo "**************************************"
    echo ""
  fi
  stop
  ;;
*)
  echo "usage: $0 start|stop|test|all"
esac
