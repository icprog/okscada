
## Makefile.am -- Process this file with automake to produce Makefile.in

bin_PROGRAMS = ohubd
ohubd_SOURCES =	hub-main.c hub-util.c hub-config.c hub-proper.c \
		hub-quarks.c hub-netinfo.c hub-ooid.c hub-msg.c \
		hub-watch.c hub-subj.c hub-subject.c hub-data.c \
		hub-monitor.c hub-priv.h
# FIXME: ncurses/readline shall be detected in configure.ac
ohubd_LDADD =	$(top_builddir)/util/liboutil.a \
		-lreadline -lncurses -lpthread -ldl

