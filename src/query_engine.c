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
#if defined(_DEBUG_QUERY) && !defined(_DEBUG)
#define _DEBUG
#endif
#include <debug.h>
#include <string_helpers.h>
#include <query_engine.h>
#include <bplus.h>
#include <path_helpers.h>
#include <set_ops.h>


static inline qelem *_qtree_make_isany() {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_IS_ANY;
  return node;
}

static inline qelem *_qtree_make_is(const char *tag) {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_IS;
  node->tag=strdup(tag);
  return node;
}

static inline qelem *_qtree_make_is_nosub(const char *tag) {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_IS_NOSUB;
  node->tag=strdup(tag);
  return node;
}

static inline qelem *_qtree_make_is_inode(const fileptr inode) {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_IS_INODE;
  node->inode=inode;
  return node;
}

static inline qelem *_qtree_make_not_tag(const char *tag) {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_NOT;
  node->tag=strdup(tag);
  return node;
}

static inline qelem *_qtree_make_not(const qelem *target) {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_NOT;
  node->next[0]=(qelem *)target;
  return node;
}

static inline qelem *_qtree_make_and(const qelem *node1, const qelem *node2) {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_AND;
  node->next[0]=(qelem *)node1;
  node->next[1]=(qelem *)node2;
  return node;
}

static inline qelem *_qtree_make_or(const qelem *node1, const qelem *node2) {
  qelem *node=calloc(1, sizeof(qelem));
  if (!node) {
    PMSG(LOG_ERR, "Failed to allocate space for query node");
    return NULL;
  }
  node->type=QUERY_OR;
  node->next[0]=(qelem *)node1;
  node->next[1]=(qelem *)node2;
  return node;
}

/**
 * Traverse query tree to find a node of the given type.
 *
 * @param root The root of the tree to traverse.
 * @param type The type of node to find.
 * @returns Non-zero if the node type was found, or zero if not (or root was
 * NULL).
 */
static int _qtree_contains(const qelem *root, const unsigned int type) {
  if (!root) {
    return 0;
  }
  if (root->type == type) {
    return 1;
  }

  switch (root->type) {
    case QUERY_IS_ANY:
    case QUERY_IS:
    case QUERY_IS_NOSUB:
    case QUERY_IS_INODE:
      return 0;
      break;

    case QUERY_NOT:
      if (root->tag) {
        return 0;
      } else {
        return _qtree_contains(root->next[0], type);
      }
      break;

    case QUERY_AND:
    case QUERY_OR:
      return _qtree_contains(root->next[0], type) || _qtree_contains(root->next[1], type);
      break;

    default:
      PMSG(LOG_ERR, "Unrecognised query tree element type %d", root->type);
      return 0;
      break;
  }
}

static void _qtree_dump(const qelem *node, int indent) {
#ifndef _DEBUG
  (void)node;
  (void)indent;
#else
  char *spc=calloc(indent*2+1, sizeof(char));
  memset(spc, ' ', indent*2);

  if (!node) {
    DEBUG("%s[NULL]", spc);
    ifree(spc);
    return;
  }

  switch (node->type) {
    case QUERY_IS_ANY:
      DEBUG("%sIS_ANY", spc);
      break;

    case QUERY_IS:
      DEBUG("%sIS: %s", spc, node->tag);
      break;

    case QUERY_IS_NOSUB:
      DEBUG("%sIS_NOSUB: %s", spc, node->tag);
      break;

    case QUERY_IS_INODE:
      DEBUG("%sIS_INODE: %08lX", spc, node->inode);
      break;

    case QUERY_NOT:
      if (node->tag) {
        DEBUG("%sNOT: %s", spc, node->tag);
      } else {
        DEBUG("%sNOT", spc);
        _qtree_dump(node->next[0], indent+1);
      }
      break;

    case QUERY_AND:
      DEBUG("%sAND", spc);
      _qtree_dump(node->next[0], indent+1);
      _qtree_dump(node->next[1], indent+1);
      break;

    case QUERY_OR:
      DEBUG("%sOR", spc);
      _qtree_dump(node->next[0], indent+1);
      _qtree_dump(node->next[1], indent+1);
      break;

    default:
      PMSG(LOG_ERR, "Unrecognised query tree element type %d", node->type);
      DEBUG("%sUNKNOWN: %d", spc, node->type);
      break;
  }
  ifree(spc);
#endif
}


/**
 * Recursively free a query tree.
 *
 * @param root      The address of the root element to be freed.
 * @param free_tags If true, will also free tags (if non-null).
 */
void qtree_free(qelem **root, int free_tags) {
  /* trivial cases */
  if (!root)  return;
  if (!*root) return;

  if (free_tags && (*root)->tag) {
    ifree((*root)->tag);
  }

  qtree_free(&((*root)->next[0]), free_tags);
  qtree_free(&((*root)->next[1]), free_tags);
  ifree(*root);
  *root=NULL;
}

/**
 * Recursively check a query tree for consistency. This ensures that:
 *  - All QUERY_IS_ANY elements have no tag or next values [strict]
 *  - All QUERY_IS elements have a tag
 *  - All QUERY_IS elements have no next values [strict]
 *  - All QUERY_NOT elements have either a tag or a next value, but not both
 *  - All QUERY_NOT elements have only one next value [strict]
 *  - All QUERY_AND elements have two next values
 *  - All QUERY_AND elements have no tag [strict]
 *  - All QUERY_OR elements have two next values
 *  - All QUERY_OR elements have no tag [strict]
 *
 * @param root    The address of the root element to be checked.
 * @param strict  If true, strict checking is enabled.
 * @returns Non-zero if the tree is consistent, zero otherwise.
 */
int qtree_consistent(qelem *root, int strict) {
  /* trivial case - a null tree is consistent */
  if (!root)  return 1;

  switch (root->type) {
    case QUERY_IS_ANY:
      if (!strict) return 1;
      if (root->inode || root->tag || root->next[0] || root->next[1]) return 0;
      return 1;
      break;

    case QUERY_IS_NOSUB:
      if (!root->tag) return 0;
      if (!strict) return 1;
      if (root->inode || root->next[0] || root->next[1]) return 0;
      return 1;
      break;

    case QUERY_IS_INODE:
      if (!strict) return 1;
      if (root->tag) return 0;
      if (root->next[0] || root->next[1]) return 0;
      return 1;
      break;

    case QUERY_NOT:
      if ( (root->tag && root->next[0]) || (!root->tag && !root->next[0]) )
        return 0;
      if (strict && (root->inode || root->next[1])) return 0;
      return 1;
      break;

    case QUERY_AND:
    case QUERY_OR:
      if (!root->next[0] || !root->next[1]) return 0;
      if (strict && (root->inode || root->tag)) return 0;
      return qtree_consistent(root->next[0], strict) && qtree_consistent(root->next[0], strict);
      break;

    default:
      PMSG(LOG_ERR, "Unrecognised query tree element type %d", root->type);
      return 0;
      break;
  }
}

/**
 * Small procedure to create a basic conjunctive query tree from a tokenised path.
 *
 * @param str  Incoming path token.
 * @param data A qelem** in disguise that should be updated.
 * @return Always zero.
 */
static int _path_to_query_proc(const char *str, unsigned long data) {
  DEBUG("str: \"%s\"", str);
  qelem *newnode=NULL;
  qelem **qroot=(qelem**)data;
  int is_tag = get_tag(str);
  unsigned long path_hash = hash_path(str);
  int is_file = have_file_by_hash(path_hash);

  if (is_tag) {
    tdata dblock;
    if (tree_read(is_tag, (tblock*)&dblock)) {
      PMSG(LOG_ERR, "I/O error reading block\n");
      qtree_free(qroot, 1);
      return -EIO;
    }
    if (dblock.flags & DATA_FLAGS_NOSUB) {
      newnode=_qtree_make_is_nosub(str);
    } else {
      newnode=_qtree_make_is(str);
    }

  } else if (is_file) {
    newnode=_qtree_make_is_inode(path_hash);

  } else {
    DEBUG("Tag \"%s\" does not exist");
    qtree_free(qroot, 1);
    return -ENOENT;
  }

  if (*qroot) {
    *qroot=_qtree_make_and(newnode, *qroot);
  } else {
    *qroot=newnode;
  }
  return 0;
}

/**
 * Converts a path to a simple conjunctive query.
 *
 * @param path The (not necessarily canonical) path to be parsed.
 * @returns A pointer to a tree structure representing the query, or NULL if an
 * error occurred. If an error occurs, \c errno is set appropriately.
 */
qelem *path_to_query(const char *path) {
  if (!path) return NULL;

  char *dup = get_canonical_path(path);
  DEBUG("Canonical path: \"%s\"", path);
  if (!dup || errno) {
    DEBUG("Error in get_canonical_path()");
    if (dup) ifree(dup);
    errno=ENOENT;
    return NULL;
  }
  char *tmp = dup;
  qelem *qroot=NULL;

  /* remove leading slash characters */
  while (*tmp && *tmp=='/')
    tmp++;

  /* If we have a zero-length path, then that matches any tag */
  if (!*tmp) {
    DEBUG("Top-level ISANY");
    qroot = _qtree_make_isany();

  } else {
    int res = strsplitmap(tmp, '/', _path_to_query_proc, (unsigned long)&qroot);

    if (res==0) {
      DEBUG("Success");
    } else if (res==-ENOENT) {
      DEBUG("A component of the path does not exist");
      qtree_free(&qroot, 1);
      ifree(dup);
      errno=ENOENT;
      return NULL;
    } else {
      DEBUG("Problem with strsplitmap");
      qtree_free(&qroot, 1);
      ifree(dup);
      errno=-res;
      return NULL;
    }

    if (_qtree_contains(qroot, QUERY_IS_INODE) && !query_inode_count(qroot)) {
      /* cannot find that inode with the query */
      DEBUG("Could not find the given file");
      qtree_free(&qroot, 1);
      ifree(dup);
      errno=ENOENT;
      return NULL;
    }
  }

  DEBUG("Dumping tree...");
  _qtree_dump(qroot, 0);
  DEBUG("Tree dump done.");

  ifree(dup);
  return qroot;
}

/**
 * Work out what tags we should display, based on a query tree. If \a parent is
 * an empty string, then we should display the root-level tags. Otherwise, we
 * should display the subtags of \a parent. If the subtag name is longer than
 * \a len then it will be truncated.
 *
 * @param[in]  query   The query tree to examine.
 * @param[out] parent  Pointer to a string to be filled with the subtag parent.
 * @param[in]  len     The maximum length for \a parent.
 * @returns Zero on success, 1 on failure, or 2 if the query tree gives an
 * ambiguous answer.
 */
int query_get_subtags(const qelem *query, char *parent, int len) {
  if (!query || !parent) return 1;

  /* Essentially just search the tree for incomplete tags */
  switch (query->type) {
    case QUERY_IS_ANY:
    case QUERY_IS_INODE:
      strncpy(parent, "", len);
      return 0;
      break;

    case QUERY_IS:
    case QUERY_IS_NOSUB:
      if (last_char_in(query->tag)==INSIGHT_SUBKEY_IND_C) {
        /* incomplete tag - aha! */
        DEBUG("Found incomplete tag \"%s\"!", query->tag);
        strncpy(parent, query->tag, len);
        while (*parent && last_char_in(parent)==INSIGHT_SUBKEY_IND_C) {
          last_char_in(parent)='\0';
        }
      } else {
        /* complete tag */
        DEBUG("Found complete tag \"%s\"", query->tag);
        strncpy(parent, "", len);
      }
      return 0;
      break;

    case QUERY_NOT:
      if (query->tag) {
        if (last_char_in(query->tag)==INSIGHT_SUBKEY_IND_C) {
          /* incomplete tag - aha! */
          DEBUG("Found incomplete tag \"%s\"!", query->tag);
          strncpy(parent, query->tag, len);
          while (*parent && last_char_in(parent)==INSIGHT_SUBKEY_IND_C) {
            last_char_in(parent)='\0';
          }
        } else {
          /* complete tag */
          DEBUG("Found complete tag \"%s\"", query->tag);
          strncpy(parent, "", len);
        }
        return 0;
      } else {
        DEBUG("Recursing into NOT node");
        return query_get_subtags(query->next[0], parent, len);
      }
      break;

    /* Ambiguities can only arise when we have two tags to consider */
    case QUERY_AND:
    case QUERY_OR:
      {
        int wasset=0;

        DEBUG("AND/OR: checking first branch");
        int retval=query_get_subtags(query->next[0], parent, len);
        if (retval) {
          DEBUG("AND/OR: first branch returned an error");
          return retval;
        }
        DEBUG("AND/OR: first branch gives us tag \"%s\"", parent);
        wasset = (*parent) ? 1 : 0;

        char *tmp;
        if (wasset) {
          tmp=calloc(len, sizeof(char));
          if (!tmp) {
            PMSG(LOG_ERR, "Failed to allocate space for temp string");
            return 1;
          }
        } else {
          tmp=parent;
        }

        DEBUG("AND/OR: checking second branch");
        retval=query_get_subtags(query->next[1], tmp, len);
        if (retval) {
          DEBUG("AND/OR: second branch returned an error");
          if (wasset) ifree(tmp);
          return retval;
        }
        DEBUG("AND/OR: second branch gives us tag \"%s\"", tmp);

        if (wasset && *tmp) {
          PMSG(LOG_WARNING, "Ambiguous result detected in query tree!");
          if (wasset) ifree(tmp);
          return 2;
        }

        DEBUG("AND/OR: therefore our resulting tag is \"%s\"", parent);
        if (wasset) ifree(tmp);
        return 0;
      }
      break;

    default:
      PMSG(LOG_ERR, "Unrecognised query tree element type %d", query->type);
      return 1;
      break;
  }

  return 0;
}

/**
 * Get number of inodes a query would return. May be optimised.
 *
 * @param query The root of the query to use.
 * @returns The number of inodes that would be found by the query.
 */
int query_inode_count(const qelem * const query) {
  int count=0, neg=0;
  fileptr *inodes = query_to_inodes(query, &count, &neg);
  if (!inodes) {
    DEBUG("Error");
    return 0;
  }
  ifree(inodes);
  return count;
}

/**
 * Get inode list from query tree. The returned list is allocated with malloc()
 * and should be freed with free().
 *
 * @param query The root of the query tree to calculate the inode list for.
 * @param count The number of inodes in the returned list.
 * @param neg   True if the query results should be negated.
 * @returns The list of inodes available given the query tree, or NULL on error
 * or if count is zero.
 */
fileptr *query_to_inodes(const qelem * const query, int * const count, int * const neg) {
  DEBUG("Function entry");
  if (!query) {
    PMSG(LOG_ERR, "Query was null");
    return NULL;
  }
  if (!count) {
    PMSG(LOG_ERR, "Count was null");
    return NULL;
  }
  if (!neg) {
    PMSG(LOG_ERR, "Neg was null");
    return NULL;
  }
  switch (query->type) {
    case QUERY_IS_ANY:
      /* IS_ANY node, output set is the set of limbo inodes, with internal
       * negation flag set to false
       */
      DEBUG("IS_ANY: creating list of limbo inodes, negation 0");
      *neg=0;
      *count = inode_get_all(0, NULL, 0);
      if (*count>0) {
        fileptr *inodes = calloc(*count, sizeof(fileptr));
        inode_get_all(0, inodes, *count);
        return inodes;
      } else if (*count==0) {
        /* empty return list to avoid error; must still be freed though */
        return calloc(1, sizeof(fileptr));
      } else if (*count<0) {
        DEBUG("Error calling inode_get_all(): %s", strerror(-*count));
        return NULL;
      }
      break;

    case QUERY_IS:
      /* IS node, the output set is the recursive union of the inodes belonging
       * to that tag and its subtags, with internal negation flag set to false
       */
      {
        fileptr dblock = get_tag(query->tag);

        DEBUG("IS: fetching recursive list of inodes from block %lu, negation 0", dblock);
        *neg=0;
        fileptr *inodes = inode_get_all_recurse(dblock, count);
        DEBUG("IS: %d inodes in block %lu", *count, dblock);
        if (inodes && *count>=0) {
          return inodes;
        } else {
          if (inodes) ifree(inodes);
          DEBUG("Error calling inode_get_all(): %s", strerror(-*count));
          return NULL;
        }
      }
      break;

    case QUERY_IS_NOSUB:
      /* IS_NOSUB node, the output set is the set of inodes belonging tag, with
       * internal negation flag set to false
       */
      {
        fileptr dblock = get_tag(query->tag);

        DEBUG("IS_NOSUB: creating list of inodes from block %lu, negation 0", dblock);
        *neg=0;
        *count = inode_get_all(dblock, NULL, 0);
        DEBUG("IS_NOSUB: %d inodes in block %lu", *count, dblock);
        if (*count>0) {
          fileptr *inodes = calloc(*count, sizeof(fileptr));
          inode_get_all(dblock, inodes, *count);
          return inodes;
        } else if (*count==0) {
          /* empty return list to avoid error; must still be freed though */
          return calloc(1, sizeof(fileptr));
        } else if (*count<0) {
          DEBUG("Error calling inode_get_all(): %s", strerror(-*count));
          return NULL;
        }
      }
      break;

    case QUERY_IS_INODE:
      /* IS_INODE node, the output set contains a single element: the inode. */
      {
        fileptr *ret = calloc(1, sizeof(fileptr));
        DEBUG("IS_INODE: single inode 0x%08lx, negation 0", query->inode);
        *count=1;
        *neg=0;
        *ret=query->inode;
        return ret;
      }
      break;

    case QUERY_NOT:
      if (query->tag) {
        /* IS_NOT node with a tag, the output set is the same as for an IS
         * node, with an internal negation flag set to true */
        PMSG(LOG_ERR, "Unhandled type of NOT query! Tag is available.");
        return NULL;
      } else if (query->next[0]) {
        /* IS_NOT node with a subquery, the output set is identical to the
         * subquery resultset, with an internal negation flag inverted */
        fileptr *res = query_to_inodes(query, count, neg);
        *neg = !*neg;
        return res;
      } else {
        PMSG(LOG_ERR, "Unknown type of NOT query! Both tag and next[0] are null.");
        return NULL;
      }
      break;

    case QUERY_AND:
      /* AND node output depends on the negation flags of its subqueries:
       *  * Both false: output is the set intersection of its subqueries, with
       *  negation flag clear
       *  * Both true: output is union of subqueries, with negation flag set
       *  * Otherwise: output is set difference, with the negation-true set
       *  removed from the negation-false set, and the negation flag cleared
       */
      {
        DEBUG("AND: Conjunction of two query subtrees");
        int count1=0, count2=0, neg1=0, neg2=0;
        fileptr *res1 = query_to_inodes(query->next[0], &count1, &neg1);
        if (!res1) {
          DEBUG("Error in left branch");
          return NULL;
        }
        if (!count1 && !neg1) {
          /* short-circuit query evaluation */
          DEBUG("Short-circuit: first branch yielded no results");
          *count=0;
          *neg=neg1;
          return res1;
        }
        fileptr *res2 = query_to_inodes(query->next[1], &count2, &neg2);
        if (!res2) {
          DEBUG("Error in right branch");
          ifree(res1);
          return NULL;
        }
        if (!count2 && !neg2) {
          /* no need to perform set operations */
          DEBUG("Short-circuit: second branch yielded no results");
          *count=0;
          *neg=neg2;
          ifree(res1);
          return res2;
        }
        /* both subtrees returned results */
        if (!neg1 && !neg2) {
          fileptr *res = calloc(MIN(count1, count2), sizeof(fileptr));
          if (!res) {
            PMSG(LOG_ERR, "Failed to allocate memory for return array");
            ifree(res1);
            ifree(res2);
            return NULL;
          }
          /* output is the intersection of subqueries, with negation flag clear */
          *neg=0;
          *count=set_intersect(res1, res2, res, count1, count2, MIN(count1, count2), sizeof(fileptr), inodecmp);
          if (*count < 0) {
            PMSG(LOG_ERR, "Error in set intersection operation.");
            ifree(res1);
            ifree(res2);
            ifree(res);
            return NULL;
          } else {
            DEBUG("Set intersection succeeded; %d results", *count);
            ifree(res1);
            ifree(res2);
            return res;
          }
        /* } else if (neg1 && neg2) { */
          /* output is union of subqueries, with negation flag set */
        /* } else { */
          /* output is set difference, with the negation-true set removed from
           * the negation-false set, and the negation flag cleared */
        } else {
          PMSG(LOG_ERR, "Unhandled case in AND");
          ifree(res1);
          ifree(res2);
          return NULL;
        }
      }
      break;

    case QUERY_OR:
      /* OR node output depends on the negation flags of its subqueries:
       *  * Both false: output is union of subquery results, with negation flag
       *  clear
       *  * Both true: output is intersection of subquery results, with
       *  negation flag set
       *  * Otherwise: output is ???
       */
      {
        PMSG(LOG_ERR, "Cannot handle OR queries yet!");
        return NULL;

        DEBUG("OR: Disjunction of two query subtrees");
        int count1=0, count2=0, neg1=0, neg2=0;
        fileptr *res1 = query_to_inodes(query->next[0], &count1, &neg1);
        if (!res1) {
          DEBUG("Error in left branch");
          return NULL;
        }
        fileptr *res2 = query_to_inodes(query->next[1], &count2, &neg2);
        if (!res2) {
          DEBUG("Error in right branch");
          ifree(res1);
          return NULL;
        }
        if (!count1 && !neg1) {
          /* no need to perform set operations */
          DEBUG("Short-circuit: first branch yielded no results");
          *count=0;
          *neg=neg2;
          ifree(res1);
          return res2;
        }
        if (!count2 && !neg2) {
          /* no need to perform set operations */
          DEBUG("Short-circuit: second branch yielded no results");
          *count=0;
          *neg=neg1;
          ifree(res2);
          return res1;
        }
        /* both subtrees returned results */
        if (!neg1 && !neg2) {
          fileptr *res = calloc(count1 + count2, sizeof(fileptr));
          if (!res) {
            PMSG(LOG_ERR, "Failed to allocate memory for return array");
            ifree(res1);
            ifree(res2);
            return NULL;
          }
          /* output is the union of subqueries, with negation flag clear */
          *neg=0;
          *count=set_union(res1, res2, res, count1, count2, MIN(count1, count2), sizeof(fileptr), inodecmp);
          if (*count < 0) {
            PMSG(LOG_ERR, "Error in set union operation.");
            ifree(res1);
            ifree(res2);
            ifree(res);
            return NULL;
          } else {
            DEBUG("Set union succeeded; %d results", *count);
            ifree(res1);
            ifree(res2);
            return res;
          }
        /* } else if (neg1 && neg2) { */
          /* output is union of subqueries, with negation flag set */
        /* } else { */
          /* output is set difference, with the negation-true set removed from
           * the negation-false set, and the negation flag cleared */
        } else {
          PMSG(LOG_ERR, "Unhandled case in AND");
          ifree(res1);
          ifree(res2);
          return NULL;
        }
      }
      break;

    default:
      PMSG(LOG_ERR, "Unknown query tree element type %d!", query->type);
      return NULL;
  }
  return NULL;
}
