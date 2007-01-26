
## Makefile.am -- Process this file with automake to produce Makefile.in

dist_man1_MANS = \
	ohubd.1 \
	oscan.1

dist_man3_MANS = \
        owatchInit.3 \
        owatchExit.3 \
        owatchGetFileHandles.3 \
        owatchWork.3 \
        owatchGetAsyncTimeout.3 \
        owatchSetAsyncTimeout.3 \
        owatchWaitOp.3 \
        owatchCancelOp.3 \
        owatchDetachOp.3 \
        owatchAddOpHandler.3 \
        owatchRemoveOpHandler.3 \
        owatchConnect.3 \
        owatchDisconnect.3 \
        owatchGetSubjectList.3 \
        owatchGetAlive.3 \
        owatchAddAliveHandler.3 \
        owatchRemoveAliveHandler.3 \
        owatchAddMonitor.3 \
        owatchRemoveMonitor.3 \
        owatchRemoveAllMonitors.3 \
        owatchAddDataHandler.3 \
        owatchRemoveDataHandler.3 \
        owatchEnableMonitoring.3 \
        owatchFlushMonitor.3 \
        owatchFlushAllMonitors.3 \
        owatchRenewMonitor.3 \
        owatchRenewAllMonitors.3 \
        owatchGetDesc.3 \
        owatchGetValue.3 \
        owatchConvertValue.3 \
        owatchRead.3 \
        owatchReadByName.3 \
        owatchGetAutoCommit.3 \
        owatchSetAutoCommit.3 \
        owatchCommit.3 \
        owatchCommitAt.3 \
        owatchGetTime.3 \
        owatchWrite.3 \
        owatchWriteByName.3 \
        owatchReadAsDouble.3 \
        owatchReadAsFloat.3 \
	owatchReadAsLong.3 \
        owatchReadAsString.3 \
        owatchWriteAsDouble.3 \
        owatchWriteAsFloat.3 \
        owatchWriteAsLong.3 \
        owatchWriteAsString.3 \
        owatchSendMessage.3 \
        owatchSendCtlMsg.3

EXTRA_DIST = $(libref_texts_maker) $(libref_man_texts) $(text_man_texts)

A2X = a2x
ASCII2MAN = $(A2X) --format=manpage

%.1: %.1.txt
	@$(ASCII2MAN) $<
	@rm -f $@.xml

%.3: %.3.txt
	@$(ASCII2MAN) $<
	@rm -f $@.xml

text_mans = $(dist_man1_MANS)
text_man_texts = $(text_mans:%=%.txt)
libref_mans = $(dist_man3_MANS)
libref_man_texts = $(libref_mans:%=%.txt)
libref_texts_maker = split-reference.pl lib-reference.txt

$(libref_man_texts): $(libref_texts_maker)
	$(PERL) $(libref_texts_maker)

clean-local:
	list='$(text_mans)'; for p in $$list; do rm -f $$p.xml; done
	list='$(libref_mans)'; for p in $$list; do rm -f $$p.xml; done

HARDCLEAN=no

maintainer-clean-local:
	if test "$(HARDCLEAN)" = "yes"; then \
	  list='$(text_mans)'; for p in $$list; do rm -f $$p; done; \
	  list='$(libref_mans)'; for p in $$list; do rm -f $$p $$p.txt; done; \
	fi

# FIXME: the docdir definition must be synchronous with the root Makefile.
docdir = $(datadir)/doc

# This is for HTML and other documentation you want to install.
# Add your documentation files (in doc/) in addition to these
# top-level boilerplate files.  Also add a TODO file if you have one.
# We'll add to this later, on a library-by-library basis
dist_doc_DATA = 

