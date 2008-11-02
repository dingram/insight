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

#include <stdint.h>
#include <insight.h>
#if defined(_DEBUG_STRING) && !defined(_DEBUG)
#define _DEBUG
#endif
#include <debug.h>
#include <string_helpers.h>

/**
 * Count the number of occurences of a character within a string.
 *
 * @param haystack The string to be searched.
 * @param needle   The character for which to count occurences.
 * @returns The number of times the character appears within the string.
 */
int strcount(const char *haystack, const char needle) {
  char *tmp = (char *)haystack;
  int count=0;

  DEBUG("Counting instances of \"%c\" in \"%s\".", needle, haystack);
  while (tmp && *tmp) {
    if (*(tmp++)==needle)
      count++;
  }
  DEBUG("String has %d instances of %c", count, needle);
  tmp=NULL;
  return count;
}

/**
 * Get the last component of a string, as split by \a sep.
 *
 * @param input The input string.
 * @param sep   The character to split by.
 * @returns The last component of the string, as split by \a sep, or the entire
 * input string if \a sep is not found.
 */
char *strlast(const char *input, const char sep) {
  char *ret=rindex(input, sep);
  if (ret) {
    ret=strdup(ret+1);
  } else {
    ret=strdup(input);
  }
  return ret;
}

/**
 * Split a string into multiple substrings based on one character, and call a
 * user-defined function on each substring.
 *
 * The user-defined function should return zero, although it may return 1 to
 * cause the loop to immediately terminate successfully or any negative value
 * to indicate an internal error, terminating the loop and exiting
 * strsplitmap() with that value as its return value.
 *
 * @param[in] input The string to be split.
 * @param[in] sep   The character which the string should be used to split \a input.
 * @param[in] func  The function to be called.
 * @param[in] data  Data passed to the user-defined function on each invocation.
 * @returns Zero on success, non-zero on failure.
 */
int strsplitmap(const char *input, const char sep, int (*func)(const char *, unsigned long), unsigned long data) {

  DEBUG("Splitting \"%s\" by \"%c\" with user function call", input, sep);

  int i=0, ret=0;
  char *dup = strdup(input);
  char *sepstr=calloc(2, sizeof(char));
  if (!sepstr) {
    PMSG(LOG_ERR, "Could not allocate space for sepstr");
    ifree(dup);
    return ENOMEM;
  }
  sepstr[0]=sep;

  if (!strcount(input, sep)) {
    DEBUG("Dealing with entire string (\"%s\")", input);
    ret = func(input, data);
    DEBUG("User function returned %d", ret);
    if (ret>0) ret=0;
    ifree(sepstr);
    ifree(dup);
    return ret;
  }

  char *saveptr;

  while (1) {
    char *tmptok;
    tmptok = strtok_r(i++?NULL:dup, sepstr, &saveptr);
    if (!tmptok || !*tmptok) {
      DEBUG("Done");
      break;
    }
    DEBUG("Dealing with token[%d] \"%s\"", i, tmptok);
    ret = func(tmptok, data);
    DEBUG("User function returned %d", ret);
    if (ret) {
      if (ret>0) ret=0;
      break;
    }
  }
  ifree(sepstr);

  DEBUG("Freeing dup");
  ifree(dup);

  return ret;
}


/**
 * Split a string into multiple substrings based on one character.
 *
 * @param[in]  input The string to be split.
 * @param[in]  sep   The character which the string should be used to split \a input.
 * @param[out] count The number of items in the array.
 * @returns A pointer to the array, or NULL if an error occured. May be freed using free().
 */
char **strsplit(const char *input, const char sep, int *count) {

  DEBUG("Splitting \"%s\" by \"%c\"", input, sep);

  *count=strcount(input, sep)+1;
  char *tmp = strdup(input);
  char **bits = calloc(*count, sizeof(char*));
  int i;
  if (!bits) {
    PMSG(LOG_ERR, "Failed to allocate \"bits\" array.");
    *count = 0;
    return NULL;
  }

  char *sepstr=calloc(2, sizeof(char));
  sepstr[0]=sep;

  for (i=0; i<*count; i++) {
    DEBUG("Dealing with bits[%d]", i);
    char *tmptok = NULL;
    tmptok = strtok(i?NULL:tmp, sepstr);
    if (tmptok) {
      bits[i] = strdup(tmptok);
    } else {
      bits[i] = calloc(1, sizeof(char));
    }
    DUMPSTR(bits[i]);
  }
  ifree(sepstr);

  DEBUG("Freeing tmp");
  ifree(tmp);

  return bits;
}

/**
 * Very efficient conversion of a value to its eight-digit uppercase
 * hexadecimal representation.
 *
 * @param[out] buf A buffer of at least 9 characters to be filled with the hex
 *                 representation of \a val.
 * @param[in]  val The number to convert to hex.
 */
inline void hex_to_string(char *buf, unsigned long val) {
  int i;
  buf[8]='\0';
  for (i=0;i<8;i++) {
    buf[i]='0'+((val>>4*(7-i))&0x0f);
    if (buf[i]>'9') buf[i]+=('A'-':');
  }
}


/* ************************************************************************ */
/*   This code copied from reiserfs due to time constraints. Algorithm is   */
/*   a keyed 32-bit hash function using TEA in a Davis-Meyer function, as   */
/*   taken from Applied Cryptography, 2nd edition, p448. Implementation     */
/*   by Jeremy Fitzhardinge <jeremy@zip.com.au> 1998                        */
/* ************************************************************************ */

#define DELTA 0x9E3779B9
#define FULLROUNDS 10		/* 32 is overkill, 16 is strong crypto */
#define PARTROUNDS 6		/* 6 gets complete mixing */

/* a, b, c, d - data; h0, h1 - accumulated hash */
#define TEACORE(rounds)							\
	do {								\
		unsigned long sum = 0;						\
		int n = rounds;						\
		unsigned long b0, b1;						\
									\
		b0 = h0;						\
		b1 = h1;						\
									\
		do							\
		{							\
			sum += DELTA;					\
			b0 += ((b1 << 4)+a) ^ (b1+sum) ^ ((b1 >> 5)+b);	\
			b1 += ((b0 << 4)+c) ^ (b0+sum) ^ ((b0 >> 5)+d);	\
		} while(--n);						\
									\
		h0 += b0;						\
		h1 += b1;						\
	} while(0)


/**
 * Old path hashing algorithm, based on one from reiserfs. This leads to quite
 * a few collisions though.
 *
 * @param path The path to be hashed.
 * @return The 32-bit hash of the path.
 * @author Jeremy Fitzhardinge <jeremy@zip.com.au>
 */
unsigned long hash_path_old(char const *path) {

	unsigned long k[] = { 0x9464a485, 0x542e1a94, 0x3e846bff, 0xb75bcfc3 };

	unsigned long h0 = k[0], h1 = k[1];
	unsigned long a, b, c, d;
	unsigned long pad;
	int i;

  int len = strlen(path);

	pad = (unsigned long) len | ((unsigned long) len << 8);
	pad |= pad << 16;

	while (len >= 16) {
		a = (unsigned long) path[0] |
		    (unsigned long) path[1] << 8 | (unsigned long) path[2] << 16 | (unsigned long) path[3] << 24;
		b = (unsigned long) path[4] |
		    (unsigned long) path[5] << 8 | (unsigned long) path[6] << 16 | (unsigned long) path[7] << 24;
		c = (unsigned long) path[8] |
		    (unsigned long) path[9] << 8 |
		    (unsigned long) path[10] << 16 | (unsigned long) path[11] << 24;
		d = (unsigned long) path[12] |
		    (unsigned long) path[13] << 8 |
		    (unsigned long) path[14] << 16 | (unsigned long) path[15] << 24;

		TEACORE(PARTROUNDS);

		len -= 16;
		path += 16;
	}

	if (len >= 12) {
		a = (unsigned long) path[0] |
		    (unsigned long) path[1] << 8 | (unsigned long) path[2] << 16 | (unsigned long) path[3] << 24;
		b = (unsigned long) path[4] |
		    (unsigned long) path[5] << 8 | (unsigned long) path[6] << 16 | (unsigned long) path[7] << 24;
		c = (unsigned long) path[8] |
		    (unsigned long) path[9] << 8 |
		    (unsigned long) path[10] << 16 | (unsigned long) path[11] << 24;

		d = pad;
		for (i = 12; i < len; i++) {
			d <<= 8;
			d |= path[i];
		}
	} else if (len >= 8) {
		a = (unsigned long) path[0] |
		    (unsigned long) path[1] << 8 | (unsigned long) path[2] << 16 | (unsigned long) path[3] << 24;
		b = (unsigned long) path[4] |
		    (unsigned long) path[5] << 8 | (unsigned long) path[6] << 16 | (unsigned long) path[7] << 24;

		c = d = pad;
		for (i = 8; i < len; i++) {
			c <<= 8;
			c |= path[i];
		}
	} else if (len >= 4) {
		a = (unsigned long) path[0] |
		    (unsigned long) path[1] << 8 | (unsigned long) path[2] << 16 | (unsigned long) path[3] << 24;

		b = c = d = pad;
		for (i = 4; i < len; i++) {
			b <<= 8;
			b |= path[i];
		}
	} else {
		a = b = c = d = pad;
		for (i = 0; i < len; i++) {
			a <<= 8;
			a |= path[i];
		}
	}

	TEACORE(FULLROUNDS);

	return h0 ^ h1;
}

#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

/**
 * New and fast path hashing algorithm, based on an algorithm by ???.
 *
 * @param path The path to be hashed.
 * @return The 32-bit hash of the path.
 * @author ???
 */
unsigned long hash_path(char const * path) {
  int len = strlen(path);

  uint32_t hash = len, tmp;
  int rem;
  if (len <= 0 || path == NULL) {
    return 0;
  }
  rem = len & 3;
  len >>= 2;
  for (;len > 0; len--) {
    hash += get16bits (path);
    tmp   = (get16bits (path+2) << 11) ^ hash;
    hash  = (hash << 16) ^ tmp;
    path += 2*sizeof (uint16_t);
    hash += hash >> 11;
  }
  switch (rem) {
    case 3: hash += get16bits (path);
            hash ^= hash << 16;
            hash ^= path[sizeof (uint16_t)] << 18;
            hash += hash >> 11;
            break;
    case 2: hash += get16bits (path);
            hash ^= hash << 11;
            hash += hash >> 17;
            break;
    case 1: hash += *path;
            hash ^= hash << 10;
            hash += hash >> 1;
  }
  hash ^= hash << 3;
  hash += hash >> 5;
  hash ^= hash << 4;
  hash += hash >> 17;
  hash ^= hash << 25;
  hash += hash >> 6;

  return hash;
}
