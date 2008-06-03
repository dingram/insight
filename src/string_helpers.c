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
#include <string_helpers.h>

/**
 * Count the number of occurences of a character within a string.
 *
 * @param haystack The string to be searched.
 * @param needle   The character for which to count occurences.
 * @returns The number of times the character appears within the string.
 */
int strcount(const char *haystack, const char needle) {
  DEBUG("Function entry");
  char *tmp = strdup(haystack);
  int count=0;

  DEBUG("Counting instances of \"%c\" in \"%s\".", needle, haystack);
  while (tmp && *tmp) {
    if (*(tmp++)==needle)
      count++;
  }
  DEBUG("String has %d instances of %c", count, needle);
  return count;
}

/**
 * Get the last component of a string, as split by \a sep.
 *
 * @param input The input string.
 * @param sep   The character to split by.
 * @returns The last component of the string, as split by \sep, or the entire
 * input string if \a sep is not found.
 */
char *strlast(const char *input, const char sep) {
  char *dup = strdup(input);
  char *ret=rindex(dup, sep);
  if (ret) ret++;
  else ret=dup;
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
    return ENOMEM;
  }
  sepstr[0]=sep;

  while (1) {
    char *tmptok = NULL;
    tmptok = strtok(i++?NULL:dup, sepstr);
    if (!tmptok) {
      DEBUG("Done");
      break;
    }
    DEBUG("Dealing with token[%d] \"%s\"", i, tmptok);
    ret = func(tmptok, data);
    DEBUG("User function returned %d", ret);
    if (ret) {
      if (ret==1) ret=0;
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


