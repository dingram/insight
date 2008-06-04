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
#ifndef _DEBUG
#define _DEBUG
#endif
#include <debug.h>
#include <path_helpers.h>
#include <string_helpers.h>
#include <query_engine.h>

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
  /* INSIGHTFS_OPT("-c",         use_cache,    1), */

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



static int insight_getattr(const char *path, struct stat *stbuf)
{
  char *canon_path = get_canonical_path(path);
  if (errno) {
    DEBUG("Error in get_canonical_path.");
    return -ENOENT;
  }

  struct stat fstat;

  DEBUG("Getattr called on path \"%s\"", canon_path);

  if (have_file_by_name(strlast(canon_path+1, '/'), &fstat, insight.repository)) {
    DEBUG("Got a file");

    memcpy(stbuf, &fstat, sizeof(struct stat));
    return 0;

  } else if (validate_path(canon_path)) {
    /* steal a default stat structure */
    memcpy(stbuf, &insight.mountstat, sizeof(struct stat));

    /* special case: root */
    if (strcmp(canon_path, "/")==0) {
      DEBUG("Getattr called on root path\n");
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = tree_key_count();
      stbuf->st_ino = 1;
      return 0;
    }

    if (strlen(canon_path)>2 && last_char_in(canon_path)==INSIGHT_SUBKEY_IND_C && canon_path[strlen(canon_path)-2]=='/') {
      /* just a subkey indicator directory */
      DEBUG("Getattr on a subkey indicator directory");
      stbuf->st_mode = S_IFDIR | 0555;
      stbuf->st_nlink = 1;
      stbuf->st_ino = 2;
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
        return -ENOENT;
      }
      DEBUG("Found tag \"%s\"", canon_path+1);
      if (tree_read(tagdata, (tblock*)&dnode)) {
        PMSG(LOG_ERR, "IO error reading data block");
        return -EIO;
      }
      if (dnode.flags & DATA_FLAGS_SYNONYM) {
        stbuf->st_mode = S_IFLNK | 0555;
      } else {
        stbuf->st_mode = S_IFDIR | 0555;
      }
      stbuf->st_nlink = 1;
      stbuf->st_ino = -1;
    }
  } else {
    DEBUG("Path does not exist\n");
    return -ENOENT;
  }
  DEBUG("Returning\n");
  return 0;
}

static int insight_readdir(const char *path, void *buf,
               fuse_fill_dir_t filler, off_t offset,
               struct fuse_file_info *fi)
{
  (void) offset;
  (void) fi;
  tnode node;
  int i;
  char *canon_path = get_canonical_path(path);

  DEBUG("readdir(path=\"%s\", buf=%p, offset=%lld)", canon_path, buf, offset);

  if (!validate_path(canon_path)) {
    DEBUG("Path does not exist\n");
    return -ENOENT;
  }

  char *last_tag=calloc(255, sizeof(char));

  DEBUG("Generating query tree");
  qelem *q = path_to_query(canon_path);

  DEBUG("Finding subtag parent...");
  (void)query_get_subtags(q, last_tag, 255);
  DEBUG("Subtag parent: \"%s\"", last_tag);

  DEBUG("Freeing query tree...");
  qtree_free(&q, 1);
  DEBUG("Query tree freed.");

  fileptr tree_root=tree_get_root();

  DEBUG("Calling get_tag");
  if (*last_tag && (tree_root=get_tag(last_tag))==0) {
    DEBUG("Tag \"%s\" not found", last_tag);
    ifree(last_tag);
    return -ENOENT;
  }
  DEBUG("Found tag \"%s\"; tree root now %lu", last_tag, tree_root);

  DEBUG("Filling directory");
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  if (tree_sub_get_min(tree_root, &node)) {
    PMSG(LOG_ERR, "tree_sub_get_min() failed\n");
    ifree(last_tag);
    return -EIO;
  }

  if (node.magic != MAGIC_TREENODE) {
    if (*last_tag)
      DEBUG("No subtags");
    else
      PMSG(LOG_ERR, "Something went very, very wrong\n");
    ifree(last_tag);
    return 0;
  }

  DEBUG("Should we add subtag dir?");
  /* add special directory containing subkeys of this key */
  if (strcmp(path, "/") != 0 && !*last_tag) {
    DEBUG("Maybe - fetching last bit of path");
    char *lastbit = strlast(canon_path, '/');
    tnode n;
    /* but only if it actually has subkeys */
    DEBUG("Calling get_tag(\"%s\")", lastbit);
    tree_root=get_tag(lastbit);

    DEBUG("Getting minimum node");
    if (tree_sub_get_min(tree_root, &n)) {
      PMSG(LOG_ERR, "tree_sub_get_min() failed\n");
      ifree(last_tag);
      return -EIO;
    }

    if (n.magic==MAGIC_TREENODE) {
      DEBUG("Adding subkey indicator directory");
      filler(buf, INSIGHT_SUBKEY_IND, NULL, 0);
    }
  }


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
      return -EIO;
    }
  } while (node.ptrs[0]);

  ifree(last_tag);

  DEBUG("Freeing bits");
  for (count--;count>=0; count--) {
    ifree(bits[count]);
  }
  DEBUG("Freeing bits top-level");
  ifree(bits);

  DEBUG("Returning\n");

  return 0;
}

static int insight_mkdir(const char *path, mode_t mode)
{
  (void) mode;

  char *newdir = strlast(path, '/');
  char *dup = strdup(path);
  char *canon_parent = rindex(dup, '/');
  *canon_parent='\0';
  canon_parent = get_canonical_path(dup);
  ifree(dup);

  if (canon_parent[0]=='/')
    canon_parent++;

  if (strcmp(newdir, INSIGHT_SUBKEY_IND)==0) {
    PMSG(LOG_WARN, "Cannot create directories named \"%s\"\n", INSIGHT_SUBKEY_IND);
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
  tdata datan;
  char *parent_tag=rindex(canon_parent, '/');

  if (!parent_tag) parent_tag=canon_parent;
  else             parent_tag++;

  while (last_char_in(parent_tag)==INSIGHT_SUBKEY_IND_C) {
    DEBUG("Subtag");
    last_char_in(parent_tag)='\0';
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

  initDataNode(&datan);
  if (!tree_sub_insert(tree_root, newdir, &datan)) {
    PMSG(LOG_ERR, "Tree insertion failed\n");
    return -ENOSPC;
  }
  DEBUG("Successfully inserted \"%s\" into parent \"%s\"", newdir, parent_tag);

  DEBUG("Returning\n");

  return 0;
}

static int insight_rmdir(const char *path)
{
  char *olddir = strlast(path, '/');
  char *dup = strdup(path);
  char *canon_parent = rindex(dup, '/');
  *canon_parent='\0';
  canon_parent = get_canonical_path(dup);
  ifree(dup);

  if (canon_parent[0]=='/')
    canon_parent++;

  if (strcmp(olddir, INSIGHT_SUBKEY_IND)==0) {
    PMSG(LOG_WARN, "Cannot remove directories named \"%s\"\n", INSIGHT_SUBKEY_IND);
    return -EPERM;
  }

  while (*olddir && last_char_in(olddir)==INSIGHT_SUBKEY_IND_C) {
    DEBUG("Subkey indicator removed from end of new directory name");
    last_char_in(olddir)='\0';
  }

  if (!*olddir) {
    DEBUG("Invalid (empty) directory name");
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
    DEBUG("Subtag");
    last_char_in(parent_tag)='\0';
    subtag=1;
  }

  if (*parent_tag && (tree_root=get_tag(parent_tag))==0) {
    DEBUG("Tag \"%s\" not found", parent_tag);
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
    PMSG(LOG_ERR, "Tree removal failed\n");
    return -EIO;
  }
  DEBUG("Successfully removed \"%s\" from parent \"%s\"", olddir, parent_tag);

  DEBUG("Returning\n");
  return 0;
}

static int insight_unlink(const char *path)
{
  (void) path;

  DEBUG("Unlink \"%s\"", path);
  return -ENOENT;
}

static int insight_mknod(const char *path, mode_t mode, dev_t rdev)
{
  (void) path;
  (void) mode;
  (void) rdev;

  DEBUG("Mknod: \"%s\"", path);
  return -ENOTSUP;
}

static int insight_rename(const char *from, const char *to)
{
  (void) from;
  (void) to;

  DEBUG("Rename path from \"%s\" to \"%s\"", from, to);
  return -ENOTSUP;
}

static int insight_readlink(const char *path, char *buf, size_t size)
{
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
  DEBUG("Found tag \"%s\"", canon_path+1);

  tdata dnode;
  if (tree_read(tagdata, (tblock*)&dnode)) {
    PMSG(LOG_ERR, "IO error reading data block");
    return -EIO;
  }

  if (dnode.flags & DATA_FLAGS_SYNONYM) {
    strncpy(buf, dnode.target, size);
  } else {
    /* not a symlink */
    DEBUG("Requested tag \"%s\" is not a symlink.", canon_path+1);
    return -EINVAL;
  }

  return 0;
}

static int insight_symlink(const char *from, const char *to)
{
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

  finaldest = gen_repos_path(s_hash, 1, insight.repository);
  if (!finaldest) {
    PMSG(LOG_ERR, "Failed to generate repository path");
    return -EIO;
  }

  DEBUG("Final symlink call: symlink(\"%s\", \"%s\")", from, finaldest);

  if (symlink(from, finaldest)==-1) {
    PMSG(LOG_ERR, "Symlink creation failed: %s", strerror(errno));
    ifree(finaldest);
    return -EIO;
  }

  ifree(finaldest);
  return 0;
}

static int insight_link(const char *from, const char *to)
{
  (void) from;
  (void) to;

  DEBUG("Create hard link from \"%s\" to \"%s\"", from, to);
  return -ENOTSUP;
}

static int insight_chmod(const char *path, mode_t mode)
{
  (void) path;
  (void) mode;

  DEBUG("Change mode of \"%s\" to %05o", path, mode & 07777);
  return -ENOTSUP;
}

static int insight_chown(const char *path, uid_t uid, gid_t gid)
{
  (void) path;
  (void) uid;
  (void) gid;

  DEBUG("Change ownership of \"%s\" to %d:%d", path, uid, gid);
  return -ENOTSUP;
}

static int insight_truncate(const char *path, off_t size)
{
  (void) path;
  (void) size;

  DEBUG("Truncate \"%s\" to %lld bytes", path, size);
  return -ENOTSUP;
}

#if FUSE_VERSION >= 26

static int insight_utimens(const char *path, const struct timespec tv[2])
{
  (void) path;
  (void) tv;

  DEBUG("Changing times of \"%s\"", path);
  return -ENOTSUP;
}

#else

static int insight_utime(const char *path, struct utimbuf *buf)
{
  (void) path;
  (void) buf;

  DEBUG("Changing times of \"%s\"", path);
  return -ENOTSUP;
}

#endif

static int insight_open(const char *path, struct fuse_file_info *fi)
{
  (void) fi;
  DEBUG("Open on path \"%s\"", path);
  if (strcmp(path, "/insightFS") != 0) {
    DEBUG("Unknown path\n");
    return -ENOENT;
  }

  DEBUG("Everything OK\n");
  return -EACCES;
}

static int insight_read(const char *path, char *buf, size_t size,
            off_t offset, struct fuse_file_info *fi)
{
  (void) path;
  (void) buf;
  (void) size;
  (void) offset;
  (void) fi;
  DEBUG("Read on path \"%s\"\n", path);

  return -EPERM;
}

static int insight_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi)
{
  (void) path;
  (void) buf;
  (void) size;
  (void) offset;
  (void) fi;
  DEBUG("Write on path \"%s\"\n", path);

  return -EPERM;
}

#if FUSE_VERSION >= 26

static int insight_statvfs(const char *path, struct statvfs *stbuf)
{
  (void) path;
  (void) stbuf;

  DEBUG("Statfs on \"%s\"", path);
  return 0;
}

#else

static int insight_statfs(const char *path, struct statfs *stbuf)
{
  (void) path;
  (void) stbuf;

  DEBUG("Statfs on \"%s\"", path);
  return 0;
}
#endif

static int insight_release(const char *path, struct fuse_file_info *fi)
{
  (void) path;
  (void) fi;

  DEBUG("Statfs on \"%s\"", path);
  return 0;
}

static int insight_fsync(const char *path, int isdatasync,
             struct fuse_file_info *fi)
{
  (void) path;
  (void) isdatasync;
  (void) fi;

  DEBUG("fsync() called on \"%s\"", path);
  return -EPERM;
}

static int insight_access(const char *path, int mode)
{
  (void) path;
  (void) mode;

  DEBUG("access() called on \"%s\"", path);
  return 0;
}

#if FUSE_VERSION >= 26
static void *insight_init(struct fuse_conn_info *conn)
{
  (void) conn;
  return NULL;
}
#else
static void *insight_init(void)
{
  return NULL;
}
#endif




/**
 * Print usage message on \c STDOUT
 *
 * @param progname The command name used to invoke insight
 */
void usage(char *progname)
{
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
static int insight_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
    (void) data;

    switch (key) {
    case FUSE_OPT_KEY_NONOPT:
      if (!insight.mountpoint) {
        insight.mountpoint = strdup(arg);
        return 1;
      }
      return 0;

      case KEY_HELP:
          usage(outargs->argv[0]);
          fuse_opt_add_arg(outargs, "-ho");
#if FUSE_VERSION <= 25
          fuse_main(outargs->argc, outargs->argv, &insight_oper);
#else
          fuse_main(outargs->argc, outargs->argv, &insight_oper, NULL);
#endif
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
          exit(0);

      default:
          fprintf(stderr, "Extra parameter provided\n");
          usage(outargs->argv[0]);
    }

  return 0;
}

static void insight_process_args(struct fuse_args *args) {
  fuse_opt_add_arg(args, "-ofsname=insight");       /* filesystem name */
  fuse_opt_add_arg(args, "-ouse_ino,readdir_ino");  /* honour the inode fields in getattr() and fill_dir() */
  fuse_opt_add_arg(args, "-oallow_other");          /* allow non-root users to access the file system */

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
  if (insight.verbose) {
    if (!insight.quiet)
      fprintf(stderr, "%s: Will log verbosely\n", insight.progname);
    fuse_opt_add_arg(args, "-d");
  }
}

static int insight_open_store() {
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
          return 2;
        }
      } else if (!S_ISDIR(dst.st_mode)) {
        FMSG(LOG_ERR, "Default Insight directory %s exists but must be a directory!\n\n", insightdir);
        return 2;
      }
    } else {
      usage(insight.progname);
      if (!insight.quiet)
        MSG(LOG_ERR, "%s: No tree store filename given\n", insight.progname);
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
    return 2;
  }

  return 0;
}

static int insight_check_repos() {
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
          return 2;
        }
      } else if (!S_ISDIR(dst.st_mode)) {
        FMSG(LOG_ERR, "Default Insight directory %s exists but must be a directory!\n\n", insightdir);
        return 2;
      }
    } else {
      usage(insight.progname);
      if (!insight.quiet)
        MSG(LOG_ERR, "%s: No link repository directory given\n", insight.progname);
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
      return 2;
    }
  } else if (!S_ISDIR(dst.st_mode)) {
    FMSG(LOG_ERR, "Link repository directory %s exists but must be a directory!\n\n", insight.repository);
    return 2;
  }

  return 0;
}

/**
 * Cleanup function run when the file system is unmounted.
 *
 * @param arg An argument passed by FUSE. Ignored.
 */
void insight_destroy(void *arg)
{
  (void) arg;
	PMSG(LOG_ERR, "Cleaning up and exiting");
  tree_close();
	FMSG(LOG_ERR, "Tree store closed");
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
    exit(2);
  }

  if (insight_check_repos()) {
    /* Fatal error! Details printed by the function */
    exit(2);
  }

  FMSG(LOG_DEBUG, "Fuse options:");
  int fargc = args.argc;
  while (fargc) {
    FMSG(LOG_DEBUG, "%.2d: %s", fargc, args.argv[fargc]);
    fargc--;
  }

#if FUSE_VERSION <= 25
  res = fuse_main(args.argc, args.argv, &insight_oper);
#else
  res = fuse_main(args.argc, args.argv, &insight_oper, NULL);
#endif

  fuse_opt_free_args(&args);

  ifree(insight.treestore);
  ifree(insight.repository);

  return res;
}
