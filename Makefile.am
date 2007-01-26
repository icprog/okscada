
## Makefile.am -- Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I config

EXTRA_DIST = autogen.sh config/COPYING.gwqt

SUBDIRS = include util hub watch subj \
	test bindings qforms \
	doc

docdir = $(datadir)/doc
# This is for HTML and other documentation you want to install.
# Add your documentation files (in doc/) in addition to these
# top-level boilerplate files.  Also add a TODO file if you have one.
# We'll add to this later, on a library-by-library basis
dist_doc_DATA = AUTHORS COPYING ChangeLog INSTALL NEWS README

# Shared data installation takes long, shall be the last one.
SUBDIRS += share

