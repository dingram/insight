#ifndef _DEBUG_H
#define _DEBUG_H

/** @cond */
#if !defined(_MSG_SYSLOG) && !defined(_MSG_STDERR)
#define _MSG_STDERR
#endif

#ifdef _MSG_STDERR
#include <stdio.h>
#define MSG(l, f, ...)  fprintf(stderr, f "\n",## __VA_ARGS__)
#define INIT_LOG()
#else
#include <syslog.h>
#define MSG(l, f, ...)  do { if (!insight.quiet) syslog(l, "%s: " f "\n", __FUNCTION__,## __VA_ARGS__) } while (0)
#define INIT_LOG()      do { openlog("insight", LOG_PID, LOG_DAEMON); } while(0)
#endif

#define FMSG(l, f, ...) MSG(l, "%s: " f, __FUNCTION__,## __VA_ARGS__)
#define LMSG(l, f, ...) MSG(l, "%s:%d: " f, __FILE__, __LINE__,## __VA_ARGS__)
#define PMSG(l, f, ...) MSG(l, "%s:%d:%s: " f, __FILE__, __LINE__, __FUNCTION__,## __VA_ARGS__)

#ifdef _DEBUG
# define DEBUG(f, ...)  PMSG(LOG_DEBUG, f,## __VA_ARGS__)
# define DEBUGMSG(x)    DEBUG("%s", x)
# define DUMPINT(x)     DEBUG("%s = %d\n", #x, x)
# define DUMPSTR(x)     DEBUG("%s = %s\n", #x, x)
#else
# define DEBUG(f, ...)
# define DEBUGMSG(x)
# define DUMPINT(x)
# define DUMPSTR(x)
#endif

#define COL_RED     "\033[1;31m"
#define COL_GREEN   "\033[1;32m"
#define COL_YELLOW  "\033[1;33m"
#define COL_BLUE    "\033[1;34m"
#define COL_MAGENTA "\033[1;35m"
#define COL_CYAN    "\033[1;36m"
#define COL_WHITE   "\033[1;37m"
#define COL_RESET   "\033[m"
/** @endcond */

#endif
