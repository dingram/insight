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
#ifndef _DEBUG
#define _DEBUG
#endif
#include <debug.h>
#include <string_helpers.h>
#include <query_engine.h>


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

static void _qtree_dump(const qelem *node, int indent) {
  char *spc=calloc(indent*2+1, sizeof(char));
  memset(spc, ' ', indent*2);

  if (!node) {
    printf("%s[NULL]\n", spc);
    ifree(spc);
    return;
  }

  switch (node->type) {
    case QUERY_IS_ANY:
      printf("%sIS_ANY\n", spc);
      break;

    case QUERY_IS:
      printf("%sIS: %s\n", spc, node->tag);
      break;

    case QUERY_NOT:
      if (node->tag) {
        printf("%sNOT: %s\n", spc, node->tag);
      } else {
        printf("%sNOT\n", spc);
        _qtree_dump(node->next[0], indent+1);
      }
      break;

    case QUERY_AND:
      printf("%sAND\n", spc);
      _qtree_dump(node->next[0], indent+1);
      _qtree_dump(node->next[1], indent+1);
      break;

    case QUERY_OR:
      printf("%sOR\n", spc);
      _qtree_dump(node->next[0], indent+1);
      _qtree_dump(node->next[1], indent+1);
      break;

    default:
      PMSG(LOG_ERR, "Unrecognised query tree element type %d", node->type);
      printf("%sUNKNOWN: %d\n", spc, node->type);
      break;
  }
  ifree(spc);
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
    DEBUG("Freeing tag");
    ifree((*root)->tag);
    DEBUG("Tag free");
  }

  qtree_free(&((*root)->next[0]), free_tags);
  qtree_free(&((*root)->next[1]), free_tags);
  DEBUG("Freeing element");
  ifree(*root);
  DEBUG("Element free");
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
      if (root->tag || root->next[0] || root->next[1]) return 0;
      return 1;
      break;

    case QUERY_IS:
      if (!root->tag) return 0;
      if (!strict) return 1;
      if (root->next[0] || root->next[1]) return 0;
      return 1;
      break;

    case QUERY_NOT:
      if ( (root->tag && root->next[0]) || (!root->tag && !root->next[0]) )
        return 0;
      if (strict && root->next[1]) return 0;
      return 1;
      break;

    case QUERY_AND:
    case QUERY_OR:
      if (!root->next[0] || !root->next[1]) return 0;
      if (strict && root->tag) return 0;
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
  qelem **qroot=(qelem**)data;
  if (!*qroot) {
    *qroot=_qtree_make_is(str);
  } else {
    *qroot=_qtree_make_and(_qtree_make_is(str), *qroot);
  }
  return 0;
}

/**
 * Converts a path to a simple conjunctive query.
 *
 * @param path The path to be parsed.
 * @returns A pointer to a tree structure representing the query, or NULL if an
 * error occurred.
 */
qelem *path_to_query(const char *path) {
  if (!path) return NULL;

  char *dup = strdup(path);
  qelem *qroot=NULL;

  /* remove leading slash characters */
  while (dup[0] && dup[0]=='/')
    dup++;

  /* If we have a zero-length path, then that matches any tag */
  if (!*dup) {
    DEBUG("Top-level ISANY");
    qroot = _qtree_make_isany();
  } else {
    if (strsplitmap(dup, '/', _path_to_query_proc, (unsigned long)&qroot)) {
      return NULL;
    }
  }

  DEBUG("\033[1;31mDumping tree...\033[m");
  _qtree_dump(qroot, 0);
  DEBUG("\033[1;32mTree dump done.\033[m");

  // XXX: ifree(dup);
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
      strncpy(parent, "", len);
      return 0;
      break;

    case QUERY_IS:
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
          PMSG(LOG_WARN, "Ambiguous result detected in query tree!");
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
