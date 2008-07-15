#ifndef __SET_OPS_H
#define __SET_OPS_H
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

int inodecmp(const void *p1, const void *p2);
int pstrcmp(const void *p1, const void *p2);
int set_union(void *set1, void *set2, void *out, size_t in1count, size_t in2count, size_t outmax, size_t elem_size, int (*cmp)(const void*, const void*));
int set_intersect(void *set1, void *set2, void *out, size_t in1count, size_t in2count, size_t outmax, size_t elem_size, int (*cmp)(const void*, const void*));
int set_diff(void *set1, const void const * const set2, size_t in1count, size_t in2count, size_t elem_size, int (*cmp)(const void*, const void*));
int set_uniq(void *set, size_t count, size_t elem_size, int (*cmp)(const void*, const void*));

#endif
