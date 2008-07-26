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

#undef get32bits
/*
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
  */
#define get32bits(d) (*((uint32_t *)(d)))
/*#endif*/

#undef get16bits
/*
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
  */
#define get16bits(d) (*((uint16_t *)(d)))
/*#endif*/

#undef get8bits
#define get8bits(d,o) (((uint8_t *)(d))[o])

#define SHORTSWAP(x, y) do { (x) ^= (y); (y) ^= (x); (x) ^= (y); } while (0)


/**
 * Swap two items using fast 32-bit XOR operations rather than memcpy().
 */
inline void _doSwap(void *a, void *b, size_t elem_size) {
  int rem;
  void *ai=a, *bi=b;
  rem = elem_size & 3;
  elem_size >>= 2;

  for (;elem_size > 0; elem_size--) {
    SHORTSWAP(get32bits(ai), get32bits(bi));
    ai+=sizeof(uint32_t);
    bi+=sizeof(uint32_t);
  }
  switch (rem) {
    case 3: SHORTSWAP(get16bits(ai),   get16bits(bi)  );
            SHORTSWAP(get8bits(ai, 2), get8bits(bi, 2));
            break;
    case 2: SHORTSWAP(get16bits(ai),   get16bits(bi)  );
            break;
    case 1: SHORTSWAP(get8bits(ai, 0), get8bits(bi, 0));
  }
}


/**
 * Creates the union of two sorted inode arrays. The output array should be \a
 * in1count + \a in2count unless the output size is already known. Optimised
 * for dealing with inodes.
 *
 * @param set1      The first set to be unioned.
 * @param set2      The second set to be unioned.
 * @param out       The output array.
 * @param in1count  The number of items in \a set1.
 * @param in2count  The number of items in \a set2.
 * @param outmax    The maximum number of items in \a out.
 * @returns A negative error code, or the number of items written to the output
 * array.
 */
int set_union_inode(fileptr *set1, fileptr *set2, fileptr *out, size_t in1count, size_t in2count, size_t outmax) {
  DEBUG("set_union_inode(set1: %p, set2: %p, out: %p, in1count: %lu, in2count: %lu, outmax: %lu)", set1, set2, out, in1count, in2count, outmax, elem_size, cmp);
  if (!set1 || !set2 || !out || (in1count && in2count && !outmax)) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  if (!in1count && !in2count) {
    DEBUG("No elements in both input arrays means no union possible");
    return 0;
  }

  size_t s1=0, s2=0, oi=0, eq=0;
  fileptr oldval=0;
  while (s1<in1count && s2<in2count && oi<outmax) {
    if (set1[s1] == set2[s2]) {
      DEBUG("Equal");
      if (!eq || set1[s1] != oldval) {
        DEBUG("Copying set1[%u] to output[%u]", s1, oi);
        /* make sure that the output array has distinct elements */
        out[oi++] = set1[s1];
        /* keep the old value so we can tell if there are repeated elements */
        oldval = set2[s2];
      }
      s1++; s2++;
      eq=1;
    } else if (set1[s1] < set2[s2]) {
      DEBUG("Copying set1[%u] to output[%u]", s1, oi);
      oldval = set1[s1];
      out[oi++] = set1[s1++];
      eq=0;
    } else if (set1[s1] > set2[s2]) {
      DEBUG("Copying set2[%u] to output[%u]", s2, oi);
      oldval = set2[s2];
      out[oi++] = set2[s2++];
      eq=0;
    }
  }
  DEBUG("Copying from set1?");
  while (s1<in1count && oi<outmax) {
    DEBUG("Copying set1[%u] to output[%u]", s1, oi);
    out[oi++] = set1[s1++];
  }
  DEBUG("Copying from set2?");
  while (s2<in2count && oi<outmax) {
    DEBUG("Copying set2[%u] to output[%u]", s2, oi);
    out[oi++] = set2[s2++];
  }
  return oi;
}

/**
 * Creates the union of two sorted string arrays. The output array should be \a
 * in1count + \a in2count unless the output size is already known. Just calls
 * set_union() internally.
 *
 * @param set1      The first set to be unioned.
 * @param set2      The second set to be unioned.
 * @param out       The output array.
 * @param in1count  The number of items in \a set1.
 * @param in2count  The number of items in \a set2.
 * @param outmax    The maximum number of items in \a out.
 * @returns A negative error code, or the number of items written to the output
 * array.
 */
int set_union_string(char **set1, char **set2, char **out, size_t in1count, size_t in2count, size_t outmax) {
  DEBUG("set_union_string(set1: %p, set2: %p, out: %p, in1count: %lu, in2count: %lu, outmax: %lu)", set1, set2, out, in1count, in2count, outmax, elem_size, cmp);
  return set_union(set1, set2, out, in1count, in2count, outmax, sizeof(char*), pstrcmp);
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
 * Creates the intersection of two sorted inode arrays. The output array is
 * guaranteed never to be larger than MIN(\a in1count, \a in2count). Optimised
 * for inode arrays.
 *
 * @param set1      The first set to be used.
 * @param set2      The second set to be used.
 * @param out       The output array.
 * @param in1count  The number of items in \a set1.
 * @param in2count  The number of items in \a set2.
 * @param outmax    The maximum number of items in \a out.
 * @returns A negative error code, or the number of items written to the output
 * array.
 */
int set_intersect_inode(fileptr *set1, fileptr *set2, fileptr *out, size_t in1count, size_t in2count, size_t outmax) {
  if (!set1 || !set2 || !out || !outmax) {
    DEBUG("At least one argument was null or zero");
    return -EINVAL;
  }

  if (!in1count || !in2count) {
    DEBUG("No elements in an input array means no intersection possible");
    return 0;
  }

  size_t s1=0, s2=0, oi=0, eq=0;
  fileptr oldval=0;
  while (s1<in1count && s2<in2count && oi<outmax) {
    if (set1[s1] == set2[s2]) {
      DEBUG("Equal");
      if (!eq || set1[s1] != oldval) {
        /* make sure that the output array has distinct elements */
        out[oi++] = set1[s1];
        /* keep the old value so we can tell if there are repeated elements */
        oldval = set2[s2];
      }
      s1++; s2++;
      eq=1;
    } else if (set1[s1] < set2[s2]) {
      oldval = set1[s1];
      eq=0;
    } else if (set1[s1] > set2[s2]) {
      oldval = set2[s2];
      eq=0;
    }
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
          _doSwap(p, q, elem_size);
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
  p=set;
  q=set+elem_size;
  max=set+(count*elem_size);
  while (p<max && q<max) {
    while (p<max && q<max && cmp(p, q)!=0) {
      p+=elem_size;
      if (q<max) {
        if (p<q) {
          _doSwap(p, q, elem_size);
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
  if (count > (p-set)/elem_size)
    count = 1 + (p-set)/elem_size;
  return count;
}
