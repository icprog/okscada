
## Makefile.am -- Process this file with automake to produce Makefile.in

# ======== sample runtime environment ========

dist_sysconf_DATA = ocommon.ini odomain.ini

dist_bin_SCRIPTS = oscan ocontrol sample-script

bin_PROGRAMS = sample-subject sample-echo

sample_subject_SOURCES = sample-subject.c
sample_subject_CFLAGS = -O -gstabs
sample_subject_LDFLAGS = -rdynamic
sample_subject_LDADD = $(top_builddir)/subj/libosubj.a \
			$(top_builddir)/util/liboutil.a \
			-lpthread -ldl -lm

sample_echo_SOURCES = sample-echo.c
sample_echo_LDADD = $(top_builddir)/watch/libowatch.a \
		$(top_builddir)/util/liboutil.a \
		-lreadline -lncurses -lpthread -ldl

install-exec-hook:
	$(mkdir_p) $(localstatedir)/log
	$(mkdir_p) $(localstatedir)/run
	$(mkdir_p) $(localstatedir)/sample

# ======== test harness ========

EXTRA_DIST = test-common.ini test-domain.ini test02-a.ini test02-b.ini \
	test-common.sh test01-loglimit.sh test02-ini.sh test03-scan.sh \
	test04-see1.sh test05-watch.sh test06-watchas.sh \
	test07-watchmsg.sh test08-subjmsg.sh test09-rlog.sh \
	test10-stress1.sh

TESTS = test01-loglimit.sh test02-ini.sh test03-scan.sh \
	test04-see1.sh test05-watch.sh test06-watchas.sh \
	test07-watchmsg.sh test08-subjmsg.sh test09-rlog.sh \
	test10-stress1.sh

XFAIL_TESTS = 
TESTS_ENVIRONMENT = TOP_SRCDIR=$(top_srcdir) TOP_BUILDDIR=$(top_builddir)

check_PROGRAMS = test01-loglimit test02-ini test05-watch test06-watchas \
		test07-watchmsg test08-subjmsg test09-rlog test10-stress1

test01_loglimit_SOURCES = test01-loglimit.c
test01_loglimit_CFLAGS = -O -gstabs
test01_loglimit_LDFLAGS = -rdynamic
test01_loglimit_LDADD = $(top_builddir)/util/liboutil.a -lpthread -ldl

test02_ini_SOURCES = test02-ini.c
test02_ini_LDADD = $(top_builddir)/util/liboutil.a -lpthread -ldl

test05_watch_SOURCES = test05-watch.c
test05_watch_LDADD =	$(top_builddir)/watch/libowatch.a \
			$(top_builddir)/util/liboutil.a \
			-lpthread -ldl

test06_watchas_SOURCES = test06-watchas.c
test06_watchas_LDADD =	$(top_builddir)/watch/libowatch.a \
			$(top_builddir)/util/liboutil.a \
			-lpthread -ldl -lm

test07_watchmsg_SOURCES = test07-watchmsg.c
test07_watchmsg_LDADD = $(top_builddir)/watch/libowatch.a \
			$(top_builddir)/util/liboutil.a \
			-lpthread -ldl -lm

test08_subjmsg_SOURCES = test08-subjmsg.c
test08_subjmsg_LDADD =	$(top_builddir)/watch/libowatch.a \
			$(top_builddir)/util/liboutil.a \
			-lpthread -ldl -lm

test09_rlog_SOURCES = test09-rlog.c
test09_rlog_LDADD = $(top_builddir)/watch/libowatch.a \
		$(top_builddir)/subj/libosubj.a \
		$(top_builddir)/util/liboutil.a \
		-lpthread -ldl

test10_stress1_SOURCES = test10-stress1.c
test10_stress1_LDADD =	$(top_builddir)/watch/libowatch.a \
			$(top_builddir)/util/liboutil.a \
			-lpthread -ldl -lm

clean-local:
	-rm -f test01-loglimit.log* test09-rlog.log*
	-rm -f sample-subject.log sample-subject.psv tmp-sample-subject.*
	-rm -f oscan.log oscan.out osee1.log
	-rm -f ohub.log ohub.pid ooid-cache.lst

