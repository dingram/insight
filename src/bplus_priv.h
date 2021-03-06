#ifndef _BPLUS_PRIV_H
#define _BPLUS_PRIV_H

#include <config.h>

/** @cond */
#ifndef FALSE
  #define FALSE 0
#endif
#ifndef TRUE
  #define TRUE (!FALSE)
#endif
/** @endcond */


/** zero some memory */
#define zero_mem(s, n) memset(s, 0, n)
/** zero a structure */
#define zero_block(s) zero_mem(s, TREEBLOCK_SIZE)

/* ***************************************************************************
 *  STATISTICS
 ************************************************************************** */

#ifdef TREE_STATS_ENABLED
typedef struct {
  unsigned long long reads;
  unsigned long long writes;
#ifdef TREE_CACHE_ENABLED
  unsigned long long cache_reads;
  unsigned long long cache_writes;
#endif
} stats_ent;

/** Per-block statistics storage */
static stats_ent *tree_stats;
#endif

/* ***************************************************************************
 *  CACHING
 ************************************************************************** */

#ifdef TREE_CACHE_ENABLED
/** Cache entry */
typedef struct {
  unsigned int writecount;  /**< Number of times this block has been written without being flushed to disk */
  fileptr addr;             /**< Block address of this cache entry */
  tblock data;              /**< Block data */
} cache_ent;

/** Cache array */
static cache_ent *block_cache;

/** Maximum number of times to update a block inside the cache before writing it to disk and resetting write count. For safety. */
#define CACHE_MAX_WRITES 25

/** Maximum size of cache in bytes (1MiB) */
#define CACHE_MAX_SIZE (1024*1024)

/** Number of blocks to store in cache */
#define CACHE_COUNT (CACHE_MAX_SIZE/sizeof(cache_ent))
#endif

/* ***************************************************************************
 *  STATIC PROTOTYPES
 ************************************************************************** */

static int      tree_format        ();
static int      tree_format_free   (fileptr start, fileptr size);
static fileptr  tree_alloc         (void);
static int      tree_free          (fileptr block);
static int      tree_find_key      (tnode *node, const char *key);
static int      tree_insert_key    (tnode *node, unsigned int keyindex, char **key, fileptr *ptr);
static int      tree_insert_recurse(fileptr root, char **key, fileptr *ptr);
static int      _tree_write        (fileptr block, tblock *data);
#ifdef TREE_CACHE_ENABLED
static int      tree_cache_init    ();
static int      tree_cache_flush   (int clear);
static int      tree_cache_drop    ();
static tblock * tree_cache_get     (fileptr block);
static int      tree_cache_put     (fileptr block, tblock *data);
#endif
static inline void _tree_touch();

/** Internal storage of file handle for tree file */
static int tree_fp = -1;
/** In-memory version of tree superblock */
static tsblock *tree_sb;
/** Time of last tree change */
static time_t last_modified;

#ifdef TREE_CACHE_ENABLED
/**
 * Get the cache entry for the given block number. Uses a very simplistic algorithm for calculating location.
 *
 * @param block The block number for which the cache table index should be calculated.
 * @returns An index into the cache table for the given block
 * @todo Make this more complex?
 */
static inline int tree_get_cache_loc(fileptr block) {
  return (block-1) % CACHE_COUNT;
}
#endif

#ifndef ifree
/** free() with guard to avoid double-freeing anything */
#define ifree(x) do {\
	if (x) {\
		free(x);\
		x = NULL;\
	} else {\
		PMSG(LOG_ERR, "ERROR in free(): %s is NULL!", #x);\
	}\
} while (0)
#endif

#endif
