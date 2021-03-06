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

/* Define required to get prototypes for pread/pwrite */
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif
#define _XOPEN_SOURCE 500

/* But doing so might hide other interfaces */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
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
#include <sys/time.h>
#include <utime.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <plugin_handler.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <fuse.h>
#include <fuse/fuse_opt.h>

#ifndef MAX
#define MAX(a,b)      ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)      ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX3
#define MAX3(a,b,c)   (MAX(a, MAX(b, c)))
#endif

#ifndef MIN3
#define MIN3(a,b,c)   (MIN(a, MIN(b, c)))
#endif

#include <insight_consts.h>

/** Macro for last character in a string */
#define last_char_in(x) ((x)[strlen(x)-1])

/**
 * Command line options
 */
struct insight {
  int    debug;          /**< Debug mode */
  int    foreground;     /**< Run in foreground */
  int    singlethread;   /**< Single thread mode */
  int    readonly;       /**< Mount filesystem readonly */
  int    verbose;        /**< Enable verbose logging (syslog only) */
  int    quiet;          /**< Log very little */

  char  *progname;       /**< "insight" */
  char  *mountpoint;     /**< Where insight has been mounted */
  char  *treestore;      /**< Path to the tree storage file */
  char  *repository;     /**< Path to the symlink repository */
  size_t repository_len; /**< Length of "repository" (for speed) */
  struct stat mountstat; /**< lstat() results for mountpoint */
  struct insight_plugin *plugins; /**< Linked list of plugins */
  struct insight_funcs funcs; /**< Set of functions for export to plugins */
};

/**
 * A global structure containing information about this Insight instance.
 */
/*extern*/ struct insight insight;

#ifndef ifree
/** free() with guard to try to avoid double-freeing anything */
#define ifree(x) do {\
	if (x) {\
		free(x);\
		x = NULL;\
	} else {\
		PMSG(LOG_ERR, "ERROR in free(): %s is NULL!", #x);\
	}\
} while (0)
#endif

#include <insight_log.h>
