XCACHE_PROC_SRC=$(srcdir)/processor/main.m4
XCACHE_PROC_OUT=$(builddir)/processor.out.c
XCACHE_PROC_C=$(builddir)/xc_processor.c.h
XCACHE_PROC_H=$(builddir)/xc_processor.h
XCACHE_INCLUDES_SRC=$(srcdir)/includes.c
XCACHE_INCLUDES_I=$(builddir)/includes.i
XCACHE_STRUCTINFO_OUT=$(builddir)/structinfo.m4

$(XCACHE_INCLUDES_I): $(XCACHE_INCLUDES_SRC) $(srcdir)/xcache.h
	$(CC) -I. -I$(srcdir) $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) -E $(XCACHE_INCLUDES_SRC) -o $(XCACHE_INCLUDES_I)

$(XCACHE_STRUCTINFO_OUT): $(XCACHE_INCLUDES_I) $(srcdir)/gen_structinfo.awk
	@echo $(XCACHE_STRUCTINFO_OUT) is optional if XCache test is not enabled, feel free if it awk failed to produce it
	-$(XCACHE_AWK) -f $(srcdir)/gen_structinfo.awk < $(XCACHE_INCLUDES_I) > $(XCACHE_STRUCTINFO_OUT).tmp && mv $(XCACHE_STRUCTINFO_OUT).tmp $(XCACHE_STRUCTINFO_OUT)

$(XCACHE_PROC_OUT): $(XCACHE_PROC_SRC) $(XCACHE_STRUCTINFO_OUT) $(XCACHE_PROC_SOURCES)
	$(M4) -D srcdir=$(XCACHE_BACKTICK)"$(srcdir)'" -D builddir=$(XCACHE_BACKTICK)"$(builddir)'" $(XCACHE_ENABLE_TEST) $(XCACHE_PROC_SRC) > $(XCACHE_PROC_OUT).tmp
	mv $(XCACHE_PROC_OUT).tmp $(XCACHE_PROC_OUT)

$(XCACHE_PROC_H): $(XCACHE_PROC_OUT)
	$(GREP) "export: " $(XCACHE_PROC_OUT) | $(SED) "s/.*export:\(.*\):export.*/\1/g" > $(XCACHE_PROC_H)
	-$(XCACHE_INDENT) < $(XCACHE_PROC_H) > $(XCACHE_PROC_H).tmp && mv $(XCACHE_PROC_H).tmp $(XCACHE_PROC_H)

$(XCACHE_PROC_C): $(XCACHE_PROC_OUT) $(XCACHE_PROC_H)
	cp $(XCACHE_PROC_OUT) $(XCACHE_PROC_C)
	-$(XCACHE_INDENT) < $(XCACHE_PROC_OUT) > $(XCACHE_PROC_C).tmp && mv $(XCACHE_PROC_C).tmp $(XCACHE_PROC_C)

$(builddir)/xcache/xc_processor.lo: $(XCACHE_PROC_C) $(XCACHE_PROC_H)

$(builddir)/submodules/xc_disassembler.lo: $(XCACHE_PROC_H)

$(builddir)/xc_opcode_spec.lo: $(srcdir)/xcache.h $(srcdir)/xc_opcode_spec.c $(srcdir)/xc_opcode_spec_def.h $(srcdir)/xc_const_string.h

$(builddir)/xcache.lo: $(XCACHE_PROC_H) $(srcdir)/xcache/xc_shm.h $(srcdir)/util/xc_stack.h $(srcdir)/xcache_globals.h $(srcdir)/xcache.c $(srcdir)/util/xc_foreachcoresig.h $(srcdir)/xcache/xc_utils.h

xcachesvnclean: clean
	-svn propget svn:ignore . > .svnignore.tmp 2>/dev/null && mv .svnignore.tmp .svnignore
	cat .svnignore | grep -v devel | grep -v svnignore | grep -v ^Makefile | grep -v ^config.nice | xargs rm -rf

xcachetest: all
	$(SED) "s#\\./modules/#$(top_builddir)/modules/#" < $(srcdir)/xcache-test.ini > $(top_builddir)/tmp-php.ini
	TEST_PHP_SRCDIR=$(srcdir) $(srcdir)/run-xcachetest $(TESTS) $(TEST_ARGS) -c $(top_builddir)/tmp-php.ini
	$(srcdir)/run-xcachetest $(TESTS) $(TEST_ARGS) -c $(top_builddir)/tmp-php.ini
