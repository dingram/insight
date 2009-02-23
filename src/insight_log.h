#ifndef __INSIGHT_LOG_H
#define __INSIGHT_LOG_H
/*
 * Copyright (C) 2008 David Ingram
 *
 * This program is released under a Creative Commons
 * Attribution-NonCommerical-ShareAlike2.5 License.
 *
 * For more information, please see
 *   http://creativecommons.org/licenses/by-nc-sa/2.5/
 *
 * You are free:
 *
 *   * to copy, distribute, display, and perform the work
 *   * to make derivative works
 *
 * Under the following conditions:
 *   Attribution:   You must attribute the work in the manner specified by the
 *                  author or licensor.
 *   Noncommercial: You may not use this work for commercial purposes.
 *   Share Alike:   If you alter, transform, or build upon this work, you may
 *                  distribute the resulting work only under a license identical
 *                  to this one.
 *
 *   * For any reuse or distribution, you must make clear to others the
 *     license terms of this work.
 *   * Any of these conditions can be waived if you get permission from the
 *     copyright holder.
 *
 * Your fair use and other rights are in no way affected by the above.
 */

/** @cond */
#if !defined(_MSG_SYSLOG) && !defined(_MSG_STDERR)
#define _MSG_STDERR
#endif

#ifdef _MSG_STDERR
#include <stdio.h>
//#define MSG(l, f, ...)  fprintf((l<=LOG_WARNING)?stderr:stdout, f "\n",## __VA_ARGS__)
#define INIT_LOG()
#else
#ifndef S_SPLINT_S
#include <syslog.h>
#endif
//#define MSG(l, f, ...)  do { if (!insight.quiet) syslog(l, "%s: " f "\n", __FUNCTION__,## __VA_ARGS__) } while (0)
#define INIT_LOG()      do { openlog("insight", LOG_PID, LOG_DAEMON); } while(0)
#endif
/** @endcond */

#define MSG(l, f, ...)  insight_log(l, f,## __VA_ARGS__)

#ifndef LOG_EMERG
# define	LOG_EMERG	0	/**< system is unusable */
# define	LOG_ALERT	1	/**< action must be taken immediately */
# define	LOG_CRIT	2	/**< critical conditions */
# define	LOG_ERR		3	/**< error conditions */
# define	LOG_WARNING	4	/**< warning conditions */
# define	LOG_NOTICE	5	/**< normal but significant condition */
# define	LOG_INFO	6	/**< informational */
# define	LOG_DEBUG	7	/**< debug-level messages */
#endif

void insight_log(int lvl, const char *fmt, ...);

#endif
