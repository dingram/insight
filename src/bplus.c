#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#if defined(_DEBUG_BPLUS) && !defined(_DEBUG)
#define _DEBUG
#endif
#include "bplus.h"
#include "bplus_debug.h"
#include "bplus_priv.h"
#include <set_ops.h>
#include <insight.h>

void tree_dump_dot_item(FILE *target, fileptr root) {
  tblock block;

  tree_read(root, &block);
  switch (block.magic) {
    case MAGIC_SUPERBLOCK:
      {
        tsblock *node = (tsblock*)&block;
        fprintf(target, "id%lu [label=\"SUPERBLOCK\\n" "v%d.%d\\n" "max_size: %lu\\n" "limbo count: %lu\", shape=\"box\", penwidth=3.0, style=\"filled\", fillcolor=\"#00ee33\"];\n", root, node->version>>8, node->version & 0xff, node->max_size, node->limbo_count);
        if (node->root_index) {
          fprintf(target, "id%lu -> id%lu [label=\"root\"];\n", root, node->root_index);
          fprintf(target, "  subgraph cluster_rootgraph {\n");
          tree_dump_dot_item(target, node->root_index);
          fprintf(target, "  }\n");
        }
        if (node->inode_limbo) {
          fprintf(target, "id%lu -> id%lu [label=\"limbo\", constraint=\"false\"];\n", root, node->inode_limbo);
          fprintf(target, "  subgraph cluster_limbo {\n");
          tree_dump_dot_item(target, node->inode_limbo);
          fprintf(target, "  }\n");
        }
        if (node->inode_root) {
          fprintf(target, "id%lu -> id%lu [label=\"inode\"];\n", root, node->inode_root);
          fprintf(target, "  subgraph cluster_inodetree {\n");
          tree_dump_dot_item(target, node->inode_root);
          fprintf(target, "  }\n");
        }
      }
      break;
    case MAGIC_TREENODE:
      {
        tnode *node = (tnode*)&block;
        unsigned int i;
        fprintf(target, "id%lu [label=\"NODE %lu\\n" "keycount: %d\", shape=\"box\", style=\"filled\"", root, root, node->keycount);
        if (node->leaf) {
          fprintf(target, ", fillcolor=\"#ccccff\"");
        } else {
          fprintf(target, ", fillcolor=\"#9999ff\"");
        }
        fprintf(target, "];\n");
        if (node->keycount && !node->leaf) {
          fprintf(target, "id%lu -> id%lu [label=\"<%s\"];\n", root, node->ptrs[0], node->keys[0]);
          tree_dump_dot_item(target, node->ptrs[0]);
        }
        for(i=0;i<node->keycount;i++) {
          fprintf(target, "id%lu -> id%lu [label=\"%s\"];\n", root, node->ptrs[i+1], node->keys[i]);
          tree_dump_dot_item(target, node->ptrs[i+1]);
        }
        if (node->leaf && node->ptrs[0]) {
          fprintf(target, "id%lu -> id%lu [color=\"#ff7700\", constraint=false];\n", root, node->ptrs[0]);
        }
      }
      break;
    case MAGIC_DATANODE:
      {
        tdata *node = (tdata*)&block;
        fprintf(target, "id%lu [label=\"DATA\\n" "name: %s\\n" "inodecount: %d\\n" "flags: %d\\n" "parent: %lu\", shape=\"box\", style=\"filled\", fillcolor=\"#ccff66\"];\n", root, node->name, node->inodecount, node->flags, node->parent);
        if (node->subkeys) {
          fprintf(target, "id%lu -> id%lu [label=\"subkeys\", color=\"#cc0000\"];\n", root, node->subkeys);
          tree_dump_dot_item(target, node->subkeys);
        }
#if 0
        fprintf(target, " inodes:");
        unsigned int i;
        for(i=0;i<DATA_INODE_MAX;i++) fprintf(target, (i>=node->inodecount?" [\033[4m%08lX\033[m]":" [%08lX]"), node->inodes[i]);
        fprintf(target, "\n");
        fprintf(target, " next_inodes: %lu\n", node->next_inodes);
#endif
        if (node->next_inodes) {
          fprintf(target, "id%lu -> id%lu [label=\"next_inodes\", color=\"#cc0000\", constraint=\"false\"];\n", root, node->next_inodes);
          tree_dump_dot_item(target, node->next_inodes);
        }
      }
      break;
    case MAGIC_FREEBLOCK:
      {
        // NOTE: does not link to next free (as we could then get EVERYTHING in the graph, which would be awkward)
        freeblock *node = (freeblock*)&block;
        fprintf(target, "id%lu [label=\"FREE\\n" "next: %lu\", style=\"filled\", fillcolor=\"#00cc00\"];\n", root, node->next);
      }
      break;
    case MAGIC_INODEBLOCK:
      {
        tinode *node = (tinode*)&block;
        fprintf(target, "id%lu [label=\"INODE\\n" "inodecount: %d\", shape=\"box\", style=\"filled\", fillcolor=\"#cc66ff\"];\n", root, node->inodecount);
#if 0
        fprintf(target, " inodes:");
        unsigned int i;
        for(i=0;i<INODE_MAX;i++) fprintf(target, (i>=node->inodecount?" [\033[4m%08lX\033[m]":" [%08lX]"), node->inodes[i]);
        fprintf(target, "\n");
        fprintf(target, " next_inodes: %lu\n", node->next_inodes);
#endif
        if (node->next_inodes) {
          fprintf(target, "id%lu -> id%lu [label=\"next_inodes\", color=\"#cc0000\", constraint=\"false\"];\n", root, node->next_inodes);
          tree_dump_dot_item(target, node->next_inodes);
        }
      }
      break;
    case MAGIC_INODEDATA:
      {
        tidata *node = (tidata*)&block;
        fprintf(target, "id%lu [label=\"INODEDATA\\n" "refcount: %d\", shape=\"box\", style=\"filled\", fillcolor=\"#ffcc66\"];\n", root, node->refcount);
#if 0
        fprintf(target, " refcount: %d\n", node->refcount);
        fprintf(target, " refs:");
        unsigned int i;
        for(i=0;i<REF_MAX;i++) fprintf(target, (i>=node->refcount?" [\033[4m%08lX\033[m]":" [%08lX]"), node->refs[i]);
        fprintf(target, "\n");
#endif
      }
      break;
  }
}

void tree_dump_dot(FILE *target, fileptr root) {
  fprintf(target, "digraph btree {\n");
  fprintf(target, "pad=0.3;\n");
  tree_dump_dot_item(target, root);
  fprintf(target, "}\n");
}

void tree_dump_tree(FILE *target, fileptr root, int indent) {
  tblock block;
  char ind[indent+1];
  memset(ind, ' ', indent);
  ind[indent]='\0';

  tree_read(root, &block);
  switch (block.magic) {
    case MAGIC_SUPERBLOCK:
      {
        tsblock *node = (tsblock*)&block;
        fprintf(target, "%s [%lu:SUPERBLOCK]\n", ind, root);
        fprintf(target, "%s  version:     %d.%d\n", ind, node->version>>8, node->version & 0xff);
        fprintf(target, "%s  root index:  %lu\n", ind, node->root_index);
        if (node->root_index) {
          tree_dump_tree(target, node->root_index, indent+2);
        }
        fprintf(target, "%s  max size:    %lu\n", ind, node->max_size);
        fprintf(target, "%s  free head:   %lu\n", ind, node->free_head);
        fprintf(target, "%s  limbo inode: %lu\n", ind, node->inode_limbo);
        if (node->inode_limbo) {
          tree_dump_tree(target, node->inode_limbo, indent+2);
        }
        fprintf(target, "%s  limbo count: %lu\n", ind, node->limbo_count);
        fprintf(target, "%s  inode root:  %lu\n", ind, node->inode_root);
        if (node->inode_root) {
          tree_dump_tree(target, node->inode_root, indent+2);
        }
      }
      break;
    case MAGIC_TREENODE:
      {
        tnode *node = (tnode*)&block;
        unsigned int i;
        fprintf(target, "%s [%lu:TREE NODE]\n", ind, root);
        fprintf(target, "%s  leaf:     %d\n", ind, node->leaf);
        fprintf(target, "%s  keycount: %d\n", ind, node->keycount);
        fprintf(target, "%s  ptr[0]: %lu\n", ind, node->ptrs[0]);
        fprintf(target, "%s  keys:\n", ind);
        for(i=0;i<ORDER-1;i++) {
          if (i < node->keycount) {
            fprintf(target, "%s \"%s\"-->[%lu]\n", ind, node->keys[i], node->ptrs[i+1]);
            tree_dump_tree(target, node->ptrs[i+1], indent+2);
          } else {
            fprintf(target, "%s \"\033[4m%s\033[m\"\n", ind, node->keys[i]);
          }
        }
      }
    case MAGIC_DATANODE:
      {
        tdata *node = (tdata*)&block;
        unsigned int i;
        fprintf(target, "%s [%lu:DATA NODE]\n", ind, root);
        fprintf(target, "%s  inodecount:     %d\n", ind, node->inodecount);
        fprintf(target, "%s  flags: %x\n", ind, node->flags);
        fprintf(target, "%s  subkeys: %lu\n", ind, node->subkeys);
        if (node->subkeys) {
          tree_dump_tree(target, node->subkeys, indent+2);
        }
        fprintf(target, "%s  inodes:", ind);
        for(i=0;i<DATA_INODE_MAX;i++) fprintf(target, (i>=node->inodecount?" [\033[4m%08lX\033[m]":" [%08lX]"), node->inodes[i]);
        fprintf(target, "\n");
        fprintf(target, "%s  name: \"%s\"\n", ind, node->name);
        fprintf(target, "%s  parent: %lu\n", ind, node->parent);
        fprintf(target, "%s  next_inodes: %lu\n", ind, node->next_inodes);
        if (node->next_inodes) {
          tree_dump_tree(target, node->next_inodes, indent+2);
        }
      }
      break;
    case MAGIC_FREEBLOCK:
      {
        freeblock *node = (freeblock*)&block;
        fprintf(target, "%s [%lu:FREE BLOCK]\n", ind, root);
        fprintf(target, "%s  next free block: %lu\n", ind, node->next);
      }
      break;
    case MAGIC_INODEBLOCK:
      {
        tinode *node = (tinode*)&block;
        unsigned int i;
        fprintf(target, "%s [%lu:INODE BLOCK]\n", ind, root);
        fprintf(target, "%s  inodecount:     %d\n", ind, node->inodecount);
        fprintf(target, "%s  inodes:", ind);
        for(i=0;i<INODE_MAX;i++) fprintf(target, (i>=node->inodecount?" [\033[4m%08lX\033[m]":" [%08lX]"), node->inodes[i]);
        fprintf(target, "\n");
        fprintf(target, "%s  next_inodes: %lu\n", ind, node->next_inodes);
        if (node->next_inodes) {
          tree_dump_tree(target, node->next_inodes, indent+2);
        }
      }
      break;
    case MAGIC_INODEDATA:
      {
        tidata *node = (tidata*)&block;
        unsigned int i;
        fprintf(target, "%s [%lu:INODE DATA BLOCK]\n", ind, root);
        fprintf(target, "%s  refcount: %d\n", ind, node->refcount);
        fprintf(target, "%s  refs:", ind);
        for(i=0;i<REF_MAX;i++) fprintf(target, (i>=node->refcount?" [\033[4m%08lX\033[m]":" [%08lX]"), node->refs[i]);
        fprintf(target, "\n");
      }
      break;
  }
}

/**
 * Format the tree file, initialising the superblock and free space.
 *
 * @retval 0 Success.
 * @retval ENOMEM Could not allocate space for tree superblock.
 * @retval (other) See tree_write().
 * @sa tree_write(), tree_format_free(), tree_open()
 */
static int tree_format () {
  tnode root;
  int result;

  DEBUG("Entered tree_format()");

  if (tree_sb) free(tree_sb);
  tree_sb=malloc(sizeof(tsblock));
  if (!tree_sb) {
    PMSG(LOG_ERR, "Failed to allocate memory for superblock");
    return ENOMEM;
  }

  DEBUG("Initialising structures");
  zero_block(tree_sb);
  initTreeNode(&root);

  DEBUG("Setting up superblock structure");
  tree_sb->magic=MAGIC_SUPERBLOCK;
  tree_sb->version=TREE_FILE_VERSION;
  tree_sb->root_index=1;
  tree_sb->inode_root=2;
  tree_sb->max_size=DEFAULT_BLOCKS;
  tree_sb->free_head=0;

  DEBUG("Setting up root structure");
  root.leaf=1;
  root.keycount=0;

  DEBUG("Writing root blocks");
  if ((result=tree_write(1, (tblock*)&root))) {
    PMSG(LOG_ERR, "Problem writing tree root: %s", strerror(errno));
    return errno;
  }
  if ((result=tree_write(2, (tblock*)&root))) {
    PMSG(LOG_ERR, "Problem writing inode root: %s", strerror(errno));
    return errno;
  }

  /* now let's initialise some space - this also writes the superblock */
  return tree_format_free(3, DEFAULT_BLOCKS - 2); /* Remove two to account for the root blocks */
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

  DEBUG("Zeroing free block structure");
  initFreeNode(&f);

  DEBUG("Seeking to start");
  /* NOTE: not using tree_write() here because that would be seek overkill */
  if (lseek(tree_fp, start * TREEBLOCK_SIZE, SEEK_SET)<0) {
    PMSG(LOG_ERR, "Seek failed: %s", strerror(errno));
    return EIO;
  }
  DEBUG("Writing free blocks");
  for (i=0; i < size - 1; i++) {
    f.next = i + start + 1;
    if (write(tree_fp, &f, TREEBLOCK_SIZE)<0) {
      PMSG(LOG_ERR, "Write failed: %s", strerror(errno));
      return EIO;
    }
  }
  DEBUG("Writing last free block");
  f.next = tree_sb->free_head;
  if (write(tree_fp, &f, TREEBLOCK_SIZE)<0) {
    PMSG(LOG_ERR, "Final write failed: %s", strerror(errno));
    return EIO;
  }

  DEBUG("Updating and writing superblock");
  tree_sb->free_head = start;
  if ((result=tree_write_sb(tree_sb))) {
    PMSG(LOG_ERR, "Problem writing superblock: %s", strerror(errno));
    if (result!=EIO) errno=result;
    return errno;
  }

  DEBUG("Complete");
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

  DEBUG("Allocating a block");
  errno=0;
  if (!tree_sb->free_head) {
    PMSG(LOG_ERR, "But we don't have any free");
    errno=ENOMEM;
    return 0;
  }
  h = tree_sb->free_head;
  DEBUG("Free block %lu obtained", h);
  if (tree_read(h, (tblock *)&t)) {
    PMSG(LOG_ERR, "Problem reading free block");
    return 0;
  }
  if (t.magic != MAGIC_FREEBLOCK) {
    PMSG(LOG_ERR, "Invalid magic number (%lX)", t.magic);
    errno=EBADF;
    return 0;
  }
  DEBUG("Updating next free block to %lu", t.next);
  tree_sb->free_head = t.next;
  if ((result=tree_write_sb(tree_sb))) {
    PMSG(LOG_ERR, "Problem writing superblock: %s", strerror(errno));
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
    PMSG(LOG_ERR, "Tried to free zero block!");
    return EINVAL;
  }
  DEBUG("Freeing block %lu", block);
  initFreeNode(&t);
  DEBUG("  Adding block to free list (previous head %lu)", tree_sb->free_head);
  t.next = tree_sb->free_head;
  if ((result=tree_write(block, (tblock*)&t))) {
    PMSG(LOG_ERR, "Problem writing block: %s", strerror(errno));
    return errno;
  }
  tree_sb->free_head = block;
  if ((result=tree_write_sb(tree_sb))) {
    PMSG(LOG_ERR, "Problem writing superblock: %s", strerror(errno));
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

  /* sanity checks */
#define BLOCK_TYPE_CHECK(t) do { if (sizeof(t)!=TREEBLOCK_SIZE) { FMSG(LOG_ERR, "Assert failed: %s is %u bytes; should be %u bytes", #t, sizeof(t), TREEBLOCK_SIZE); return EFAULT; } } while (0)
  BLOCK_TYPE_CHECK(tblock);
  BLOCK_TYPE_CHECK(freeblock);
  BLOCK_TYPE_CHECK(tsblock);
  BLOCK_TYPE_CHECK(tnode);
  BLOCK_TYPE_CHECK(tdata);
  BLOCK_TYPE_CHECK(tinode);
  BLOCK_TYPE_CHECK(tidata);
#undef BLOCK_TYPE_CHECK

  if (tree_fp >= 0) {
    FMSG(LOG_ERR, "Tree already open");
    return EMFILE; /* Too many open files */

  } else if ( (tree_fp = open(path, O_RDWR)) >= 0) {
    DEBUG("Opened tree...");
    tsblock *superb = malloc(sizeof(tsblock));
    if (!superb) return ENOMEM;

    DEBUG("Reading superblock");
    /* read the superblock from the index */
    if ( (result=tree_read_sb(superb)) ) {
      PMSG(LOG_ERR, "Superblock error");
      tree_close();
      return result; /* I/O error */
    } else {
      DEBUG("Superblock OK");
      if (tree_sb) free(tree_sb);
      tree_sb = superb;
      FMSG(LOG_INFO, "Superblock version %d.%d", (tree_sb->version>>8), (tree_sb->version & 0xff));
#ifdef TREE_STATS_ENABLED
      tree_stats = malloc((tree_sb->max_size+1) * sizeof(stats_ent)); /* TODO: check for failure */
      zero_mem(tree_stats, (tree_sb->max_size+1) * sizeof(stats_ent));
#endif
      /* tree last modified time */
      struct stat s;
      /* XXX: really should not fail */
      fstat(tree_fp, &s);
      last_modified = s.st_mtime;
#ifdef TREE_CACHE_ENABLED
      return tree_cache_init();
#else
      return 0;
#endif
    }

  } else if ( (tree_fp = open(path, O_RDWR|O_CREAT, 0644)) >= 0) {
    /* created the file - let's initialise it */
    DEBUG("Creating and formatting tree...");
#ifdef TREE_STATS_ENABLED
    DEBUG("Initialising stats");
    tree_stats = malloc((DEFAULT_BLOCKS+1) * sizeof(stats_ent)); /* TODO: check for failure */
    DEBUG("Malloc done");
    zero_mem(tree_stats, (DEFAULT_BLOCKS+1) * sizeof(stats_ent));
    DEBUG("Stats initialised");
#endif
    _tree_touch();
    DEBUG("Calling tree_format()");
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
  DEBUG("Closing tree");
  if (tree_fp >= 0) {
#ifdef TREE_CACHE_ENABLED
    if ((errno=tree_cache_drop())) {
      PMSG(LOG_ERR, "Failed to drop cache: %s", strerror(errno));
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
      unsigned long long total_reads=0, total_writes=0;
#ifdef TREE_CACHE_ENABLED
      unsigned long long total_cache_reads=0, total_cache_writes=0;
#endif

      DEBUG("Dumping statistics");
      for (i=0; i<tree_sb->max_size+1; i++) {
#ifdef TREE_CACHE_ENABLED
        if (tree_stats[i].reads || tree_stats[i].writes || tree_stats[i].cache_reads || tree_stats[i].cache_writes) {
          DEBUG("Stats[%4u]: %5llu reads; %5llu writes; %5llu cache reads; %5llu cache writes", i, tree_stats[i].reads, tree_stats[i].writes, tree_stats[i].cache_reads, tree_stats[i].cache_writes);
          total_reads        += tree_stats[i].reads;
          total_writes       += tree_stats[i].writes;
          total_cache_reads  += tree_stats[i].cache_reads;
          total_cache_writes += tree_stats[i].cache_writes;
        }
#else
        if (tree_stats[i].reads || tree_stats[i].writes) {
          DEBUG("Stats[%4u]: %5llu reads; %5llu writes", i, tree_stats[i].reads, tree_stats[i].writes);
          total_reads        += tree_stats[i].reads;
          total_writes       += tree_stats[i].writes;
        }
#endif
      }
#ifdef TREE_CACHE_ENABLED
      DEBUG("-----------------------------------------------------------------------------");
      FMSG(LOG_INFO, "Stats TOTAL: %5llu reads; %5llu writes; %5llu cache reads; %5llu cache writes", total_reads, total_writes, total_cache_reads, total_cache_writes);
#else
      DEBUG("--------------------------------------");
      FMSG(LOG_INFO, "Stats TOTAL: %5llu reads; %5llu writes", total_reads, total_writes);
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
    PMSG(LOG_WARNING, "Tried to shrink metadata store from %lu blocks to %lu.", tree_sb->max_size, newsize);
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
    PMSG(LOG_ERR, "Data is a null pointer!");
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
    PMSG(LOG_ERR, "tree_read(block: %lu, data: %p)", block, data);
    PMSG(LOG_ERR, "Seek failed: %s", strerror(errno));
    return EIO;
  }
  if (read(tree_fp, data, TREEBLOCK_SIZE)<0) {
    PMSG(LOG_ERR, "Read failed: %s", strerror(errno));
    return EIO;
  }
  DEBUG("Read block %lu", block);
  DUMPBLOCK((tblock*)data);
#ifdef TREE_CACHE_ENABLED
  /* Save to cache */
  if (block_cache && (errno=tree_cache_put(block, (tblock*)data))) {
    PMSG(LOG_ERR, "Failed to write to cache: %s", strerror(errno));
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
    PMSG(LOG_ERR, "Tried to superblock but it doesn't look like one! Has magic %lX instead.", data->magic);
    return EBADF;
  }
  if (lseek(tree_fp, block * TREEBLOCK_SIZE, SEEK_SET)<0) {
    PMSG(LOG_ERR, "Seek failed for block %lu: %s", block, strerror(errno));
    return EIO;
  }
  if (write(tree_fp, data, TREEBLOCK_SIZE)<0) {
    PMSG(LOG_ERR, "Write failed for block %lu: %s", block, strerror(errno));
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
  if (!data) {
    PMSG(LOG_ERR, "Cannot write null data");
    return EINVAL;
  }
#ifdef TREE_CACHE_ENABLED
  if (block_cache && (errno=tree_cache_put(block, (tblock*)data))) {
    PMSG(LOG_ERR, "Failed to write to cache: %s", strerror(errno));
    return ENOMEM;
  } else if (!block_cache) {
    _tree_touch();
    /* must write if no cache available! */
    return _tree_write(block, data);
  }
  _tree_touch();
  return 0;
#else
  _tree_touch();
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
    PMSG(LOG_ERR, "Cache already allocated");
    return EEXIST;
  }
  block_cache = calloc(CACHE_COUNT, sizeof(cache_ent));
  if (!block_cache) {
    PMSG(LOG_ERR, "Failed to allocate cache");
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
  unsigned int i;
  if (!block_cache) {
    PMSG(LOG_ERR, "Cache not allocated; cannot flush");
    return ENOENT;
  }
  DEBUG("Flushing cache to disk...");
  if (tree_sb) {
    DEBUG("Flushing superblock to disk...");
    if (_tree_write(0, (tblock*)tree_sb)) {
      PMSG(LOG_ERR, "I/O error writing superblock");
      return EIO;
    }
  }
  for (i=0; i<CACHE_COUNT; i++) {
    /* if this entry has an address and it is dirty, write to disk */
    if (block_cache[i].addr && block_cache[i].writecount) {
      DEBUG("Flushing cache entry %d to block %lu", i, block_cache[i].addr);
      if (_tree_write(block_cache[i].addr, &block_cache[i].data)) {
        PMSG(LOG_ERR, "I/O error");
        return EIO;
      }
      if (clear) zero_mem(&block_cache[i], sizeof(cache_ent));
    }
  }
  DEBUG("Cache flushed");
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
    PMSG(LOG_ERR, "Cache not allocated; cannot free");
    return ENOENT;
  }
  if (tree_cache_flush(0)) {
    PMSG(LOG_ERR, "Failed to flush cache to disk - aborting");
    return EIO;
  }
  free(block_cache);
  DEBUG("Cache freed");
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
    PMSG(LOG_WARNING, "Cannot cache superblock as it hasn't been allocated yet.");
    return 0;
  } else if (!block) {
    DEBUG("Trying to put superblock into cache; doing nothing");
#ifdef TREE_STATS_ENABLED
    if (tree_stats) tree_stats[block].cache_writes++;
#endif
    return 0;
  }
  if (!block_cache) {
    PMSG(LOG_ERR, "Cache not initialised");
    return ENOBUFS;
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
    PMSG(LOG_ERR, "Should never get here.");
    return ENOTSUP;
  }
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
static int tree_find_key(tnode *node, const char *key) {
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
#if defined(_DEBUG_TREE_SUB_KEY_COUNT) && !defined(_DEBUG)
#define _DEBUG_ONCE
#define _DEBUG
#include <debug.h>
#endif
int tree_sub_key_count(fileptr root) {
  tnode node;
  int total=0;

  errno=0;
  if (tree_sub_get_min(root, &node))
    return -EIO;
  DEBUG("Leaf node of tree root %lu with minimal key", root);

  while (1) {
    DEBUG("Total to now: %d; this node keycount: %d", total, node.keycount);
    total += node.keycount;
    DEBUG("Next node: %lu", node.ptrs[0]);
    if (!node.ptrs[0]) {
      break;
    } else if (node.ptrs[0] && tree_read(node.ptrs[0], (tblock*) &node)) {
      DEBUG("Could not read next pointer");
      return -EIO;
    }
  }
  DEBUG("Final total: %d", total);

  return total;
}
#if defined(_DEBUG_TREE_SUB_KEY_COUNT) && defined(_DEBUG_ONCE)
#undef _DEBUG_ONCE
#undef _DEBUG
#include <debug.h>
#endif

/**
 * Set the tree last modified time to the current time.
 */
static inline void _tree_touch() {
  last_modified=time(NULL);
}

/**
 * Get the tree last modified time.
 *
 * @returns The last modified time of the tree, in seconds since the epoch.
 */
time_t tree_get_mtime() {
  return last_modified;
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
 * Get the inode tree root index.
 *
 * @returns The index of the root of the inode tree, as given by the
 * superblock.
 */
fileptr tree_get_iroot() {
  return tree_sb->inode_root;
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
  if (!root) {
    DEBUG("Root is zero -- using main root");
    return tree_sub_get_min(tree_sb->root_index, node);
  }

  if (tree_read(root, (tblock*)node)) {
    PMSG(LOG_ERR, "Error reading block %lu", root);
    return EIO;
  }

  /* check node is cell block */
  if (node->magic == MAGIC_DATANODE) {
    DEBUG("Starting search at data node; using subkeys");
    root=((tdata*)node)->subkeys;
    if (root) {
      return tree_sub_get_min(root, node);
    } else {
      return ENOENT;
    }
  } else if (node->magic != MAGIC_TREENODE) {
    PMSG(LOG_ERR, "Invalid magic number (%lX) for block %lu", node->magic, root);
    return EBADF;
  }

  while (!node->leaf) {
    root=node->ptrs[0];

    if (tree_read(root, (tblock*)node)) {
      PMSG(LOG_ERR, "Error reading block %lu", root);
      return EIO;
    }
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
int tree_get(const char *key, tblock *node) {
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
int tree_sub_get(fileptr root, const char *key, tblock *node) {
  fileptr dblock = tree_sub_search(root, key);

  if (!dblock) return ENOENT;
  if (tree_read(dblock, node))
    return EIO;
  return 0;
}

/**
 * Search for given key and return data block index associated with it.
 *
 * @param key The key to used for the search.
 * @returns See tree_sub_search()
 */
fileptr tree_search(const char *key) {
  return tree_sub_search(tree_sb->root_index, key);
}

/*
 * search tree with given root for given key and return data block index
 */
fileptr tree_sub_search(fileptr root, const char *key) {
  tnode node;
  int index;

  errno=0;
  DEBUG("Searching for \"%s\" from block %lu", key, root);
  if (!root) {
    PMSG(LOG_ERR, "Root is zero; starting again at root");
    return tree_sub_search(tree_sb->root_index, key);
  }
  index=0;
  for(;;) {
    if (tree_read(root, (tblock*)&node)) {
      errno=EIO;
      return 0;
    }
    /* check node is cell block */
    if (node.magic == MAGIC_DATANODE) {
      DEBUG("Starting search at data node; using subkeys");
      root=((tdata*)&node)->subkeys;
      if (root) {
        return tree_sub_search(root, key);
      } else {
        errno=ENOENT;
        return 0;
      }
    } else if (node.magic != MAGIC_TREENODE) {
      PMSG(LOG_ERR, "tree_sub_search(%lu, \"%s\")", root, key);
      PMSG(LOG_ERR, "Invalid magic number (%lX)", node.magic);
      errno=EBADF;
      return 0;
    }

    DEBUG("Checking node at block %lu\n", root);
    //DUMPNODE(&node);

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

  DEBUG("Copying keys to insertion point");
  /* Copy keys from halfway up to insertion point in case we must split */
	for(k = (unsigned int)(ORDER/2); k < keyindex; k++) {
		strncpy(keys[k], node->keys[k], TREEKEY_SIZE);
		ptrs[k+1] = node->ptrs[k+1];
	}
  DEBUG("Inserting new key \"%s\"", *key);
  /* Insert new key in temporary array */
  strncpy(keys[keyindex], *key, TREEKEY_SIZE);
  ptrs[keyindex+1] = *ptr;

  DEBUG("Copying remaining keys to temp array");
  /* Copy remaining keys to temporary array */
	for(k = keyindex; k < node->keycount; k++) {
		strncpy(keys[k+1], node->keys[k], TREEKEY_SIZE);
		ptrs[k+2] = node->ptrs[k+1];
	}
  DEBUG("Copying items back to node");
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
    DEBUG("Splitting");

    newnode.leaf = node->leaf;
    if (!newnode.leaf) countdiff--;

    for (s = ORDER/2 + !newnode.leaf, d=0; d < countdiff; s++, d++) {
      strncpy(newnode.keys[d], keys[s], TREEKEY_SIZE);
      newnode.ptrs[d] = ptrs[s];
    }
    newnode.ptrs[d] = ptrs[s];

    newnode.keycount = countdiff;
    DEBUG("New node ready:");
    DUMPNODE(&newnode);

    DEBUG("Promoting key \"%s\"", keys[ORDER/2]);
    strncpy(*key, keys[ORDER/2], TREEKEY_SIZE);
    DEBUG("Key now: \"%s\"", *key);
    DEBUG("Setting pointer");
    if (!(*ptr = tree_alloc())) {
      PMSG(LOG_ERR, "Allocation failed");
      return 0;
    }
    DEBUG("Post-alloc");
    if (node->leaf) {
      DEBUG("Adding to linked list");
      /* Add to linked list */
      newnode.ptrs[0] = node->ptrs[0];
      node->ptrs[0] = *ptr;
    }
    DEBUG("Writing new node");
    if ((result=tree_write(*ptr, (tblock*)&newnode))) {
      PMSG(LOG_ERR, "Problem writing block: %s", strerror(errno));
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
    DEBUG("Starting insert at data node; using subkeys");
    if (!((tdata*)&node)->subkeys) {
      tnode nblock;

      DEBUG("No subkey root; creating one");
      ((tdata*)&node)->subkeys=tree_alloc();
      initTreeNode(&nblock);
      nblock.leaf=1;
      nblock.keycount=0;

      DEBUG("Writing subkey root block");
      if (tree_write(((tdata*)&node)->subkeys, (tblock*)&nblock)) {
        PMSG(LOG_ERR, "Problem writing subkey root: %s", strerror(errno));
        return 0;
      }
      DEBUG("Linking to subkeys");
      if (tree_write(root, (tblock*)&node)) {
        PMSG(LOG_ERR, "Problem updating node: %s", strerror(errno));
        return 0;
      }
    }
    root=((tdata*)&node)->subkeys;
    return tree_insert_recurse(root, key, ptr);
  } else if (node.magic != MAGIC_TREENODE) {
    PMSG(LOG_ERR, "Invalid magic number for block %lu: %lX", root, node.magic);
    errno=EBADF;
    return 0;
  }

  DEBUG("Finding key %s", *key);
  keyindex = tree_find_key(&node, *key);
  keymatch = keyindex && (strncmp(node.keys[keyindex-1], *key, TREEKEY_SIZE)==0);

  if (!node.leaf) {
    DEBUG("Not a leaf; recursing");
    tmp = tree_insert_recurse(node.ptrs[keyindex], key, ptr);
    if (errno) {
      PMSG(LOG_ERR, "Recursion returned %d and error message \"%s\"; aborting.", tmp, strerror(errno));
      return 0;
    } else {
      DEBUG("Recursion returned %d", tmp);
    }
  }
  if (keymatch) {
    PMSG(LOG_WARNING, "Key \"%s\" already in tree", *key);
    errno=EEXIST;
  } else if (tmp > 0 || node.leaf) {
    DEBUG("Inserting key \"%s\"", *key);
    tmp = tree_insert_key(&node, keyindex, key, ptr);
    DUMPNODE(&node);
    if (tree_write(root, (tblock*)&node)) {
      PMSG(LOG_ERR, "Problem writing node: %s", strerror(errno));
      return 0;
    }
  }

  return tmp;
}

/*
 * insert a data block into the tree with the given key
 */
fileptr tree_insert (const tkey key, tblock *data) {
  DEBUG("Inserting into root tree with key \"%s\"", key);
  return tree_sub_insert(0, key, data);
}

/*
 * Insert a data block into the tree starting from the given DATA node. The \a
 * root argument should be 0 if we are inserting at the top level.
 *
 * @param root The index of the \b DATA node
 */
fileptr tree_sub_insert(fileptr root, const tkey key, tblock *data) {
  fileptr newnode;
  fileptr ptr, split;
  tdata dataroot;
  /* TODO: replace with strndup() */
  char *ikey = malloc(sizeof(char)*TREEKEY_SIZE); /* TODO: check for failure */

  DEBUG("Copying key to temp location");
  strncpy(ikey, key, TREEKEY_SIZE);

  if (!root || root==tree_sb->root_index) {
    DEBUG("Faking data node");
    initDataNode(&dataroot);
    dataroot.subkeys=tree_sb->root_index;
    root=0;
  } else if (root==tree_sb->inode_root) {
    DEBUG("Faking data node");
    initDataNode(&dataroot);
    dataroot.subkeys=tree_sb->inode_root;
  } else {
    DEBUG("Fetching block %lu", root);
    errno=0;
    if (tree_read(root, (tblock*)&dataroot)) {
      errno=EIO;
      return 0;
    }
    if (dataroot.magic != MAGIC_DATANODE) {
      PMSG(LOG_ERR, "Not a data node; PANIC.");
      errno=EBADF;
      return 0;
    }
  }

  if (root && !dataroot.subkeys) {
    tnode nblock;

    DEBUG("No subkey root; creating one");
    dataroot.subkeys=tree_alloc();
    initTreeNode(&nblock);
    nblock.leaf=1;
    nblock.keycount=0;

    DEBUG("Writing subkey root block");
    if (tree_write(dataroot.subkeys, (tblock*)&nblock)) {
      PMSG(LOG_ERR, "Problem writing subkey root: %s", strerror(errno));
      return 0;
    }
    DEBUG("Linking to subkeys");
    if (tree_write(root, (tblock*)&dataroot)) {
      PMSG(LOG_ERR, "Problem updating node: %s", strerror(errno));
      return 0;
    }
  } else if (!root && !dataroot.subkeys) {
    PMSG(LOG_ERR, "PANIC! Superblock does not point to a tree!");
    return 0;
  }

  DEBUG("Inserting into subtree with key \"%s\"", ikey);
  newnode=tree_alloc();
  ptr=newnode;
  split=tree_insert_recurse(dataroot.subkeys, &ikey, &ptr);
  if (errno) {
    int tmperrno;
    PMSG(LOG_ERR, "Hit an error (%s) - freeing reallocated block", strerror(errno));
    tmperrno=errno;
    tree_free(newnode);
    free(ikey);
    errno=tmperrno;
    return 0;
  } else {
    if (split) {
      tnode newroot;

      initTreeNode(&newroot);

      DEBUG("Splitting the root");
      newroot.leaf=0;
      newroot.keycount=1;
      strncpy(newroot.keys[0], ikey, TREEKEY_SIZE);
      newroot.ptrs[0] = dataroot.subkeys;
      newroot.ptrs[1] = ptr;
      dataroot.subkeys=tree_alloc();
      tree_write(dataroot.subkeys, (tblock*)&newroot);
      if (!root) {
        tree_sb->root_index=dataroot.subkeys;
        tree_write_sb(tree_sb);
      } else if (root==tree_sb->inode_root) {
        tree_sb->inode_root=dataroot.subkeys;
        tree_write_sb(tree_sb);
      } else {
        tree_write(root, (tblock*)&dataroot);
      }
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
    DEBUG("Starting removal at data node; using subkeys");
    if (!((tdata*)&node)->subkeys) {
      PMSG(LOG_ERR, "No subkey root; can't delete from there!");
      errno=ENOENT;
      return -errno;
    }
    root=((tdata*)&node)->subkeys;
    return tree_remove_recurse(root, key, ptr);
  } else if (node.magic != MAGIC_TREENODE) {
    PMSG(LOG_ERR, "Invalid magic number for block %lu: %lX", root, node.magic);
    errno=EBADF;
    return -errno;
  }

  if (!node.leaf) {
    DEBUG("Not a leaf; recursing");
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
      DEBUG("Should steal or merge");
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
        DEBUG("Copying rightsib to dblock");
        /* copy from rsib to dblock */
        DUMPNODE(&rsib);
        for (i=0; i<rightsteal; i++) {
          DEBUG("Moving key %d (\"%s\") to dblock[%d]", i, rsib.keys[i], dblock.keycount+i);
          strncpy(dblock.keys[dblock.keycount+i], rsib.keys[i], TREEKEY_SIZE);
          DEBUG("Moving pointer %d (%lu) to dblock[%d]", i+1, rsib.ptrs[i+1], dblock.keycount+i+1);
          dblock.ptrs[dblock.keycount+i+1] = rsib.ptrs[i+1];
        }
        DEBUG("Moving rightsib's keys down");
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
        DEBUG("Cannot steal from siblings; must underflow");
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
          PMSG(LOG_ERR, "PANIC AT THE TREE! CAN'T FIND ANY SIBLINGS! ARGH!");
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
      PMSG(LOG_ERR, "Problem reading data block");
      return -EIO;
    }
    if (dblock.subkeys) {
      PMSG(LOG_WARNING, "Node has subkeys - must not be deleted!");
      errno=ENOTEMPTY;
      return -ENOTEMPTY;
    }
    if (dblock.inodecount > DATA_INODE_MAX) {
      DEBUG("Node has inode blocks - must delete those too!");
      if (inode_free_chain(dblock.next_inodes)) {
        PMSG(LOG_ERR, "Failed to free attached inode chain");
        return -EIO;
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
int tree_remove (const tkey key) {
  DEBUG("Removing key \"%s\" from root tree", key);
  return tree_sub_remove(0, key);
}

/*
 * Remove a node from the given subtree and any associated inode blocks
 */
int tree_sub_remove(fileptr root, const tkey key) {
  fileptr ptr=0;
  int merge;
  tdata dataroot;
  char *ikey = malloc(sizeof(char)*TREEKEY_SIZE); /* TODO: check for failure */

  DEBUG("Removing key \"%s\" from subtree rooted at %lu", key, root);

  DEBUG("Copying key to temp location");
  strncpy(ikey, key, TREEKEY_SIZE);

  if (!root || root==tree_sb->root_index) {
    DEBUG("Faking data node");
    initDataNode(&dataroot);
    dataroot.subkeys=tree_sb->root_index;
    root=0;
  } else {
    DEBUG("Fetching block %lu", root);
    errno=0;
    if (tree_read(root, (tblock*)&dataroot)) {
      errno=EIO;
      return -EIO;
    }
    if (dataroot.magic != MAGIC_DATANODE) {
      PMSG(LOG_ERR, "Not a data node; PANIC.");
      DUMPBLOCK((tblock*)&dataroot);
      errno=EBADF;
      return -EBADF;
    }
  }

  if (!dataroot.subkeys) {
    PMSG(LOG_WARNING, "No tree to remove from!");
    errno=ENOENT;
    return -ENOENT;
  }

  DEBUG("Removing \"%s\" from subtree", ikey);
  merge=tree_remove_recurse(dataroot.subkeys, &ikey, &ptr);

  if (errno) {
    PMSG(LOG_ERR, "Hit an error (%s) - chickening out", strerror(errno));
    free(ikey);
    return (errno==ENOTEMPTY)?-ENOTEMPTY:-EIO;
  } else {
    tnode node;
    tree_read(dataroot.subkeys, (tblock*)&node); /* TODO: check for failure */
    if (!node.leaf && !node.keycount) {
      DEBUG("Replacing root: new index = %lu", node.ptrs[0]);
      tree_free(dataroot.subkeys);
      dataroot.subkeys = node.ptrs[0];
      if (!root) {
        tree_sb->root_index=dataroot.subkeys;
        tree_write_sb(tree_sb);
      } else {
        tree_write(root, (tblock*)&dataroot);
      }
    } else if (root && !node.keycount && !node.ptrs[0]) {
      /* if node is an empty leaf and we're not at the superblock level, we can
       * free it to reclaim some more space! */
      DEBUG("Can free node %lu as it's not part of the root tree", root);
      tree_free(root);
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

/**
 * Apply a given user-defined function to all keys in the tree rooted at \a root. The function needs to take three arguments:
 *
 *  - <tt>const char *</tt> - The key.
 *  - <tt>const fileptr</tt> - The block index that this key points to.
 *  - <tt>void *data</tt> - The data argument given to the tree_map_keys() function.
 *
 * The user-defined function should return zero, although it may return 1 to
 * cause the loop to immediately terminate successfully or any negative value
 * to indicate an internal error, terminating the loop and exiting
 * tree_map_keys() with that value as its return value.
 *
 * @returns Zero on success, non-zero on failure.
 */
#if defined(_DEBUG_TREE_MAP_KEYS) && !defined(_DEBUG)
#define _DEBUG_ONCE
#define _DEBUG
#include <debug.h>
#endif
int tree_map_keys(const fileptr root, int (*func)(const char *, const fileptr, void *), void *data) {
  tnode node;
  int ret=0, i;

  if (tree_sub_get_min(root, &node)) {
    PMSG(LOG_ERR, "tree_sub_get_min() failed\n");
    return -EIO;
  }

  while (1) {
    for (i=0; i<node.keycount; i++) {
      DEBUG("Applying function to key[%d] = \"%s\"", i, node.keys[i]);
      ret = func(node.keys[i], node.ptrs[i+1], data);
      DEBUG("Function returned %d for key[%d] = \"%s\"", ret, i, node.keys[i]);
      if (ret) {
        if (ret==1) ret=0;
        break;
      }
    }
    if (!node.ptrs[0]) {
      break;
    } else if (node.ptrs[0] && tree_read(node.ptrs[0], (tblock*) &node)) {
      PMSG(LOG_ERR, "I/O error reading block\n");
      return -EIO;
    }
  }

  return ret;
}
#if defined(_DEBUG_TREE_MAP_KEYS) && defined(_DEBUG_ONCE)
#undef _DEBUG_ONCE
#undef _DEBUG
#include <debug.h>
#endif

/**
 * Fetch all keys from the tree rooted at \a root as an array.
 *
 * @param[in]  root   The root of the tree to read.
 * @param[out] keys   A pointer which will be filled with an array of strings.
 * If NULL, then the function returns the number of items required (@see
 * tree_sub_key_count()).
 * @param[in]  max    Maximum number of keys the array can contain.
 * @returns Zero on success, the number of keys if \a keys is NULL, or a
 * negative error code on failure.
 */
int tree_get_all_keys(fileptr root, tkey *keys, unsigned int max) {

  if (!keys) {
    return tree_sub_key_count(root);
  }

  tnode node;
  size_t cur=0;

  if (tree_sub_get_min(root, &node)) {
    PMSG(LOG_ERR, "tree_sub_get_min() failed\n");
    return -EIO;
  }

  while (1) {
    unsigned int i = 0;
    while (i<node.keycount && cur<max) {
      strncpy(keys[cur++], node.keys[i++], TREEKEY_SIZE);
    }
    if (!node.ptrs[0]) {
      break;
    } else if (cur>=max) {
      PMSG(LOG_ERR, "Ran out of buffer space\n");
      return -ENOSPC;
    } else if (node.ptrs[0] && tree_read(node.ptrs[0], (tblock*) &node)) {
      PMSG(LOG_ERR, "I/O error reading block\n");
      return -EIO;
    }
  }

  return cur;
}

/**
 * Get the length of the fully-qualified key for the given data block.
 *
 * @param fileptr dataptr The data block for which to retrieve the key length.
 * @return The length of the fully-qualified key.
 */
size_t tree_get_full_key_len(fileptr dataptr) {
  tdata dnode;

  if (tree_read(dataptr, (tblock*)&dnode)) {
    PMSG(LOG_ERR, "Error reading block %lu", dataptr);
    errno = EIO;
    /* key length will never be zero - this is always an error */
    return 0;
  }
  if (dnode.parent) {
    return strlen(dnode.name) + 1 + tree_get_full_key_len(dnode.parent);
  } else {
    return strlen(dnode.name);
  }
}

/**
 * Get the fully-qualified key for the given data block.
 *
 * @param fileptr dataptr The data block for which to retrieve the fully-qualified key.
 * @return A pointer to the key that must be freed when no longer required.
 */
char *tree_get_full_key(fileptr dataptr) {
  signed long key_len = tree_get_full_key_len(dataptr);
  PMSG(LOG_ERR, "Full key length: %lu", key_len);
  if (key_len==0) return NULL;
  char *ret = calloc(key_len+1, sizeof(char));
  tdata dnode;

  do {
    PMSG(LOG_ERR, "Reading block %lu", dataptr);
    if (tree_read(dataptr, (tblock*)&dnode)) {
      PMSG(LOG_ERR, "Error reading block %lu", dataptr);
      errno = EIO;
      return NULL;
    }
    size_t this_len = strlen(dnode.name);
    PMSG(LOG_ERR, "Name length: %lu", this_len);
    key_len -= this_len;
    PMSG(LOG_ERR, "Key len now: %lu", key_len);
    PMSG(LOG_ERR, "strncpy(%p, %p, %lu)", &ret[key_len], dnode.name, key_len);
    strncpy(&ret[key_len], dnode.name, this_len); /* to avoid copying the terminating null */
    PMSG(LOG_ERR, "strncpy(%p, %p, %lu) done", &ret[key_len], dnode.name, key_len);
    PMSG(LOG_ERR, "dnode.parent: %lu", dnode.parent);
    if (dnode.parent) {
      ret[--key_len]=INSIGHT_SUBKEY_SEP_C;
      PMSG(LOG_ERR, "ret[%lu] = '%c'", key_len, INSIGHT_SUBKEY_SEP_C);
    }
    dataptr = dnode.parent;
  } while (dataptr);

  PMSG(LOG_ERR, "Returning \"%s\"", ret);

  return ret;
}

/**
 * Follow the linked list of inodes starting at the given block number, freeing
 * all of them.
 *
 * @param block The block index of the first inode block to be freed.
 */
int inode_free_chain(fileptr block) {
  fileptr inodeptr;
  tinode ib;

  for (inodeptr = block; inodeptr; inodeptr=ib.next_inodes) {
    if (tree_read(inodeptr, (tblock*)&ib) || tree_free(inodeptr)) {
      PMSG(LOG_ERR, "Problem reading inode block");
      return -EIO;
    }
  }

  return 0;
}

/**
 * Insert the given inode into the given data block. If block is zero, insert into the limbo list.
 *
 * @param block The data block index, or zero for the limbo list.
 * @param inode The inode to be inserted.
 * @returns Zero on success, error code on failure.
 */
int inode_insert(fileptr block, fileptr inode) {
  DEBUG("Inserting %08lX into inode list at block %lu", inode, block);
  int count = inode_get_all(block, NULL, 0);
  DEBUG("Need %d inodes of space", count);
  if (count < 0) {
    PMSG(LOG_ERR, "IO error in getting inode list");
    return EIO;
  }

  fileptr *inodes = malloc((count+1)*sizeof(fileptr));
  if (!inodes) {
    PMSG(LOG_ERR, "Failed to allocate memory for inodes array");
    return ENOMEM;
  }
  fileptr *inodes_new = calloc(count+2, sizeof(fileptr));
  if (!inodes_new) {
    PMSG(LOG_ERR, "Failed to allocate memory for inodes array");
    ifree(inodes);
    return ENOMEM;
  }
  if (inode_get_all(block, inodes, count)<0) {
    PMSG(LOG_ERR, "Failed to get inode array from block %lu", block);
    ifree(inodes_new);
    ifree(inodes);
    return EIO;
  }

  /* insert node */
  int res = set_union(inodes, &inode, inodes_new, count, 1, count+1, sizeof(fileptr), inodecmp);
  if (res<0) {
    PMSG(LOG_ERR, "Set union failed!");
    return -res;
  }

  /* save array back to target block */
  if (inode_put_all(block, inodes_new, res)<0) {
    PMSG(LOG_ERR, "Failed to save inode array to block %lu", block);
    free(inodes_new);
    free(inodes);
    return EIO;
  }

  free(inodes_new);
  free(inodes);
  return 0;
}

/**
 * Remove the given inode from the given data block. If block is zero, remove from the limbo list.
 *
 * @param block The data block index, or zero for the limbo list.
 * @param inode The inode to be inserted.
 * @returns Zero on success, error code on failure.
 */
int inode_remove(fileptr block, fileptr inode) {
  fileptr *inodes;
  DEBUG("Remove %08lX from inode list at block %lu", inode, block);
  int count = inode_get_all(block, NULL, 0);
  DEBUG("Need %d inodes of space", count);
  if (count < 0) {
    PMSG(LOG_ERR, "IO error in getting inode list");
    return EIO;
  }

  DEBUG("Allocating space");
  inodes = malloc(count*sizeof(fileptr));
  if (!inodes) {
    PMSG(LOG_ERR, "Failed to allocate memory for inodes array");
    return ENOMEM;
  }
  DEBUG("Retrieving inodes");
  if (inode_get_all(block, inodes, count)<0) {
    PMSG(LOG_ERR, "Failed to get inode array from block %lu", block);
    ifree(inodes);
    return EIO;
  }

  /* find node */
  /* TODO: make this binary search */
  DEBUG("Finding target");
  int i;
  for (i=0; i<count; i++) {
    if (inodes[i]==inode) break;
  }

  if (i>=count) {
    DEBUG("Target lost");
    /* not found */
    ifree(inodes);
    return ENOENT;
  }
  DEBUG("Eliminating target at position %d", i);

  /* remove node */
  while (i++<count-1) {
    inodes[i-1]=inodes[i];
  }

  DEBUG("Saving array back to target %lu", block);
  /* save array back to target block */
  if (inode_put_all(block, inodes, count-1)<0) {
    PMSG(LOG_ERR, "Failed to save inode array to block %lu", block);
    ifree(inodes);
    return EIO;
  }
  DEBUG("Done!");

  ifree(inodes);
  return 0;
}

/**
 * Write all inodes to the given block, following links as required. The \a
 * block argument may refer to either a data block or the superblock (if zero).
 * Additional inode blocks will be allocated if required, and they may also be
 * freed.
 *
 * @note The input inodes array will be sorted if it is not already.
 *
 * @param[in] block  The block index to write an inode list for.
 * @param[in] inodes A pointer to an array of inodes.
 * @param[in] count  Number of inodes in the array.
 * @returns Zero on success, or a negative error code on failure.
 */
int inode_put_all(fileptr block, fileptr *inodes, unsigned int count) {
  tdata datablock;
  unsigned int curinode=0;
  bzero(&datablock, sizeof(datablock));

  DEBUG("inode_put_all(block: %lu, inodes: %p, count: %d)", block, inodes, count);

  if (!inodes) {
    PMSG(LOG_ERR, "No inode list given");
    return -ENOENT;
  }

  /* XXX: make sure the list is sorted */
  qsort(inodes, count, sizeof(fileptr), inodecmp);

  if (!block) {
    DEBUG("Starting at superblock and writing inodes in limbo");
    tree_sb->limbo_count = count;
    datablock.inodecount = tree_sb->limbo_count;
    if (count && !tree_sb->inode_limbo) {
      DEBUG("Creating inode limbo area");
      tree_sb->inode_limbo = tree_alloc();
    } else if (!count && tree_sb->inode_limbo) {
      DEBUG("Freeing limbo inode block chain");
      if (inode_free_chain(tree_sb->inode_limbo)) {
        PMSG(LOG_ERR, "Could not free inode chain");
        return -EIO;
      }
      tree_sb->inode_limbo=0;
      tree_write_sb(tree_sb);
    }

    datablock.next_inodes = tree_sb->inode_limbo;
  } else {
    DEBUG("Starting at block index %lu", block);
    if (tree_read(block, (tblock*)&datablock)) {
      PMSG(LOG_ERR, "Problem reading data block");
      return -EIO;
    }

    DEBUG("Copying %u inodes from user buffer direct to block", MIN(count, DATA_INODE_MAX));
    datablock.inodecount = MIN(count, DATA_INODE_MAX);
    memcpy(datablock.inodes, inodes, datablock.inodecount*sizeof(fileptr));
    curinode += datablock.inodecount;
    datablock.inodecount = count;

    if (curinode<count && !datablock.next_inodes) {
      DEBUG("Creating inode block");
      datablock.next_inodes = tree_alloc();
    } else if (curinode >= count && datablock.next_inodes) {
      DEBUG("Freeing inode block chain");
      if (inode_free_chain(datablock.next_inodes)) {
        PMSG(LOG_ERR, "Could not free inode chain");
        return -EIO;
      }
      datablock.next_inodes=0;
    }

    DUMPBLOCK((tblock*)&datablock);
    if (tree_write(block, (tblock*)&datablock)) {
      PMSG(LOG_ERR, "Problem writing data block");
      return -EIO;
    }
  }

  fileptr inodeptr;
  tinode ib;

  DUMPUINT(curinode);
  DUMPUINT(count);
  for (inodeptr = datablock.next_inodes; curinode<count; inodeptr=ib.next_inodes) {
    if (!inodeptr) {
      PMSG(LOG_ERR, "Followed a null pointer!");
      return -EIO;
    }
    if (tree_read(inodeptr, (tblock*)&ib)) {
      PMSG(LOG_ERR, "Problem verifying inode block");
      return -EIO;
    }
    DUMPBLOCK((tblock*)&ib);

    if (ib.magic == MAGIC_FREEBLOCK) {
      /* XXX: taking a big chance here */
      initInodeBlock(&ib);
    } else if (ib.magic != MAGIC_INODEBLOCK) {
      PMSG(LOG_ERR, "Failed to verify inode");
      return -EIO;
    }
    ib.inodecount = MIN(count-curinode, INODE_MAX);
    DEBUG("Copying %u inodes from user buffer (start: %u) to disk block %lu", ib.inodecount, curinode, inodeptr);
    memcpy(ib.inodes, &inodes[curinode], ib.inodecount*sizeof(fileptr));
    curinode += ib.inodecount;
    if (curinode<count && !ib.next_inodes) {
      DUMPUINT(curinode);
      DUMPUINT(count);
      DEBUG("Creating another inode block");
      ib.next_inodes=tree_alloc();
      if (!ib.next_inodes) {
        PMSG(LOG_ERR, "Failed to allocate another inode block");
        return -ENOSPC;
      }
      DEBUG("New inode block number: %lu", ib.next_inodes);
    } else if (curinode >= count && ib.next_inodes) {
      DEBUG("Freeing chain starting at block %lu", ib.next_inodes);
      if (inode_free_chain(ib.next_inodes)) {
        PMSG(LOG_ERR, "Could not free inode chain");
        return -EIO;
      }
      ib.next_inodes=0;
      DEBUG("Chain freed");
    }
    DUMPBLOCK((tblock*)&ib);
    if (tree_write(inodeptr, (tblock*)&ib)) {
      PMSG(LOG_ERR, "Problem writing data block");
      return -EIO;
    }
  }

  return 0;
}

static int _rec_inode_func(const char *key, const fileptr ptr, void *data) {
  struct {
    int count;
    fileptr *list;
  } *pdata = data;
  int subcount;
  (void) key;
  DEBUG("Fetching sublist items from block %lu", ptr);
  fileptr *sublist = inode_get_all_recurse(ptr, &subcount);
  if (!sublist) {
    PMSG(LOG_ERR, "Failed to get sublist");
    return -EIO;
  }
  DEBUG("Got %d items from sublist", subcount);
  fileptr *oldlist = pdata->list;
  pdata->list = calloc(pdata->count + subcount, sizeof(fileptr));
  DEBUG("Merging sublist and previous list");
  pdata->count = set_union(oldlist, sublist, pdata->list, pdata->count, subcount, pdata->count+subcount, sizeof(fileptr), inodecmp);
  DEBUG("Union complete");
  DEBUG("Union contains %d items", pdata->count);
  if (pdata->count<0) {
    PMSG(LOG_ERR, "Set union failed");
    return -EIO;
  }
  ifree(oldlist);
  ifree(sublist);
  return 0;
}

/**
 * Fetch all inodes recursively from the given block, following links as
 * required. The \a block argument may refer to either a data block, an inode
 * block, a tree block, or the superblock (if zero).
 *
 * @param[in]  block  The first block index to read.
 * @param[out] count  The number of inodes in the array
 * @returns An array of inodes that should be freed after use, or NULL if an
 * error occurred.
 */
fileptr *inode_get_all_recurse(fileptr block, int *count) {
  if (!block) {
    DEBUG("Starting at superblock");
    *count = inode_get_all(block, NULL, 0);
    if (*count<0) {
      PMSG(LOG_ERR, "Problem reading inode list");
      return NULL;
    }
    fileptr *list = calloc(*count?*count:1, sizeof(fileptr));
    if (!list) {
      PMSG(LOG_ERR, "Failed to allocate memory for list");
      return NULL;
    }
    if (*count) {
      inode_get_all(block, list, *count);
    }
    return list;
  }

  tblock readblock;

  DEBUG("Starting at block index %lu", block);
  if (tree_read(block, &readblock)) {
    PMSG(LOG_ERR, "Problem reading block");
    return NULL;
  }

  switch (readblock.magic) {
    case MAGIC_DATANODE:
      {
        DEBUG("Fetching list from this node");
        int count1 = inode_get_all(block, NULL, 0);
        if (count1<0) {
          PMSG(LOG_ERR, "Problem reading inode list");
          return NULL;
        }
        fileptr *thislist = calloc(count1?count1:1, sizeof(fileptr));
        if (!thislist) {
          PMSG(LOG_ERR, "Failed to allocate memory for list");
          return NULL;
        }
        DEBUG("Fetching %d inodes from this node", count1);
        inode_get_all(block, thislist, count1);
        if (!((tdata*)&readblock)->subkeys) {
          DEBUG("No subkeys, so no union needed");
          *count = count1;
          return thislist;
        }
        int count2=0;
        DEBUG("Fetching subkeys list");
        fileptr *sublist = inode_get_all_recurse(((tdata*)&readblock)->subkeys, &count2);
        DEBUG("Fetched %d inodes from subkeys", count2);
        if (!sublist) {
          PMSG(LOG_ERR, "Error getting sublist");
          ifree(thislist);
          return NULL;
        }
        fileptr *outlist = calloc(count1+count2, sizeof(fileptr));
        *count = set_union(thislist, sublist, outlist, count1, count2, count1+count2, sizeof(fileptr), inodecmp);
        ifree(thislist);
        ifree(sublist);
        return outlist;
      }
      break;

    case MAGIC_TREENODE:
      {
        struct {
          int count;
          fileptr *list;
        } pdata;

        DEBUG("Allocating list");
        pdata.list=calloc(1, sizeof(fileptr)); /* TODO: check for failure */
        pdata.count= 0;

        DEBUG("Mapping across keys");
        int ret = tree_map_keys(block, _rec_inode_func, &pdata);
        if (ret<0) {
          PMSG(LOG_ERR, "Mapping across keys failed");
          errno=ret;
          return NULL;
        }
        DEBUG("Success");
        *count = pdata.count;
        return pdata.list;
      }
      break;

    default:
      PMSG(LOG_ERR, "Unknown block magic %08lX", readblock.magic);
      return NULL;
  }
}

/**
 * Fetch all inodes from the given block, following links as required. The \a
 * block argument may refer to either a data block or the superblock (if zero).
 *
 * @param[in]  block  The first block index to read.
 * @param[out] inodes A pointer which will be filled with an array of inodes.
 * If NULL, then the function returns the space required.
 * @param[in]  max    Maximum number of inodes the array can contain.
 * @returns Zero on success, the number of inodes if \a inodes is NULL, or a
 * negative error code on failure.
 */
int inode_get_all(fileptr block, fileptr *inodes, unsigned int max) {
  tdata datablock;
  unsigned int curinode=0;

  bzero(&datablock, sizeof(tdata));

  if (!block) {
    DEBUG("Starting at superblock and reading inodes in limbo");
    if (!inodes) {
      DEBUG("Number of array entries required: %lu", tree_sb->limbo_count);
      return (int)tree_sb->limbo_count;
    }
    datablock.inodecount = tree_sb->limbo_count;
    datablock.next_inodes = tree_sb->inode_limbo;
  } else {
    DEBUG("Starting at block index %lu", block);
    if (tree_read(block, (tblock*)&datablock)) {
      PMSG(LOG_ERR, "Problem reading data block");
      return -EIO;
    }
    if (!inodes) {
      DEBUG("Number of array entries required: %u", datablock.inodecount);
      return datablock.inodecount;
    }
    if (datablock.inodecount > max) {
      PMSG(LOG_ERR, "Number of inodes (%u) greater than output array size (%d)", datablock.inodecount, max);
      return -ENOMEM;
    }

    DEBUG("Copying inodes to user buffer");
    memcpy(inodes, datablock.inodes, MIN(datablock.inodecount, DATA_INODE_MAX)*sizeof(fileptr));
    curinode += MIN(datablock.inodecount, DATA_INODE_MAX);
  }

  DEBUG("Following pointers if required...");

  fileptr inodeptr=0;
  tinode ib;
  /* read last inode pointer of block while( nextptr = ((tinode*)&dblock)->inodes[INODE_MAX-1] ), and free that block */
  for (inodeptr = datablock.next_inodes; inodeptr; inodeptr=ib.next_inodes) {
    DEBUG("Following pointer... to block %lu", inodeptr);
    if (tree_read(inodeptr, (tblock*)&ib)) {
      PMSG(LOG_ERR, "Problem reading inode block");
      return -EIO;
    }
    if (ib.magic != MAGIC_INODEBLOCK) {
      PMSG(LOG_ERR, "Block %lu is not an inode block!", inodeptr);
      return -EBADF;
    }
    if (curinode + ib.inodecount > datablock.inodecount) {
      PMSG(LOG_WARNING, "Inode block %lu stores more inodes (%u) than the data block %lu says (%u)!", inodeptr, ib.inodecount, block, datablock.inodecount);
      return 0;
    } else if (curinode + ib.inodecount > max) {
      PMSG(LOG_WARNING, "Did not allocate enough space for all inodes; truncating");
      memcpy(&inodes[curinode], ib.inodes, (ib.inodecount - (max-curinode))*sizeof(fileptr));
      return 0;
    } else {
      memcpy(&inodes[curinode], ib.inodes, ib.inodecount*sizeof(fileptr));
    }
    curinode += MIN(ib.inodecount, INODE_MAX);
  }

  return 0;
}
