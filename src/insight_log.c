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

#include <insight_log.h>
#include <stdarg.h>

/**
 * Common logging function. Depending on whether _MSG_STDERR is defined, will
 * log either to <tt>stderr</tt>/<tt>stdout</tt> or syslog. This function will
 * automatically append a newline to the message.
 *
 * @param lvl The logging level. Anything LOG_WARNING or more serious will be
 *            logged to <tt>stderr</tt>, otherwise it will be logged to
 *            <tt>stdout</tt>.
 * @param fmt The printf()-style format for the log message.
 */
void insight_log(int lvl, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
#ifdef _MSG_STDERR
  vfprintf((lvl<=LOG_WARNING)?stderr:stdout, fmt, ap);
  fprintf((lvl<=LOG_WARNING)?stderr:stdout, "\n");
#else
  vsyslog(lvl, fmt, ap);
  syslog(lvl, "\n");
#endif
  va_end(ap);
}
