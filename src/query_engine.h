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

#ifndef __QUERY_ENGINE_H
#define __QUERY_ENGINE_H

/**
 * The type of a query tree element.
 */
enum qelem_type {
  QUERY_IS_ANY, /**< Matches any tag */
  QUERY_IS,     /**< Matches a specific tag */
  QUERY_NOT,    /**< Matches if not a specific tag or subquery */
  QUERY_AND,    /**< Matches if both subqueries match */
  QUERY_OR      /**< Matches is at least one subquery matches */
};

/**
 * Query tree element
 */
typedef struct qelem {
  enum qelem_type type;   /**< Type of this element (see qelem_type) */
  char *tag;              /**< The tag. Only used in QUERY_IS and QUERY_NOT nodes */
  struct qelem *next[2];  /**< Pointers to subparts of query */
} qelem;

/* Prototypes */
void qtree_free(qelem **root, int free_tags);
int qtree_consistent(qelem *root, int strict);
qelem *path_to_query(const char *path);
int query_get_subtags(const qelem *query, char *parent, int len);
#endif
