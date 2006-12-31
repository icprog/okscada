
## Makefile.am -- Process this file with automake to produce Makefile.in

EXTRA_DIST = \
	perl/MANIFEST perl/README perl/Changes \
	perl/META.yml perl/typemap \
	perl/ppport.h perl/optikus-perl.h \
	perl/Watch.xs perl/Log.xs perl/Optikus.xs \
	perl/lib/Optikus/Watch.pm perl/lib/Optikus/CmdLine.pm \
	perl/lib/Optikus/Ini.pm perl/lib/Optikus/Log.pm \
	perl/lib/Optikus/Pretty.pm perl/lib/Optikus/Optikus.pm \
	perl/lib/Optikus/Utils.pm \
	perl/t/test01log.t perl/t/test02watch.t perl/t/test03subj.t \
	perl/t/test04msg.t perl/t/test05imp.t \
	perl/test.sh

# add the following to all targets
all-local:  @PERL_BINDINGS@

install-data-local: install-perl-local

# rules for building the perl module

perl_binding: perl/Makefile
	cd perl && $(MAKE)

perl/Makefile: perl/Makefile.PL
	test -d perl/lib || \
	(mkdir perl/t && mkdir perl/lib && mkdir perl/lib/Optikus \
	&& for f in $(EXTRA_DIST); do ln -s $$PWD/$(srcdir)/$$f $$f; done)
	cd perl && \
	$(PERL) Makefile.PL @PERL_FLAGS@ RPATH=$(exec_prefix)/lib

install-perl-local:
	test -f perl/Makefile && cd perl && $(MAKE) install

check-local:
	cd perl && \
	TOP_SRCDIR=../$(top_srcdir) TOP_BUILDDIR=../$(top_builddir) \
	MAKE=$(MAKE) ./test.sh all

clean-local:
	test -f perl/Makefile && cd perl && $(MAKE) clean || true
	test -f perl/Makefile && rm -f perl/Makefile || true
	-rm -f perl/Makefile.old perl/perl2file.log perl/host.log

