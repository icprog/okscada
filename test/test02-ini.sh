#!/bin/sh
TOP_SRCDIR=${TOP_SRCDIR:=".."}
source "$TOP_SRCDIR/test/test-common.sh"
$domdir/test02-ini "$srcdir/test02-a.ini" "$srcdir/test02-b.ini"
