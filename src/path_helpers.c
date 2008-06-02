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
/*
#ifndef _DEBUG
#define _DEBUG
#endif
*/
#include <debug.h>
#include <path_helpers.h>

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
    DEBUG("Failed to allocate \"bits\" array.");
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
 * Create canonical path from the argument. A canonical path contains no
 * <strong>incomplete keys</strong>. An <strong>incomplete key</strong> is
 * defined to be a subsequence such as:
 *
 *  - <tt>/tag:/subtag/</tt>
 *  - <tt>/tag/:/subtag/</tt>
 *
 * Each of these is converted to the canonical form <tt>/tag.subtag/</tt>. This
 * function also removes any leading or trailing slashes.
 *
 * @param path The path to be made canonical.
 * @returns The canonical version of \a path.
 */
char *get_canonical_path(const char *path) {
  errno=0;

  DEBUG("Path to canonicalise: %s", path);

  /* if there are no subkey indicators or it's empty, then it is trivially canonical. */
  if (!path || !strlen(path) || !strstr(path, INSIGHT_SUBKEY_IND)) {
    DEBUG("Path already canonical");
    return strdup(path);
  }

  int slashcount;
  char **bits = strsplit((char*)(path+1), '/', &slashcount);

  char *tmp=calloc(strlen(path), sizeof(char));

  int i, prevsub=0;
  for (i=0; i<slashcount; i++) {
    DEBUG("Examining bits[%d] = \"%s\"", i, bits[i]);
    if (!prevsub) {
      if (last_char_in(bits[i])==INSIGHT_SUBKEY_IND_C) {
        DEBUG("Ends in a colon");
        /* not just a colon */
        if (strlen(bits[i])>1) {
          /* remove colon unless we're incomplete (i.e. last element) */
          if (i+1<slashcount) last_char_in(bits[i])='\0';
          strcat(tmp, "/");
          strcat(tmp, bits[i]);
        } else if (i+1==slashcount) {
          /* add colon if we're incomplete (i.e. last element) */
          strcat(tmp, INSIGHT_SUBKEY_IND);
        }
        prevsub=1;
      } else {
        DEBUG("Regular tag");
        strcat(tmp, "/");
        strcat(tmp, bits[i]);
      }
    } else {
      DEBUG("Subtag part");
      if (bits[i][0]==INSIGHT_SUBKEY_IND_C && strlen(bits[i])==1) {
        DEBUG("Looks like we have two %ss in a row.", INSIGHT_SUBKEY_IND);
        errno=ENOENT;
        prevsub=1;
      } else if (last_char_in(bits[i])==INSIGHT_SUBKEY_IND_C) {
        DEBUG("Next is still a subtag part");
        if (i+1<slashcount) last_char_in(bits[i])='\0';
        prevsub=1;
        strcat(tmp, INSIGHT_SUBKEY_SEP);
        strcat(tmp, bits[i]);
      } else {
        prevsub=0;
        strcat(tmp, INSIGHT_SUBKEY_SEP);
        strcat(tmp, bits[i]);
      }
    }
  }

  DEBUG("Freeing bits");
  //ifree(bits);
  DEBUG("Bits free");

  if (!strlen(tmp)) {
    DEBUG("Not allowing empty canonical path; using root");
    strcpy(tmp, "/");
  }
  DEBUG("Passed canonical path");

  DEBUG("Final canonical path: %s", tmp);
  DEBUG("Preparing to return");
  return tmp;
}

/**
 * Retrieve data block associated with tag. May require splitting it into
 * subtags. Will ignore any trailing INSIGHT_SUBKEY_IND character.
 *
 * @param tagname The tag to find
 * @return A pointer to the data block associated with tag, if found, or zero
 * otherwise.
 */
fileptr get_tag(const char *tagname) {
  errno=0;

  DEBUG("Tag to find: %s", tagname);

  /* if it's empty, then nothing to do. */
  if (!tagname || !strlen(tagname)) {
    DEBUG("Null tag");
    return 0;
  }

  int sepcount, i;
  char **bits = strsplit(tagname, INSIGHT_SUBKEY_SEP_C, &sepcount);
  fileptr cur_root = tree_get_root();

  for (i=0; i<sepcount; i++) {
    if (last_char_in(bits[i])==':')
      last_char_in(bits[i])='\0';
    DEBUG("Looking up tag[%d]=\"%s\" from subnode %lu", i, bits[i], cur_root);
    cur_root = tree_sub_search(cur_root, bits[i]);
    if (!cur_root) {
      DEBUG("Could not find tag!");
      ifree(bits);
      return 0;
    }
  }

  DEBUG("Freeing bits");
  ifree(bits);

  return cur_root;
}

/**
 * Retrieve data block associated with last tag in given path.  Will ignore any
 * trailing INSIGHT_SUBKEY_IND character.
 *
 * @param path The path from which to extract the final tag
 * @return A pointer to the data block associated with the tag, if found, or
 * zero otherwise.
 */
fileptr get_last_tag(const char *path) {
  DEBUG("Path to get last tag from: %s", path);

  /* if it's empty, then nothing to do. */
  if (!path || !strlen(path)) {
    DEBUG("Null path");
    return 0;
  }

  char *tag = strlast(path, '/');
  return get_tag(tag);
}

/**
 * Validate a path by ensuring each element exists. This may require checking
 * for subtags. Any trailing INSIGHT_SUBKEY_IND characters are ignored.
 *
 * @param path The path to be validated.
 * @returns Non-zero if the path is valid, zero if the path is invalid.
 */
int validate_path(const char *path) {
  // TODO: split path by '/' and then check each tag, splitting the tag as required.
  int count;
  char **bits = strsplit(path, '/', &count);

  DEBUG("Freeing bits");
  for (count--;count>=0; count--) {
    ifree(bits[i]);
  }
  DEBUG("Freeing bits top-level");
  ifree(bits);
  DEBUG("Returning");
  return 1;
}
