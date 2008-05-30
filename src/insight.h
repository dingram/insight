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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 26
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#if FUSE_USE_VERSION == 25
# if HAVE_SYS_STATVFS_H
#   include <sys/statvfs.h>
# endif
#else
# if HAVE_SYS_STATFS_H
#   include <sys/statfs.h>
# endif
#endif
#include <stddef.h>
#include <utime.h>
#include <signal.h>
#include <syslog.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <fuse.h>
#include <fuse/fuse_opt.h>

/**
 * Command line options
 */
struct insight {
  int    debug;         /**< Debug mode */
  int    foreground;    /**< Run in foreground */
  int    singlethread;  /**< Single thread mode */
  int    readonly;      /**< Mount filesystem readonly */
  int    verbose;       /**< Enable verbose logging (syslog only) */
  int    quiet;         /**< Log very little */

  char  *progname;      /**< "insight" */
  char  *mountpoint;    /**< Where insight has been mounted */
  char  *treestore;     /**< Path to the tree storage file */
};

/*extern struct insight insight;*/

/** free() with guard to avoid double-freeing anything */
#define ifree(x) do {\
	if (x) {\
		free(x);\
		x = NULL;\
	} else {\
		PMSG(LOG_ERR, "ERROR in free(): %s is NULL!", #x);\
	}\
} while (0)
