
## Makefile.am -- Process this file with automake to produce Makefile.in

%.moc.cpp: %.h
	$(MOC) -o $@ $<

AM_CXXFLAGS := $(AM_CXXFLAGS) $(QT_CXXFLAGS) -Wno-non-virtual-dtor

clean-local:
	rm -f *.moc.cpp

dist_sysconf_DATA = cmsg.xml cmsg.dtd

bin_PROGRAMS = oqlogs oqdock oqecho oqedit oqforms oqmap oqmsg oqsee

common_src = oqcommon.cpp oqcommon.h oqwatch.cpp oqwatch.h
common_ndsrc = oqcommon.moc.cpp oqwatch.moc.cpp
canvas_src = oqcanvas.cpp ocwidgets.cpp oqcanvas.h ocwidgets.h
canvas_ndsrc = oqcanvas.moc.cpp
LDADD = $(top_builddir)/watch/libowatch.a \
	$(top_builddir)/util/liboutil.a -lpthread -ldl \
	$(QT_LDADD)

oqlogs_SOURCES = oqlogs.cpp oqlogs.h $(common_src)
nodist_oqlogs_SOURCES = oqlogs.moc.cpp $(common_ndsrc)

oqdock_SOURCES = oqdock.cpp oqdock.h $(common_src)
nodist_oqdock_SOURCES = oqdock.moc.cpp $(common_ndsrc)

oqecho_SOURCES = oqecho.cpp oqecho.h $(common_src)
nodist_oqecho_SOURCES = oqecho.moc.cpp $(common_ndsrc)

oqedit_SOURCES = oqedit.cpp oqedit.h $(common_src) $(canvas_src)
nodist_oqedit_SOURCES = oqedit.moc.cpp $(common_ndsrc) $(canvas_ndsrc)

oqforms_SOURCES = oqforms.cpp oqforms.h $(common_src) $(canvas_src)
nodist_oqforms_SOURCES = oqforms.moc.cpp $(common_ndsrc) $(canvas_ndsrc)

oqmap_SOURCES = oqmap.cpp oqmap.h $(common_src)
nodist_oqmap_SOURCES = oqmap.moc.cpp $(common_ndsrc)

oqmsg_SOURCES = oqmsg.cpp oqmsg.h $(common_src)
nodist_oqmsg_SOURCES = oqmsg.moc.cpp $(common_ndsrc)

oqsee_SOURCES = oqsee.cpp oqsee.h $(common_src)
nodist_oqsee_SOURCES = oqsee.moc.cpp $(common_ndsrc)

