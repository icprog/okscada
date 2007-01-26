
## Makefile.am -- Process this file with automake to produce Makefile.in

EXTRA_DIST = $(PFORMS_PERLMOD)

dist_bin_SCRIPTS = opdock opedit opscreens opecho opmap oplogs opmsg

dist_sysconf_DATA = cmsg.lst cmsg.hash

nobase_dist_data_DATA = themes/simple/gtkrc


# ================= Perl bindings ====================

PFORMS_PERLMOD = $(PFORMS_PM_MAKE) $(PFORMS_PM_UTIL) $(PFORMS_PM_WIDGETS)

# add the following to all targets
all-local:  @PERL_BINDINGS@

install-data-local:
	test -f perl/Makefile && cd perl && $(MAKE) install

# rules for building the perl module

perl_binding: perl/Makefile
	cd perl && $(MAKE)

perl/Makefile: perl/Makefile.PL
	test -d perl || \
	($(mkdir_p) perl/lib && $(mkdir_p) perl/t \
	&& for f in $(PFORMS_PERLMOD); do \
	d=`dirname $$f`; test -d $$d || $(mkdir_p) $$d; \
	ln -s `pwd`/$(srcdir)/$$f $$f; done)
	cd perl && \
	$(PERL) Makefile.PL @PERL_FLAGS@

clean-local:
	test -f perl/Makefile && cd perl && $(MAKE) clean || true
	test -f perl/Makefile && rm -f perl/Makefile || true
	-rm -f perl/Makefile.old

PFORMS_PM_MAKE = perl/Changes perl/README perl/MANIFEST perl/Makefile.PL

PFORMS_PM_UTIL = perl/lib/Optikus/Canvas/Util.pm

PFORMS_PM_WIDGETS = \
	perl/lib/Optikus/Canvas/Widget.pm \
	perl/lib/Optikus/Canvas/DataWidget.pm \
	perl/lib/Optikus/Canvas/TimerWidget.pm \
	perl/lib/Optikus/Canvas/Widget/Label.pm \
	perl/lib/Optikus/Canvas/Widget/String.pm \
	perl/lib/Optikus/Canvas/Widget/MultiString.pm \
	perl/lib/Optikus/Canvas/Widget/Pixmap.pm \
	perl/lib/Optikus/Canvas/Widget/LED.pm \
	perl/lib/Optikus/Canvas/Widget/Bar.pm \
	perl/lib/Optikus/Canvas/Widget/Anim.pm \
	perl/lib/Optikus/Canvas/Widget/ManoMeter.pm \
	perl/lib/Optikus/Canvas/Widget/Curve.pm \
	perl/lib/Optikus/Canvas/Widget/Control.pm \
	perl/lib/Optikus/Canvas/Widget/Control/Runner.pm \
	perl/lib/Optikus/Canvas/Widget/Control/Offnominal.pm \
	perl/lib/Optikus/Canvas/Widget/Control/Link.pm \
	perl/lib/Optikus/Canvas/Widget/Toggle.pm \
	perl/lib/Optikus/Canvas/Widget/Toggle/Button.pm \
	perl/lib/Optikus/Canvas/Widget/Toggle/Switch.pm \
	perl/lib/Optikus/Canvas/Widget/Range.pm \
	perl/lib/Optikus/Canvas/Widget/Range/Dial.pm \
	perl/lib/Optikus/Canvas/Widget/Range/Ruler.pm \
	perl/lib/Optikus/Canvas/Widget/Custom.pm \
	perl/lib/Optikus/Canvas/Widget/Custom/SolArray.pm \
	perl/lib/Optikus/Canvas/Widget/Meter.pm \
	perl/lib/Optikus/Canvas/Widget/Meter/RoundTank.pm \
	perl/lib/Optikus/Canvas/Widget/Meter/Volume.pm \
	perl/lib/Optikus/Canvas/Widget/Meter/ThermoMeter.pm \
	perl/lib/Optikus/Canvas/Widget/Meter/SmallTank.pm \
	perl/lib/Optikus/Canvas/Widget/Meter/BigTank.pm

