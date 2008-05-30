#ifndef _BPLUS_DEBUG_H
#define _BPLUS_DEBUG_H

#include "debug.h"

/** @cond */
#ifdef _DEBUG
static inline void DUMPSB(tsblock *node) {
  printf(" [SUPERBLOCK]\n");
  printf("  version:    %d.%d\n", node->version>>8, node->version & 0xff);
  printf("  root index: %lu\n", node->root_index);
  printf("  max size:   %lu\n", node->max_size);
  printf("  free head:  %lu\n", node->free_head);
}
static inline void DUMPNODE(tnode *node) {
  int i;
  printf(" [TREE NODE]\n");
  printf("  leaf:     %d\n", node->leaf);
  printf("  keycount: %d\n", node->keycount);
  printf("  ptrs:");
  for(i=0;i<ORDER;i++) printf(node->ptrs[i]>99?" [\033[1;33m%lu\033[m]":(i>node->keycount?" [\033[4m%lu\033[m]":" [%lu]"), node->ptrs[i]);
  printf("\n");
  printf("  keys:");
  for(i=0;i<ORDER-1;i++) printf(i>=node->keycount?" [\033[4m%s\033[m]":" [%s]", node->keys[i]);
  printf("\n");
}
static inline void DUMPDATA(tdata *node) {
  int i;
  printf(" [DATA NODE]\n");
  printf("  inodecount:     %d\n", node->inodecount);
  printf("  flags: %x\n", node->flags);
  printf("  subkeys: %lu\n", node->subkeys);
  printf("  inodes:");
  for(i=0;i<INODECOUNT;i++) printf(" [%lx]", node->inodes[i]);
  printf("\n");
}
static inline void DUMPFREE(freeblock *node) {
  printf(" [FREE BLOCK]\n");
  printf("  next free block: %lu\n", node->next);
}
static inline void DUMPBLOCK(tblock *node) {
  printf("Node magic: %08lX\n", node->magic);
  switch (node->magic) {
    case MAGIC_SUPERBLOCK:
      DUMPSB((tsblock*)node);
      break;
    case MAGIC_TREENODE:
      DUMPNODE((tnode*)node);
      break;
    case MAGIC_DATANODE:
      DUMPDATA((tdata*)node);
      break;
    case MAGIC_FREEBLOCK:
      DUMPFREE((freeblock*)node);
      break;
    case MAGIC_STRINGENTRY:
    case MAGIC_INODEBLOCK:
    default:
      break;
  }
}
#else
# define DUMPSB(x)
# define DUMPNODE(x)
# define DUMPDATA(x)
# define DUMPFREE(x)
# define DUMPBLOCK(x)
#endif
/** @endcond */

#endif
