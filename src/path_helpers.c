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
#if defined(_DEBUG_PATH) && !defined(_DEBUG)
#define _DEBUG
#endif
#include <debug.h>
#include <string_helpers.h>
#include <path_helpers.h>

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
  char *dup=strdup(tagname);
  errno=0;

  while (last_char_in(dup)==INSIGHT_SUBKEY_IND_C) {
    DEBUG("Removing trailing %s character", INSIGHT_SUBKEY_IND);
    last_char_in(dup)='\0';
  }

  DEBUG("Tag to find: %s", tagname);

  /* if it's empty, then nothing to do. */
  if (!dup || !strlen(dup)) {
    DEBUG("Null tag");
    ifree(dup);
    return 0;
  }

  int sepcount, i;
  char **bits = strsplit(dup, INSIGHT_SUBKEY_SEP_C, &sepcount);
  fileptr cur_root = tree_get_root();

  for (i=0; i<sepcount; i++) {
    if (last_char_in(bits[i])==':')
      last_char_in(bits[i])='\0';
    DEBUG("Looking up tag[%d]=\"%s\" from subnode %lu", i, bits[i], cur_root);
    cur_root = tree_sub_search(cur_root, bits[i]);
    if (!cur_root) {
      DEBUG("Could not find tag!");
      break;
    }
  }

  DEBUG("Freeing dup");
  ifree(dup);

  DEBUG("Freeing bits");
  for (sepcount--;sepcount>=0; sepcount--)
    ifree(bits[sepcount]);
  DEBUG("Freeing bits top-level");
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
  if (!path || !*path) {
    DEBUG("Null path");
    return 0;
  }

  char *tag = strlast(path, '/');
  return get_tag(tag);
}

/**
 * Validate a path by ensuring each element is a valid tag. This may require
 * checking for subtags. Any trailing INSIGHT_SUBKEY_IND characters are
 * ignored.
 *
 * @param path The path to be validated.
 * @returns Non-zero if the path is valid, zero if the path is invalid.
 */
int validate_path(const char *path) {
  /* Empty and root paths are trivially valid */
  if (!*path || strcmp(path, "/")==0) return 1;

  int count, i, isvalid=1;
  char **bits = strsplit(path, '/', &count);

  for (i=0; i<count && isvalid; i++) {
    DEBUG("Examining tag \"%s\"", bits[i]);
    isvalid &= (!*bits[i] || get_tag(bits[i]))?1:0;
    DEBUG("%s", isvalid?"Valid":"Invalid!");
  }

  DEBUG("Freeing bits");
  for (count--;count>=0; count--) {
    ifree(bits[count]);
  }
  DEBUG("Freeing bits top-level");
  ifree(bits);
  DEBUG("Returning %d", isvalid);
  return isvalid;
}

int checkdir(const char *path) {
  struct stat dst;
  return !((lstat(path, &dst) == -1) && (errno == ENOENT));
}

int check_mkdir(const char *path) {
  struct stat dst;

  if ((lstat(path, &dst) == -1) && (errno == ENOENT)) {
    DEBUG("Creating directory %s", path);
    if (mkdir(path, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
      FMSG(LOG_ERR, "Directory %s does not exist and can't be created!\n\n", path);
      return 1;
    }
    return 0;
  } else if (!S_ISDIR(dst.st_mode)) {
    FMSG(LOG_ERR, "Default Insight directory %s exists but must be a directory!\n\n", path);
    return 2;
  }
  return 0;
}

char *gen_repos_path(const char *hash, int create, const char *repos_base) {
  char *finaldest;
  int i;

  finaldest = calloc(strlen(repos_base)+strlen("/01/23/45/01234567")+1, sizeof(char));
  if (!finaldest) {
    PMSG(LOG_ERR, "Failed to allocate memory");
    return NULL;
  }
  strcpy(finaldest, repos_base);
  /* finaldest guaranteed to exist so far */

  for (i=0; i<6; i+=2) {
    strcat(finaldest, "/");
    strncat(finaldest, hash+i, 2);
    DEBUG("Checking %s exists...", finaldest);
    if (!create && !checkdir(finaldest)) {
      ifree(finaldest);
      return NULL;
    } else if (create && check_mkdir(finaldest)) {
      ifree(finaldest);
      return NULL;
    }
  }

  strcat(finaldest, "/");
  strcat(finaldest, hash);

  return finaldest;
}

int have_file_by_hash(const char *hash, struct stat *fstat, const char *repos_base) {
  char *filepath = gen_repos_path(hash, 0, repos_base);
  if (!filepath) return 0;
  return !((stat(filepath, fstat) == -1) && (errno == ENOENT));
}

int have_file_by_name(const char *path, struct stat *fstat, const char *repos_base) {
  char hash[9];
  DEBUG("Input path: %s", path);
  snprintf(hash, 9, "%08lX", hash_path(path, strlen(path)));
  DEBUG("Path hash: %s", hash);
  return have_file_by_hash(hash, fstat, repos_base);
}

char *basename_from_inode(const fileptr inode, const char *repos_base) {
  char *linkres = fullname_from_inode(inode, repos_base);
  if (!linkres) {
    PMSG(LOG_ERR, "Failed to get fullname");
    return NULL;
  }

  char *answer = strlast(linkres, '/');

  DEBUG("Freeing linkres");
  ifree(linkres);

  return answer;
}

char *fullname_from_inode(const fileptr inode, const char *repos_base) {
  char hash[9];
  snprintf(hash, 9, "%08lX", inode);
  DEBUG("Input hash: %s", hash);
  char *filepath = gen_repos_path(hash, 0, repos_base);
  DEBUG("Repository path: %s", filepath);
  if (!filepath) {
    PMSG(LOG_ERR, "Inode %08lX has vanished from repository", inode);
    return NULL;
  }

  char *linkres = calloc(512, sizeof(char));
  if (!linkres) {
    PMSG(LOG_ERR, "Failed to allocate memory");
    return NULL;
  }

  ssize_t result = readlink(filepath, linkres, 512);
  if (result<0) {
    PMSG(LOG_ERR, "Failed to read link \"%s\": %s", filepath, strerror(errno));
    DEBUG("Freeing filepath");
    ifree(filepath);
    DEBUG("Freeing linkres");
    ifree(linkres);
    return NULL;
  }

  DEBUG("Freeing filepath");
  ifree(filepath);

  return linkres;
}
