
## Makefile.am -- Process this file with automake to produce Makefile.in

EXTRA_DIST = $(PFORMS_PERLMOD)

dist_bin_SCRIPTS = opdock opedit opforms opecho opmap oplogs opmsg

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

PFORMS_PM_UTIL = perl/lib/Optikus/Forms/Util.pm

PFORMS_PM_WIDGETS = \
	perl/lib/Optikus/Forms/Widget.pm \
	perl/lib/Optikus/Forms/WidgetActive.pm \
	perl/lib/Optikus/Forms/WidgetPeriodic.pm \
	perl/lib/Optikus/Forms/Widget/Label.pm \
	perl/lib/Optikus/Forms/Widget/String.pm \
	perl/lib/Optikus/Forms/Widget/MultiString.pm \
	perl/lib/Optikus/Forms/Widget/Pixmap.pm \
	perl/lib/Optikus/Forms/Widget/LED.pm \
	perl/lib/Optikus/Forms/Widget/Bar.pm \
	perl/lib/Optikus/Forms/Widget/Anim.pm \
	perl/lib/Optikus/Forms/Widget/ManoMeter.pm \
	perl/lib/Optikus/Forms/Widget/Curve.pm \
	perl/lib/Optikus/Forms/Widget/Control.pm \
	perl/lib/Optikus/Forms/Widget/Control/Runner.pm \
	perl/lib/Optikus/Forms/Widget/Control/Offnominal.pm \
	perl/lib/Optikus/Forms/Widget/Control/FormLink.pm \
	perl/lib/Optikus/Forms/Widget/Toggle.pm \
	perl/lib/Optikus/Forms/Widget/Toggle/Button.pm \
	perl/lib/Optikus/Forms/Widget/Toggle/Switch.pm \
	perl/lib/Optikus/Forms/Widget/Range.pm \
	perl/lib/Optikus/Forms/Widget/Range/Dial.pm \
	perl/lib/Optikus/Forms/Widget/Range/Ruler.pm \
	perl/lib/Optikus/Forms/Widget/Custom.pm \
	perl/lib/Optikus/Forms/Widget/Custom/SolArray.pm \
	perl/lib/Optikus/Forms/Widget/Meter.pm \
	perl/lib/Optikus/Forms/Widget/Meter/RoundTank.pm \
	perl/lib/Optikus/Forms/Widget/Meter/Volume.pm \
	perl/lib/Optikus/Forms/Widget/Meter/ThermoMeter.pm \
	perl/lib/Optikus/Forms/Widget/Meter/SmallTank.pm \
	perl/lib/Optikus/Forms/Widget/Meter/BigTank.pm

