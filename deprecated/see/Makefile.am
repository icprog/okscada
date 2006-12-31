
## Makefile.am -- Process this file with automake to produce Makefile.in

bin_PROGRAMS = osee1
osee1_SOURCES = see-gtk1.c see-proper1.c see-icons.c see-priv.h
osee1_CPPFLAGS = @GTK_CFLAGS@
osee1_LDFLAGS = @GTK_LIBS@
osee1_LDADD =	$(top_builddir)/watch/libowatch.a \
		$(top_builddir)/util/liboutil.a \
		-lpthread -ldl

