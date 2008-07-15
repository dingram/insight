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
  //DEBUG("pstrcmp(%p, %p)", p1, p2);
  //DEBUG("pstrcmp(\"%s\", \"%s\")", *(char * const *) p1, *(char * const *) p2);
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
  DEBUG("set_union(set1: %p, set2: %p, out: %p, in1count: %lu, in2count: %lu, outmax: %lu, elem_size: %lu, cmp: %p)", set1, set2, out, in1count, in2count, outmax, elem_size, cmp);
  if (!set1 || !set2 || !out || (in1count && in2count && !outmax) || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  if (!in1count && !in2count) {
    DEBUG("No elements in both input arrays means no union possible");
    return 0;
  }

  size_t s1=0, s2=0, oi=0, eq=0, lp=outmax;
  void *oldval=NULL;
  while (s1<in1count && s2<in2count && oi<outmax) {
    int val = cmp((set1 + s1 * elem_size), (set2 + s2 * elem_size));
    if (val==0) {
      DEBUG("Equal");
      if (!eq || cmp((set1 + s1 * elem_size), oldval)!=0) {
        DEBUG("Copying set1[%u] to output[%u]", s1, oi);
        /* make sure that the output array has distinct elements */
        memcpy((out + oi++ * elem_size), (set1 + s1 * elem_size), elem_size);
        /* keep the old value so we can tell if there are repeated elements */
        oldval = (set2 + s2 * elem_size);
        DEBUG("Copying set2[%u] to end of output[%u]", s2, lp-1);
        /* copy the other value to the end of oi */
        memcpy((out + --lp * elem_size), (set2 + s2 * elem_size), elem_size);
      } else {
        DEBUG("Copying set1[%u] to end of output[%u]", s1, lp-1);
        memcpy((out + --lp * elem_size), (set1 + s1 * elem_size), elem_size);
        DEBUG("Copying set2[%u] to end of output[%u]", s2, lp-1);
        memcpy((out + --lp * elem_size), (set2 + s2 * elem_size), elem_size);
      }
      s1++; s2++;
      eq=1;
    } else if (val < 0) {
      DEBUG("Copying set1[%u] to output[%u]", s1, oi);
      memcpy((out + oi++ * elem_size), (set1 + s1++ * elem_size), elem_size);
      oldval = (set1 + s1 * elem_size);
      eq=0;
    } else if (val > 0) {
      DEBUG("Copying set2[%u] to output[%u]", s2, oi);
      memcpy((out + oi++ * elem_size), (set2 + s2++ * elem_size), elem_size);
      oldval = (set2 + s2 * elem_size);
      eq=0;
    }
  }
  DEBUG("Copying from set1?");
  while (s1<in1count && oi<outmax) {
    DEBUG("Copying set1[%u] to output[%u]", s1, oi);
    memcpy((out + oi++ * elem_size), (set1 + s1++ * elem_size), elem_size);
  }
  DEBUG("Copying from set2?");
  while (s2<in2count && oi<outmax) {
    DEBUG("Copying set2[%u] to output[%u]", s2, oi);
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
 * set1. The output is left in \a set1, with the return value indicating how
 * many items there are after the difference. The discarded items then fill the
 * rest of \a set1, to allow them to be freed.
 *
 * @param set1      The first set to be used.
 * @param set2      The second set to be used (subtracted from the first).
 * @param in1count  The number of items in \a set1.
 * @param in2count  The number of items in \a set2.
 * @param elem_size The size of an element in the set arrays.
 * @param cmp       A comparison function that can be used on the array
 * elements.
 * @returns A negative error code, or the number of items that are in the
 * ordered partition of the array.
 */
int set_diff(void *set1, const void const * const set2, size_t in1count, size_t in2count, size_t elem_size, int (*cmp)(const void*, const void*)) {
  DEBUG("set_diff(set1: %p, set2: %p, in1count: %lu, in2count: %lu, elem_size: %lu, cmp: %p)", set1, set2, in1count, in2count, elem_size, cmp);
  if (!set1 || !set2 || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  if (!in1count || !in2count) {
    DEBUG("No elements in an input array means no difference possible");
    return in1count;
  }

  void *tmp=calloc(1, elem_size); /* could eliminate this with careful XORing, but is that much optimisation really necessary? */
  if (!tmp) return -ENOMEM;
  void *p=set1-elem_size;
  void *q=set1;
  void *max=set1+(in1count*elem_size);
  void const * ri=(void const *)set2;
  void const * const rmax=(void const * const)set2+(in2count*elem_size);
  int c=0;
  while (p<max && q<max) {
    if (ri<rmax && q<max) {
      c = cmp(q, ri);
    } else {
      c = -1;
    }
    while (p<max && c<0) {
      DEBUG("Skipping item \"%s\" not in diff list", *(char * const *) q);
      p+=elem_size;
      if (q<max) {
        if (p<q) {
          DEBUG("Swapping");
          /* TODO: better swap function? */
          memcpy(tmp, p, elem_size);
          memcpy(p, q, elem_size);
          memcpy(q, tmp, elem_size);
        }
        q+=elem_size;
      }
      else if (in1count > (p-set1)/elem_size)
        in1count = (p-set1)/elem_size;
      if (ri<rmax && q<max)
        c = cmp(q, ri);
    }
    while (q<max && c==0) {
      DEBUG("Skipping item \"%s\" in diff list", *(char * const *) q);
      q+=elem_size;
      if (q<max)
        c = cmp(q, ri);
    }
    while (ri<rmax && c>0) {
      DEBUG("Advancing diff list");
      ri+=elem_size;
      if (q<max && ri<rmax)
        c = cmp(q, ri);
    }
  }
  if (in1count > (p-set1)/elem_size)
    in1count = 1 + (p-set1)/elem_size;
  else if (ri<rmax && c==0)
    in1count=0;
  ifree(tmp);
  return in1count;
}

/**
 * Ensures that an ordered set contains no duplicate items.  The output is left
 * in \a set, with the return value indicating how many unique items there
 * are. The discarded items then fill the rest of \a set, to allow them to be
 * freed.
 *
 * @param set       The set to be used.
 * @param count     The number of items in \a set.
 * @param elem_size The size of an element in the set arrays.
 * @param cmp       A comparison function that can be used on the array
 * elements.
 * @returns A negative error code, or the number of items written to the output
 * array.
 */
int set_uniq(void *set, size_t count, size_t elem_size, int (*cmp)(const void*, const void*)) {
  if (!set || !elem_size || !cmp) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  if (!count) {
    DEBUG("No elements in input array means it's trivially unique");
    return 0;
  }

  void *p, *q, *max;
  void *tmp=calloc(1, elem_size); /* could eliminate this with careful XORing, but is that much optimisation really necessary? */
  if (!tmp) return -ENOMEM;
  p=set;
  q=set+elem_size;
  max=set+(count*elem_size);
  while (p<max && q<max) {
    while (p<max && q<max && cmp(p, q)!=0) {
      p+=elem_size;
      if (q<max) {
        if (p<q) {
          /* TODO: better swap function? */
          memcpy(tmp, p, elem_size);
          memcpy(p, q, elem_size);
          memcpy(q, tmp, elem_size);
        }
        q+=elem_size;
      }
      else if (count > (p-set)/elem_size) {
        count = (p-set)/elem_size;
      }
    }
    while (q<max && cmp(p, q)==0) {
      q+=elem_size;
    }
  }
  DUMPUINT(count);
  DUMPUINT(p-set);
  if (count > (p-set)/elem_size)
    count = 1 + (p-set)/elem_size;
  ifree(tmp);
  return count;
}
