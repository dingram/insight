#ifndef __BPLUS_H
#define __BPLUS_H

/** Default number of blocks to create in new storage file */
#define DEFAULT_BLOCKS 512
/** Tree block size */
#define TREEBLOCK_SIZE 512
/** Maximum key length including terminating null */
#define TREEKEY_SIZE 33
/** Order of the tree - i.e. how many pointers are stored */
#define ORDER ((TREEBLOCK_SIZE - 2*sizeof(short))/(sizeof(fileptr)+sizeof(tkey))+1)
/** Order of the inode tree - i.e. how many pointers are stored */
#define DORDER ((TREEBLOCK_SIZE - 2*sizeof(short))/(sizeof(fileptr)+sizeof(fileptr))+1)
/** Maximum number of inodes in a data block */
#define INODECOUNT 124
/** Maximum number of inodes in an inode block */
#define INODE_MAX ((TREEBLOCK_SIZE - sizeof(short) - 2*sizeof(unsigned long))/sizeof(fileptr))
/** Maximum length of a path reference in an inode tree data block, including terminating null */
#define IDATA_MAX_LEN (TREEBLOCK_SIZE - sizeof(unsigned long))

/**
 * @defgroup MagicDefs Magic numbers
 */
/*@{*/
#define MAGIC_SUPERBLOCK  0x00bab10c /**< Superblock (uber block) */
#define MAGIC_TREENODE    0xce11b10c /**< Internal tree node (cell block) */
#define MAGIC_DATANODE    0xda7ab10c /**< Tree data node (data block) */
#define MAGIC_STRINGENTRY 0x7ec5b10c /**< String table entry (text block) [currently unused] */
#define MAGIC_FREEBLOCK   0xf1eeb10c /**< Free block (free block) */
#define MAGIC_INODEBLOCK  0x10deb10c /**< Inode block (inode block) */
#define MAGIC_INODETABLE  0x7ab1b10c /**< Inode translation table entry (table block) [currently unused] */
/*@}*/

/** File format that this code will write */
#define TREE_FILE_VERSION 0x0100

/** Initialise a tree node (zero it and set its magic number) */
#define initTreeNode(n) do { bzero((n),sizeof(tnode)); (n)->magic=MAGIC_TREENODE; } while (0)
/** Initialise a data node (zero it and set its magic number) */
#define initDataNode(n) do { bzero((n),sizeof(tdata)); (n)->magic=MAGIC_DATANODE; } while (0)
/** Initialise a free block (zero it and set its magic number) */
#define initFreeNode(n) do { bzero((n),sizeof(freeblock)); (n)->magic=MAGIC_FREEBLOCK; } while (0)
/** Initialise a inode block (zero it and set its magic number) */
#define initInodeBlock(n) do { bzero((n),sizeof(freeblock)); (n)->magic=MAGIC_INODEBLOCK; } while (0)

/** An address within a file */
typedef unsigned long fileptr;

/** A key within the tree */
typedef char tkey[TREEKEY_SIZE];

/** A generic block */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long magic;                                /**< Magic number */
  char data[TREEBLOCK_SIZE - sizeof(unsigned long)];  /**< The rest of the block data */
} tblock;

/** A free block */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long magic;      /**< Magic number 0xf1eeb10c */
  fileptr next;             /**< Address of the next free block (0 if none) */
  char unused[TREEBLOCK_SIZE - sizeof(unsigned long) - sizeof(fileptr)];  /**< Unused space */
} freeblock;

/** The superblock */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long magic;        /**< Magic number 0x00bab10c */
  unsigned short version;     /**< Version code for the file. First byte (little-endian) is major number, second is minor. */
  unsigned short reserved;    /**< Reserved for future expansion. Makes for better alignment. */
  fileptr root_index;         /**< Address of root of top-level tree */
  fileptr max_size;           /**< Max size of tree file, in blocks (not including superblock) */
  fileptr free_head;          /**< Address of first free block (0 if none) */
  fileptr inode_limbo;        /**< Address of first block of the inode limbo area */
  unsigned long limbo_count;  /**< Number of inodes total in limbo */
  char padding[TREEBLOCK_SIZE - (2*sizeof(unsigned long) + 2*sizeof(unsigned short) + 4*sizeof(fileptr))]; /**< Unused space */
} tsblock;

/** Node in the tree */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long  magic;            /**< Magic number 0xce11b10c */
  unsigned short leaf;             /**< 0x01 if this node is a leaf node, 0x00 otherwise */
  unsigned short keycount;         /**< The number of keys in this node */
  fileptr        ptrs[ORDER];      /**< Addresses of child nodes if not leaf, or of data nodes and next sibling (index 0) if leaf */
  tkey           keys[ORDER-1];    /**< The keys stored in this node */
                                   /** unused space */
  char           unused[TREEBLOCK_SIZE - sizeof(unsigned long) - 2*sizeof(short) - ORDER*(sizeof(fileptr)) - (ORDER-1)*(sizeof(tkey))];
} tnode;

/** Node in the inode tree */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long  magic;            /**< Magic number 0xce11b10c */
  unsigned short leaf;             /**< 0x01 if this node is a leaf node, 0x00 otherwise */
  unsigned short keycount;         /**< The number of keys in this node */
  fileptr        ptrs[DORDER];     /**< Addresses of child nodes if not leaf, or of data nodes and next sibling (index 0) if leaf */
  fileptr        keys[DORDER-1];   /**< The keys stored in this node */
                                   /** unused space */
  char           unused[TREEBLOCK_SIZE - sizeof(unsigned long) - 2*sizeof(short) - ORDER*(sizeof(fileptr)) - (ORDER-1)*(sizeof(fileptr))];
} tdnode;

/** Data block in the tree */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long magic;        /**< Magic number 0xda7ab10c */
  unsigned short inodecount;  /**< Number of inodes related to this key (not necessarily all stored in this node) */
  short flags;                /**< Flags and other information about this node */
  fileptr subkeys;            /**< Root node of subkeys tree or address of synonym target if \a flags contain \c DATA_FLAGS_SYNONYM */
  union {
    fileptr inodes[INODECOUNT]; /**< List of inodes */
    char    target[INODECOUNT*(sizeof(unsigned long)/sizeof(char))]; /**< Synonym target name */
  };
  fileptr next_inodes;        /**< Address of next block of inodes, or zero if none */
} tdata;

/** Inode block referenced by a data block */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long magic;        /**< Magic number 0x10deb10c */
  unsigned short inodecount;  /**< Number of inodes held in this block */
  short unused;               /**< Unused */
  fileptr inodes[INODE_MAX];  /**< List of inodes */
  fileptr next_inodes;        /**< Address of next block of inodes, or zero if none */
} tinode;

/** Data block in the inode tree */
typedef struct /** @cond */ __attribute__((__packed__)) /** @endcond */ {
  unsigned long magic;        /**< Magic number */
  char path[IDATA_MAX_LEN];   /**< Referenced path */
} tddata;

/** If a data node has this flag, it is a synonym for another node, and its \a
 * subkeys field is the address of the synonym target (another data block) */
#define DATA_FLAGS_SYNONYM 0x01

/** If a data node has this flag, it should only list the inodes that directly
 * belong to it and NOT recurse.  */
#define DATA_FLAGS_NOSUB 0x02

/** Flag set macro */
#define SET_FLAG(v,f) do { (v) |= (f); } while (0)
/** Flag clear macro */
#define CLEAR_FLAG(v,f) do { (v) = ((v) | (f)) ^ (f); } while (0)

/*****************************************************************************
 * FUNCTION PROTOTYPES
 ****************************************************************************/

int     tree_open         (char *path);
int     tree_close        (void);
int     tree_read         (fileptr block, tblock *data);
int     tree_write        (fileptr block, tblock *data);
int     tree_grow         (fileptr newsize);
time_t  tree_get_mtime    ();
fileptr tree_get_root     ();
int     tree_get_min      (tnode *node);
int     tree_sub_get_min  (fileptr root, tnode *node);
int     tree_key_count    ();
int     tree_sub_key_count(fileptr root);
int     tree_get          (tkey key, tdata *node);
int     tree_sub_get      (fileptr root, tkey key, tdata *node);
fileptr tree_search       (tkey key);
fileptr tree_sub_search   (fileptr root, tkey key);
int     tree_remove       (tkey key);
int     tree_sub_remove   (fileptr root, tkey key);
fileptr tree_insert       (tkey key, tdata *data);
fileptr tree_sub_insert   (fileptr node, tkey key, tdata *data);
int     tree_read_sb      (tsblock *super);
int     tree_write_sb     (tsblock *super);
int     tree_map_keys     (const fileptr root, int (*func)(const char *, const fileptr, void *), void *data);

int inode_free_chain(fileptr block);
int inode_insert(fileptr block, fileptr inode);
int inode_put_all(fileptr block, fileptr *inodes, int count);
int inode_get_all(fileptr block, fileptr *inodes, int max);
fileptr *inode_get_all_recurse(fileptr block, int *count);

#ifndef MAX
#define MAX(a,b)      ((a)>(b)?(a):(b))
#endif

#ifndef MIN
#define MIN(a,b)      ((a)<(b)?(a):(b))
#endif

#ifndef MAX3
#define MAX3(a,b,c)   (MAX(a,MAX(b,c)))
#endif

#ifndef MIN3
#define MIN3(a,b,c)   (MIN(a,MIN(b,c)))
#endif

#endif /* #ifndef __BPLUS_H */

// vim:ts=2:sw=2:et:nowrap
