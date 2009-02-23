# initial project information
AC_INIT([Insight], [0.1], [insight@dmi.me.uk])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_COPYRIGHT([Insight is (C) David Ingram <insight@dmi.me.uk>])

## check source folder
AC_CONFIG_SRCDIR(src/insight.c)

# check for programs
AC_PROG_CC
AC_PROG_LIBTOOL

# automake oddness
AM_PROG_CC_C_O

# check for libraries
AC_SEARCH_LIBS(dlsym, [dl])

# check for taglib
AC_DEFUN([AC_CHECK_TAGLIB], [
  succeeded=no

  if test -z "$TAGLIB_CONFIG"; then
    AC_PATH_PROG(TAGLIB_CONFIG, taglib-config, no)
  fi

  if test "$TAGLIB_CONFIG" = "no" ; then
    echo "*** The taglib-config script could not be found. Make sure it is"
    echo "*** in your path, and that taglib is properly installed."
    echo "*** Or see http://developer.kde.org/~wheeler/taglib.html"
  else
    TAGLIB_VERSION=`$TAGLIB_CONFIG --version`
    AC_MSG_CHECKING(for taglib >= $1)
        VERSION_CHECK=`expr $TAGLIB_VERSION \>\= $1`
        if test "$VERSION_CHECK" = "1" ; then
            AC_MSG_RESULT(yes)
            succeeded=yes

            AC_MSG_CHECKING(TAGLIB_CFLAGS)
            TAGLIB_CFLAGS=`$TAGLIB_CONFIG --cflags`
            AC_MSG_RESULT($TAGLIB_CFLAGS)

            AC_MSG_CHECKING(TAGLIB_LIBS)
            TAGLIB_LIBS=`$TAGLIB_CONFIG --libs`
            AC_MSG_RESULT($TAGLIB_LIBS)
        else
            TAGLIB_CFLAGS=""
            TAGLIB_LIBS=""
            ## If we have a custom action on failure, don't print errors, but
            ## do set a variable so people can do so.
            ifelse([$3], ,echo "can't find taglib >= $1",)
        fi

        AC_SUBST(TAGLIB_CFLAGS)
        AC_SUBST(TAGLIB_LIBS)
  fi

  if test $succeeded = yes; then
     ifelse([$2], , :, [$2])
  else
     ifelse([$3], , AC_MSG_ERROR([Library requirements (taglib) not met.]), [$3])
  fi
])
AC_CHECK_TAGLIB(1.5, have_taglib=yes, have_taglib=no)

# check for headers
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS([errno.h fcntl.h stddef.h stdlib.h string.h sys/statfs.h sys/statvfs.h sys/vfs.h syslog.h unistd.h utime.h])

# check for typedefs, structures and compiler characteristics
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])

# check for library functions
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([memset mkdir rmdir setenv strdup strerror utime setxattr])

# set up defualt CFLAGS
AC_SUBST([CFLAGS],["${CFLAGS} -D_FILE_OFFSET_BITS=64 -Wall -W"])

# option: debug mode
AC_MSG_CHECKING(whether to build with debug information)

debugmode=no
AC_ARG_ENABLE(debug,
              AC_HELP_STRING(
                             [--enable-debug],
                             [enable debug mode (@<:@default=no@:>@)]
                            ),
              debugmode="$enableval"
             )

AC_MSG_RESULT($debugmode)

if test x"$debugmode" = x"yes"; then
  #AC_SUBST([CFLAGS],["-D_DEBUG ${CFLAGS}"])
  AC_DEFINE(_DEBUG, 1, [Enable debug mode])
fi


# option: tree statistics
AC_MSG_CHECKING(whether to output tree statistics)

treestats=no
AC_ARG_ENABLE(treestats,
              AC_HELP_STRING(
                             [--enable-tree-stats],
                             [enable tree statistics output (@<:@default=no@:>@)]
                            ),
              treestats="$enableval"
             )

AC_MSG_RESULT($treestats)

if test x"$treestats" = x"yes"; then
  AC_DEFINE(TREE_STATS_ENABLED, 1, [Enable tree statistics])
fi


# option: tree cache
AC_MSG_CHECKING(whether to enable tree cache)

treecache=no
AC_ARG_ENABLE(treecache,
              AC_HELP_STRING(
                             [--enable-tree-cache],
                             [enable tree memory cache (HIGHLY RECOMMENDED) (@<:@default=yes@:>@)]
                            ),
              treecache="$enableval"
             )

AC_MSG_RESULT($treecache)

if test x"$treecache" = x"yes"; then
  AC_DEFINE(TREE_CACHE_ENABLED, 1, [Enable tree cache])
fi


# option: plugin directory
AC_MSG_CHECKING(for plugin directory)

if test "$prefix" = "NONE"; then
    prefix=$ac_default_prefix
fi

if test "$exec_prefix" = "NONE"; then
    exec_prefix=$prefix
fi

INSIGHT_PLUGIN_DIR=`eval echo "${libdir}/insight"`

AC_ARG_WITH(plugin-dir,
              AC_HELP_STRING(
                             [--with-plugin-dir=DIR],
                             [Specify location of Insight plugins (@<:@default=LIBDIR/insight@:>@)]
                            ),
              INSIGHT_PLUGIN_DIR="$withval"
             )

AC_MSG_RESULT($INSIGHT_PLUGIN_DIR)

if ! test -z "$INSIGHT_PLUGIN_DIR"; then
  AC_DEFINE_UNQUOTED(INSIGHT_DEFAULT_PLUGIN_DIR, "${INSIGHT_PLUGIN_DIR}", [Plugin directory])
fi



# verify FUSE is installed
AC_CHECK_LIB([fuse],[fuse_main],,[
  echo "FUSE (Filesystem in USErspace) is a userspace library which allows Linux"
  echo "programmers to create filesystem implementations without dealing with complex"
  echo "kernel internals. Insight is entirely based on FUSE. Please install the"
  echo "libfuse-dev package (or equivalent which best fits your Linux distribution) or"
  echo "download and compile sources from: http://fuse.sourceforge.net/"
  exit 1
])

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4]);

# output files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Doxyfile Makefile src/Makefile test/Makefile])

# do it
AC_OUTPUT
