
## Makefile.am -- Process this file with automake to produce Makefile.in

lib_LIBRARIES = liboslayer.a

liboslayer_a_SOURCES = misc-ttb.c ttb.h conf-os.h

if LINUX
liboslayer_a_SOURCES += os-linux.c
endif
if SOLARIS
liboslayer_a_SOURCES += os-solaris.c
endif
if VXWORKS
liboslayer_a_SOURCES += os-vxworks.c
endif

