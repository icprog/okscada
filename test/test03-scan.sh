#!/bin/sh
TOP_SRCDIR=${TOP_SRCDIR:-".."}
source "$TOP_SRCDIR/test/test-common.sh"
$srcdir/oscan -c "$srcdir/test-common.ini" -f -t
test -r "$domdir/sample-subject.psv" && exit 0 || exit 1
