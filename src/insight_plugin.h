#ifndef __INSIGHT_PLUGIN_H
#define __INSIGHT_PLUGIN_H
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

/* need the LOG_* constants */
#include <syslog.h>
/* need the E* constants */
#include <errno.h>
#include <sys/types.h>

#define INSIGHT_PLUGIN_PREFIX      "libinsight_"
#define INSIGHT_PLUGIN_FUNC_PREFIX "insight_plugin_"

#define INSIGHT_PLUGIN_INIT    INSIGHT_PLUGIN_FUNC_PREFIX "init"
#define INSIGHT_PLUGIN_CHMOD   INSIGHT_PLUGIN_FUNC_PREFIX "chmod"
#define INSIGHT_PLUGIN_CHOWN   INSIGHT_PLUGIN_FUNC_PREFIX "chown"
#define INSIGHT_PLUGIN_WRITE   INSIGHT_PLUGIN_FUNC_PREFIX "write"
#define INSIGHT_PLUGIN_RENAME  INSIGHT_PLUGIN_FUNC_PREFIX "rename"
#define INSIGHT_PLUGIN_IMPORT  INSIGHT_PLUGIN_FUNC_PREFIX "import"
#define INSIGHT_PLUGIN_CLEANUP INSIGHT_PLUGIN_FUNC_PREFIX "cleanup"

#ifndef _TYPE_FILEPTR
#define _TYPE_FILEPTR
/** An address within a file */
typedef unsigned long fileptr;
#endif

typedef struct insight_funcs {
  int (*add_tag)(fileptr, const char *);
  int (*add_auto_tag)(fileptr, const char *);
  int (*del_tag)(fileptr, const char *);
  int (*del_auto_tag)(fileptr, const char *);
  fileptr (*get_inode)(const char *);

  void (*log)(int, const char*, ...);
} insight_funcs;

#define INSIGHT_CAP_CANNOT  0
#define INSIGHT_CAP_CAN     1
#define INSIGHT_CAP_CAN_ALL 2

typedef struct insight_plugin_caps {
  int interface_version;                /**< Interface version supported by this plugin */
  unsigned int can_transduce_import:2;  /**< Can transduce files on import */
  unsigned int can_transduce_chown:2;   /**< Can transduce files on chown */
  unsigned int can_transduce_chmod:2;   /**< Can transduce files on chmod */
  unsigned int can_transduce_write:2;   /**< Can transduce files on write */
  unsigned int can_transduce_rename:2;  /**< Can transduce files on rename */
  unsigned int padding:22;              /**< Padding */
  char **transduce_mime_types;          /**< Array of MIME types the plugin can handle, terminated by NULL pointer */
  char **transduce_exts;                /**< Array of file extension the plugin can handle, terminated by NULL pointer */
} insight_plugin_caps;

typedef struct insight_plugin {
  char *path;
  void *dlhandle;
  /* ***** function pointers begin ********************************************/
  int (*init)(insight_plugin_caps *, insight_funcs *);
  int (*chmod)(const char *, const char *, mode_t);
  int (*chown)(const char *, const char *, uid_t, gid_t);
  int (*write)(const char *, const char *);
  int (*rename)(const char *, const char *, const char *);
  int (*import)(const char *, const char *);
  void (*cleanup)();
  /* *****  function pointers end  ********************************************/
  insight_plugin_caps caps;
  struct insight_plugin *next;
} insight_plugin;

#endif
