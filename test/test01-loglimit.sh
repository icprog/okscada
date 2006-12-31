#!/bin/sh
TOP_SRCDIR=${TOP_SRCDIR:-".."}
source "$TOP_SRCDIR/test/test-common.sh"
rm -f test.log*
$domdir/test01-loglimit || exit 1
test -r test01-loglimit.log
for n in 0 1 2 3 4 ; do
  test -r "test01-loglimit.log.$n" || exit 1
done
exit 0
