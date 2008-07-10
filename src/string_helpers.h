#ifndef __STRING_HELPERS_H
#define __STRING_HELPERS_H
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

int strcount(const char *haystack, const char needle);
char *strlast(const char *input, const char sep);
int strsplitmap(const char *input, const char sep, int (*func)(const char *, unsigned long), unsigned long data);
char **strsplit(const char *input, const char sep, int *count);
unsigned long hash_path(const char *path, int len);
unsigned long get_inode(const char *path);

#endif
