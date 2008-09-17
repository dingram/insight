bin_PROGRAMS=insight
insight_SOURCES=insight.c \
                query_engine.c \
                query_engine.h \
                string_helpers.c \
                string_helpers.h \
                set_ops.h \
                set_ops.c \
                path_helpers.h \
                path_helpers.c \
                insight_log.h \
                insight_log.c \
                plugin_handler.h \
                plugin_handler.c \
                debug.h \
								bplus_debug.h \
								bplus.c \
                bplus.h \
								bplus_priv.h \
                insight.h

lib_LTLIBRARIES = libinsight_autoowner-0.1.la\
									libinsight_autogroup-0.1.la\
									libinsight_autoext-0.1.la
libinsight_autoowner_0_1_la_SOURCES = insight_plugin.h plugins/auto_owner.c
libinsight_autogroup_0_1_la_SOURCES = insight_plugin.h plugins/auto_group.c
libinsight_autoext_0_1_la_SOURCES   = insight_plugin.h plugins/auto_ext.c
