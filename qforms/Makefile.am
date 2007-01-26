
## Makefile.am -- Process this file with automake to produce Makefile.in

AM_CXXFLAGS := $(AM_CXXFLAGS) $(QT_CXXFLAGS) -Wno-non-virtual-dtor

# MOC in Qt 3.3 does not adore preprocessor directives.
# Here is a workaround for this bug.
%.moc.h: %.h
	grep '#include' $< > $<.1.tmp.h
	grep -v '#include' $< > $<.2.tmp.h
	$(CXXCOMPILE) -x c++ -E -o $<.3.tmp.h $<.2.tmp.h
	cat $<.1.tmp.h $<.3.tmp.h > $@
	rm -f $<.1.tmp.h $<.2.tmp.h $<.3.tmp.h

%.moc.cpp: %.moc.h
	$(MOC) -o $@ $<

clean-local:
	rm -f *.moc.cpp *.moc.h *.tmp.h

dist_sysconf_DATA = cmsg.xml cmsg.dtd

nobase_dist_data_DATA = themes/simple/qtrc

bin_PROGRAMS = oqlogs oqdock oqecho oqedit oqscreens oqmap oqmsg oqsee

noinst_LIBRARIES = liboqcommon.a liboqcanvas.a

LDADD = $(top_builddir)/watch/libowatch.a \
	$(top_builddir)/util/liboutil.a -lpthread -ldl \
	$(QT_LDADD)

liboqcommon_a_SOURCES = oqcommon.cpp oqapp.cpp oqcommon.h \
			oqwatch.cpp oqwatch.h \
			oqpiclabel.cpp oqpiclabel.h
nodist_liboqcommon_a_SOURCES = oqcommon.moc.cpp oqwatch.moc.cpp \
				oqpiclabel.moc.cpp

liboqcanvas_a_SOURCES = oqcanvas.h oqcanvas.cpp oqcanvasview.cpp ocwidget.cpp \
			ocwidgets.h occlasses.cpp oclabels.cpp occontrols.cpp \
			ocbuttons.cpp ocranges.cpp ocmeters.cpp ocindicators.cpp \
			occustom.cpp
nodist_liboqcanvas_a_SOURCES = oqcanvas.moc.cpp ocwidgets.moc.cpp

oqlogs_SOURCES = oqlogs.cpp oqlogs.h
nodist_oqlogs_SOURCES = oqlogs.moc.cpp
oqlogs_LDADD = liboqcommon.a $(LDADD)

oqdock_SOURCES = oqdock.cpp oqdock.h
nodist_oqdock_SOURCES = oqdock.moc.cpp
oqdock_LDADD = liboqcommon.a $(LDADD)

oqecho_SOURCES = oqecho.cpp oqecho.h
nodist_oqecho_SOURCES = oqecho.moc.cpp
oqecho_LDADD = liboqcommon.a $(LDADD)

oqedit_SOURCES = oqedit.cpp oqedit.h
nodist_oqedit_SOURCES = oqedit.moc.cpp
oqedit_LDADD = liboqcanvas.a liboqcommon.a $(LDADD)

oqscreens_SOURCES = oqscreens.cpp oqscreens.h
nodist_oqscreens_SOURCES = oqscreens.moc.cpp
oqscreens_LDADD = liboqcanvas.a liboqcommon.a $(LDADD)

oqmap_SOURCES = oqmap.cpp oqmap.h
nodist_oqmap_SOURCES = oqmap.moc.cpp
oqmap_LDADD = liboqcommon.a $(LDADD)

oqmsg_SOURCES = oqmsg.cpp oqmsg.h
nodist_oqmsg_SOURCES = oqmsg.moc.cpp
oqmsg_LDADD = liboqcommon.a $(LDADD)

oqsee_SOURCES = oqsee.cpp oqsee.h
nodist_oqsee_SOURCES = oqsee.moc.cpp
oqsee_LDADD = liboqcommon.a $(LDADD)

