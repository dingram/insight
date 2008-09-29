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
#include <set_ops.h>

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

  char *tmp=calloc(strlen(path)+1, sizeof(char));

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

  for (slashcount--;slashcount>=0; slashcount--) {
    ifree(bits[slashcount]);
  }
  ifree(bits);

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
  fileptr ret = get_tag(tag);
  ifree(tag);
  return ret;
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

/**
 * Generate a repository path, given a hash. If \a create is 1, then the
 * directories will be created as required. If \a create is 2, then the
 * existence of the directories will be checked. Otherwise, if \a create is 0,
 * the path will just be generated.
 */
char *gen_repos_path(const char *hash, int create) {
  char *finaldest = calloc(insight.repository_len+strlen("/01/23/45/01234567")+1, sizeof(char));
  if (!finaldest) {
    PMSG(LOG_ERR, "Failed to allocate memory");
    return NULL;
  }
  strcpy(finaldest, insight.repository);
  char *finalptr = finaldest + insight.repository_len;
  /* finaldest guaranteed to exist so far */
  int i;

  for (i=0; i<6; i+=2) {
    *finalptr++ = '/';
    *finalptr++ = hash[i];
    *finalptr++ = hash[i+1];
    if (create==0) continue;
    if (create==1 && check_mkdir(finaldest)) {
      DEBUG("Path %s could not be created", finaldest);
      ifree(finaldest);
      return NULL;
    } else if (create==2 && !checkdir(finaldest)) {
      DEBUG("Path %s does not exist", finaldest);
      ifree(finaldest);
      return NULL;
    }
  }

  *finalptr++ = '/';
  strcpy(finalptr, hash);
  DEBUG("Final repository path: \"%s\"", finaldest);

  return finaldest;
}

static int limbo_search(fileptr inode) {
  profile_init_start();
  fileptr *inodes=NULL;
  int i, count = inode_get_all(0, NULL, 0);
  if (count>0) {
    inodes = calloc(count, sizeof(fileptr));
    inode_get_all(0, inodes, count);
  } else if (count==0) {
    /* can't be found if no inodes in list */
    profile_stop();
    return 0;
  } else if (count<0) {
    DEBUG("Error calling inode_get_all(): %s", strerror(-count));
    profile_stop();
    return count;
  }

  for (i=0; i<count; i++) {
    if (inodes[i]==inode)
      break;
  }
  ifree(inodes);
  profile_stop();
  return (i<count)?(i+1):0;
}

int have_file_by_hash(const unsigned long hash) {
  char s_hash[9];
  hex_to_string(s_hash, hash);
  return tree_sub_search(tree_get_iroot(), s_hash) || limbo_search(hash)>0;
}

inline int have_file_by_name(const char *path) {
  return have_file_by_hash(hash_path(path));
}

inline int get_file_link_by_shash(const char *hash, struct stat *fstat) {
  char *filepath = gen_repos_path(hash, 0);
  if (!filepath) return 0;

  int ret = !((stat(filepath, fstat) == -1) && (errno == ENOENT));
  ifree(filepath);
  return ret;
}

inline int get_file_link_by_hash(const unsigned long hash, struct stat *fstat) {
  char s_hash[9];
  hex_to_string(s_hash, hash);
  return get_file_link_by_shash(s_hash, fstat);
}

inline int get_file_link_by_name(const char *path, struct stat *fstat) {
  return get_file_link_by_hash(hash_path(path), fstat);
}

char *basename_from_inode(const fileptr inode) {
  char *linkres = fullname_from_inode(inode);
  if (!linkres) {
    PMSG(LOG_ERR, "Failed to get fullname");
    return NULL;
  }

  char *answer = strlast(linkres, '/');

  DEBUG("Freeing linkres");
  ifree(linkres);

  return answer;
}

char *fullname_from_inode(const fileptr inode) {
  char hash[9];
  hex_to_string(hash, inode);
  DEBUG("Input hash: %s", hash);
  char *filepath = gen_repos_path(hash, 0);
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


static int _sibcallback(const char *key, const fileptr val, void *data) {
  (void) val;
  struct {
    char **sibs;
    char *prefix;
    int cur;
    int sibcount;
  } *thing = data;
  if (thing->cur >= thing->sibcount) {
    PMSG(LOG_ERR, "\033[1;31mSERIOUS ERROR\033[m");
    PMSG(LOG_ERR, "\033[1;31mcur: %d; sibcount: %d\033[m", thing->cur, thing->sibcount);
    return -1;
  }
#ifdef _DEBUG
  DEBUG("Key: \"%s\"", key);
  DEBUG("Prefix: \"%s\"", thing->prefix);
  char *fullkey = tree_get_full_key(val);
  DEBUG("Full key: \"%s\"", fullkey);
  ifree(fullkey);
#endif
  if (thing->prefix && *(thing->prefix)) {
    thing->sibs[thing->cur] = calloc(strlen(thing->prefix)+strlen(key)+1, sizeof(char));
    strcpy(thing->sibs[thing->cur], thing->prefix);
    strcat(thing->sibs[thing->cur], key);
  } else {
    thing->sibs[thing->cur] = strdup(key);
  }
  thing->cur++;
  return 0;
}

char **path_get_subkeys(const char *path, unsigned int *count) {
/* Basically:
 *  - build list of all directories in path
 *  - remove any that don't have the same prefix as the : item
 *  - remove the common prefix and add to array REM
 *  - get set of all subkeys of : item and add to array SET
 *  - do difference (SET-REM) and there's the answer
 */
  char *prefix = rindex(path, '/');
  if (!prefix) {
    prefix = strdup(path);
  } else {
    prefix = strdup(prefix+1);
  }
  unsigned int prefixlen = strlen(prefix);
  /* convert trailing INSIGHT_SUBKEY_IND_C */
  if (prefix[prefixlen-1]==INSIGHT_SUBKEY_IND_C)
    prefix[prefixlen-1]=INSIGHT_SUBKEY_SEP_C;
  else {
    DEBUG("Not a subkey path");
    ifree(prefix);
    return NULL;
  }

  DEBUG("Prefix: %s", prefix);

  unsigned int alloc_count=0, pathcount=0;
  alloc_count+=strcount(path, '/');

  int bitscount;
  char **bits = strsplit(path, '/', &bitscount);

  char **path_set=calloc(alloc_count, sizeof(char*));
  unsigned int i=0, p=0;
  for (i=0; i<(unsigned int)bitscount; i++) {
    if (!*bits[i])
      continue;
    if (strncmp(bits[i], prefix, prefixlen)!=0)
      continue;
    DEBUG("Adding \"%s\" to path set", bits[i]+prefixlen);
    path_set[p++]=strdup(bits[i]+prefixlen);
  }

  DEBUG("Freeing bits");

  for (bitscount--;bitscount>=0; bitscount--) {
    ifree(bits[bitscount]);
  }
  ifree(bits);

  qsort(path_set, p, sizeof(char*), pstrcmp);
  pathcount = set_uniq(path_set, p, sizeof(char*), pstrcmp);

  for (i=pathcount; i<p; i++) {
    ifree(path_set[i]);
  }

  prefix[prefixlen-1]='\0';
  DEBUG("Children of %s", prefix);
  fileptr tree_root = get_tag(prefix);
  prefix[prefixlen-1]=INSIGHT_SUBKEY_SEP_C;
  unsigned int sibcount = tree_sub_key_count(tree_root);
  DEBUG("%d children", sibcount);

  char **sibset=calloc(sibcount+1, sizeof(char*));
  struct {
    char **sibs;
    char *prefix;
    int cur;
    int sibcount;
  } thing;
  thing.sibs = sibset;
  thing.cur=0;
  thing.sibcount=sibcount;
  thing.prefix=NULL;
  tree_map_keys(tree_root, _sibcallback, (void*)&thing);

  *count=set_diff(sibset, path_set, sibcount, pathcount, sizeof(char*), pstrcmp);
  for (i=*count; i<sibcount; i++) {
    ifree(sibset[i]);
  }

  for (i=0; i<pathcount; i++) {
    ifree(path_set[i]);
  }
  ifree(path_set);
  ifree(prefix);
  return sibset;
}

char **path_get_dirs(const char *path, unsigned int *count) {
  unsigned int i;

  if (last_char_in(path)==INSIGHT_SUBKEY_IND_C) {
    return path_get_subkeys(path, count);
  }

  DEBUG("Getting directories for path: %s", path);

  unsigned int alloc_count=0, pathcount=0;
  // count amount of space we'll need in PATHS set
  alloc_count+=strcount(path, INSIGHT_SUBKEY_SEP_C);
  alloc_count+=strcount(path, '/');
  DEBUG("Path set count: %u", alloc_count);

  if (strcmp(path, "/")==0) {
    DEBUG("Children of root");
    *count = tree_sub_key_count(0);
    DEBUG("%d children of root", *count);

    char **sibset=calloc(*count+1, sizeof(char*));
    struct {
      char **sibs;
      char *prefix;
      int cur;
      int sibcount;
    } thing;
    thing.sibs = sibset;
    thing.cur=0;
    thing.sibcount=*count;
    thing.prefix=NULL;
    tree_map_keys(0, _sibcallback, (void*)&thing);

    return sibset;
  }

  int bitscount;
  char **bits = strsplit(path, '/', &bitscount);

  char **path_set=calloc(alloc_count, sizeof(char*));
  unsigned int p=0;
  for (i=0; i<(unsigned int)bitscount; i++) {
    char *tmp;
    if (!*bits[i])
      continue;
    DEBUG("Adding \"%s\" to path set", bits[i]);
    path_set[p++]=strdup(bits[i]);
    while ((tmp=rindex(bits[i], INSIGHT_SUBKEY_SEP_C))) {
      *tmp='\0';
      path_set[p++]=strdup(bits[i]);
      DEBUG("Adding \"%s\" to path set", bits[i]);
    }
  }

  DEBUG("Freeing bits");

  for (bitscount--;bitscount>=0; bitscount--) {
    ifree(bits[bitscount]);
  }
  ifree(bits);

  qsort(path_set, alloc_count, sizeof(char*), pstrcmp);
  pathcount = set_uniq(path_set, alloc_count, sizeof(char*), pstrcmp);

  for (i=pathcount; i<alloc_count; i++) {
    ifree(path_set[i]);
  }


  /*
   * for each element in PATHS:
   *  - union set of all (fully-qualified) siblings with TAGS set
   */
  alloc_count=pathcount;
  char **outset = malloc(alloc_count*sizeof(char*));
  char **tmpset = NULL;
  memcpy(outset, path_set, alloc_count*sizeof(char*));
  char *curprefix=NULL;
  int out_count;
  for (i=0; i<pathcount; i++) {
    /* get parent tag */
    fileptr tree_root;
    char *tmp=rindex(path_set[i], INSIGHT_SUBKEY_SEP_C);
    if (!tmp) {
      tree_root=0;
      curprefix=strdup("");
    } else {
      *tmp='\0';
      tree_root=get_tag(path_set[i]);
      curprefix=calloc(strlen(path_set[i])+2, sizeof(char));
      strcat(curprefix, path_set[i]);
      strcat(curprefix, INSIGHT_SUBKEY_SEP);
      *tmp=INSIGHT_SUBKEY_SEP_C;
    }
    DEBUG("Current prefix: \"%s\"", curprefix);

    /* get set of all siblings */
    unsigned int sibcount = tree_sub_key_count(tree_root);
    DEBUG("%d siblings of tag \"%s\", including itself", sibcount, path_set[i]);

    char **sibset=calloc(sibcount+1, sizeof(char*));
    struct {
      char **sibs;
      char *prefix;
      int cur;
      int sibcount;
    } thing;
    thing.sibs = sibset;
    thing.cur=0;
    thing.sibcount=sibcount;
    thing.prefix=curprefix;
    DEBUG("tree_map_keys();");
    tree_map_keys(tree_root, _sibcallback, (void*)&thing);
    DEBUG("tree_map_keys(); // done");

    DEBUG("Realloc()ing tmpset");
    tmpset=realloc(tmpset, alloc_count*sizeof(char*)); /* TODO: check for failure */
    DEBUG("memcpy()");
    memcpy(tmpset, outset, alloc_count*sizeof(char*));
    DEBUG("Realloc()ing outset");
    out_count = alloc_count+sibcount;
    outset=realloc(outset, out_count*sizeof(char*));

    unsigned int k;

    DEBUG("Set union");
    alloc_count = set_union(tmpset, sibset, outset, alloc_count, sibcount, out_count, sizeof(char*), pstrcmp);
    for (k=alloc_count; k<(unsigned int)out_count; k++) {
      DEBUG("Freeing outset[%u] %p (\"%s\")", k, outset[k], outset[k]);
      ifree(outset[k]);
    }
    ifree(sibset);
    ifree(curprefix);
  }
  ifree(tmpset);

  DEBUG("Calling set_uniq()");
  out_count = set_uniq(outset, alloc_count, sizeof(char*), pstrcmp);

  out_count = set_diff(outset, path_set, out_count, pathcount, sizeof(char*), pstrcmp);
  ifree(path_set);

  for (i=out_count; i<alloc_count; i++) {
    DEBUG("Freeing \"%s\" (%p)", outset[i], outset[i]);
    ifree(outset[i]);
  }

  *count = out_count;
  return outset;
}
