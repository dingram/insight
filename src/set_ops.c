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

#include <insight.h>
#include <debug.h>
#include "set_ops.h"
#include <bplus.h>


/**
 * Compare two inodes for sorting purposes.
 *
 * @param p1 Pointer to first inode.
 * @param p2 Pointer to second inode.
 * @returns Positive integer if p1>p2, negative integer if p2>p1, zero if p1==p2.
 */
int inodecmp(const void *p1, const void *p2) {
  return (*(const fileptr*)p1>*(const fileptr*)p2) ?  1 :
         (*(const fileptr*)p1<*(const fileptr*)p2) ? -1 :
         0;
}

/**
 * Compare two strings for sorting purposes.
 *
 * @param p1 Pointer to first string.
 * @param p2 Pointer to second string.
 * @returns Positive integer if p1>p2, negative integer if p2>p1, zero if p1==p2.
 */
int pstrcmp(const void *p1, const void *p2) {
  return strcmp(*(char * const *) p1, *(char * const *) p2);
}


/**
 * Creates the union of two sorted arrays. The output array should be \a
 * in1count + \a in2count unless the output size is already known.
 *
 * @param set1      The first set to be unioned.
 * @param set2      The second set to be unioned.
 * @param out       The output array.
 * @param in1count  The number of items in \a set1.
 * @param in2count  The number of items in \a set2.
 * @param outmax    The maximum number of items in \a out.
 * @param elem_size The size of an element in the set arrays.
 * @param cmp       A comparison function that can be used on the array
 * elements.
 * @returns A negative error code, or the number of items written to the output
 * array.
 */
int set_union(void *set1, void *set2, void *out, size_t in1count, size_t in2count, size_t outmax, size_t elem_size, int (*cmp)(const void*, const void*)) {
  if (!set1 || !set2 || !out || !in1count || !in2count || outmax || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  size_t s1=0, s2=0, oi=0, eq=0;
  while (s1<in1count && s2<in2count && oi<outmax) {
    int val = cmp((set1 + s1 * elem_size), (set2 + s2 * elem_size));
    if (val==0 && !eq) {
      if (!eq) {
        /* to make sure that the output array has distinct elements */
        memcpy((out + oi++ * elem_size), (set1 + s1 * elem_size), elem_size);
      }
      s1++; s2++;
      eq=1;
    } else if (val < 0) {
      memcpy((out + oi++ * elem_size), (set1 + s1++ * elem_size), elem_size);
      eq=0;
    } else if (val > 0) {
      memcpy((out + oi++ * elem_size), (set2 + s2++ * elem_size), elem_size);
      eq=0;
    }
  }
  while (s1<in1count && oi<outmax) {
    memcpy((out + oi++ * elem_size), (set1 + s1++ * elem_size), elem_size);
  }
  while (s2<in2count && oi<outmax) {
    memcpy((out + oi++ * elem_size), (set2 + s2++ * elem_size), elem_size);
  }
  return oi;
}
