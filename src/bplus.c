#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "bplus.h"
#include "bplus_debug.h"
#include "bplus_priv.h"

/**
 * Format the tree file, initialising the superblock and free space.
 *
 * @retval 0 Success.
 * @retval ENOMEM Could not allocate space for tree superblock.
 * @retval (other) See tree_write().
 * @sa tree_write(), tree_format_free(), tree_open()
 */
static int tree_format () {
  tnode   root;
  int result;

  DEBUGMSG("Entered tree_format()");

  if (tree_sb) free(tree_sb);
  tree_sb=malloc(sizeof(tsblock));
  if (!tree_sb) return ENOMEM;

  DEBUGMSG("Initialising structures");
  zero_block(tree_sb);
  initTreeNode(&root);

  DEBUGMSG("Setting up superblock structure");
  tree_sb->magic=MAGIC_SUPERBLOCK;
  tree_sb->version=TREE_FILE_VERSION;
  tree_sb->root_index=1;
  tree_sb->max_size=DEFAULT_BLOCKS;
  tree_sb->free_head=0;

  DEBUGMSG("Setting up root structure");
  root.leaf=1;
  root.keycount=0;

  DEBUGMSG("Writing root block");
  if ((result=tree_write(1, (tblock*)&root))) {
    DEBUG("Problem writing root: %s", strerror(errno));
    return errno;
  }

  /* now let's initialise some space - this also writes the superblock */
  return tree_format_free(2, DEFAULT_BLOCKS - 1); /* Remove one to account for the root block */
}

/**
 * Set up free block linked list and link it to the superblock.
 *
 * @param start The first block to be marked free
 * @param size  The number of blocks to be formatted
 * @retval 0 Success.
 * @retval EIO I/O error - see \c errno for more details. Either seek or write failed.
 * @retval (other) See tree_write().
 * @sa tree_format(), tree_grow(), tree_write()
 */
static int tree_format_free (fileptr start, fileptr size) {
  freeblock f;
  fileptr result;
  unsigned int i;

  DEBUGMSG("Zeroing free block structure");
  initFreeNode(&f);

  DEBUGMSG("Seeking to start");
  /* NOTE: not using tree_write() here because that would be seek overkill */
  if (lseek(tree_fp, start * TREEBLOCK_SIZE, SEEK_SET)<0) {
    DEBUG("Seek failed: %s", strerror(errno));
    return EIO;
  }
  DEBUGMSG("Writing free blocks");
  for (i=0; i < size - 1; i++) {
    f.next = i + start + 1;
    if (write(tree_fp, &f, TREEBLOCK_SIZE)<0) {
      DEBUG("Write failed: %s", strerror(errno));
      return EIO;
    }
  }
  DEBUGMSG("Writing last free block");
  f.next = tree_sb->free_head;
  if (write(tree_fp, &f, TREEBLOCK_SIZE)<0) {
    DEBUG("Final write failed: %s", strerror(errno));
    return EIO;
  }

  DEBUGMSG("Updating and writing superblock");
  tree_sb->free_head = start;
  if ((result=tree_write_sb(tree_sb))) {
    DEBUG("Problem writing superblock: %s", strerror(errno));
    if (result!=EIO) errno=result;
    return errno;
  }

  DEBUGMSG("Complete");
  return 0;
}

/**
 * Allocate a block in the tree and return its index.
 *
 * @returns The address of the newly-allocated block, or 0 on failure.
 * @par \c errno values
 *  - \b ENOMEM: No free space available
 *  - \b EBADF: Invalid magic number when reading free block
 * @note Will not grow the tree if no free space is available.
 * @sa tree_free(), tree_read(), tree_write(), tree_write_sb()
 */
static fileptr tree_alloc (void) {
  freeblock t;
  fileptr h;
  int result;

  DEBUGMSG("Allocating a block");
  errno=0;
  if (!tree_sb->free_head) {
    DEBUGMSG("But we don't have any free");
    errno=ENOMEM;
    return 0;
  }
  h = tree_sb->free_head;
  DEBUG("Free block %lu obtained", h);
  if (tree_read(h, (tblock *)&t)) {
    DEBUGMSG("Problem reading free block");
    return 0;
  }
  if (t.magic != MAGIC_FREEBLOCK) {
    DEBUG("Invalid magic number (%lX)", t.magic);
    errno=EBADF;
    return 0;
  }
  DEBUG("Updating next free block to %lu", t.next);
  tree_sb->free_head = t.next;
  if ((result=tree_write_sb(tree_sb))) {
    DEBUG("Problem writing superblock: %s", strerror(errno));
    if (result!=EIO) errno=result;
    return 0;
  }
  return h;
}

/**
 * Free a previously-allocated block. Overwrites the block and adds it to the
 * free block list.
 *
 * @param block Address of the block to be freed
 * @retval 0 Success.
 * @retval EINVAL Attempted to free block with address 0 (i.e. the superblock).
 * @retval (other) See tree_write() or tree_write_sb().
 * @sa tree_alloc()
 */
static int tree_free (fileptr block) {
  freeblock t;
  int result;

  errno=0;
  if (!block) {
    DEBUGMSG("Tried to free zero block!");
    return EINVAL;
  }
  DEBUG("Freeing block %lu", block);
  initFreeNode(&t);
  DEBUG("  Adding block to free list (previous head %lu)", tree_sb->free_head);
  t.next = tree_sb->free_head;
  if ((result=tree_write(block, (tblock*)&t))) {
    DEBUG("Problem writing block: %s", strerror(errno));
    return errno;
  }
  tree_sb->free_head = block;
  if ((result=tree_write_sb(tree_sb))) {
    DEBUG("Problem writing superblock: %s", strerror(errno));
    if (result!=EIO) errno=result;
    return errno;
  }

  return 0;
}

/**
 * Open tree storage file (creating and formatting it if necessary) and
 * allocate/prepare internal structures, including cache.
 *
 * @param path The path to the storage file to be used.
 * @retval 0 Success.
 * @retval EMFILE Tree storage file already open.
 * @retval ENOMEM Could not allocate memory to store superblock.
 * @retval EIO Failed to open or create the file - details in <tt>errno</tt>.
 * @retval (other) See tree_format() or tree_read_sb() or tree_cache_init()
 * @sa tree_read_sb(), #tree_fp, #tree_sb, tree_close(), tree_format(), tree_cache_init()
 */
int tree_open (char *path) {
  int result;

  if (tree_fp >= 0) {
    DEBUGMSG("Already open");
    return EMFILE; /* Too many open files */

  } else if ( (tree_fp = open(path, O_RDWR)) >= 0) {
    DEBUGMSG("Opened tree...");
    tsblock *superb = malloc(sizeof(tsblock));
    if (!superb) return ENOMEM;

    DEBUGMSG("Reading superblock");
    /* read the superblock from the index */
    if ( (result=tree_read_sb(superb)) ) {
      DEBUGMSG("Superblock error");
      tree_close();
      return result; /* I/O error */
    } else {
      DEBUGMSG("Superblock OK");
      if (tree_sb) free(tree_sb);
      tree_sb = superb;
      DEBUG("Superblock version %d.%d", (tree_sb->version>>8), (tree_sb->version & 0xff));
#ifdef TREE_STATS_ENABLED
      tree_stats = malloc((tree_sb->max_size+1) * sizeof(stats_ent)); /* TODO: check for failure */
      zero_mem(tree_stats, (tree_sb->max_size+1) * sizeof(stats_ent));
#endif
#ifdef TREE_CACHE_ENABLED
      return tree_cache_init();
#else
      return 0;
#endif
    }

  } else if ( (tree_fp = open(path, O_RDWR|O_CREAT, 0644)) >= 0) {
    /* created the file - let's initialise it */
    DEBUGMSG("Creating and formatting tree...");
#ifdef TREE_STATS_ENABLED
    DEBUGMSG("Initialising stats");
    tree_stats = malloc((DEFAULT_BLOCKS+1) * sizeof(stats_ent)); /* TODO: check for failure */
    DEBUGMSG("Malloc done");
    zero_mem(tree_stats, (DEFAULT_BLOCKS+1) * sizeof(stats_ent));
    DEBUGMSG("Stats initialised");
#endif
    DEBUGMSG("Calling tree_format()");
    result=tree_format();
    if (result) return result;
#ifdef TREE_CACHE_ENABLED
    return tree_cache_init();
#else
    return 0;
#endif

  } else {
    /* failed to open or create a file */
    tree_fp = -1;
    return EIO;
  }
}

/**
 * Close tree storage file and free #tree_sb and cache.
 *
 * @retval 0 Success.
 * @retval EIO I/O error when closing file.
 * @retval ENOENT No tree storage file open.
 * @retval (other) See tree_cache_drop()
 * @sa tree_open(), tree_cache_drop()
 */
int tree_close (void) {
#ifdef TREE_STATS_ENABLED
  unsigned int i;
#endif
  DEBUGMSG("Closing tree");
  if (tree_fp >= 0) {
#ifdef TREE_CACHE_ENABLED
    if ((errno=tree_cache_drop())) {
      DEBUG("Failed to drop cache: %s", strerror(errno));
      return EFAULT;
    }
#endif
    if (close(tree_fp)) {
      return EIO;
    }
    tree_fp = -1;
#ifdef TREE_STATS_ENABLED
    /* dump statistics */
    if (tree_stats) {
      int total_reads=0, total_writes=0;
#ifdef TREE_CACHE_ENABLED
      int total_cache_reads=0, total_cache_writes=0;
#endif

      DEBUGMSG("Dumping statistics");
      for (i=0; i<tree_sb->max_size+1; i++) {
#ifdef TREE_CACHE_ENABLED
        if (tree_stats[i].reads || tree_stats[i].writes || tree_stats[i].cache_reads || tree_stats[i].cache_writes) {
          DEBUG("Stats[%4u]: %5u reads; %5u writes; %5u cache reads; %5u cache writes", i, tree_stats[i].reads, tree_stats[i].writes, tree_stats[i].cache_reads, tree_stats[i].cache_writes);
          total_reads        += tree_stats[i].reads;
          total_writes       += tree_stats[i].writes;
          total_cache_reads  += tree_stats[i].cache_reads;
          total_cache_writes += tree_stats[i].cache_writes;
        }
#else
        if (tree_stats[i].reads || tree_stats[i].writes) {
          DEBUG("Stats[%4u]: %5u reads; %5u writes", i, tree_stats[i].reads, tree_stats[i].writes);
          total_reads        += tree_stats[i].reads;
          total_writes       += tree_stats[i].writes;
        }
#endif
      }
#ifdef TREE_CACHE_ENABLED
      DEBUGMSG("-----------------------------------------------------------------------------");
      DEBUG("Stats TOTAL: %5u reads; %5u writes; %5u cache reads; %5u cache writes", i, total_reads, total_writes, total_cache_reads, total_cache_writes);
#else
      DEBUGMSG("--------------------------------------");
      DEBUG("Stats TOTAL: %5u reads; %5u writes", total_reads, total_writes);
#endif
      free(tree_stats);
    }
#endif
    if (tree_sb) free(tree_sb);
    tree_sb = NULL;
    errno=0;
    return 0;
  } else {
    return ENOENT;
  }
}

/**
 * Add more free blocks to the tree storage file.
 *
 * @param newsize The new size of the tree file, in blocks.
 * @retval 0 Success.
 * @retval EINVAL Tried to shrink the storage file.
 * @retval (other) See tree_format_free()
 */
int tree_grow (fileptr newsize) {
  if (newsize < tree_sb->max_size) {
    DEBUG("Tried to shrink metadata store from %lu blocks to %lu.", tree_sb->max_size, newsize);
    return EINVAL;
  }
  if (newsize == tree_sb->max_size) {
    DEBUG("No-op. Staying at %lu blocks", tree_sb->max_size);
    return 0;
  }
  return tree_format_free(tree_sb->max_size, newsize - tree_sb->max_size);
}

/**
 * Read a block (possibly from cache).
 *
 * @param[in] block Block index to be read.
 * @param[out] data Pointer to a tblock structure to fill.
 * @retval 0 Success.
 * @retval EINVAL \a data is a null pointer.
 * @retval EIO Error seeking or reading tree store. More details may be
 * available in \c errno.
 * @retval ENOMEM Failed to write entry to cache - details in \c errno.
 * @todo Make sure we're not reading outside the boundaries of the store.
 */
int tree_read (fileptr block, tblock *data) {
#ifdef TREE_CACHE_ENABLED
  tblock *cache;
#endif
  if (!data) {
    DEBUGMSG("Data is a null pointer!");
    return EINVAL;
  }
#ifdef TREE_CACHE_ENABLED
  /* Check cache */
  if ((cache=tree_cache_get(block))) {
    DEBUG("Using cached version of block %lu", block);
    memcpy(data,cache,sizeof(tblock));
    return 0;
  }
#endif
  /* Read from disk */
  if (lseek(tree_fp, block * TREEBLOCK_SIZE, SEEK_SET)<0) {
    DEBUG("Seek failed: %s", strerror(errno));
    return EIO;
  }
  if (read(tree_fp, data, TREEBLOCK_SIZE)<0) {
    DEBUG("Read failed: %s", strerror(errno));
    return EIO;
  }
  DEBUG("Read block %lu", block);
  DUMPBLOCK((tblock*)data);
#ifdef TREE_CACHE_ENABLED
  /* Save to cache */
  if ((errno=tree_cache_put(block, (tblock*)data))) {
    DEBUG("Failed to write to cache: %s", strerror(errno));
    return ENOMEM;
  }
#endif
#ifdef TREE_STATS_ENABLED
  if (tree_stats) tree_stats[block].reads++;
#endif
  return 0;
}

/**
 * Actually write a block.
 *
 * @param[in] block Block index to be written.
 * @param[in] data Pointer to a tblock structure to write.
 * @retval 0 Success.
 * @retval EINVAL \a data is a null pointer.
 * @retval EBADF Writing to block index 0 but passed data does not appear to be
 * a superblock. Should help stop the superblock being clobbered.
 * @retval EIO Error seeking or writing to tree store. More details may be
 * available in \c errno.
 * @todo Make sure we're not writing outside the boundaries of the store.
 */
static int _tree_write (fileptr block, tblock *data) {
  if (block==0 && data->magic!=MAGIC_SUPERBLOCK) {
    DEBUG("Tried to superblock but it doesn't look like one! Has magic %lX instead.", data->magic);
    return EBADF;
  }
  if (lseek(tree_fp, block * TREEBLOCK_SIZE, SEEK_SET)<0) {
    DEBUG("Seek failed: %s", strerror(errno));
    return EIO;
  }
  if (write(tree_fp, data, TREEBLOCK_SIZE)<0) {
    DEBUG("Write failed: %s", strerror(errno));
    return EIO;
  }
  DEBUG("Wrote block %lu", block);
  DUMPBLOCK((tblock*)data);
#ifdef TREE_STATS_ENABLED
  if (tree_stats) tree_stats[block].writes++;
#endif
  return 0;
}

/**
 * Write a block (possibly updating cache).
 *
 * @param[in] block Block index to be written.
 * @param[in] data Pointer to a tblock structure to write.
 * @retval 0 Success.
 * @retval EINVAL \a data is a null pointer.
 * @retval EBADF Writing to block index 0 but passed data does not appear to be
 * a superblock. Should help stop the superblock being clobbered.
 * @retval EIO Error seeking or writing to tree store. More details may be
 * available in \c errno.
 */
int tree_write (fileptr block, tblock *data) {
  if (!data) return EINVAL;
#ifdef TREE_CACHE_ENABLED
  if ((errno=tree_cache_put(block, (tblock*)data))) {
    DEBUG("Failed to write to cache: %s", strerror(errno));
    return ENOMEM;
  }
  return 0;
#else
  return _tree_write(block, data);
#endif
}

#ifdef TREE_CACHE_ENABLED
/**
 * Initialise cache.
 *
 * @retval 0 Success.
 * @retval EEXIST The cache is already initialised. To re-initialise, call tree_cache_drop() first.
 * @retval ENOMEM There is no memory available to initialise the cache.
 */
static int tree_cache_init() {
  if (block_cache) {
    DEBUGMSG("Cache already allocated");
    return EEXIST;
  }
  block_cache = malloc(CACHE_COUNT * sizeof(cache_ent));
  if (!block_cache) {
    DEBUGMSG("Failed to allocate cache");
    return ENOMEM;
  }
  DEBUG("Cache allocated: %u bytes containing %d entries", (CACHE_COUNT*sizeof(cache_ent)), CACHE_COUNT);
  return 0;
}

/**
 * Flush cache contents to disk.
 *
 * @param clear If true then the cache is cleared after being flushed to disk,
 * otherwise cache entries remain.
 * @retval 0 Success.
 * @retval EIO An I/O error occurred. More details may be available in \c errno.
 * @retval ENOENT The cache does has not been allocated.
 */
static int tree_cache_flush(int clear) {
  int i;
  if (!block_cache) {
    DEBUGMSG("Cache not allocated; cannot flush");
    return ENOENT;
  }
  DEBUGMSG("Flushing cache to disk...");
  if (tree_sb) {
    DEBUGMSG("Flushing superblock to disk...");
    if (_tree_write(0, (tblock*)tree_sb)) {
      DEBUGMSG("I/O error writing superblock");
      return EIO;
    }
  }
  for (i=0; i<CACHE_COUNT; i++) {
    /* if this entry has an address and it is dirty, write to disk */
    if (block_cache[i].addr && block_cache[i].writecount) {
      DEBUG("Flushing cache entry %d to block %lu", i, block_cache[i].addr);
      if (_tree_write(block_cache[i].addr, &block_cache[i].data)) {
        DEBUGMSG("I/O error");
        return EIO;
      }
      if (clear) zero_mem(&block_cache[i], sizeof(cache_ent));
    }
  }
  DEBUGMSG("Cache flushed");
  return 0;
}

/**
 * De-allocate cache.
 *
 * @retval 0 Success.
 * @retval ENOENT The cache has not been initialised. To initialise, call
 * tree_cache_init() first.
 * @retval EIO Error flushing cache to disk.
 */
static int tree_cache_drop() {
  if (!block_cache) {
    DEBUGMSG("Cache not allocated; cannot free");
    return ENOENT;
  }
  if (tree_cache_flush(0)) {
    DEBUGMSG("Failed to flush cache to disk - aborting");
    return EIO;
  }
  free(block_cache);
  DEBUGMSG("Cache freed");
  return 0;
}

/**
 * Get block from cache.
 *
 * @param block The block to search for in the cache.
 * @returns A pointer to the cached contents of the block.
 */
static tblock * tree_cache_get(fileptr block) {
  cache_ent *entry;
  if (!block) {
#ifdef TREE_STATS_ENABLED
    if (tree_stats) tree_stats[block].cache_reads++;
#endif
    return (tblock*)tree_sb;
  }
  if (!block_cache) {
    return NULL;
  }
  entry = &block_cache[tree_get_cache_loc(block)];
  if (entry->addr==block) {
#ifdef TREE_STATS_ENABLED
    if (tree_stats) tree_stats[block].cache_reads++;
#endif
    DEBUG("Read block %lu from cache", block);
    return &(entry->data);
  }
  return NULL;
}

/**
 * Put block into cache (update if exists).
 *
 * @param block The block address to cache.
 * @param data  The block data to save in the cache.
 * @retval 0 Success.
 * @retval EACCES Cache not initialised.
 */
static int tree_cache_put(fileptr block, tblock *data) {
  cache_ent *entry;
  /* if cache full, delete least-recently-used item */
  if (!block && tree_sb && data!=(tblock*)tree_sb) {
    DEBUG("Copying to stored superblock as %p != %p", data, tree_sb);
    memcpy(tree_sb, data, TREEBLOCK_SIZE);
#ifdef TREE_STATS_ENABLED
    if (tree_stats) tree_stats[block].cache_writes++;
#endif
    return 0;
  } else if (!tree_sb) {
    DEBUGMSG("Cannot cache superblock as it hasn't been allocated yet.");
    return 0;
  } else if (!block) {
    DEBUGMSG("Trying to put superblock into cache; doing nothing");
#ifdef TREE_STATS_ENABLED
    if (tree_stats) tree_stats[block].cache_writes++;
#endif
    return 0;
  }
  if (!block_cache) {
    DEBUGMSG("Cache not initialised");
    return EACCES;
  }
  /* if cache collision, overwrite */
  entry = &block_cache[tree_get_cache_loc(block)];
  if (entry->addr && entry->addr != block) {
    /* cache collision; output block to disk then overwrite */
    DEBUG("Collision while putting block %lu into cache; writing block %lu to disk", block, entry->addr);
    _tree_write(entry->addr, &entry->data);
    memcpy(&entry->data, data, TREEBLOCK_SIZE);
    entry->addr=block;
    entry->writecount=0;
#ifdef TREE_STATS_ENABLED
    if (tree_stats) tree_stats[block].cache_writes++;
#endif
    return 0;
  } else if (entry->addr == block || !entry->addr) {
    DEBUG("Putting block %lu into cache; %s", block, (entry->addr)?"overwriting itself":"nothing there yet");
    memcpy(&entry->data, data, TREEBLOCK_SIZE);
    if (entry->addr) entry->writecount++;
    entry->addr=block;
    /* Sync block to disk if we've written it a few times */
    if (entry->writecount >= CACHE_MAX_WRITES) {
      DEBUG("Syncing cache entry %u (block %lu) to disk", tree_get_cache_loc(block), entry->addr);
      _tree_write(entry->addr, &entry->data);
      entry->writecount = 0;
    }
#ifdef TREE_STATS_ENABLED
    if (tree_stats) tree_stats[block].cache_writes++;
#endif
    return 0;
  } else {
    DEBUGMSG("Should never get here.");
    return ENOTSUP;
  }
}

/**
 * Remove block from cache.
 *
 * @todo Implement this function.
 * @todo Is this really necessary?
 */
static void tree_cache_del(fileptr block) {
  return;
}
#endif

/**
 * Find a key in a given node.
 *
 * @param node The node to search.
 * @param key  They key for which to search.
 * @returns The index of the first key in the node that is greater than \a key.
 * @todo Make this use binary search.
 */
static int tree_find_key(tnode *node, char *key) {
  int k=0;
  //DEBUG("Searching for key \"%s\"", key);
  for (k=0; k<node->keycount; k++) {
    //DEBUG("Checking key[%d]=\"%s\": %d", k, node->keys[k], strncmp(node->keys[k], key, TREEKEY_SIZE) > 0);
    if (strncmp(node->keys[k], key, TREEKEY_SIZE) > 0)
      break;
  }
  //DEBUG("Key \"%s\" found at index %d", key, k);
  return k;
}

/**
 * Get the total number of keys in the tree.
 *
 * @returns The number of keys in the tree. Returns a negative error code on failure.
 */
int tree_key_count() {
  return tree_sub_key_count(tree_sb->root_index);
}

/**
 * Get the total number of keys in the given tree.
 *
 * @param[in]  root The root of the subtree to search
 * @returns The number of keys in the tree. Returns a negative error code on failure.
 */
int tree_sub_key_count(fileptr root) {
  tnode node;
  int total=0;

  errno=0;
  if (tree_sub_get_min(root, &node))
    return -EIO;

  do {
    total += node.keycount;
    if (node.ptrs[0] && tree_read(node.ptrs[0], (tblock*) &node)) {
      return -EIO;
    }
  } while (node.ptrs[0]);

  return node.keycount;
}

/**
 * Get the tree root index.
 *
 * @returns The index of the ultimate root of the tree, as given by the
 * superblock.
 */
fileptr tree_get_root() {
  return tree_sb->root_index;
}

/**
 * Get the <strong>node</strong> block associated with the minimum key in the
 * tree.
 *
 * @param[out] node A pointer to the minimum node block in the tree rooted at
 * \a root.
 * @returns See tree_sub_get_min()
 */
int tree_get_min(tnode *node) {
  return tree_sub_get_min(tree_sb->root_index, node);
}

/**
 * Get the <strong>node</strong> block associated with the minimum key in the
 * given tree.
 *
 * @param[in]  root The root of the subtree to search
 * @param[out] node A pointer to the minimum node block in the tree rooted at
 * \a root.
 * @retval 0 Success.
 * @retval EIO I/O error while reading a block from disk.
 */
int tree_sub_get_min(fileptr root, tnode *node) {
  DEBUG("tree_sub_get_min(%lu)", root);
  if (!root)
    return 0;

  if (tree_read(root, (tblock*)node))
    return EIO;

  /* check node is cell block */
  if (node->magic == MAGIC_DATANODE) {
    DEBUGMSG("Starting search at data node; using subkeys");
    root=((tdata*)node)->subkeys;
    return tree_sub_get_min(root, node);
  } else if (node->magic != MAGIC_TREENODE) {
    DEBUG("Invalid magic number (%lX)", node->magic);
    return EBADF;
  }

  while (!node->leaf) {
    root=node->ptrs[0];
    if (tree_read(root, (tblock*)node))
      return EIO;
  }
  return 0;
}

/**
 * Search for the given key in the root tree and get the associated data block.
 *
 * @param[in]  key  The key to find in the tree.
 * @param[out] node A pointer to the data block associated with \a key.
 * @returns See tree_sub_get()
 */
int tree_get(char *key, tdata *node) {
  return tree_sub_get(tree_sb->root_index, key, node);
}

/**
 * Search tree with given root for given key and get the associated data block.
 *
 * @param[in]  root The block index of the root of the tree to search.
 * @param[in]  key  The key to find in the given tree.
 * @param[out] node A pointer to the data block associated with \a key.
 *
 * @retval 0 Success.
 * @retval ENOENT The key was not found in the tree.
 * @retval EIO There was an I/O error while reading the block.
 */
int tree_sub_get(fileptr root, char *key, tdata *node) {
  fileptr dblock = tree_sub_search(root, key);

  if (!dblock) return ENOENT;
  if (tree_read(dblock, (tblock*)node))
    return EIO;
  return 0;
}

/*
 * search for given key and return data block index
 */
fileptr tree_search(char *key) {
  return tree_sub_search(tree_sb->root_index, key);
}

/*
 * search tree with given root for given key and return data block index
 */
fileptr tree_sub_search(fileptr root, char *key) {
  tnode node;
  int index;

  errno=0;
  DEBUG("Searching for \"%s\" from block %lu", key, root);
  if (!root) {
    DEBUGMSG("Root is zero; nowhere to start");
    errno=ENOENT;
    return 0;
  }
  index=0;
  for(;;) {
    if (tree_read(root, (tblock*)&node)) {
      errno=EIO;
      return 0;
    }
    /* check node is cell block */
    if (node.magic == MAGIC_DATANODE) {
      DEBUGMSG("Starting search at data node; using subkeys");
      root=((tdata*)&node)->subkeys;
      return tree_sub_search(root, key);
    } else if (node.magic != MAGIC_TREENODE) {
      DEBUG("Invalid magic number (%lX)", node.magic);
      errno=EBADF;
      return 0;
    }

    DEBUG("Checking node at block %lu\n", root);
    DUMPNODE(&node);

    index=tree_find_key(&node, key);
    if (node.leaf) break;
    root=node.ptrs[index];
  }
  if (!index || (strncmp(node.keys[index-1], key, TREEKEY_SIZE)!=0)) {
    DEBUG("Could not find \"%s\"", key);
    errno=ENOENT; /* Not found */
    return 0;
  }
  DEBUG("Found \"%s\", and it has data block %lu", key, node.ptrs[index]);
  return node.ptrs[index];
}

/*
 * insert a key into the given node
 */
static int tree_insert_key(tnode *node, unsigned int keyindex, char **key, fileptr *ptr) {
  fileptr ptrs[ORDER+1];
  tkey    keys[ORDER];
  unsigned int k, newcount, countdiff;

  DEBUG("Inserting key \"%s\"", *key);

  zero_mem(ptrs, sizeof(fileptr)*(ORDER+1));
  zero_mem(keys, sizeof(char)*TREEKEY_SIZE*ORDER);

  /* How many keys will be in this node when we're done? */
  newcount  = (unsigned int)(node->keycount+1) < (unsigned int)(ORDER) ? (unsigned int)(node->keycount+1) : (unsigned int)(ORDER/2);
  countdiff = node->keycount+1 - newcount; /* 0 unless we must split */

  DEBUGMSG("Copying keys to insertion point");
  /* Copy keys from halfway up to insertion point in case we must split */
	for(k = (unsigned int)(ORDER/2); k < keyindex; k++) {
		strncpy(keys[k], node->keys[k], TREEKEY_SIZE);
		ptrs[k+1] = node->ptrs[k+1];
	}
  DEBUG("Inserting new key \"%s\"", *key);
  /* Insert new key in temporary array */
  strncpy(keys[keyindex], *key, TREEKEY_SIZE);
  ptrs[keyindex+1] = *ptr;

  DEBUGMSG("Copying remaining keys to temp array");
  /* Copy remaining keys to temporary array */
	for(k = keyindex; k < node->keycount; k++) {
		strncpy(keys[k+1], node->keys[k], TREEKEY_SIZE);
		ptrs[k+2] = node->ptrs[k+1];
	}
  DEBUGMSG("Copying items back to node");
  /* Now copy up to newcount items back to node */
	for(k = keyindex; k < newcount; k++) {
		strncpy(node->keys[k], keys[k], TREEKEY_SIZE);
		node->ptrs[k+1] = ptrs[k+1];
	}

  DEBUG("Updating keycount to %d", newcount);
  node->keycount = newcount;

  errno=0;
  if (countdiff) {
    unsigned int s, d;
    int result;
    tnode newnode;
    initTreeNode(&newnode);
    DEBUGMSG("Splitting");

    newnode.leaf = node->leaf;
    if (!newnode.leaf) countdiff--;

    for (s = ORDER/2 + !newnode.leaf, d=0; d < countdiff; s++, d++) {
      strncpy(newnode.keys[d], keys[s], TREEKEY_SIZE);
      newnode.ptrs[d] = ptrs[s];
    }
    newnode.ptrs[d] = ptrs[s];

    newnode.keycount = countdiff;
    DEBUGMSG("New node ready:");
    DUMPNODE(&newnode);

    DEBUG("Promoting key \"%s\"", keys[ORDER/2]);
    strncpy(*key, keys[ORDER/2], TREEKEY_SIZE);
    DEBUG("Key now: \"%s\"", *key);
    DEBUGMSG("Setting pointer");
    if (!(*ptr = tree_alloc())) {
      DEBUGMSG("Allocation failed");
      return 0;
    }
    DEBUGMSG("Post-alloc");
    if (node->leaf) {
      DEBUGMSG("Adding to linked list");
      /* Add to linked list */
      newnode.ptrs[0] = node->ptrs[0];
      node->ptrs[0] = *ptr;
    }
    DEBUGMSG("Writing new node");
    if ((result=tree_write(*ptr, (tblock*)&newnode))) {
      DEBUG("Problem writing block: %s", strerror(errno));
      return 0;
    }
  }

  return countdiff;
}

/*
 * Recursively insert a pointer into the tree
 */
static int tree_insert_recurse (fileptr root, char **key, fileptr *ptr) {
  tnode node;
  int tmp=0;
  int keyindex, keymatch;

  errno=0;
  if (tree_read(root, (tblock*)&node)) {
    errno=EIO;
    return 0;
  }

  if (node.magic == MAGIC_DATANODE) {
    DEBUGMSG("Starting insert at data node; using subkeys");
    if (!((tdata*)&node)->subkeys) {
      tnode nblock;

      DEBUGMSG("No subkey root; creating one");
      ((tdata*)&node)->subkeys=tree_alloc();
      initTreeNode(&nblock);
      nblock.leaf=1;
      nblock.keycount=0;

      DEBUGMSG("Writing subkey root block");
      if (tree_write(((tdata*)&node)->subkeys, (tblock*)&nblock)) {
        DEBUG("Problem writing subkey root: %s", strerror(errno));
        return 0;
      }
      DEBUGMSG("Linking to subkeys");
      if (tree_write(root, (tblock*)&node)) {
        DEBUG("Problem updating node: %s", strerror(errno));
        return 0;
      }
    }
    root=((tdata*)&node)->subkeys;
    return tree_insert_recurse(root, key, ptr);
  } else if (node.magic != MAGIC_TREENODE) {
    DEBUG("Invalid magic number for block %lu: %lX", root, node.magic);
    errno=EBADF;
    return 0;
  }

  DEBUG("Finding key %s", *key);
  keyindex = tree_find_key(&node, *key);
  keymatch = keyindex && (strncmp(node.keys[keyindex-1], *key, TREEKEY_SIZE)==0);

  if (!node.leaf) {
    DEBUGMSG("Not a leaf; recursing");
    tmp = tree_insert_recurse(node.ptrs[keyindex], key, ptr);
    if (errno) {
      DEBUG("Recursion returned %d and error message \"%s\"; aborting.", tmp, strerror(errno));
      return 0;
    } else {
      DEBUG("Recursion returned %d", tmp);
    }
  }
  if (keymatch) {
    DEBUG("Key \"%s\" already in tree", *key);
    errno=EEXIST;
  } else if (tmp > 0 || node.leaf) {
    DEBUG("Inserting key \"%s\"", *key);
    tmp = tree_insert_key(&node, keyindex, key, ptr);
    DUMPNODE(&node);
    if (tree_write(root, (tblock*)&node)) {
      DEBUG("Problem writing node: %s", strerror(errno));
      return 0;
    }
  }

  return tmp;
}

/*
 * insert a data block into the tree with the given key
 */
fileptr tree_insert (tkey key, tdata *data) {
  DEBUG("Inserting into root tree with key \"%s\"", key);
  return tree_sub_insert(tree_sb->root_index, key, data);
}

/*
 * insert a data block into the tree starting from the given node
 */
fileptr tree_sub_insert(fileptr root, tkey key, tdata *data) {
  fileptr newnode=tree_alloc();
  fileptr ptr;
  fileptr split;
  char *ikey = malloc(sizeof(char)*TREEKEY_SIZE); /* TODO: check for failure */

  DEBUGMSG("Copying key to temp location");
  strncpy(ikey, key, TREEKEY_SIZE);

  DEBUG("Inserting into subtree with key \"%s\"", ikey);
  ptr=newnode;
  split=tree_insert_recurse(root, &ikey, &ptr);
  if (errno) {
    int tmperrno;
    DEBUG("Hit an error (%s) - freeing reallocated block", strerror(errno));
    tmperrno=errno;
    tree_free(newnode);
    free(ikey);
    errno=tmperrno;
    return 0;
  } else {
    if (split) {
      tnode newroot;

      initTreeNode(&newroot);

      DEBUGMSG("Splitting the root");
      newroot.leaf=0;
      newroot.keycount=1;
      strncpy(newroot.keys[0], ikey, TREEKEY_SIZE);
      newroot.ptrs[0] = tree_sb->root_index;
      newroot.ptrs[1] = ptr;
      tree_sb->root_index=tree_alloc();
      tree_write(tree_sb->root_index, (tblock*)&newroot);
      tree_write_sb(tree_sb);
    }
    tree_write(newnode, (tblock*)data);
    free(ikey);
    return newnode;
  }
}

/**
 * Remove a key from the given node.
 *
 * @param[in,out] node     The node from which to remove the key
 * @param[in]     keyindex The (1-based) index of the key to remove
 * @param[in,out] key      The key to remove; updated with new leftmost key if changed
 *
 * @retval 0 Success
 * @retval 1 Success, but this node has underflowed
 * @retval 2 Success, but the leftmost key has changed and so should be updated
 * @retval 3 Success, but the node has underflowed and its leftmost key has changed
 */
static int tree_remove_key(tnode *node, int keyindex, char **key) {
  int k;

  DEBUG("Removing key \"%s\" at index %d", *key, keyindex);
  DUMPNODE(node);

  /* Move keys above keyindex down */
  for(k = keyindex; k<node->keycount; k++) {
    DEBUG("Moving key %d (\"%s\") to %d", k, node->keys[k], k-1);
    strncpy(node->keys[k-1], node->keys[k], TREEKEY_SIZE);
    DEBUG("Moving pointer %d (%lu) to %d", k+1, node->ptrs[k+1], k);
    node->ptrs[k] = node->ptrs[k+1];
  }

  node->keycount--;

  DUMPNODE(node);

  if (keyindex==1) {
    DEBUG("Leftmost key should be updated to \"%s\"", node->keys[0]);
    strncpy(*key, node->keys[0], TREEKEY_SIZE);
  }

  return ((node->keycount < ORDER/2)?1:0) | ((keyindex==1)?2:0);
}

/*
 * Recursively remove a pointer from the tree
 */
static int tree_remove_recurse (fileptr root, char **key, fileptr *ptr) {
  tnode node;
  int tmp=0;
  int keyindex, keymatch;
  fileptr theptr;

  errno=0;
  if (tree_read(root, (tblock*)&node)) {
    errno=EIO;
    return -errno;
  }

  DEBUG("Finding key %s", *key);
  keyindex = tree_find_key(&node, *key);
  keymatch = keyindex && (strncmp(node.keys[keyindex-1], *key, TREEKEY_SIZE)==0);

  if (node.magic == MAGIC_DATANODE) {
    DEBUGMSG("Starting removal at data node; using subkeys");
    if (!((tdata*)&node)->subkeys) {
      DEBUGMSG("No subkey root; can't delete from there!");
      errno=ENOENT;
      return -errno;
    }
    root=((tdata*)&node)->subkeys;
    return tree_remove_recurse(root, key, ptr);
  } else if (node.magic != MAGIC_TREENODE) {
    DEBUG("Invalid magic number for block %lu: %lX", root, node.magic);
    errno=EBADF;
    return -errno;
  }

  if (!node.leaf) {
    DEBUGMSG("Not a leaf; recursing");
    tmp = tree_remove_recurse(node.ptrs[keyindex], key, ptr);
    DEBUG("Recursion returned %d and error message \"%s\"", tmp, strerror(errno));
    if (tmp<0) return tmp;
  }
  if (!keymatch && node.leaf) {
    DEBUG("Key \"%s\" not found in tree", *key);
    errno=ENOENT;
    return -errno;
  } else if (tmp) {
    tnode lsib, rsib;
    tnode dblock;
    int i;
    zero_block(&lsib);
    zero_block(&rsib);
    zero_block(&dblock);

    if (tree_read(node.ptrs[keyindex], (tblock*)&dblock)) {
      return -EIO;
    }

    if (tmp & 1) {
      int leftsteal=0;
      int rightsteal=0;

      /* if (tmp) then node needs to steal from siblings if possible */
      DEBUGMSG("Should steal or merge");
      DUMPNODE(&dblock);
      /* try to steal from left sibling (if its keycount>=ORDER/2), so both have equal number; update keys as required */
      if (keyindex-1 > 0) {
        /* TODO: we have a left sibling */
        tree_read(node.ptrs[keyindex-1], (tblock*)&lsib); /* TODO: check for failure */
        DEBUG("We (%lu) have a left sibling (%lu) of node to merge (%lu) with keycount %d", root, node.ptrs[keyindex-1], node.ptrs[keyindex], lsib.keycount);
        if (lsib.keycount > ORDER/2) {
          DUMPINT(lsib.keycount);
          DUMPINT(dblock.keycount);
          leftsteal = (lsib.keycount - dblock.keycount)/2;
          DUMPINT(leftsteal);
          /* make sure we don't try to steal too many! */
          if ((unsigned int)leftsteal > ORDER-1-dblock.keycount)
            leftsteal = ORDER-1-dblock.keycount;
        }
        DEBUG("We can steal %d key(s) from left sibling.", leftsteal);
      }
      /* try to steal from right sibling (if its keycount>=ORDER/2), so both have equal number; update keys as required */
      /* we have a right sibling if we're a leaf and keyindex <= keycount, or if we're not a leaf and keyindex < keycount */
      if (!leftsteal && ( (node.leaf && keyindex <= node.keycount) || (!node.leaf && keyindex < node.keycount) ) ) {
        /* TODO: we have a right sibling */
        tree_read(node.ptrs[keyindex+1], (tblock*)&rsib); /* TODO: check for failure */
        DEBUG("We (%lu) have a right sibling (%lu) of node to merge (%lu) with keycount %d", root, node.ptrs[keyindex+1], node.ptrs[keyindex], rsib.keycount);
        if (rsib.keycount > ORDER/2) {
          rightsteal = (rsib.keycount - dblock.keycount)/2;
          DUMPINT(rightsteal);
          /* make sure we don't try to steal too many! */
          if ((unsigned int)rightsteal > ORDER-1-dblock.keycount)
            rightsteal = ORDER-1-dblock.keycount;
        }
        DEBUG("We can steal %d key(s) from right sibling.", rightsteal);
      }
      if (leftsteal) {
        /* move dblock keys up by leftsteal */
        for (i=dblock.keycount-1; i>=0; i--) {
          strncpy(dblock.keys[i+leftsteal], dblock.keys[i], TREEKEY_SIZE);
          dblock.ptrs[i+leftsteal+1] = dblock.ptrs[i+1];
        }
        /* copy from lsib to dblock */
        for (i=0; i<leftsteal; i++) {
          strncpy(dblock.keys[i], lsib.keys[lsib.keycount-leftsteal+i], TREEKEY_SIZE);
          dblock.ptrs[i+1] = lsib.ptrs[lsib.keycount-leftsteal+i+1];
        }
        /* reduce lsib's keycount and write it */
        lsib.keycount -= leftsteal;
        tree_write(node.ptrs[keyindex-1], (tblock*)&lsib); /* TODO: check for failure */
        /* increment dblock's keycount and write it */
        dblock.keycount += leftsteal;
        tree_write(node.ptrs[keyindex], (tblock*)&dblock); /* TODO: check for failure */
        /* update dblock's placeholder key */
        strncpy(*key, dblock.keys[0], TREEKEY_SIZE);
        tmp |= 2;
      } else if (rightsteal) {
        DEBUGMSG("Copying rightsib to dblock");
        /* copy from rsib to dblock */
        DUMPNODE(&rsib);
        for (i=0; i<rightsteal; i++) {
          DEBUG("Moving key %d (\"%s\") to dblock[%d]", i, rsib.keys[i], dblock.keycount+i);
          strncpy(dblock.keys[dblock.keycount+i], rsib.keys[i], TREEKEY_SIZE);
          DEBUG("Moving pointer %d (%lu) to dblock[%d]", i+1, rsib.ptrs[i+1], dblock.keycount+i+1);
          dblock.ptrs[dblock.keycount+i+1] = rsib.ptrs[i+1];
        }
        DEBUGMSG("Moving rightsib's keys down");
        /* move rsib's keys down by rightsteal */
        for (i=0; i<rsib.keycount-rightsteal; i++) {
          strncpy(rsib.keys[i], rsib.keys[i+rightsteal], TREEKEY_SIZE);
          rsib.ptrs[i+1] = rsib.ptrs[i+rightsteal+1];
        }
        /* reduce rsib's keycount by rightsteal and save it */
        rsib.keycount -= rightsteal;
        tree_write(node.ptrs[keyindex+1], (tblock*)&rsib); /* TODO: check for failure */
        /* update rsib's placeholder key in this node */
        strncpy(node.keys[keyindex], rsib.keys[0], TREEKEY_SIZE);
        /* increment dblock's keycount by rightsteal */
        dblock.keycount += rightsteal;
        tree_write(node.ptrs[keyindex], (tblock*)&dblock); /* TODO: check for failure */
      } else {
        DEBUGMSG("Cannot steal from siblings; must underflow");
        /* otherwise give as many pointers to left sibling as possible (until
         * it's full), then give rest to right (and update its key) */
        if (lsib.magic)
          leftsteal = ORDER-1-lsib.keycount;
        if (leftsteal>dblock.keycount) leftsteal=dblock.keycount;
        if (rsib.magic)
          rightsteal = ORDER-1-rsib.keycount - leftsteal;
        if (rightsteal<0) rightsteal=0;
        DEBUG("Handing %u keys to left node, %u to right", leftsteal, rightsteal);
        if (!leftsteal && !rightsteal) {
          DEBUGMSG("\033[1;5;31mPANIC AT THE TREE! CAN'T FIND ANY SIBLINGS! ARGH!\033[m");
          return -EIO;
        }
        /* give keys and pointers to the left sibling */
        for (i=0; i<leftsteal; i++) {
          DEBUG("Moving key[%d] \"%s\" to left sibling", i, dblock.keys[i]);
          strncpy(lsib.keys[lsib.keycount+i], dblock.keys[i], TREEKEY_SIZE);
          lsib.ptrs[lsib.keycount+i+1] = dblock.ptrs[i+1];
        }
        lsib.keycount += leftsteal;
        /* move keys and pointers up in right sibling */
        for (i=rsib.keycount-1; i>=0; i--) {
          strncpy(rsib.keys[i+rightsteal], rsib.keys[i], TREEKEY_SIZE);
          rsib.ptrs[i+rightsteal+1] = rsib.ptrs[i+1];
        }
        /* give keys and pointers to the right sibling */
        for (i=0; i<rightsteal; i++) {
          DEBUG("Moving key[%d] \"%s\" to right sibling", leftsteal+i, dblock.keys[leftsteal+i]);
          strncpy(rsib.keys[i], dblock.keys[leftsteal+i], TREEKEY_SIZE);
          rsib.ptrs[i+1] = dblock.ptrs[leftsteal+i+1];
        }
        rsib.keycount += rightsteal;
        /* update rsib's placeholder key in this node */
        strncpy(node.keys[keyindex], rsib.keys[0], TREEKEY_SIZE);
        /* update left sibling linked list entry */
        if (keyindex>0) {
          if (!lsib.magic) {
            tree_read(node.ptrs[keyindex-1], (tblock*)&lsib); /* TODO: check for failure */
          }
          lsib.ptrs[0] = node.ptrs[keyindex+1];
          /* write left sibling */
          tree_write(node.ptrs[keyindex-1], (tblock*)&lsib); /* TODO: check for failure */
        }
        /* write right sibling */
        if (rsib.magic) tree_write(node.ptrs[keyindex+1], (tblock*)&rsib); /* TODO: check for failure */
        /* free dblock */
        tree_free(node.ptrs[keyindex]);
        DUMPNODE(&node);
        /* remove separating key from this node */
        DEBUG("Removing key %d from this node (which has keycount %d)", keyindex, node.keycount);
        for (i=keyindex; i<node.keycount; i++) {
          strncpy(node.keys[i-1], node.keys[i], TREEKEY_SIZE);
          node.ptrs[i] = node.ptrs[i+1];
        }
        node.keycount--;
        tmp &= ~2;
      }
    }
    if (tmp & 2) {
      /* update placeholder key */
      DEBUG("Updating placeholder key for block %lu from \"%s\" to \"%s\"", root, node.keys[keyindex-1], *key);
      strncpy(node.keys[keyindex-1], *key, TREEKEY_SIZE);

      if (keyindex==1) {
        DEBUG("Leftmost key should be updated to \"%s\"", node.keys[0]);
        strncpy(*key, node.keys[0], TREEKEY_SIZE);
      }
    }
    tree_write(root, (tblock*)&node); /* TODO: check for failure */

    /* if current keycount < ORDER/2 then return 1, and if our leftmost key changed, update our key in parent */
    return ((node.keycount < ORDER/2)?1:0) | (((tmp & 2) && keyindex==1)?2:0);
  } else if (node.leaf) {
    tdata dblock;

    DEBUG("Considering removing key \"%s\"", *key);
    theptr = node.ptrs[keyindex];
    if (tree_read(theptr, (tblock*)&dblock)) {
      DEBUGMSG("Problem reading data block");
      return -EIO;
    }
    if (dblock.subkeys) {
      DEBUGMSG("Node has subkeys - must not be deleted!");
      errno=ENOTEMPTY;
      return -ENOTEMPTY;
    }
    if (dblock.inodecount > INODECOUNT) {
      fileptr inodeptr;
      tinode ib;
      DEBUGMSG("Node has inode blocks - must delete those too!");
      /* read last inode pointer of block while( nextptr = ((tinode*)&dblock)->inodes[INODE_MAX-1] ), and free that block */
      for (inodeptr = dblock.next_inodes; inodeptr; inodeptr=ib.next_inodes) {
        if (tree_read(inodeptr, (tblock*)&ib) || tree_free(inodeptr)) {
          DEBUGMSG("Problem reading inode block");
          return -EIO;
        }
      }
    }
    DEBUG("Removing key \"%s\"", *key);
    /* then go through the block and free... */
    tmp = tree_remove_key(&node, keyindex, key); /* TODO: check for failure */
    /* should read the node and check there's nothing attached to it */
    tree_free(theptr); /* TODO: check for failure */
    tree_write(root, (tblock*)&node); /* TODO: check for failure */
  }

  return tmp;
}

/*
 * Remove a node and any associated inode blocks
 */
int tree_remove (tkey key) {
  DEBUG("Removing key \"%s\" from root tree", key);
  return tree_sub_remove(tree_sb->root_index, key);
}

/*
 * Remove a node from the given subtree and any associated inode blocks
 */
int tree_sub_remove(fileptr root, tkey key) {
  fileptr ptr;
  fileptr merge;
  char *ikey = malloc(sizeof(char)*TREEKEY_SIZE); /* TODO: check for failure */

  DEBUG("Removing key \"%s\" from subtree rooted at %lu", key, root);

  DEBUGMSG("Copying key to temp location");
  strncpy(ikey, key, TREEKEY_SIZE);

  DEBUG("Removing \"%s\" from subtree", ikey);
  ptr=0;
  merge=tree_remove_recurse(root, &ikey, &ptr);

  if (errno) {
    DEBUG("Hit an error (%s) - chickening out", strerror(errno));
    free(ikey);
    return -EIO;
  } else {
    tnode node;
    tree_read(root, (tblock*)&node); /* TODO: check for failure */
    if (node.leaf) {
      /* DO NOTHING */
      DEBUGMSG("Root is a leaf; nothing to do, nothing to merge.");
    } else if (!node.keycount) {
      DEBUG("\033[1;31mReplacing root\033[m: new index = %lu", node.ptrs[0]);
      tree_sb->root_index = node.ptrs[0];
      tree_free(root); /* Also calls tree_write_sb() */

#if 0
    } else if (node.keycount==1) {
      tnode lsib, rsib;
      DEBUGMSG("\033[1;34mRoot keycount: 1\033[m; time to replace?");
      /* if both nodes have keycount <= ORDER/2 then merge into left node and update root_index */
      if (node.ptrs[0] && node.ptrs[1]) {
        tree_read(node.ptrs[0], (tblock*)&lsib); /* TODO: check for failure */
        tree_read(node.ptrs[1], (tblock*)&rsib); /* TODO: check for failure */
        if (lsib.keycount < ORDER/2 && rsib.keycount < ORDER/2) {
          int i;
          DEBUGMSG("\033[1;31mMERGING AND REPLACING ROOT\033[m");
          /* merge into left node */
          for (i=0; i<rsib.keycount; i++) {
            DEBUG("Moving key[%d] \"%s\" to left sibling", i, rsib.keys[i]);
            strncpy(lsib.keys[lsib.keycount+i], rsib.keys[i], TREEKEY_SIZE);
            lsib.ptrs[lsib.keycount+i+1] = rsib.ptrs[i+1];
          }
          lsib.keycount += rsib.keycount;
          /* ... and update root */
          tree_sb->root_index = node.ptrs[0];
          tree_free(node.ptrs[1]); /* Also calls tree_write_sb() */
          tree_free(root); /* Also calls tree_write_sb() */
        } else {
          DEBUGMSG("\033[1;34mNo need to merge\033[m");
        }
      } else if (node.ptrs[0]) {
        /* Very unlikely; will this ever happen? */
        tree_sb->root_index = node.ptrs[0];
        tree_free(root); /* Also calls tree_write_sb() */
      } else if (node.ptrs[1]) {
        /* Very unlikely; will this ever happen? */
        tree_sb->root_index = node.ptrs[1];
        tree_free(root); /* Also calls tree_write_sb() */
      } else {
        /* PANIC! */
        DEBUGMSG("\033[1;31mPANIC! AT THE ROOT LEVEL! NO VALID POINTERS!\033[m");
        return -EIO;
      }
#endif
    }

  }
  free(ikey);
  return 0;
}

/**
 * Read superblock from tree storage file.
 *
 * @param super Pointer to a tsblock structure to fill.
 * @retval 0 Success.
 * @retval EBADF Superblock read but has invalid magic number.
 * @retval (other) See tree_read()
 * @sa tree_read(), tree_write_sb()
 */
int tree_read_sb (tsblock *super) {
  int res=tree_read(0, (tblock*)super);
  if (res) return res;
  if (super->magic != MAGIC_SUPERBLOCK) return EBADF;

  return 0;
}

/**
 * Write superblock to tree storage file.
 *
 * @param super Pointer to superblock structure to be written.
 * @retval 0 Success.
 * @retval EINVAL \a super is a null pointer.
 * @retval EBADF \a super has an invalid number and does not appear to be a superblock.
 * @retval (other) See tree_write()
 * @sa tree_write(), tree_read_sb()
 */
int tree_write_sb (tsblock *super) {
  if (!super) return EINVAL;
  if (super->magic != MAGIC_SUPERBLOCK) return EBADF;
  return tree_write(0, (tblock*)super);
}


/****************************************************************************/


#if 0


/*****************************************************************************
 *  tree_destroy_data
 * --------------------------------------------------------------------------
 *    bplusdata ** node - a pointer to the data node to destroy
 * --------------------------------------------------------------------------
 *  Destroys a data node, including its subkeys tree if applicable, and
 *  freeing its inode linked list. Returns FALSE if the node has not been
 *  completely destroyed for any reason.
 ****************************************************************************/
int tree_destroy_data(bplusdata** node) {
  bplusdata *n = *node;

  if (!n) return TRUE;

  printf("Destroying data node %p with inode count %d\n", n, n->inodecount);
  tree_dump_data(n, 0);

  if (n->inodecount > INODECOUNT) {
#ifdef _DEBUG
    fprintf(stderr, COL_YELLOW "Asked to destroy a node with external inodes...\n" COL_RESET);
#endif
    return FALSE;
  }

  if (n->subkeys) {
#ifdef _DEBUG
    fprintf(stderr, COL_YELLOW "Asked to destroy a node with subkeys...\n" COL_RESET);
#endif
    return FALSE;
  }

  /* Actually free it */
  free(n);
  *node = NULL;
  return TRUE;
}


/*****************************************************************************
 *  tree_destroy_node
 * --------------------------------------------------------------------------
 *    bplusnode ** tree - a pointer to the root node of the tree
 * --------------------------------------------------------------------------
 *  Recurses through the tree, destroying and freeing all child data and tree
 *  nodes. Returns FALSE if the tree has not been completely destroyed.
 ****************************************************************************/
int tree_destroy_node(bplusnode** tree) {
  bplusnode *t = *tree;
  int i;

  if (!t) return TRUE;

  printf("Destroying node with %d keys...\n", t->keycount);

  if (t->keycount) {
    for (i=0; i<t->keycount+1; i++) {
      if (i<t->keycount) printf("Key: %s\n", t->keys[i]);
      if (t->ptrs[i].n) {
        if (t->leaf) {
          if (!tree_destroy_data(&t->ptrs[i].d)) return FALSE;
        } else {
          if (!tree_destroy_node(&t->ptrs[i].n)) return FALSE;
        }
      }
    }
  }

  /* Actually free it */
  free(t);
  *tree = NULL;
  return TRUE;
}

#endif
