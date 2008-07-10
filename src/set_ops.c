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
#if defined(_DEBUG_SETOPS) && !defined(_DEBUG)
#define _DEBUG
#endif
#include <debug.h>
#include <set_ops.h>
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
  DEBUG("Function entry");
  DEBUG("set_union(set1: %p, set2: %p, out: %p, in1count: %lu, in2count: %lu, outmax: %lu, elem_size: %lu, cmp: %p)", set1, set2, out, in1count, in2count, outmax, elem_size, cmp);
  if (!set1 || !set2 || !out || (in1count && in2count && !outmax) || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  DEBUG("Checking in1[%lu] and in2[%lu]", in1count, in2count);
  if (!in1count && !in2count) {
    DEBUG("No elements in both input arrays means no union possible");
    return 0;
  }

  size_t s1=0, s2=0, oi=0, eq=0;
  void *oldval=NULL;
  DEBUG("While condition");
  while (s1<in1count && s2<in2count && oi<outmax) {
    DEBUG("Inside while loop");
    int val = cmp((set1 + s1 * elem_size), (set2 + s2 * elem_size));
    if (val==0) {
      DEBUG("Equal");
      if (!eq || cmp((set1 + s1 * elem_size), oldval)!=0) {
        /* make sure that the output array has distinct elements */
        memcpy((out + oi++ * elem_size), (set1 + s1 * elem_size), elem_size);
        /* keep the old value so we can tell if there are repeated elements */
        oldval = (set2 + s2 * elem_size);
      }
      s1++; s2++;
      eq=1;
    } else if (val < 0) {
      DEBUG("Copy from set1");
      memcpy((out + oi++ * elem_size), (set1 + s1++ * elem_size), elem_size);
      oldval = (set1 + s1 * elem_size);
      eq=0;
    } else if (val > 0) {
      DEBUG("Copy from set2");
      memcpy((out + oi++ * elem_size), (set2 + s2++ * elem_size), elem_size);
      oldval = (set2 + s2 * elem_size);
      eq=0;
    }
  }
  DEBUG("Copying from set1?");
  while (s1<in1count && oi<outmax) {
    DEBUG("Copying elem %lu from set1", s1);
    memcpy((out + oi++ * elem_size), (set1 + s1++ * elem_size), elem_size);
  }
  DEBUG("Copying from set2?");
  while (s2<in2count && oi<outmax) {
    DEBUG("Copying elem %lu from set2", s2);
    memcpy((out + oi++ * elem_size), (set2 + s2++ * elem_size), elem_size);
  }
  return oi;
}

/**
 * Creates the intersection of two sorted arrays. The output array is
 * guaranteed never to be larger than MIN(\a in1count, \a in2count).
 *
 * @param set1      The first set to be used.
 * @param set2      The second set to be used.
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
int set_intersect(void *set1, void *set2, void *out, size_t in1count, size_t in2count, size_t outmax, size_t elem_size, int (*cmp)(const void*, const void*)) {
  if (!set1 || !set2 || !out || !outmax || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  if (!in1count || !in2count) {
    DEBUG("No elements in an input array means no intersection possible");
    return 0;
  }

  size_t s1=0, s2=0, oi=0, eq=0;
  void *oldval=NULL;
  while (s1<in1count && s2<in2count && oi<outmax) {
    int val = cmp((set1 + s1 * elem_size), (set2 + s2 * elem_size));
    if (val==0) {
      DEBUG("Equal");
      if (!eq || cmp((set1 + s1 * elem_size), oldval)!=0) {
        /* make sure that the output array has distinct elements */
        memcpy((out + oi++ * elem_size), (set1 + s1 * elem_size), elem_size);
        /* keep the old value so we can tell if there are repeated elements */
        oldval = (set2 + s2 * elem_size);
      }
      s1++; s2++;
      eq=1;
    } else if (val < 0) {
      oldval = (set1 + s1++ * elem_size);
      eq=0;
    } else if (val > 0) {
      oldval = (set2 + s2++ * elem_size);
      eq=0;
    }
  }
  return oi;
}

/**
 * Creates the difference of two sorted arrays by removing \a set2 from \a
 * set1. The output array is guaranteed never to be larger than \a in1count.
 *
 * @param set1      The first set to be used.
 * @param set2      The second set to be used (subtracted from the first).
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
int set_diff(void *set1, void *set2, void *out, size_t in1count, size_t in2count, size_t outmax, size_t elem_size, int (*cmp)(const void*, const void*)) {
  if (!set1 || !set2 || !out || !outmax || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  /* TODO: this is wrong. Empty set1 returns 0; empty set2 returns set1 */
  if (!in1count || !in2count) {
    DEBUG("No elements in an input array means no difference possible");
    return 0;
  }

  size_t s1, oi=0;
  for (s1=0; s1<in1count; s1++) {
    size_t s2;
    int eq;
    eq=0;
    /* check against each element in the "remove" list */
    for (s2=0; s2<in2count; s2++) {
      if (cmp((set1 + s1 * elem_size), (set2 + s2 * elem_size))==0) {
        eq=1;
        break;
      }
    }
    if (!eq) {
      memcpy((out + oi++ * elem_size), (set1 + s1 * elem_size), elem_size);
    }
  }

  return oi;
}

/**
 * Ensures that a set is ordered and contains no duplicate items.
 *
 * @param set       The set to be used.
 * @param out       The output set.
 * @param in_count  The number of items in \a set.
 * @param outmax    The maximum number of items in \a out.
 * @param elem_size The size of an element in the set arrays.
 * @param cmp       A comparison function that can be used on the array
 * elements.
 * @returns A negative error code, or the number of items written to the output
 * array.
 * @todo This can be made much more efficient.
 */
int set_uniq(void *set, void *out, size_t in_count, size_t outmax, size_t elem_size, int (*cmp)(const void*, const void*)) {
  if (!set || !out || !outmax || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  if (!in_count) {
    DEBUG("No elements in input array means it's trivially unique");
    return 0;
  }

  void *tmp = malloc(in_count*elem_size);
  if (!tmp) {
    PMSG(LOG_ERR, "Failed to allocate memory for temporary array");
    return -ENOMEM;
  }
  DEBUG("Copying to temporary array");
  memcpy(tmp, set, in_count*elem_size);

  DEBUG("Sorting temporary array");
  qsort(tmp, in_count, elem_size, cmp);

  size_t s1=0, oi=0;
  void *last=NULL;
  memcpy((out + oi++ * elem_size), (tmp + s1 * elem_size), elem_size);
  last = (tmp + s1 * elem_size);
  /* copy only unique values */
  for (s1=1; s1<in_count; s1++) {
    if (cmp((tmp + s1 * elem_size), last)!=0) {
      memcpy((out + oi++ * elem_size), (tmp + s1 * elem_size), elem_size);
      last = (tmp + s1 * elem_size);
    }
  }

  return oi;
}
