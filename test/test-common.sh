#!/bin/sh

#set -x

hub_verb=${hub_verb:-"3"}
start_subject=${start_subject:-"yes"}
subject_verb=${subject_verb:-"2"}

TOP_SRCDIR=${TOP_SRCDIR:-".."}
TOP_BUILDDIR=${TOP_BUILDDIR:-".."}
export TOP_SRCDIR TOP_BUILDDIR

srcdir="$TOP_SRCDIR/test"
domdir="$TOP_BUILDDIR/test"

clean()
{
  rm -f $domdir/sample-subject.psv $domdir/tmp-sample-subject.*
  rm -f $domdir/ohub.pid $domdir/ohub.log
}

start_local()
{
  rm -f $domdir/sample-subject.psv
  $srcdir/oscan -c "$srcdir/test-common.ini" -f -t >> $domdir/oscan.out || return 1
  test -r $domdir/sample-subject.psv || return 1
  rm -f $domdir/ooid-cache.lst
  $TOP_BUILDDIR/hub/ohubd -c "$srcdir/test-common.ini" -g$hub_verb &
  test -x /bin/usleep && usleep 200000 || sleep 1
  test -r $domdir/ohub.pid || return 1
  if test x$start_subject = x"yes" ; then
    $domdir/sample-subject $subject_verb &
    SUBJECT_PID=$!
    export SUBJECT_PID
  fi
  sleep 1
  return 0
}

start()
{
  if start_local
  then
    export SUBJECT_PID
    return 0
  else
    stop
    return 1
  fi
}

stop()
{
  test -n "$SUBJECT_PID" && kill -TERM $SUBJECT_PID
  test -r $domdir/ohub.pid && kill -TERM `cat $domdir/ohub.pid`
  test -x /bin/usleep && usleep 100000 || sleep 1
}

setup()
{
  clean
  stop
  start
}

