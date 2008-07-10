#ifndef __PLUGIN_HANDLER_H
#define __PLUGIN_HANDLER_H
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

#ifdef MACOSX
#	define PLUGIN_EXT ".dylib"
#else
#	define PLUGIN_EXT ".so"
#endif

#include <insight_plugin.h>

/* process the plugin chain */
int plugin_process_chmod(const char *original_path, const char *filename, mode_t mode);
int plugin_process_chown(const char *original_path, const char *filename, uid_t uid, gid_t gid);
int plugin_process_write(const char *original_path, const char *filename);
int plugin_process_rename(const char *original_path, const char *oldname, const char *newname);
int plugin_process_import(const char *original_path, const char *filename);

/* load/unload plugins */
int plugin_load(const char *plugin_dir, const char *plugin_filename);
int plugin_load_all(const char *plugin_dir);
int plugin_unload_all();

#endif
