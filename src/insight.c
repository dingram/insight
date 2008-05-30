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

#include "insight.h"
#include "bplus.h"
#define _DEBUG
#include "debug.h"

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
static int   insight_utime(const char *path, struct utimbuf *buf);
static int   insight_open(const char *path, struct fuse_file_info *fi);
static int   insight_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int   insight_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int   insight_statvfs(const char *path, struct statvfs *stbuf);
static int   insight_release(const char *path, struct fuse_file_info *fi);
static int   insight_fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
static int   insight_access(const char *path, int mode);
static void *insight_init(struct fuse_conn_info *conn);

/* static */ struct insight insight;

#define INSIGHTFS_OPT(t, p, v) { t, offsetof(struct insight, p), v }

enum {
    KEY_HELP,
    KEY_VERSION,
};

static struct fuse_opt insight_opts[] = {
  INSIGHTFS_OPT("-d",         debug,        1),
  INSIGHTFS_OPT("--store=%s", treestore,    0),
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
#if 0
  .readlink   = insight_readlink,
  .mknod      = insight_mknod,
  .symlink    = insight_symlink,
  .unlink     = insight_unlink,
  .rmdir      = insight_rmdir,
  .rename     = insight_rename,
  .link       = insight_link,
  .chmod      = insight_chmod,
  .chown      = insight_chown,
  .truncate   = insight_truncate,
  .utime      = insight_utime,
  .write      = insight_write,
#if FUSE_USE_VERSION >= 25
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
#endif
};

static int usage_already_printed = 0;



static int insight_getattr(const char *path, struct stat *stbuf)
{
  char *pathdup = strdup(path);

  DEBUG("Getattr called on path \"%s\"", pathdup);
  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(pathdup, "/")==0) {
    DEBUG("Getattr called on root path");
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = tree_key_count();
    stbuf->st_ino = 1;
    return 0;
  }

  if (rindex(pathdup, ':') && rindex(pathdup, ':') == rindex(pathdup, '/')+1) {
    /* there's a colon at the end */
    DEBUG("Getattr on a colon directory");
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 1;
    stbuf->st_ino = -1;
  } else {
    if (pathdup[strlen(pathdup)-1]==':') {
      pathdup[strlen(pathdup)-1]='\0';
    } else if (!tree_search(pathdup+1)) {
      DEBUG("Tag \"%s\" not found", pathdup+1);
      return -ENOENT;
    }
    DEBUG("Found tag \"%s\"", pathdup+1);
    stbuf->st_mode = S_IFDIR | 0555;
    stbuf->st_nlink = 1;
    stbuf->st_ino = -1;
  }
  DEBUG("Returning\n");
  return 0;
}

static int insight_readlink(const char *path, char *buf, size_t size)
{
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

  DEBUG("readdir(path=\"%s\", buf=%p, offset=%lu)", path, buf, offset);
  if (strcmp(path, "/") != 0 && !tree_search(path+1)) {
    return -ENOENT;
  }

  DEBUG("Filling directory");
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  if (tree_get_min(&node))
    return -EIO;
  if (strcmp(path, "/") != 0) {
    /* subkeys of this key */
    filler(buf, ":", NULL, 0);
  }
  do {
    for (i=0; i<node.keycount; i++) {
      filler(buf, node.keys[i], NULL, 0);
    }
    if (node.ptrs[0] && tree_read(node.ptrs[0], (tblock*) &node)) {
      return -EIO;
    }
  } while (node.ptrs[0]);

  DEBUG("Returning\n");

  return 0;
}

static int insight_mknod(const char *path, mode_t mode, dev_t rdev)
{
  return -ENOSPC;
}

static int insight_mkdir(const char *path, mode_t mode)
{
  (void) mode;
  tdata datan;

  DEBUG("Called upon to create directory \"%s\"", path);
  if (tree_search(path+1)) {
    DEBUG("Tag \"%s\" already exists", path+1);
    return -EEXIST;
  }

  initDataNode(&datan);
  if (!tree_insert(path+1, &datan)) {
  }
  return -ENOSPC;
}

static int insight_unlink(const char *path)
{
  return -ENOENT;
}

static int insight_rmdir(const char *path)
{
  return -ENOENT;
}

static int insight_rename(const char *from, const char *to)
{
  return -ENOENT;
}

static int insight_symlink(const char *from, const char *to)
{
  return -EPERM;
}

static int insight_link(const char *from, const char *to)
{
  return -EPERM;
}

static int insight_chmod(const char *path, mode_t mode)
{
  return -EPERM;
}

static int insight_chown(const char *path, uid_t uid, gid_t gid)
{
  return -EPERM;
}

static int insight_truncate(const char *path, off_t size)
{
  return -EPERM;
}

static int insight_utime(const char *path, struct utimbuf *buf)
{
  return 0;
}

static int insight_open(const char *path, struct fuse_file_info *fi)
{
  DEBUG("Open on path \"%s\"", path);
  if (strcmp(path, "/insightFS") != 0) {
    DEBUG("Unknown path\n");
    return -ENOENT;
  }

  DEBUG("Checking flags");
  if ((fi->flags & 3) != O_RDONLY) {
    DEBUG("Only read-only access allowed\n");
    return -EPERM;
  }

  DEBUG("Everything OK\n");
  return -EPERM;
}

static int insight_read(const char *path, char *buf, size_t size,
            off_t offset, struct fuse_file_info *fi)
{
  DEBUG("Read on path \"%s\"\n", path);
  size_t len;
  (void) fi;
  if (strcmp(path, "/insightFS") != 0) {
    return -ENOENT;
  }

  return -EPERM;
}

static int insight_write(const char *path, const char *buf, size_t size,
             off_t offset, struct fuse_file_info *fi)
{
  return -EPERM;
}

static int insight_statvfs(const char *path, struct statvfs *stbuf)
{
  return 0;
}

static int insight_release(const char *path, struct fuse_file_info *fi)
{
  return 0;
}

static int insight_fsync(const char *path, int isdatasync,
             struct fuse_file_info *fi)
{
  return -EPERM;
}

static int insight_access(const char *path, int mode)
{
  return -EPERM;
}

static void *insight_init(struct fuse_conn_info *conn)
{
  return 0;
}





/**
 * Print usage message on \c STDOUT
 *
 * @param progname The command name used to invoke insight
 */
void usage(char *progname)
{
  if (usage_already_printed++)
    return;

  fprintf(stderr, "\n"
    " Insight v.%s FUSE_USE_VERSION: %d\n"
    " Semantic file system for Linux\n"
    " (c) 2008 Dave Ingram <insight@dmi.me.uk>\n"
    " \n"
/*  " This program is free software; you can redistribute it and/or modify\n"
    " it under the terms of the GNU General Public License as published by\n"
    " the Free Software Foundation; either version 2 of the License, or\n"
    " (at your option) any later version.\n\n"

    " This program is distributed in the hope that it will be useful,\n"
    " but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    " MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    " GNU General Public License for more details.\n\n"

    " You should have received a copy of the GNU General Public License\n"
    " along with this program; if not, write to the Free Software\n"
    " Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n"
    " \n" */
    " Usage: %s [OPTIONS] --store=PATH /mountpoint\n"
    "\n"
    "    -q              Less logging (has priority over -v)\n"
    "    -r              Mount readonly\n"
    "    --store=PATH    Path to tree storage file\n"
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

static int insight_process_args(struct fuse_args *args) {
  fuse_opt_add_arg(args, "-ofsname=insight");      /* filesystem name */
  fuse_opt_add_arg(args, "-ouse_ino,readdir_ino"); /* XXX: honour the inode fields in getattr() and fill_dir() */
  fuse_opt_add_arg(args, "-oallow_other");         /* allow non-root users to access the file system */

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

      if (insight.treestore) free(insight.treestore);
      insight.treestore = calloc(storelen, sizeof(char));
      strcat(insight.treestore, getenv("HOME"));
      strcat(insight.treestore, "/.insightfs/tree");
      if (!insight.quiet)
        FMSG(LOG_INFO, "Using default tree store %s", insight.treestore);

      /* check the .insightfs directory exists and create if need be */
      strcat(insightdir, getenv("HOME"));
      strcat(insightdir, "/.insightfs");

      if ((lstat(insightdir, &dst) == -1) && (errno == ENOENT)) {
        FMSG(LOG_INFO, "Creating default store directory %s", insightdir);
        if (mkdir(insightdir, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
          if (!insight.quiet)
            FMSG(LOG_ERR, "Tree directory %s does not exist and can't be created!\n\n", insightdir);
          return 2;
        }
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
  struct stat mst;
  if ((lstat(insight.mountpoint, &mst) == -1) && (errno == ENOENT)) {
    if (mkdir(insight.mountpoint, S_IRWXU|S_IRGRP|S_IXGRP) != 0) {
      usage(insight.progname);
      if (!insight.quiet)
        fprintf(stderr, "\n Mountpoint %s does not exist and can't be created!\n\n", insight.mountpoint);
      exit(1);
    }
  }

  /* banner */
  if (!insight.quiet) {
    fprintf(stderr, "\n");
    fprintf(stderr,
      " Insight semantic file system v.%s FUSE_USE_VERSION: %d\n"
      " (c) 2008 David Ingram <insight@dmi.me.uk>\n"
      " For license informations, see %s -h\n\n"
      , PACKAGE_VERSION, FUSE_USE_VERSION, insight.progname
    );
  }

  if (insight_open_store()) {
    /* Fatal error! Details printed by the function */
    exit(2);
  }

  DEBUG("Fuse options:");
  int fargc = args.argc;
  while (fargc) {
    DEBUG("%.2d: %s", fargc, args.argv[fargc]);
    fargc--;
  }

#if FUSE_VERSION <= 25
  res = fuse_main(args.argc, args.argv, &insight_oper);
#else
  res = fuse_main(args.argc, args.argv, &insight_oper, NULL);
#endif

  fuse_opt_free_args(&args);

  return res;
}
