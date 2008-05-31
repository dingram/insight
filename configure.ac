# initial project information
AC_INIT([Insight], [0.1], [insight@dmi.me.uk])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_COPYRIGHT([Insight is (C) David Ingram <insight@dmi.me.uk>])

## check source folder
AC_CONFIG_SRCDIR(src/insight.c)

# check for programs
AC_PROG_CC

# check for libraries
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
AC_CHECK_FUNCS([memset mkdir rmdir setenv strdup strerror utime])

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
  AC_SUBST([CFLAGS],["-D_DEBUG ${CFLAGS}"])
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

# output files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

# do it
AC_OUTPUT
