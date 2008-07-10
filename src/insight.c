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
#include <bplus.h>
#if defined(_DEBUG_CORE) && !defined(_DEBUG)
#define _DEBUG
#endif
#include <debug.h>
#include <path_helpers.h>
#include <string_helpers.h>
#include <query_engine.h>
#include <set_ops.h>
#include <stdarg.h>

static int   insight_getattr(const char *path, struct stat *stbuf);
static int   insight_readlink(const char *path, char *buf, size_t size);
static int   insight_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int   insight_mknod(const char *path, mode_t mode, dev_t rdev);
static int   insight_mkdir(const char *path, mode_t mode);
static int   insight_unlink(const char *path);
static int   insight_rmdir(const char *path);
static int   insight_rename(const char *from, const char *to);
static int   insight_symlink(const char *from, const char *to);
static int   insight_link(const char *from, const char *to);
static int   insight_chmod(const char *path, mode_t mode);
static int   insight_chown(const char *path, uid_t uid, gid_t gid);
static int   insight_truncate(const char *path, off_t size);
#if FUSE_VERSION >= 26
static int   insight_utimens(const char *path, const struct timespec tv[2]);
#else
static int   insight_utime(const char *path, struct utimbuf *buf);
#endif
static int   insight_open(const char *path, struct fuse_file_info *fi);
static int   insight_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int   insight_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
#if FUSE_VERSION >= 25
static int   insight_statvfs(const char *path, struct statvfs *stbuf);
#else
static int   insight_statfs(const char *path, struct statfs *stbuf);
#endif
static int   insight_release(const char *path, struct fuse_file_info *fi);
static int   insight_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
static int   insight_access(const char *path, int mode);
#ifdef HAVE_SETXATTR
static int   insight_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
static int   insight_getxattr(const char *path, const char *name, char *value, size_t size);
static int   insight_listxattr(const char *path, char *list, size_t size);
static int   insight_removexattr(const char *path, const char *name);
#endif /* HAVE_SETXATTR */
static void  insight_destroy(void *arg);
#if FUSE_VERSION >= 26
static void *insight_init(struct fuse_conn_info *conn);
#else
static void *insight_init(void);
#endif

/* static */ struct insight insight;

#define INSIGHTFS_OPT(t, p, v) { t, offsetof(struct insight, p), v }

enum {
    KEY_HELP,
    KEY_VERSION,
};

static struct fuse_opt insight_opts[] = {
  INSIGHTFS_OPT("-d",         debug,        1),
  INSIGHTFS_OPT("--store=%s", treestore,    0),
  INSIGHTFS_OPT("--repos=%s", repository,   0),
  INSIGHTFS_OPT("-f",         foreground,   1),
  INSIGHTFS_OPT("-s",         singlethread, 1),
  INSIGHTFS_OPT("-r",         readonly,     1),
  INSIGHTFS_OPT("-v",         verbose,      1),
  INSIGHTFS_OPT("-q",         quiet,        1),

  FUSE_OPT_KEY("-V",            KEY_VERSION),
  FUSE_OPT_KEY("--version",     KEY_VERSION),
  FUSE_OPT_KEY("-h",            KEY_HELP),
  FUSE_OPT_KEY("--help",        KEY_HELP),
  FUSE_OPT_END
};

static struct fuse_operations insight_oper = {
  .getattr    = insight_getattr,
  .readdir    = insight_readdir,
  .open       = insight_open,
  .read       = insight_read,
  .mkdir      = insight_mkdir,
  .destroy    = insight_destroy,
  .rmdir      = insight_rmdir,
  .readlink   = insight_readlink,
  .mknod      = insight_mknod,
  .symlink    = insight_symlink,
  .unlink     = insight_unlink,
  .rename     = insight_rename,
  .link       = insight_link,
  .chmod      = insight_chmod,
  .chown      = insight_chown,
  .truncate   = insight_truncate,
#if FUSE_VERSION >= 26
  .utimens    = insight_utimens,
#else
  .utime      = insight_utime,
#endif
  .write      = insight_write,
#if FUSE_VERSION >= 25
  .statfs     = insight_statvfs,
#else
  .statfs     = insight_statfs,
#endif
  .release    = insight_release,
  .fsync      = insight_fsync,
#ifdef HAVE_SETXATTR
  .setxattr   = insight_setxattr,
  .getxattr   = insight_getxattr,
  .listxattr  = insight_listxattr,
  .removexattr= insight_removexattr,
#endif
  .access     = insight_access,
  .init       = insight_init,
};

static int usage_printed = 0;

static int tag_ensure_create(const char *tag) {
  profile_init_start();
  fileptr attrid;
  size_t len = strlen(tag);
  char *curtag=calloc(len+1, sizeof(char)); /* TODO: check for failure */
  strcpy(curtag, tag);

  /* loop backwards to find the closest parent tag that doesn't exist */
  /* i.e. check foo`bar`qux then foo`bar then foo */
  DEBUG("Checking \"%s\"", curtag);
  int found=1;
  while (!(attrid=get_tag(curtag))) {
    char *t = rindex(curtag, INSIGHT_SUBKEY_SEP_C);
    if (t) {
      *t='\0';
    } else {
      found=0;
      break;
    }
    DEBUG("Checking \"%s\"", curtag);
  }

  if (!attrid) {
    attrid = tree_get_root();
  }

  do {
    /* if found is set then curtag exists... so we skip it and go on to its child */
    if (!found) {
      tdata datan;

      /* create curtag */
      DEBUG("Creating tag \"%s\" as a child of block %lu", curtag, attrid);

      char *the_tag = strlast(curtag, INSIGHT_SUBKEY_SEP_C);
      initDataNode(&datan);
      if (!tree_sub_insert(attrid, the_tag, (tblock*)&datan)) {
        PMSG(LOG_ERR, "Tree insertion failed");
        profile_stop();
        return -ENOSPC;
      }
      DEBUG("Successfully inserted \"%s\" into parent", curtag);
      ifree(the_tag);

      attrid = get_tag(curtag);
      if (!attrid) {
        PMSG(LOG_ERR, "Could not find the tag I just created!");
        ifree(curtag);
        profile_stop();
        return -EIO;
      }
    } else {
      found=0;
    }
    /* if we're done, exit... */
    if (strlen(curtag)==len) break;
    /* advance string */
    curtag[strlen(curtag)]=INSIGHT_SUBKEY_SEP_C;
  } while (strlen(curtag) <= len);

  ifree(curtag);
  profile_stop();
  return attrid;
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

static int attr_add(fileptr inode, fileptr attrid) {
  profile_init_start();
  DEBUG("attr_add(inode: %08lx, attrid: %lu)", inode, attrid);
  char s_hash[9];
  sprintf(s_hash, "%08lX", inode);

  if (!attrid) {
    DEBUG("Cannot add null attribute");
    profile_stop();
    return -ENOENT;
  }

  DEBUG("Searching limbo");
  int searchres=limbo_search(inode);

  if (searchres<0) {
    DEBUG("Error searching limbo");
    profile_stop();
    return searchres;
  }

  /* if (inode in limbo)             */
  if (searchres) {
    /* remove from limbo;            */
    DEBUG("Removing inode from limbo");
    if ((errno=inode_remove(0, inode))) {
      PMSG(LOG_ERR, "IO error: Failed to remove inode from limbo: %s", strerror(errno));
      profile_stop();
      return -EIO;
    }

    /* add to inode tree with attrid */
    DEBUG("Adding entry to inode tree");
    tidata iblock;
    initInodeDataBlock(&iblock);
    if (!tree_sub_insert(tree_get_iroot(), s_hash, (tblock*)&iblock)) {
      PMSG(LOG_ERR, "Tree insertion failed");
      profile_stop();
      return -ENOSPC;
    }

    /* add to attribute list         */
    DEBUG("Adding to attribute list");
    int res = inode_insert(attrid, inode);
    if (res) {
      DEBUG("Inode addition failed: %s", strerror(res));
      if (res==ENOENT) res=EPERM;
      profile_stop();
      return -res;
    }
  } else {
    /* add attrid to inode tree      */
    tidata iblock;
    fileptr piblock;

    DEBUG("Adding to inode tree");
    piblock = tree_sub_search(tree_get_iroot(), s_hash);

    if (tree_read(piblock, (tblock*)&iblock)) {
      PMSG(LOG_ERR, "IO error reading inode data block");
      profile_stop();
      return -EIO;
    }

    iblock.refs[iblock.refcount] = inode;
    iblock.refcount++;

    qsort(iblock.refs, iblock.refcount, sizeof(fileptr), inodecmp);

    if (tree_write(piblock, (tblock*)&iblock)) {
      PMSG(LOG_ERR, "IO error writing inode data block");
      profile_stop();
      return -EIO;
    }

    /* add to attribute list         */
    DEBUG("Adding to attribute list");
    int res = inode_insert(attrid, inode);
    if (res) {
      DEBUG("Inode insertion failed: %s", strerror(res));
      if (res==ENOENT) res=EPERM;
      profile_stop();
      return -res;
    }
  }
  profile_stop();
  return 0;
}

static int attr_addbyname(fileptr inode, const char *attr) {
  profile_init_start();
  if (!attr || !*attr) {
    profile_stop();
    return -EINVAL;
  }
  fileptr attrid = get_tag(attr);
  if (!attrid) {
    profile_stop();
    return -ENOENT;
  }
  profile_stop();
  return attr_add(inode, attrid);
}

static int attr_addbyname_rec(fileptr inode, const char *attr) {
  profile_init_start();
  if (!attr || !*attr) {
    profile_stop();
    return -EINVAL;
  }

  fileptr attrid = get_tag(attr);
  if (attrid) {
    /* easy case - tag exists */
    profile_stop();
    return attr_add(inode, attrid);
  }

  attrid = tag_ensure_create(attr);

  profile_stop();
  return attr_addbyname(inode, attr);
}

static int auto_attr_addbyname_rec(fileptr inode, const char *attr) {
  /* make attribute an "automatic" one */
  char *auto_attr=calloc(strlen(attr)+2, sizeof(char));
  strcat(auto_attr, "_");
  strcat(auto_attr, attr);
  int res=attr_addbyname_rec(inode, auto_attr);
  ifree(auto_attr);
  return res;
}

static int attr_del(fileptr inode, fileptr attrid) {
  profile_init_start();
  DEBUG("attr_del(%08lx, %lu)", inode, attrid);
  char s_hash[9];
  sprintf(s_hash, "%08lX", inode);

  if (!attrid) {
    DEBUG("Searching limbo");
    int searchres=limbo_search(inode);

    if (searchres<0) {
      DEBUG("Error searching limbo");
      profile_stop();
      return searchres;
    }

    /* if (inode in limbo)              */
    if (searchres) {
      DEBUG("Repos symlink therefore: %s", s_hash);

      char *finaldest = gen_repos_path(s_hash, 1);
      if (!finaldest) {
        PMSG(LOG_ERR, "IO error: Failed to generate repository path");
        profile_stop();
        return -EIO;
      }

      struct stat s;
      if (stat(finaldest, &s) == -1) {
        /* file does not exist or error finding it... */
        if (errno==ENOENT) {
          PMSG(LOG_ERR, "The repository symlink has vanished!");
          ifree(finaldest);
          profile_stop();
          return -EIO;
        } else {
          PMSG(LOG_ERR, "Error while stat()ing %s: %s", finaldest, strerror(errno));
          ifree(finaldest);
          profile_stop();
          return -EIO;
        }
      }


      /* remove from limbo;             */
      DEBUG("Removing inode from tree at root level (for now)");
      if ((errno=inode_remove(0, inode))) {
        PMSG(LOG_ERR, "IO error: Failed to remove inode from tree: %s", strerror(errno));
        ifree(finaldest);
        profile_stop();
        return -EIO;
      }

      /* unlink symlink                 */
      DEBUG("Final unlink call: unlink(\"%s\")", finaldest);
      if (unlink(finaldest)==-1) {
        PMSG(LOG_ERR, "IO error: Symlink removal failed: %s", strerror(errno));
        ifree(finaldest);
        profile_stop();
        return -EIO;
      }

      ifree(finaldest);
    } else {

      DEBUG("Not in limbo");
      profile_stop();
      return -ENOENT;
    }

  } else {
    /* remove from attribute list     */
    DEBUG("Removing from inode list of attribute %08lx", attrid);
    int res = inode_remove(attrid, inode);
    if (res) {
      DEBUG("Inode removal failed: %s", strerror(res));
      if (res==ENOENT) res=EPERM;
      profile_stop();
      return -res;
    }
    DEBUG("Removed.");

    /* remove attrid from inode tree  */
    DEBUG("Removing attribute ref from inode tree");
    tidata iblock;
    fileptr piblock;

    DEBUG("Fetching inode tree data block");
    if (tree_get_iroot()) {
      piblock = tree_sub_search(tree_get_iroot(), s_hash);

      if (tree_read(piblock, (tblock*)&iblock)) {
        PMSG(LOG_ERR, "IO error reading inode data block");
        profile_stop();
        return -EIO;
      }

      /* find ref */
      DEBUG("Finding ref...");
      /* TODO: make this binary search */
      int i;
      for (i=0; i<iblock.refcount; i++) {
        if (iblock.refs[i]==attrid) break;
      }

      if (i>=iblock.refcount) {
        /* not found */
        DEBUG("Could not find ref!");
        profile_stop();
        return -ENOENT;
      }

      DEBUG("Removing node...");
      /* remove node */
      iblock.refcount--;
      while (i++<iblock.refcount) {
        iblock.refs[i-1]=iblock.refs[i];
      }

      /* if (inode attr list not empty) {   */
      if (iblock.refcount) {
        if (tree_write(piblock, (tblock*)&iblock)) {
          PMSG(LOG_ERR, "IO error writing inode data block");
          profile_stop();
          return -EIO;
        }

      } else {
        DEBUG("Inode attribute list now empty");
        /* remove entry from inode tree */
        DEBUG("Removing entry from inode tree");
        if ((errno=tree_sub_remove(tree_get_iroot(), s_hash))) {
          PMSG(LOG_ERR, "Tree removal failed with error: %s", strerror(-res));
          profile_stop();
          return -EIO;
        }
        /* insert inode into limbo list */
        DEBUG("Inserting inode into limbo");
        if ((errno=inode_insert(0, inode))) {
          PMSG(LOG_ERR, "IO error: Failed to insert inode into limbo: %s", strerror(errno));
          profile_stop();
          return -EIO;
        }
      }
    }
  }

  DEBUG("All done.");
  profile_stop();
  return 0;
}

static int attr_delbyname(fileptr inode, const char *attr) {
  profile_init_start();
  if (!attr || !*attr) {
    return -EINVAL;
  }
  fileptr attrid = get_tag(attr);
  if (!attrid) {
    return -ENODATA;
  }
  profile_stop();
  return attr_del(inode, attrid);
}

static int auto_attr_delbyname(fileptr inode, const char *attr) {
  /* make attribute an "automatic" one */
  char *auto_attr=calloc(strlen(attr)+2, sizeof(char));
  strcat(auto_attr, "_");
  strcat(auto_attr, attr);
  int res=attr_delbyname(inode, auto_attr);
  ifree(auto_attr);
  return res;
}


static int insight_getattr(const char *path, struct stat *stbuf) {
  profile_init_start();
  DEBUG("Getattr called on path \"%s\"", path);

  DEBUG("Generating query tree");
  qelem *q = path_to_query(path);

  if (!q) {
    if (errno == ENOENT) {
      DEBUG("Path not valid");
    } else {
      DEBUG("Error in generating query tree: %s", strerror(errno));
    }
    profile_stopf("path: %s", path);
    return -errno;
  }

  struct stat fstat;
  char *canon_path = get_canonical_path(path);
  char *last = strlast(canon_path+1, '/');

  DEBUG("Getattr called on path \"%s\"", canon_path);

  if (have_file_by_name(last, &fstat)) {
    ifree(last);
    DEBUG("Got a file\n");

    memcpy(stbuf, &fstat, sizeof(struct stat));
    qtree_free(&q, 1);
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return 0;

  } else if (validate_path(canon_path)) {
    ifree(last);

    /* steal a default stat structure */
    memcpy(stbuf, &insight.mountstat, sizeof(struct stat));

    /* last modified times are the same as the tree last modified time */
    stbuf->st_atime = stbuf->st_ctime = stbuf->st_mtime = tree_get_mtime();

    /* special case: root */
    if (strcmp(canon_path, "/")==0) {
      DEBUG("Getattr called on root path\n");
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = tree_key_count();
      stbuf->st_ino = 1;
      qtree_free(&q, 1);
      ifree(canon_path);
      profile_stopf("path: %s", path);
      return 0;
    }

    if (strlen(path)>2 && last_char_in(path)==INSIGHT_SUBKEY_IND_C && path[strlen(path)-2]=='/') {
      /* just a subkey indicator directory */
      DEBUG("Getattr on a subkey indicator directory");
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 1;
      char *tmpp = calloc(strlen(canon_path)+2, sizeof(char));
      if (!tmpp) {
        PMSG(LOG_ERR, "Failed to allocate temporary space for hash path");
        ifree(canon_path);
        qtree_free(&q, 1);
        profile_stopf("path: %s", path);
        return -ENOMEM;
      }
      strcpy(tmpp, canon_path);
      strcat(tmpp, ":");
      stbuf->st_ino = hash_path(tmpp, strlen(tmpp));
      ifree(tmpp);
    } else {
      int subtag=0;
      fileptr tagdata;
      tdata dnode;

      if (last_char_in(canon_path)==INSIGHT_SUBKEY_IND_C) {
        last_char_in(canon_path)='\0';
        subtag=1;
      }

      tagdata=get_last_tag(canon_path+1);

      if (!tagdata) {
        DEBUG("Tag \"%s\" not found\n", canon_path+1);
        qtree_free(&q, 1);
        ifree(canon_path);
        profile_stopf("path: %s", path);
        return -ENOENT;
      }
      DEBUG("Found tag \"%s\"", canon_path+1);
      if (tree_read(tagdata, (tblock*)&dnode)) {
        PMSG(LOG_ERR, "IO error reading data block");
        qtree_free(&q, 1);
        ifree(canon_path);
        profile_stopf("path: %s", path);
        return -EIO;
      }

      stbuf->st_mode = 0555;

      if (dnode.flags & DATA_FLAGS_SYNONYM) {
        stbuf->st_mode |= S_IFLNK;
        /* must set symlink size too - length of string without final null */
        stbuf->st_size = strlen(dnode.target);
      } else {
        stbuf->st_mode |= S_IFDIR;
        if (dnode.flags & DATA_FLAGS_NOSUB) {
          /* add sticky bit */
          stbuf->st_mode |= S_ISVTX;
        }
      }
      stbuf->st_nlink = 1;
      /* provide probably unique inodes for directories */
      stbuf->st_ino = hash_path(canon_path, strlen(canon_path));
    }
  } else {
    DEBUG("Path does not exist\n");
    ifree(last);
    qtree_free(&q, 1);
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return -ENOENT;
  }
  DEBUG("Returning\n");
  qtree_free(&q, 1);
  ifree(canon_path);
  profile_stopf("path: %s", path);
  return 0;
}

static int insight_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
  profile_init_start();
  (void) offset;
  (void) fi;
  tnode node;
  int i;
  char *canon_path = get_canonical_path(path);

  DEBUG("readdir(path=\"%s\", buf=%p, offset=%lld)", canon_path, buf, offset);

  if (!validate_path(canon_path)) {
    DEBUG("Path does not exist\n");
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return -ENOENT;
  }

  char *last_tag=calloc(255, sizeof(char));

  DEBUG("Generating query tree");
  qelem *q = path_to_query(path); /* TODO: check for failure */

  DEBUG("Finding subtag parent...");
  (void)query_get_subtags(q, last_tag, 255);
  DEBUG("Subtag parent: \"%s\"", last_tag);

  fileptr tree_root=tree_get_root();

  if (*last_tag && (tree_root=get_tag(last_tag))==0) {
    DEBUG("Tag \"%s\" not found\n", last_tag);
    qtree_free(&q, 1);
    ifree(last_tag);
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return -ENOENT;
  }
  DEBUG("Found tag \"%s\"; tree root now %lu", last_tag, tree_root);

  DEBUG("Filling directory");
  if (!filler) {
    PMSG(LOG_ERR, "Filler function is undefined!\n");
    qtree_free(&q, 1);
    ifree(last_tag);
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return -EIO;
  }
  if (!buf) {
    PMSG(LOG_ERR, "Destination buffer is undefined!\n");
    qtree_free(&q, 1);
    ifree(last_tag);
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return -EIO;
  }
  DEBUG("Calling filler on dot");
  filler(buf, ".", NULL, 0);
  DEBUG("Managed .");
  filler(buf, "..", NULL, 0);
  DEBUG("Managed ..");

  /* if subtag indicator then no files should be listed */
  if (!*last_tag) {
    int inodecount, neg;
    DEBUG("Fetching inode list");
    fileptr *inodelist = query_to_inodes(q, &inodecount, &neg);

    DEBUG("query_to_inodes() has returned");
    if (!inodelist) {
      PMSG(LOG_ERR, "Error fetching inode list from query tree\n");
      qtree_free(&q, 1);
      ifree(last_tag);
      ifree(canon_path);
      profile_stopf("path: %s", path);
      return -EIO;
    }
    DEBUG("%d inodes to consider", inodecount);

    for (i=0; i<inodecount; i++) {
      char *str = basename_from_inode(inodelist[i]);
      if (str) {
        DEBUG("Adding filename \"%s\" to listing", str);
        filler(buf, str, NULL, 0);
      } else {
        FMSG(LOG_ERR, "Error getting filename for inode %08lX", inodelist[i]);
      }
      ifree(str);
    }

    DEBUG("Freeing inode array");
    ifree(inodelist);
  }

  DEBUG("Freeing query tree...");
  qtree_free(&q, 1);

  /* And now for directories */

  if (tree_sub_get_min(tree_root, &node)) {
    PMSG(LOG_ERR, "IO error: tree_sub_get_min() failed\n");
    ifree(last_tag);
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return -EIO;
  }

  if (node.magic != MAGIC_TREENODE) {
    if (*last_tag)
      DEBUG("No subtags");
    else
      PMSG(LOG_ERR, "Something went very, very wrong\n");
    ifree(last_tag);
    ifree(canon_path);
    profile_stopf("path: %s", path);
    return 0;
  }

  DEBUG("Should we add subtag dir?");
  /* add special directory containing subkeys of this key */
  if (strcmp(path, "/") != 0 && !*last_tag) {
    DEBUG("Getting last fragment");
    char *lastbit = strlast(canon_path, '/');
    tnode n;

    n.magic=0;
    /* but only if it actually has subkeys */
    DEBUG("Getting tag for \"%s\"", lastbit);
    tree_root=get_tag(lastbit);

    DEBUG("Calling tree_sub_get_min(%lu)", tree_root);
    int res=tree_sub_get_min(tree_root, &n);
    if (res && res != ENOENT) {
      PMSG(LOG_ERR, "IO error: tree_sub_get_min() failed: %s\n", strerror(errno));
      ifree(lastbit);
      ifree(last_tag);
      ifree(canon_path);
      profile_stopf("path: %s", path);
      return -EIO;
    }

    DEBUG("Checking magic");
    if (n.magic==MAGIC_TREENODE) {
      DEBUG("Adding subkey indicator directory");
      filler(buf, INSIGHT_SUBKEY_IND, NULL, 0);
    }

    ifree(lastbit);
  }

  /* TODO: change to use tree_map_keys() */
  /* TODO: new algorithm:
   *         - for each path component (each item in bits):
   *           - add (with all ancestors) to PATHS set
   *         - for each element in PATHS:
   *           - union set of all (fully-qualified) siblings with TAGS set
   *           - go up to parent tag, repeat
   *         - result = TAGS - PATHS
   */

  int count, k, isunique=1;
  char **bits = strsplit(canon_path, '/', &count);

  do {
    for (i=0; i<node.keycount; i++) {
      if (*last_tag) {
        char *incomplete_test=calloc(strlen(last_tag)+strlen(node.keys[i])+2, sizeof(char));
        strcpy(incomplete_test, last_tag);
        strcat(incomplete_test, INSIGHT_SUBKEY_SEP);
        strcat(incomplete_test, node.keys[i]);
        for (k=0, isunique=1; k<count && isunique; k++) {
          isunique &= (strcmp(node.keys[i], bits[k])!=0)?1:0;
          isunique &= (strcmp(incomplete_test, bits[k])!=0)?1:0;
        }
        ifree(incomplete_test);
      } else {
        for (k=0, isunique=1; k<count && isunique; k++) {
          isunique &= (strcmp(node.keys[i], bits[k])!=0)?1:0;
        }
      }

      if (isunique) {
        DEBUG("Adding key \"%s\" to directory listing", node.keys[i]);
        filler(buf, node.keys[i], NULL, 0);
      } else {
        DEBUG("Key \"%s\" is already in path", node.keys[i]);
      }
    }
    if (node.ptrs[0] && tree_read(node.ptrs[0], (tblock*) &node)) {
      PMSG(LOG_ERR, "I/O error reading block\n");
      ifree(last_tag);
      ifree(canon_path);
      profile_stopf("path: %s", path);
      return -EIO;
    }
  } while (node.ptrs[0]);

  ifree(last_tag);
  ifree(canon_path);

  for (count--;count>=0; count--) {
    ifree(bits[count]);
  }
  ifree(bits);

  DEBUG("Done\n");

  profile_stopf("path: %s", path);
  return 0;
}

static int insight_mkdir(const char *path, mode_t mode) {
  (void) mode;

  char *canon_path = get_canonical_path(path);
  char *newtag = strlast(canon_path, '/');
  char *newdir = strlast(path, '/');
  char *dup = strdup(path);
  char *canon_parent = rindex(dup, '/');
  *canon_parent='\0';
  canon_parent = get_canonical_path(dup);
  ifree(dup);

  if (canon_parent[0]=='/')
    canon_parent++;

  if (canon_path[0]=='/')
    canon_path++;

  if (strcmp(newdir, INSIGHT_SUBKEY_IND)==0) {
    PMSG(LOG_WARNING, "Cannot create directories named \"%s\"", INSIGHT_SUBKEY_IND);
    return -EPERM;
  }

  if (*newdir == '_') {
    PMSG(LOG_WARNING, "Cannot create directories that begin with an underscore");
    return -EPERM;
  }

  while (*newdir && last_char_in(newdir)==INSIGHT_SUBKEY_IND_C) {
    DEBUG("Subkey indicator removed from end of new directory name");
    last_char_in(newdir)='\0';
  }

  if (!*newdir) {
    DEBUG("Invalid (empty) directory name");
    return -EPERM;
  }

  DEBUG("Asked to create directory \"%s\" in parent \"%s\"", newdir, canon_parent);

  if (!validate_path(canon_parent)) {
    DEBUG("Path does not exist\n");
    return -ENOENT;
  }

  fileptr tree_root=tree_get_root();
  int subtag=0;
  char *parent_tag=rindex(canon_parent, '/');

  if (!parent_tag) parent_tag=canon_parent;
  else             parent_tag++;

  while (last_char_in(parent_tag)==INSIGHT_SUBKEY_IND_C) {
    last_char_in(parent_tag)='\0';
    subtag=1;
  }

  while (last_char_in(newdir)==INSIGHT_SUBKEY_IND_C) {
    last_char_in(newdir)='\0';
    subtag=1;
  }

  while (last_char_in(newtag)==INSIGHT_SUBKEY_IND_C) {
    last_char_in(newtag)='\0';
    subtag=1;
  }

  while (last_char_in(canon_path)==INSIGHT_SUBKEY_IND_C) {
    last_char_in(canon_path)='\0';
    subtag=1;
  }

  if (*parent_tag && (tree_root=get_tag(parent_tag))==0) {
    DEBUG("Tag \"%s\" not found", parent_tag);
    return -ENOENT;
  }
  DEBUG("Found tag \"%s\"; tree root now %lu", parent_tag, tree_root);

  if (tree_sub_search(tree_root, newdir)) {
    DEBUG("Tag \"%s\" already exists in parent \"%s\"\n", newdir, parent_tag);
    return -EEXIST;
  }

  DEBUG("About to insert \"%s\" into parent \"%s\"", newdir, parent_tag);

  /*
  initDataNode(&datan);
  if (!tree_sub_insert(tree_root, newdir, (tblock*)&datan)) {
    PMSG(LOG_ERR, "Tree insertion failed");
    return -ENOSPC;
  }
  */

  DEBUG("Creating tag \"%s\"", newtag);
  if (!tag_ensure_create(newtag)) {
    PMSG(LOG_ERR, "Tree insertion failed");
    return -ENOSPC;
  }

  DEBUG("Successfully inserted \"%s\" into parent \"%s\"", newdir, parent_tag);

  return 0;
}

static int insight_rmdir(const char *path) {
  char *olddir = strlast(path, '/');
  char *dup = strdup(path);
  char *canon_parent = rindex(dup, '/');
  *canon_parent='\0';
  canon_parent = get_canonical_path(dup);
  ifree(dup);

  if (canon_parent[0]=='/')
    canon_parent++;

  if (strcmp(olddir, INSIGHT_SUBKEY_IND)==0) {
    PMSG(LOG_WARNING, "Cannot remove directories named \"%s\"\n", INSIGHT_SUBKEY_IND);
    return -EPERM;
  }

  while (*olddir && last_char_in(olddir)==INSIGHT_SUBKEY_IND_C) {
    DEBUG("Subkey indicator removed from end of new directory name");
    last_char_in(olddir)='\0';
  }

  if (!*olddir) {
    DEBUG("Invalid (empty) directory name\n");
    return -EPERM;
  }

  DEBUG("Asked to remove directory \"%s\" in parent \"%s\"", olddir, canon_parent);

  if (!validate_path(canon_parent)) {
    DEBUG("Path does not exist\n");
    return -ENOENT;
  }

  fileptr tree_root=tree_get_root();
  int subtag=0;
  char *parent_tag=rindex(canon_parent, '/');

  if (!parent_tag) parent_tag=canon_parent;
  else             parent_tag++;

  while (last_char_in(parent_tag)==INSIGHT_SUBKEY_IND_C) {
    last_char_in(parent_tag)='\0';
    subtag=1;
  }

  if (*parent_tag && (tree_root=get_tag(parent_tag))==0) {
    DEBUG("Tag \"%s\" not found\n", parent_tag);
    return -ENOENT;
  }
  DEBUG("Found tag \"%s\"; tree root now %lu", parent_tag, tree_root);

  if (!tree_sub_search(tree_root, olddir)) {
    DEBUG("Tag \"%s\" does not exist in parent \"%s\"\n", olddir, parent_tag);
    return -ENOENT;
  }

  DEBUG("About to remove \"%s\" from parent \"%s\"", olddir, parent_tag);

  int res=tree_sub_remove(tree_root, olddir);

  if (res<0) {
    PMSG(LOG_ERR, "Tree removal failed with error: %s\n", strerror(-res));
    return res;
  } else if (res) {
    PMSG(LOG_ERR, "IO error: Tree removal failed\n");
    return -EIO;
  }
  DEBUG("Successfully removed \"%s\" from parent \"%s\"\n", olddir, parent_tag);

  return 0;
}

static int insight_unlink(const char *path) {
  unsigned long hash;
  char *canon_path = get_canonical_path(path);

  /* TODO: check paths are valid and exist! */

  DEBUG("Unlink \"%s\"", canon_path);

  if (!*canon_path) {
    FMSG(LOG_ERR, "Asked to unlink file with null path!");
    ifree(canon_path);
    return -EINVAL;
  }

  if (strcount(canon_path+1, '/')>1) {
    DEBUG("Only makes sense to remove from single-level directories");
    ifree(canon_path);
    return -EPERM;
  }

  /* IMPORTANT NOTE: rm is also called for symlinks but we don't deal with that
   * at all. Should check for tag existing first, and if it exists check its
   * flags and remove as appropriate. */

  fileptr attrid=0;
  int count;
  char **bits = strsplit(canon_path, '/', &count);

  while (!*(bits[count-1])) {
    ifree(bits[count-1]);
    count--;
  }

  /* calculate an inode number for the path */
  DEBUG("Hashing \"%s\"...", bits[count-1]);
  hash = hash_path(bits[count-1], strlen(bits[count-1]));

  if (count>1) {
    DEBUG("Getting block for \"%s\"", bits[count-2]);
    attrid = get_tag(bits[count-2]);
    DEBUG("Block for \"%s\" is %lu", bits[count-2], attrid);
  } else {
    /* we're at the root */
    attrid=0;
  }

  for (count--;count>=0; count--) {
    ifree(bits[count]);
  }
  ifree(bits);
  ifree(canon_path);

  /* remove attribute */
  int res = attr_del(hash, attrid);

  if (res<0) {
    DEBUG("Error deleting attribute");
    return res;
  }

  return 0;
}

static int insight_mknod(const char *path, mode_t mode, dev_t rdev) {
  (void) rdev;

  if (!S_ISREG(mode)) {
    DEBUG("Cannot create non-files");
    return -EPERM;
  }

  char *canon_path = get_canonical_path(path)+1;

  DEBUG("Mknod: \"%s\"", canon_path);
  int res=0;
  int count;
  char **bits = strsplit(canon_path, '/', &count);
  canon_path--;
  ifree(canon_path);

  if (count<2) {
    DEBUG("Need a tag");
    res = -EPERM;
  } else {
    DEBUG("Assigning tag \"%s\"", bits[count-2]);

    DEBUG("Last returned \"%s\"", bits[count-1]);

    unsigned long hash = hash_path(bits[count-1], strlen(bits[count-1]));
    DEBUG("Which gives us hash %08lx", hash);

    struct stat fstat;

    if (have_file_by_hash(hash, &fstat)) {
      res = attr_addbyname(hash, bits[count-2]);
      if (res) {
        DEBUG("Error tagging! %s", strerror(-res));
        return res;
      }

      DEBUG("Done");
    } else {
      DEBUG("Cannot create the file");
      res = -EPERM;
    }
  }

  for (count--;count>=0; count--) {
    ifree(bits[count]);
  }
  ifree(bits);
  return res;
}

static int insight_rename(const char *from, const char *to) {
  (void) from;
  (void) to;

  DEBUG("Rename path from \"%s\" to \"%s\"", from, to);
  return -ENOTSUP;
}

static int insight_readlink(const char *path, char *buf, size_t size) {
  char *canon_path = get_canonical_path(path);
  if (errno) {
    DEBUG("Error in get_canonical_path.");
    return -ENOENT;
  }

  DEBUG("Read symlink target for \"%s\"", canon_path);

  if (!validate_path(canon_path)) {
    DEBUG("Path does not exist\n");
    return -ENOENT;
  }

  if (last_char_in(canon_path)==INSIGHT_SUBKEY_IND_C) {
    last_char_in(canon_path)='\0';
  }

  fileptr tagdata=get_last_tag(canon_path+1);

  if (!tagdata) {
    DEBUG("Tag \"%s\" not found\n", canon_path+1);
    return -ENOENT;
  }
  DEBUG("Found last tag in \"%s\"", canon_path+1);

  tdata dnode;
  if (tree_read(tagdata, (tblock*)&dnode)) {
    PMSG(LOG_ERR, "IO error reading data block");
    return -EIO;
  }

  if (dnode.flags & DATA_FLAGS_SYNONYM) {
    unsigned int ssize=strlen(dnode.target);
    strncpy(buf, dnode.target, MIN(size,ssize));
  } else {
    /* not a symlink */
    DEBUG("Requested tag \"%s\" is not a symlink.", canon_path+1);
    return -EINVAL;
  }

  return 0;
}

static int insight_symlink(const char *from, const char *to) {
  unsigned long hash;
  char s_hash[9];
  char *finaldest;

  DEBUG("Create symlink from \"%s\" to \"%s\"", from, to);
  if (!from) {
    FMSG(LOG_ERR, "Asked to import file with null target");
    return -EINVAL;
  }
  if (*from!='/') {
    FMSG(LOG_ERR, "Can only import files with absolute paths");
    return -EPERM;
  }

  /* calculate an inode number for the path */
  DEBUG("Hashing \"%s\"...", to+1);
  hash = hash_path(to+1, strlen(to+1));
  sprintf(s_hash, "%08lX", hash);
  DEBUG("Repos target therefore: %s -> %s", s_hash, from);

  finaldest = gen_repos_path(s_hash, 1);
  if (!finaldest) {
    PMSG(LOG_ERR, "IO error: Failed to generate repository path");
    return -EIO;
  }

  struct stat s;
  if (!((stat(finaldest, &s) == -1) && (errno == ENOENT))) {
    /* file exists or error finding it... */
    if (errno) {
      PMSG(LOG_ERR, "Some other error happened while stat()ing %s: %s", finaldest, strerror(errno));
      ifree(finaldest);
      return -EIO;
    } else {
      DEBUG("Tried to create a file that already exists");
      ifree(finaldest);
      return -EEXIST;
    }
  }

  DEBUG("Final symlink call: symlink(\"%s\", \"%s\")", from, finaldest);

  if (symlink(from, finaldest)==-1) {
    PMSG(LOG_ERR, "IO error: Symlink creation failed: %s", strerror(errno));
    ifree(finaldest);
    return -EIO;
  }
  ifree(finaldest);

  DEBUG("Inserting inode into tree at root level (for now)");
  if ((errno=inode_insert(0, hash))) {
    PMSG(LOG_ERR, "IO error: Failed to insert inode into tree: %s", strerror(errno));
    return -EIO;
  }

  DEBUG("Calling plugins for attribute assignments...");
  /* TODO: check result - not that there's much we can do */
  plugin_process_import(from, to+1);

  /*
  char attr[255];

  DEBUG("  Statting...");
  if (stat(from, &s)==-1) {
    PMSG(LOG_ERR, "Failed to stat the structure");
    return -EIO;
  }

  DEBUG("  Fetching username from uid %u", s.st_uid);
  struct passwd *p=getpwuid(s.st_uid);
  if (p) {
    DEBUG("  ... \"%s\"", p->pw_name);
    sprintf(attr, "_owner%s%s", INSIGHT_SUBKEY_SEP, p->pw_name);
    DEBUG("  ... attr_addbyname(%lu, \"%s\");", hash, attr);
    attr_addbyname_rec(hash, attr);
    endpwent();
  } else {
    PMSG(LOG_ERR, "Failed to get username for uid %u", s.st_uid);
  }

  DEBUG("  Fetching group name from gid %u", s.st_gid);
  struct group *g=getgrgid(s.st_gid);
  if (g) {
    DEBUG("  ... \"%s\"", g->gr_name);
    sprintf(attr, "_group%s%s", INSIGHT_SUBKEY_SEP, g->gr_name);
    DEBUG("  ... attr_addbyname(%lu, \"%s\");", hash, attr);
    attr_addbyname_rec(hash, attr);
    endgrent();
  } else {
    PMSG(LOG_ERR, "Failed to get group name for gid %u", s.st_gid);
  }

  char *basename = strlast(from, '/');
  char *ext = strlast(basename, '.');
  if (strcmp(ext, basename)==0) {
    *ext='\0';
  }
  DEBUG("  File extension: \"%s\"", ext);
  if (*ext) {
    sprintf(attr, "_ext%s%s", INSIGHT_SUBKEY_SEP, ext);
    DEBUG("  ... attr_addbyname(%lu, \"%s\");", hash, attr);
    attr_addbyname_rec(hash, attr);
  }

  ifree(basename);
  ifree(ext);
  */

  DEBUG("Done.");

  return 0;
}

static int insight_link(const char *from, const char *to) {
  unsigned long hash;
  char *canon_from = get_canonical_path(from+1);
  char *canon_to   = get_canonical_path(to)+1;

  /* TODO: check paths are valid and exist! */

  DEBUG("Create hard link from \"%s\" to \"%s\"", canon_from, canon_to);

  if (!*canon_from || !*canon_to) {
    FMSG(LOG_ERR, "Asked to link file with null from/to path!");
    return -EINVAL;
  }

  if (strcount(canon_to+1, '/') != 1) {
    DEBUG("Only makes sense to apply a single tag at a time, and must have at least one!");
    return -EPERM;
  }

  char *last_bit = strlast(canon_from, '/');
  /* calculate an inode number for the path */
  DEBUG("Hashing \"%s\"...", last_bit);
  hash = hash_path(last_bit, strlen(last_bit));
  ifree(canon_from);

  int count;
  char **bits = strsplit(canon_to, '/', &count);

  DEBUG("Getting block for \"%s\"", bits[count-2]);
  fileptr attrid = get_tag(bits[count-2]);
  DEBUG("Block for \"%s\" is %lu", bits[count-2], attrid);

  for (count--;count>=0; count--) {
    ifree(bits[count]);
  }
  ifree(bits);

  /* isnert attribute */
  int res = attr_add(hash, attrid);

  if (res<0) {
    DEBUG("Error inserting attribute");
    return res;
  }

  /* XXX: segfaults */
  /* ifree(canon_from); */
  /* ifree(canon_to); */

  return 0;
}

static int insight_chmod(const char *path, mode_t mode) {
  char *canon_path = get_canonical_path(path);

  DEBUG("Change mode of \"%s\" to %05o", canon_path, mode & 07777);

  struct stat fstat;
  char *last = strlast(canon_path+1, '/');
  unsigned long hash = hash_path(last, strlen(last));

  if (have_file_by_hash(hash, &fstat)) {
    char *fullname = fullname_from_inode(hash);
    DEBUG("Changing mode of real file: %s", fullname);
    if (chmod(fullname, mode)==-1) {
      PMSG(LOG_ERR, "chmod(\"%s\", %05o) failed: %s", fullname, mode, strerror(errno));
      ifree(fullname);
      return -errno;
    } else {
      ifree(fullname);
      return 0;
    }
  } else if (validate_path(canon_path)) {
    char *last_tag = strlast(canon_path, '/');
    int is_sticky = (mode & S_ISVTX);
    DEBUG("%setting nosub bit of tag: %s", is_sticky?"S":"Uns", last_tag);
    int tagblock = get_tag(last_tag);
    if (!tagblock) {
      PMSG(LOG_ERR, "Serious problem. Valid path but tag not found.");
      ifree(last_tag);
      return -EIO;
    }

    tdata dblock;
    if (tree_read(tagblock, (tblock*)&dblock)) {
      PMSG(LOG_ERR, "I/O error reading block");
      ifree(last_tag);
      return -EIO;
    }
    if (is_sticky) {
      SET_FLAG(dblock.flags, DATA_FLAGS_NOSUB);
    } else {
      CLEAR_FLAG(dblock.flags, DATA_FLAGS_NOSUB);
    }
    if (tree_write(tagblock, (tblock*)&dblock)) {
      PMSG(LOG_ERR, "I/O error writing block");
      ifree(last_tag);
      return -EIO;
    }

    DEBUG("Cannot really change mode of directories though");
    ifree(last_tag);
    return 0;
  } else {
    DEBUG("Could not find path");
    return -ENOENT;
  }
}

static int insight_chown(const char *path, uid_t uid, gid_t gid) {
  char *canon_path = get_canonical_path(path);

  DEBUG("Change ownership of \"%s\" to %d:%d", canon_path, uid, gid);

  struct stat fstat;
  char *last = strlast(canon_path+1, '/');
  unsigned long hash = hash_path(last, strlen(last));

  if (have_file_by_hash(hash, &fstat)) {
    char *fullname = fullname_from_inode(hash);
    DEBUG("Change ownership of real file: %s", fullname);
    if (chown(fullname, uid, gid)==-1) {
      PMSG(LOG_ERR, "chown(\"%s\", %u, %u) failed: %s", fullname, uid, gid, strerror(errno));
      ifree(fullname);
      return -errno;
    } else {
      ifree(fullname);
      return 0;
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot change ownership of directories");
    return -EPERM;
  } else {
    DEBUG("Could not find path");
    return -ENOENT;
  }
}

static int insight_truncate(const char *path, off_t size) {
  char *canon_path = get_canonical_path(path);

  DEBUG("Truncate \"%s\" to %lld bytes", canon_path, size);

  struct stat fstat;
  char *last = strlast(canon_path+1, '/');
  unsigned long hash = hash_path(last, strlen(last));

  if (have_file_by_hash(hash, &fstat)) {
    char *fullname = fullname_from_inode(hash);
    DEBUG("Truncating real file: %s", fullname);
    if (truncate(fullname, size)==-1) {
      PMSG(LOG_ERR, "truncate(\"%s\", %lld) failed: %s", fullname, size, strerror(errno));
      ifree(fullname);
      return -errno;
    } else {
      ifree(fullname);
      return 0;
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot change ownership of directories");
    return -EPERM;
  } else {
    DEBUG("Could not find path");
    return -ENOENT;
  }
}

#if FUSE_VERSION >= 26

static int insight_utimens(const char *path, const struct timespec ts[2]) {
  char *canon_path = get_canonical_path(path);

  DEBUG("Changing times of \"%s\"", path);

  struct stat fstat;
  char *last = strlast(canon_path+1, '/');
  unsigned long hash = hash_path(last, strlen(last));

  if (have_file_by_hash(hash, &fstat)) {
    char *fullname = fullname_from_inode(hash);
    struct timeval tv[2];
    DEBUG("Change times of real file: %s", fullname);
    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;
    if (utimes(fullname, tv)==-1) {
      PMSG(LOG_ERR, "utimes(\"%s\", tv) failed: %s", fullname, strerror(errno));
      ifree(fullname);
      ifree(last);
      ifree(canon_path);
      return -errno;
    } else {
      ifree(fullname);
      ifree(last);
      ifree(canon_path);
      return 0;
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot change times of directories");
    ifree(last);
    ifree(canon_path);
    return -EPERM;
  } else {
    DEBUG("Could not find path");
    ifree(last);
    ifree(canon_path);
    return -ENOENT;
  }
}

#else

static int insight_utime(const char *path, struct utimbuf *buf) {
  char *canon_path = get_canonical_path(path);

  DEBUG("Changing times of \"%s\"", path);

  struct stat fstat;

  if (have_file_by_name(strlast(canon_path+1, '/'), &fstat)) {
    char *fullname = fullname_from_inode(hash_path(canon_path+1, strlen(canon_path)-1));
    DEBUG("Change times of real file: %s", fullname);
    if (utime(fullname, buf)==-1) {
      PMSG(LOG_ERR, "utime(\"%s\", tv) failed: %s", fullname, strerror(errno));
      ifree(fullname);
      return -errno;
    } else {
      ifree(fullname);
      return 0;
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot change times of directories");
    return -EPERM;
  } else {
    DEBUG("Could not find path");
    return -ENOENT;
  }
}

#endif

static int insight_open(const char *path, struct fuse_file_info *fi) {
  profile_init_start();
  char *canon_path = get_canonical_path(path);

  DEBUG("Open on path \"%s\"", canon_path);

  struct stat fstat;
  char *last = strlast(canon_path, '/');

  DEBUG("Last returned \"%s\"", last);

  if (have_file_by_name(last, &fstat)) {
    char *fullname = fullname_from_inode(hash_path(last, strlen(last)));
    int res;
    DEBUG("Opening real file: %s", fullname);
    if ((res=open(fullname, fi->flags|O_RDONLY))==-1) {
      PMSG(LOG_ERR, "open(\"%s\", %d) failed: %s", fullname, fi->flags, strerror(errno));
      ifree(last);
      ifree(fullname);
      ifree(canon_path);
      profile_stop();
      return -errno;
    } else {
      DEBUG("Open succeeded; closing file");
      close(res);
      DEBUG("Closed, freeing last");
      ifree(last);
      DEBUG("Freeing fullname");
      ifree(fullname);
      ifree(canon_path);
      DEBUG("Done");
      profile_stop();
      return 0;
    }
  } else if (validate_path(canon_path)) {
    PMSG(LOG_ERR, "Cannot open directories like files!");
    ifree(last);
    ifree(canon_path);
    profile_stop();
    return -EISDIR;
  } else {
    DEBUG("Could not find path");
    ifree(last);
    ifree(canon_path);
    profile_stop();
    return -ENOENT;
  }
}

static int insight_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  profile_init_start();
  char *canon_path = get_canonical_path(path);

  DEBUG("Read on path \"%s\"", canon_path);

  struct stat fstat;
  char *last = strlast(canon_path, '/');

  if (have_file_by_name(last, &fstat)) {
    unsigned long hash = hash_path(last, strlen(last));
    char *fullname = fullname_from_inode(hash);
    int fd;
    DEBUG("Opening real file: %s", fullname);
    if ((fd=open(fullname, fi->flags|O_RDONLY))==-1) {
      PMSG(LOG_ERR, "open(\"%s\", %d) failed: %s", fullname, fi->flags, strerror(errno));
      ifree(fullname);
      ifree(last);
      ifree(canon_path);
      profile_stop();
      return -errno;
    } else {
      int res, tmp_errno=0;
      res = pread(fd, buf, size, offset);
      tmp_errno=errno;
      close(fd);
      ifree(fullname);
      ifree(last);
      ifree(canon_path);
      profile_stop();
      return (res==-1)?-tmp_errno:res;
    }
  } else if (validate_path(canon_path)) {
    PMSG(LOG_ERR, "Cannot read directories like files!");
    ifree(last);
    ifree(canon_path);
    profile_stop();
    return -EISDIR;
  } else {
    DEBUG("Could not find path");
    ifree(last);
    ifree(canon_path);
    profile_stop();
    return -ENOENT;
  }
}

static int insight_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
  profile_init_start();
  char *canon_path = get_canonical_path(path);

  DEBUG("Write on path \"%s\"", canon_path);

  struct stat fstat;
  char *last = strlast(canon_path, '/');

  if (have_file_by_name(last, &fstat)) {
    unsigned long hash = hash_path(last, strlen(last));
    char *fullname = fullname_from_inode(hash);
    int fd;
    DEBUG("Opening real file: %s", fullname);
    if ((fd=open(fullname, fi->flags|O_WRONLY))==-1) {
      PMSG(LOG_ERR, "open(\"%s\", %d) failed: %s", fullname, fi->flags, strerror(errno));
      ifree(fullname);
      ifree(last);
      ifree(canon_path);
      profile_stop();
      return -errno;
    } else {
      int res, tmp_errno=0;
      res = pwrite(fd, buf, size, offset);
      tmp_errno=errno;
      close(fd);
      ifree(fullname);
      ifree(last);
      ifree(canon_path);
      profile_stop();
      return (res==-1)?-tmp_errno:res;
    }
  } else if (validate_path(canon_path)) {
    PMSG(LOG_ERR, "Cannot write to directories like files!");
    ifree(last);
    ifree(canon_path);
    profile_stop();
    return -EISDIR;
  } else {
    DEBUG("Could not find path");
    ifree(last);
    ifree(canon_path);
    profile_stop();
    return -ENOENT;
  }
}

#if FUSE_VERSION >= 26

static int insight_statvfs(const char *path, struct statvfs *stbuf) {
  (void) path;
  (void) stbuf;

  DEBUG("Statvfs on \"%s\"", path);
  return 0;
}

#else

static int insight_statfs(const char *path, struct statfs *stbuf) {
  (void) path;
  (void) stbuf;

  DEBUG("Statfs on \"%s\"", path);
  return 0;
}
#endif

static int insight_release(const char *path, struct fuse_file_info *fi) {
  (void) path;
  (void) fi;

  DEBUG("Release on \"%s\"", path);
  /* Nothing to do here */
  return 0;
}

static int insight_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
  (void) path;
  (void) isdatasync;
  (void) fi;

  DEBUG("fsync() called on \"%s\"", path);
  return -EPERM;
}

static int insight_access(const char *path, int mode) {
  (void) path;
  (void) mode;

  DEBUG("access() called on \"%s\"", path);
  return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int insight_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) {
  char *canon_path = get_canonical_path(path);
  char *last = strlast(canon_path+1, '/');

  DEBUG("Set \"%s\" extended attribute of \"%s\"", name, canon_path);

  struct stat fstat;

  if (have_file_by_name(last, &fstat)) {
    fileptr inode = hash_path(last, strlen(last));
    char *fullname = fullname_from_inode(inode);
    ifree(last);
    if (strncmp(name, "insight.", 8)==0) {
      DEBUG("Insight namespace attribute");
      char *attr = calloc(strlen(name)+strlen(value)+1, sizeof(char)); /* TODO: check for failure */
      strcpy(attr, index(name, '.') + 1);
      if (strlen(value)) {
        strcat(attr, INSIGHT_SUBKEY_SEP);
        strcat(attr, value);
      }
      if (!*attr) {
        DEBUG("Null attribute");
      }
      DEBUG("Set Insight attribute \"%s\" of inode: %08lx", attr, inode);
      int res=attr_addbyname_rec(inode, attr);
      if (res) {
        DEBUG("Error!");
        return res;
      }
      ifree(attr);
      return 0;
    } else {
      DEBUG("Set attribute of real file: %s", fullname);
      int res = setxattr(fullname, name, value, size, flags);
      if (res==-1) {
        if (errno != ENOTSUP) {
          PMSG(LOG_ERR, "setxattr(\"%s\", \"%s\", ...) failed: %s", fullname, name, strerror(errno));
        }
        ifree(fullname);
        ifree(canon_path);
        return -errno;
      } else {
        ifree(fullname);
        ifree(canon_path);
        return res;
      }
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot set extended attributes on directories");
    ifree(last);
    ifree(canon_path);
    return -EPERM;
  } else {
    DEBUG("Could not find path");
    ifree(last);
    ifree(canon_path);
    return -ENOENT;
  }
}

static int insight_getxattr(const char *path, const char *name, char *value, size_t size) {
  char *canon_path = get_canonical_path(path);
  char *last = strlast(canon_path+1, '/');
  DEBUG("Get \"%s\" extended attribute of \"%s\"", name, canon_path);

  struct stat fstat;

  if (have_file_by_name(last, &fstat)) {
    char *fullname = fullname_from_inode(hash_path(last, strlen(last)));
    ifree(last);
    DEBUG("Get attribute of real file: %s", fullname);
    int res = getxattr(fullname, name, value, size);
    if (res==-1) {
      if (errno != ENODATA && errno != ENOTSUP && errno != ERANGE) {
        /* perfectly normal */
        PMSG(LOG_ERR, "getxattr(\"%s\", \"%s\", ...) failed: %s\n", fullname, name, strerror(errno));
      }
      ifree(fullname);
      ifree(canon_path);
      return -errno;
    } else {
      DEBUG("\n");
      ifree(fullname);
      ifree(canon_path);
      return res;
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot get extended attributes on directories\n");
    ifree(last);
    ifree(canon_path);
    /* No attributes */
    return -ENODATA;
  } else {
    DEBUG("Could not find path\n");
    ifree(last);
    ifree(canon_path);
    return -ENOENT;
  }
}

static int insight_listxattr(const char *path, char *list, size_t size) {
  char *canon_path = get_canonical_path(path);
  char *last = strlast(canon_path+1, '/');
  DEBUG("List extended attributes of \"%s\"", canon_path);

  struct stat fstat;

  if (have_file_by_name(last, &fstat)) {
    char *fullname = fullname_from_inode(hash_path(last, strlen(last)));
    ifree(last);
    DEBUG("List attributes of real file: %s", fullname);
    int res = listxattr(fullname, list, size);
    if (res==-1) {
      if (errno != ENOTSUP && errno != ERANGE) {
        PMSG(LOG_ERR, "listxattr(\"%s\", ...) failed: %s\n", fullname, strerror(errno));
      }
      ifree(fullname);
      ifree(canon_path);
      return -errno;
    } else {
      ifree(fullname);
      ifree(canon_path);
      DEBUG("\n");
      return res;
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot list extended attributes on directories");
    ifree(last);
    ifree(canon_path);
    /* No attributes */
    return -ENODATA;
  } else {
    DEBUG("Could not find path");
    ifree(last);
    ifree(canon_path);
    return -ENOENT;
  }
}

static int insight_removexattr(const char *path, const char *name) {
  char *canon_path = get_canonical_path(path);
  char *last = strlast(canon_path+1, '/');
  DEBUG("Remove extended attribute \"%s\" of \"%s\"", name, canon_path);

  struct stat fstat;

  if (have_file_by_name(last, &fstat)) {
    fileptr inode = hash_path(last, strlen(last));
    char *fullname = fullname_from_inode(inode);
    ifree(last);
    if (strncmp(name, "insight.", 8)==0) {
      DEBUG("Insight namespace attribute");
      DEBUG("Remove Insight attribute \"%s\" of inode: %08lx", name+8, inode);
      int res=attr_delbyname(inode, name+8);
      if (res) {
        DEBUG("Error! %s", strerror(-res));
        return res;
      }
      return 0;
    } else {
      DEBUG("Remove attribute of real file: %s", fullname);
      int res = removexattr(fullname, name);
      if (res==-1) {
        if (errno != ENOTSUP) {
          PMSG(LOG_ERR, "removexattr(\"%s\", \"%s\") failed: %s", fullname, name, strerror(errno));
        }
        ifree(fullname);
        ifree(canon_path);
        return -errno;
      } else {
        ifree(fullname);
        ifree(canon_path);
        return res;
      }
    }
  } else if (validate_path(canon_path)) {
    DEBUG("Cannot remove extended attributes on directories");
    ifree(last);
    ifree(canon_path);
    return -EPERM;
  } else {
    DEBUG("Could not find path");
    ifree(last);
    ifree(canon_path);
    return -ENOENT;
  }

  return 0;
}
#endif /* HAVE_SETXATTR */

#if FUSE_VERSION >= 26
static void *insight_init(struct fuse_conn_info *conn) {
  (void) conn;
  return NULL;
}
#else
static void *insight_init(void) {
  return NULL;
}
#endif

/**
 * Cleanup function run when the file system is unmounted.
 *
 * @param arg An argument passed by FUSE. Ignored.
 */
void insight_destroy(void *arg) {
  (void) arg;
  profile_init_start();
	PMSG(LOG_ERR, "Cleaning up and exiting");
  tree_close();
	FMSG(LOG_ERR, "Tree store closed");
  profile_stop();
}


/**
 * Print usage message on \c STDOUT
 *
 * @param progname The command name used to invoke insight
 */
void usage(char *progname) {
  if (usage_printed++)
    return;

  fprintf(stderr, "\n"
    " Insight v.%s FUSE_USE_VERSION: %d\n"
    " Semantic file system for Linux\n"
    " (c) 2008 Dave Ingram <insight@dmi.me.uk>\n"
    " \n"
    " This program is released under a Creative Commons\n"
    " Attribution-NonCommerical-ShareAlike2.5 License.\n"
    "\n"
    " For more information, please see\n"
    "   http://creativecommons.org/licenses/by-nc-sa/2.5/\n"
    " \n"
    " Usage: %s [OPTIONS] --store=PATH --repos=PATH /mountpoint\n"
    "\n"
    "    -q              Less logging (has priority over -v)\n"
    "    -r              Mount readonly\n"
    "    --store=PATH    Path to tree storage file\n"
    "    --repos=PATH    Path to symlink storage repository\n"
    /* "    -v              Verbose (only has effect with syslog)\n" */
    "\n" /* FUSE options will follow... */
    , PACKAGE_VERSION, FUSE_USE_VERSION, progname
  );
}

static int insight_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs) {
  (void) data;
  profile_init_start();

  switch (key) {
    case FUSE_OPT_KEY_NONOPT:
      if (!insight.mountpoint) {
        insight.mountpoint = strdup(arg);
        profile_stop();
        return 1;
      }
      profile_stop();
      return 0;

    case KEY_HELP:
      usage(outargs->argv[0]);
      fuse_opt_add_arg(outargs, "-ho");
#if FUSE_VERSION <= 25
      fuse_main(outargs->argc, outargs->argv, &insight_oper);
#else
      fuse_main(outargs->argc, outargs->argv, &insight_oper, NULL);
#endif
      profile_stop();
      exit(1);

    case KEY_VERSION:
      fprintf(stderr, "Insight semantic filesystem for Linux %s (unstable)\n", VERSION);
#if FUSE_VERSION >= 25
      fuse_opt_add_arg(outargs, "--version");
#endif
#if FUSE_VERSION == 25
      fuse_main(outargs->argc, outargs->argv, &insight_oper);
#else
      fuse_main(outargs->argc, outargs->argv, &insight_oper, NULL);
#endif
      profile_stop();
      exit(0);

    default:
      fprintf(stderr, "Extra parameter provided\n");
      usage(outargs->argv[0]);
  }

  profile_stop();
  return 0;
}

static void insight_process_args(struct fuse_args *args) {
  profile_init_start();
  fuse_opt_add_arg(args, "-ofsname=insight");       /* filesystem name */
  fuse_opt_add_arg(args, "-ouse_ino");              /* honour the inode fields in getattr() */
  fuse_opt_add_arg(args, "-oreaddir_ino");          /* honour the inode fields in readdir() */
  fuse_opt_add_arg(args, "-oallow_other");          /* allow non-root users to access the file system */

  /* XXX: force single-thread mode as no locking (yet!) */
  insight.singlethread = 1;
  if (!insight.quiet)
    fprintf(stderr, "%s: Forcing single-thread mode\n", insight.progname);

  /* XXX: force foreground mode while debugging */
  /* TODO: remove this when done */
  insight.foreground = 1;
  if (!insight.quiet)
    fprintf(stderr, "%s: Forcing foreground mode\n", insight.progname);

  if (insight.singlethread) {
    if (!insight.quiet)
      fprintf(stderr, "%s: Operating in single-thread mode\n", insight.progname);
    fuse_opt_add_arg(args, "-s");
  }
  if (insight.readonly) {
    if (!insight.quiet)
      fprintf(stderr, "%s: Mounting file system read-only\n", insight.progname);
    fuse_opt_add_arg(args, "-r");
  }
  if (insight.foreground) {
    if (!insight.quiet)
      fprintf(stderr, "%s: Running in foreground\n", insight.progname);
    fuse_opt_add_arg(args, "-f");
  }
  if (insight.debug || insight.verbose) {
    if (!insight.quiet)
      fprintf(stderr, "%s: Will log verbosely\n", insight.progname);
    fuse_opt_add_arg(args, "-d");
  }
  profile_stop();
}

static int insight_open_store() {
  profile_init_start();
  /* check store file given */
  if (!insight.treestore || (strcmp(insight.treestore, "") == 0)) {
    if (strlen(getenv("HOME"))) {
      struct stat dst;
      int storelen = strlen(getenv("HOME")) + strlen("/.insightfs/tree") + 1; /* +1 for null */
      char *insightdir = calloc(strlen(getenv("HOME")) + strlen("/.insightfs") + 1, 1);

      if (insight.treestore) ifree(insight.treestore);
      insight.treestore = calloc(storelen, sizeof(char));
      strcat(insight.treestore, getenv("HOME"));
      strcat(insight.treestore, "/.insightfs/tree");
      if (!insight.quiet)
        FMSG(LOG_INFO, "Using default tree store %s", insight.treestore);

      /* check the .insightfs directory exists and create if need be */
      strcat(insightdir, getenv("HOME"));
      strcat(insightdir, "/.insightfs");

      if ((lstat(insightdir, &dst) == -1) && (errno == ENOENT)) {
        FMSG(LOG_INFO, "Creating default Insight directory %s", insightdir);
        if (mkdir(insightdir, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
          if (!insight.quiet)
            FMSG(LOG_ERR, "Default Insight directory %s does not exist and can't be created!\n\n", insightdir);
          ifree(insightdir);
          profile_stop();
          return 2;
        }
      } else if (!S_ISDIR(dst.st_mode)) {
        FMSG(LOG_ERR, "Default Insight directory %s exists but must be a directory!\n\n", insightdir);
        ifree(insightdir);
        profile_stop();
        return 2;
      }
      ifree(insightdir);
    } else {
      usage(insight.progname);
      if (!insight.quiet)
        MSG(LOG_ERR, "%s: No tree store filename given\n", insight.progname);
      profile_stop();
      return 2;
    }
  }

	/* Do we need to substitute homedir for ~? */
	if (insight.treestore[0] == '~') {
		char *home_path = getenv("HOME");
		if (home_path != NULL) {
			char *relative_path = strdup(insight.treestore + 1);
			ifree(insight.treestore);
			insight.treestore = calloc(sizeof(char), strlen(relative_path) + strlen(home_path) + 1);
			strcpy(insight.treestore, home_path);
			strcat(insight.treestore, relative_path);
			ifree(relative_path);
			FMSG(LOG_INFO, "Tree store path is %s", insight.treestore);
		} else {
			FMSG(LOG_ERR, "Tree store path starts with '~', but $HOME could not be read.");
		}
  } else if (insight.treestore[0] != '/') {
    /* Is this a relative path? */
    FMSG(LOG_INFO, "Tree store path %s is relative", insight.treestore);
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
      FMSG(LOG_ERR, "Error getting current directory, will leave tree store path alone");
    } else {
      char *abs_store = calloc(sizeof(char), strlen(insight.treestore) + strlen(cwd) + 2);
      if (abs_store == NULL) {
        PMSG(LOG_ERR, "Failed to allocate memory for absolute path");
        FMSG(LOG_ERR, "Tree store path will remain relative!");
      } else {
        strcpy(abs_store, cwd);
        strcat(abs_store, "/");
        strcat(abs_store, insight.treestore);
        ifree(insight.treestore);
        insight.treestore = abs_store;
        FMSG(LOG_INFO, "Absolute tree store path is %s", insight.treestore);
      }
    }
  }

  /* open the store */
  if (tree_open(insight.treestore)) {
    PMSG(LOG_ERR, "Failed to initialise tree!");
    profile_stop();
    return 2;
  }

  profile_stop();
  return 0;
}

static int insight_check_repos() {
  profile_init_start();
  /* check repository path we've been given */
  if (!insight.repository || (strcmp(insight.repository, "") == 0)) {
    if (strlen(getenv("HOME"))) {
      struct stat dst;
      int storelen = strlen(getenv("HOME")) + strlen("/.insightfs/repos") + 1; /* +1 for null */
      char *insightdir = calloc(strlen(getenv("HOME")) + strlen("/.insightfs") + 1, 1);

      if (insight.repository) ifree(insight.repository);
      insight.repository = calloc(storelen, sizeof(char));
      strcat(insight.repository, getenv("HOME"));
      strcat(insight.repository, "/.insightfs/repos");
      if (!insight.quiet)
        FMSG(LOG_INFO, "Using default link repository %s", insight.repository);

      /* check the .insightfs directory exists and create if need be */
      strcat(insightdir, getenv("HOME"));
      strcat(insightdir, "/.insightfs");

      if ((lstat(insightdir, &dst) == -1) && (errno == ENOENT)) {
        FMSG(LOG_INFO, "Creating default Insight directory %s", insightdir);
        if (mkdir(insightdir, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
          if (!insight.quiet)
            FMSG(LOG_ERR, "Default Insight directory %s does not exist and can't be created!\n\n", insightdir);
          ifree(insightdir);
          profile_stop();
          return 2;
        }
      } else if (!S_ISDIR(dst.st_mode)) {
        FMSG(LOG_ERR, "Default Insight directory %s exists but must be a directory!\n\n", insightdir);
        ifree(insightdir);
        profile_stop();
        return 2;
      }

      ifree(insightdir);
    } else {
      usage(insight.progname);
      if (!insight.quiet)
        MSG(LOG_ERR, "%s: No link repository directory given\n", insight.progname);
      profile_stop();
      return 2;
    }
  }

	/* Do we need to substitute homedir for ~? */
	if (insight.repository[0] == '~') {
		char *home_path = getenv("HOME");
		if (home_path != NULL) {
			char *relative_path = strdup(insight.repository + 1);
			ifree(insight.repository);
			insight.repository = calloc(sizeof(char), strlen(relative_path) + strlen(home_path) + 1);
			strcpy(insight.repository, home_path);
			strcat(insight.repository, relative_path);
			ifree(relative_path);
			FMSG(LOG_INFO, "Link repository path is %s", insight.repository);
		} else {
			FMSG(LOG_ERR, "Link repository path starts with '~', but $HOME could not be read.");
		}
  } else if (insight.repository[0] != '/') {
    /* Is this a relative path? */
    FMSG(LOG_INFO, "Link repository path %s is relative", insight.repository);
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
      FMSG(LOG_ERR, "Error getting current directory, will leave link repository path alone");
    } else {
      char *abs_store = calloc(sizeof(char), strlen(insight.repository) + strlen(cwd) + 2);
      if (abs_store == NULL) {
        PMSG(LOG_ERR, "Failed to allocate memory for absolute path");
        FMSG(LOG_ERR, "Link repository path will remain relative!");
      } else {
        strcpy(abs_store, cwd);
        strcat(abs_store, "/");
        strcat(abs_store, insight.repository);
        ifree(insight.repository);
        insight.repository = abs_store;
        FMSG(LOG_INFO, "Absolute link repository path is %s", insight.repository);
      }
    }
  }

  struct stat dst;
  /* create repository if it doesn't exist */
  if ((lstat(insight.repository, &dst) == -1) && (errno == ENOENT)) {
    FMSG(LOG_INFO, "Creating link repository %s", insight.repository);
    if (mkdir(insight.repository, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
      if (!insight.quiet)
        FMSG(LOG_ERR, "Link repository directory %s does not exist and can't be created!\n\n", insight.repository);
      profile_stop();
      return 2;
    }
  } else if (!S_ISDIR(dst.st_mode)) {
    FMSG(LOG_ERR, "Link repository directory %s exists but must be a directory!\n\n", insight.repository);
    profile_stop();
    return 2;
  }

  profile_stop();
  return 0;
}

int main(int argc, char *argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  int res;

  insight.progname = argv[0];

  if (fuse_opt_parse(&args, &insight, insight_opts, insight_opt_proc) == -1) {
    exit(1);
  }

  INIT_LOG();

  insight_process_args(&args);

  /* checking mountpoint */
  if (!insight.mountpoint) {
    usage(insight.progname);
    fprintf(stderr, "%s: No mountpoint provided\n\n", insight.progname);
    exit(2);
  }

  /* checking if mount point exists or can be created */
  if ((lstat(insight.mountpoint, &insight.mountstat) == -1) && (errno == ENOENT)) {
    if (mkdir(insight.mountpoint, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
      usage(insight.progname);
      if (!insight.quiet)
        fprintf(stderr, "\n Mountpoint %s does not exist and can't be created!\n\n", insight.mountpoint);
      exit(1);
    }
    if (lstat(insight.mountpoint, &insight.mountstat) == -1) {
      if (!insight.quiet)
        fprintf(stderr, "\n Mountpoint %s does not exist and doesn't exist after creation!\n\n", insight.mountpoint);
    }
  }

  /* banner */
  if (!insight.quiet) {
    fprintf(stderr, "\n");
    fprintf(stderr,
      " Insight semantic file system v.%s\n"
      " (c) 2008 David Ingram <insight@dmi.me.uk>\n"
      " For license informations, see %s -h\n\n"
      , PACKAGE_VERSION, insight.progname
    );
  }

  if (insight_open_store()) {
    /* Fatal error! Details printed by the function */
    if (insight.treestore) ifree(insight.treestore);
    exit(2);
  }

  if (insight_check_repos()) {
    /* Fatal error! Details printed by the function */
    ifree(insight.treestore);
    if (insight.repository) ifree(insight.repository);
    exit(2);
  }

  /* plugin function setup */
  insight.funcs.add_tag = attr_addbyname_rec;
  insight.funcs.add_auto_tag = auto_attr_addbyname_rec;
  insight.funcs.del_tag = attr_delbyname;
  insight.funcs.del_auto_tag = auto_attr_delbyname;
  insight.funcs.get_inode = get_inode;
  insight.funcs.log = insight_log;

  /* load plugins */
  char *plugin_dir = NULL;
  if (getenv("INSIGHT_PLUGINS")) {
    plugin_dir = getenv("INSIGHT_PLUGINS");
    if (!insight.quiet)
      MSG(LOG_INFO, "plugin_setup: User-defined plugin directory: %s", plugin_dir);
  } else {
    plugin_dir = INSIGHT_DEFAULT_PLUGIN_DIR;
    if (!insight.quiet)
      MSG(LOG_INFO, "plugin_setup: Default plugin directory: %s", plugin_dir);
  }

	struct stat st;
	if (lstat(plugin_dir, &st) == -1) {
    MSG(LOG_WARNING, "plugin_setup: Error opening plugin directory %s: %s", plugin_dir, strerror(errno));
	} else if (!S_ISDIR(st.st_mode)) {
    MSG(LOG_WARNING, "plugin_setup: Error opening plugin directory %s: not a directory", plugin_dir);
	} else {
    plugin_load_all(plugin_dir);
  }

  /*
  FMSG(LOG_DEBUG, "Fuse options:");
  int fargc = args.argc;
  while (fargc) {
    FMSG(LOG_DEBUG, "%.2d: %s", fargc, args.argv[fargc]);
    fargc--;
  }
  */

#if FUSE_VERSION <= 25
  res = fuse_main(args.argc, args.argv, &insight_oper);
#else
  res = fuse_main(args.argc, args.argv, &insight_oper, NULL);
#endif

  fuse_opt_free_args(&args);

  /* unload plugins */
  plugin_unload_all();

  ifree(insight.mountpoint);
  ifree(insight.treestore);
  ifree(insight.repository);

  return res;
}

void insight_log(int lvl, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
#ifdef _MSG_STDERR
  vfprintf((lvl<=LOG_WARNING)?stderr:stdout, fmt, ap);
  fprintf((lvl<=LOG_WARNING)?stderr:stdout, "\n");
#else
  vsyslog(lvl, fmt, ap);
  syslog(lvl, "\n");
#endif
  va_end(ap);
}
