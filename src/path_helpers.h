#ifndef __PATH_HELPERS_H
#define __PATH_HELPERS_H
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

#include <bplus.h>

int strcount(const char *haystack, const char needle);
char *strlast(const char *input, const char sep);
char **strsplit(const char *input, const char sep, int *count);
char *get_canonical_path(const char *path);
fileptr get_tag(const char *tagname);
fileptr get_last_tag(const char *tagname);
int validate_path(const char *path);
int checkdir(const char *path);
int check_mkdir(const char *path);
char *gen_repos_path(const char *hash, int create, const char *repos_base);
int have_file_by_name(const char *path, struct stat *stat, const char *repos_base);
int have_file_by_hash(const char *hash, struct stat *stat, const char *repos_base);
char *basename_from_inode(const fileptr inode, const char *repos_base);

#endif
