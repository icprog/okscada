dnl -*- Autoconf -*-      vi: set ts=4 sw=4 :
dnl Process this file with autoconf to produce a configure script.
AC_REVISION($Id$)dnl

dnl **** configure.ac for Optikus ****

dnl ---- Project requisites ---
AC_PREREQ([2.59])
AC_INIT([optikus], [0.1], [ivandeex@gmail.com])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE
AC_DEFINE([PACKAGE_COPYRIGHT], ["Copyright (c) 2006-2007, ivandeex"],
			[Define to the project copyright requisites.])
AC_DEFINE([PACKAGE_WEBSITE], ["https://github.com/ivandeex/optikus"],
			[Define to the project address on the web.])

dnl Set the host and host_alias variables.
AC_CANONICAL_HOST

dnl config files & directories
AC_CONFIG_SRCDIR([ChangeLog])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_FILES([Makefile include/Makefile util/Makefile
				hub/Makefile watch/Makefile subj/Makefile
				doc/Makefile data/Makefile test/Makefile])

dnl TEMPORARY, DEVELOPMENT ONLY !!!
AC_PREFIX_DEFAULT([/home/vit/optikus])


dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB


dnl --- Checks for operating system. ---
case "${host}" in
 *-*-linux* )
	oks_cv_os=LINUX
	AC_DEFINE([LINUX], [1], [Define if compiling for Linux])
	default_enable_gtk1=yes
	;;
 *-*-solaris2* )
	oks_cv_os=SOLARIS
	AC_DEFINE([SOLARIS], [1], [Define if compiling for Solaris])
	default_enable_gtk1=yes
	;;
 *-*-vxworks* )
	oks_cv_os=VXWORKS
	AC_DEFINE([VXWORKS], [1], [Define if compiling for VxWorks])
	default_enable_gtk1=no
	;;
 * )
	AC_MSG_ERROR([Building for ${host} is not supported yet.])
	;;
esac

AM_CONDITIONAL([LINUX], [test $oks_cv_os = "LINUX"])
AM_CONDITIONAL([SOLARIS], [test $oks_cv_os = "SOLARIS"])
AM_CONDITIONAL([VXWORKS], [test $oks_cv_os = "VXWORKS"])

AC_C_BIGENDIAN()


dnl --- Checks for libraries. ---

dnl --- Checks for header files. ---
dnl? AC_HEADER_STDC
dnl? AC_CHECK_HEADERS([arpa/inet.h fcntl.h libintl.h locale.h malloc.h \
dnl? 				netdb.h netinet/in.h stdlib.h string.h strings.h \
dnl? 				sys/ioctl.h sys/socket.h sys/time.h unistd.h])
AC_CHECK_HEADERS([tundra.h])

dnl Checks for typedefs, structures, and compiler characteristics.

dnl? AC_HEADER_STDBOOL
dnl? AC_C_CONST
dnl? AC_TYPE_SIZE_T
dnl? AC_HEADER_TIME
dnl? AC_STRUCT_TM
dnl? AC_C_VOLATILE

# Checks for library functions.
dnl? AC_FUNC_FORK
dnl? AC_PROG_GCC_TRADITIONAL
dnl? AC_FUNC_MALLOC
dnl? AC_FUNC_MEMCMP
dnl? AC_FUNC_MKTIME
dnl? AC_FUNC_MMAP
dnl? AC_FUNC_REALLOC
dnl? AC_FUNC_SELECT_ARGTYPES
dnl? AC_TYPE_SIGNAL
dnl? AC_FUNC_STAT
dnl? AC_FUNC_VPRINTF# tests

dnl? AC_CHECK_FUNCS([atexit bzero clock_gettime gethostbyname gethostname \
dnl? 				gettimeofday inet_ntoa localeconv localtime_r \
dnl? 				memmove memset mkdir munmap select socket \
dnl? 				strchr strcspn strdup strerror strpbrk strstr \
dnl? 				strtol strtoul tzset])


dnl Check for GTK+
test -z "$enable_gtk1" && enable_gtk1=$default_enable_gtk1
AC_ARG_ENABLE(gtk1,  [  --enable-gtk1    use GTK+])
if test "$enable_gtk1" = "yes" ; then
  dnl detect GTK1.x
  AM_PATH_GTK(1.2.7, oks_cv_gtk_version=1.2.7)
  if test -z "$oks_cv_gtk_version" ; then
	AC_MSG_ERROR([GTK+ not found])
  fi
  dnl AC_MSG_NOTICE([Found GTK+ v.$oks_cv_gtk_version, CFLAGS="$GTK_CFLAGS", LIBS="$GTK_LIBS" ])
fi
AM_CONDITIONAL([GTK], [test $enable_gtk1 = yes])


# Default flags for C programs.
oks_INCLUDES="-I\${top_srcdir}/include"
oks_DEFINES="-D${oks_cv_os}=1"
CPPFLAGS="$CPPFLAGS $oks_INCLUDES $oks_DEFINES -Wall"


dnl Check for Perl.
AC_PATH_PROG(PERL, perl, no)

AC_ARG_ENABLE(perl, [  --disable-perl          do not build the perl modules],
				[], [enable_perl=yes])

AC_ARG_VAR(PERLCC, [[] C compiler for Perl modules])
AC_ARG_VAR(PERLCCFLAGS, [[] CC flags for Perl modules])
AC_ARG_VAR(PERLLD, [[same as PERLCC] Linker for Perl modules])
AC_ARG_VAR(PERLLDFLAGS, [[] LD flags for Perl modules])

if test x"$PERL" = x"no" -o x"$enable_perl" = x"no"; then
  PERL_BINDINGS=
else
  PERL_BINDINGS="perl_binding"
  AC_MSG_CHECKING([for the perl version you are running])
  PERL_VERSION=`$PERL -MConfig -e 'print $Config{version}'`
  AC_MSG_RESULT($PERL_VERSION)
  if test -z "$PERLCC"; then
    AC_MSG_CHECKING([for the C compiler perl wants to use to build its modules])
    perlcc=`$PERL -MConfig -e 'print $Config{cc}'`
    AC_MSG_RESULT($perlcc)
    if test ! -x "$perlcc"; then
      AC_PATH_PROG(PERL_CC, "${perlcc}", no)
      if test "$PERL_CC" = "no"; then
        AC_MSG_WARN([
Cannot find the Compiler ($perlcc) that was originally used to compile
your perl binary. You should either make sure that this compiler is
available on your system, pick another compiler and set PERLCC
appropriately, or use a different perl setup that was compiled locally.
Disabling compilation of the Optikus perl module for now.
])
        PERL_BINDINGS=""
      fi
    fi   
  fi
fi

AC_MSG_CHECKING([Perl Modules to build])
AC_MSG_RESULT(${PERL_BINDINGS:-No Perl Modules will be built})

# Options to pass when configuring perl module
perlpref=$prefix
test "$perlpref" = "NONE" && perlpref=$ac_default_prefix

PERL_FLAGS="PREFIX=$perlpref"
PERL_DEFINES="$oks_INCLUDES $oks_DEFINES"

dnl pass additional perl options when generating Makefile from Makefile.PL
AC_ARG_ENABLE(perl-site-install,
  [  --enable-perl-site-install   by default the optikus perl modules are installed
                          together with optikus in $prefix/lib/perl. You have to
                          put a 'use lib qw($prefix/lib/perl5)' into your scripts
                          when you want to use them. When you set this option
                          the perl modules will get installed wherever your perl
                          setup thinks it is best.],
  [PERL_FLAGS=],[])

if test -n "$PERLCC"; then
  PERL_FLAGS="$PERL_FLAGS CC=$PERLCC"

  if test -n "$PERLCCFLAGS"; then
    PERL_FLAGS="$PERL_FLAGS CCFLAGS=\"$PERLCCFLAGS\""
  fi

  if test -z "$PERLLD"; then
    PERLLD=$PERLCC
  fi
  PERL_FLAGS="$PERL_FLAGS LD=$PERLLD"

  if test ! -z "$PERLLDFLAGS"; then
    PERL_FLAGS="$PERL_FLAGS LDFLAGS=$PERLLDFLAGS"
  fi
fi
       
AC_ARG_WITH(perl-options,
[  --with-perl-options=[OPTIONS]  options to pass on command-line when
                     generating Makefile from Makefile.PL. If you set this
                     option, you should know what you are doing!],
[PERL_FLAGS=$withval])

AC_SUBST(PERL)
AC_SUBST(PERL_VERSION)
AC_SUBST(PERL_BINDINGS)
AC_SUBST(PERL_FLAGS)
AC_SUBST(PERL_DEFINES)

AC_CONFIG_FILES([bindings/Makefile bindings/perl/Makefile.PL])


dnl Check for QT.

gw_CHECK_QT
AC_CONFIG_FILES([qforms/Makefile])


dnl Strict memory allocation checks

AC_ARG_ENABLE(malloc-assert,
  [  --enable-malloc-assert  Makes memory allocation routines immediately abort
                          program if allocation fails.])
if test "$enable_malloc_assert" = "yes" ; then
  AC_DEFINE([MALLOC_ASSERTION], [1],
			[Set to 1 to enable program abort when memory allocation fails])
fi


dnl maintainer-clean will remove Makefile.in's too
dnl this is against common automake rules, for maintainer only.
MAINTAINERCLEANFILES="Makefile.in configure"
AC_SUBST(MAINTAINERCLEANFILES)

AC_OUTPUT

