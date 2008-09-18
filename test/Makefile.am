TESTS = check_setops check_bplus
check_PROGRAMS = check_setops check_bplus
check_setops_SOURCES = check_setops.c $(top_builddir)/src/insight_log.c $(top_builddir)/src/set_ops.c
check_setops_CFLAGS = @CHECK_CFLAGS@ -I$(top_builddir)/src
check_setops_LDADD = @CHECK_LIBS@
check_bplus_SOURCES = check_bplus.c $(top_builddir)/src/bplus.c $(top_builddir)/src/insight_log.c $(top_builddir)/src/set_ops.c
check_bplus_CFLAGS = @CHECK_CFLAGS@ -I$(top_builddir)/src
check_bplus_LDADD = @CHECK_LIBS@
