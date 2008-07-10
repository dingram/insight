#ifndef _DEBUG_H
#define _DEBUG_H

/** @cond */
#if !defined(_MSG_SYSLOG) && !defined(_MSG_STDERR)
#define _MSG_STDERR
#endif

/* need the LOG_* constants */
#include <syslog.h>

#ifdef _MSG_STDERR
#include <stdio.h>
//#define MSG(l, f, ...)  fprintf((l<=LOG_WARNING)?stderr:stdout, f "\n",## __VA_ARGS__)
#define INIT_LOG()
#else
//#define MSG(l, f, ...)  do { if (!insight.quiet) syslog(l, "%s: " f "\n", __FUNCTION__,## __VA_ARGS__) } while (0)
#define INIT_LOG()      do { openlog("insight", LOG_PID, LOG_DAEMON); } while(0)
#endif
#define MSG(l, f, ...)  insight_log(l, f,## __VA_ARGS__)

#define FMSG(l, f, ...) MSG(l, "%s: " f, __FUNCTION__,## __VA_ARGS__)
#define LMSG(l, f, ...) MSG(l, "%s:%d: " f, __FILE__, __LINE__,## __VA_ARGS__)
#define PMSG(l, f, ...) MSG(l, "%s:%d:%s: " f, __FILE__, __LINE__, __FUNCTION__,## __VA_ARGS__)

#ifdef _DEBUG
# define DEBUG(f, ...)  PMSG(LOG_DEBUG, f,## __VA_ARGS__)
# define DEBUGMSG(x)    DEBUG("%s", x)
# define DUMPINT(x)     DEBUG("%s = %d", #x, x)
# define DUMPUINT(x)    DEBUG("%s = %u", #x, x)
# define DUMPFILEPTR(x) DEBUG("%s = %lu", #x, x)
# define DUMPSTR(x)     DEBUG("%s = \"%s\"", #x, x)
#else
# define DEBUG(f, ...)
# define DEBUGMSG(x)
# define DUMPINT(x)
# define DUMPUINT(x)
# define DUMPFILEPTR(x)
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

#ifdef _TIME_PROFILE
# include <sys/time.h>
# include <time.h>
# define timeval_subtract(a,b,c) do {\
  (a)->tv_sec=(b)->tv_sec - (c)->tv_sec;\
  (a)->tv_usec=(b)->tv_usec - (c)->tv_usec;\
} while (0)
# define profile_init()       struct timeval tv_start, tv_stop, result
# define profile_init_start() profile_init(); profile_start();
# define profile_start() gettimeofday(&tv_start, NULL)
# define profile_stop() do {\
	gettimeofday(&tv_stop, NULL);\
	timeval_subtract(&result, &tv_stop, &tv_start);\
	MSG(LOG_INFO, COL_GREEN ">> " COL_RESET "PROFILE[" COL_WHITE "%s" COL_RESET "]: %ld.%.6ld sec",\
		__FUNCTION__, (long int) result.tv_sec, (long int) result.tv_usec);\
} while (0)
# define profile_stopf(f,...) do {\
	gettimeofday(&tv_stop, NULL);\
	timeval_subtract(&result, &tv_stop, &tv_start);\
	MSG(LOG_INFO, COL_GREEN ">> " COL_RESET "PROFILE[" COL_WHITE "%s" COL_RESET "]: %ld.%.6ld sec: " f,\
		__FUNCTION__, (long int) result.tv_sec, (long int) result.tv_usec, ## __VA_ARGS__);\
} while (0)
#else
# define profile_init()
# define profile_init_start()
# define profile_start()
# define profile_stop()
# define profile_stopf(f,...)
#endif

#endif
