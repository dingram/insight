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
#include <plugin_handler.h>
#include <debug.h>
#include <dlfcn.h>

/* default implementations if none given */

/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
static int _plugin_default_chmod(const char *op, const char *fn, mode_t m) { (void)op;(void)fn;(void)m; return ENOTSUP; }
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
static int _plugin_default_chown(const char *op, const char *fn, uid_t u, gid_t g) { (void)op;(void)fn;(void)u;(void)g; return ENOTSUP; }
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
static int _plugin_default_write(const char *op, const char *fn)  { (void)op;(void)fn; return ENOTSUP; }
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
static int _plugin_default_rename(const char *op, const char *on, const char *nn)  { (void)op;(void)on;(void)nn; return ENOTSUP; }
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
static int _plugin_default_import(const char *op, const char *fn) { (void)op;(void)fn; return ENOTSUP; }
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
static void _plugin_default_cleanup() { }

/* process the plugin chain */

/**
 * Process all plugins that should be run when the file's mode is changed.
 *
 * The plugin should return a value to indicate how to proceed:
 *   - 0 => success; abort chain
 *   - (+)EAGAIN => success; proceed with next in chain
 *   - (+)ENOTSUP => don't handle this; proceed with next
 *   - negative error code => serious error; abort chain and report
 *   - positive error code => error; continue with next in chain, no report
 *
 * @param original_path The path to the actual file in the real VFS.
 * @param filename      The filename inside Insight.
 * @param mode          The new file mode.
 */
int plugin_process_chmod(const char *original_path, const char *filename, mode_t mode) {
  (void)original_path;
  (void)filename;
  (void)mode;
  return 0;
}

/**
 * Process all plugins that should be run when the owner/owning group of a file
 * changes.
 *
 * The plugin should return a value to indicate how to proceed:
 *   - 0 => success; abort chain
 *   - (+)EAGAIN => success; proceed with next in chain
 *   - (+)ENOTSUP => don't handle this; proceed with next
 *   - negative error code => serious error; abort chain and report
 *   - positive error code => error; continue with next in chain, no report
 *
 * @param original_path The path to the actual file in the real VFS.
 * @param filename      The filename inside Insight.
 * @param uid           The new owner ID.
 * @param gid           The new owning group ID.
 */
int plugin_process_chown(const char *original_path, const char *filename, uid_t uid, gid_t gid) {
  (void)original_path;
  (void)filename;
  (void)uid;
  (void)gid;
  return 0;
}

/**
 * Process all plugins that should be run on file write.
 *
 * The plugin should return a value to indicate how to proceed:
 *   - 0 => success; abort chain
 *   - (+)EAGAIN => success; proceed with next in chain
 *   - (+)ENOTSUP => don't handle this; proceed with next
 *   - negative error code => serious error; abort chain and report
 *   - positive error code => error; continue with next in chain, no report
 *
 * @param original_path The path to the actual file in the real VFS.
 * @param filename      The filename inside Insight.
 */
int plugin_process_write(const char *original_path, const char *filename) {
  (void)original_path;
  (void)filename;
  return 0;
}

/**
 * Process all plugins that should be run when a file is renamed.
 *
 * The plugin should return a value to indicate how to proceed:
 *   - 0 => success; abort chain
 *   - (+)EAGAIN => success; proceed with next in chain
 *   - (+)ENOTSUP => don't handle this; proceed with next
 *   - negative error code => serious error; abort chain and report
 *   - positive error code => error; continue with next in chain, no report
 *
 * @param original_path The path to the actual file in the real VFS.
 * @param oldname       The old filename inside Insight.
 * @param newname       The new filename inside Insight.
 */
int plugin_process_rename(const char *original_path, const char *oldname, const char *newname) {
  (void)original_path;
  (void)oldname;
  (void)newname;
  return 0;
}

/**
 * Process all plugins that should be run on file import.
 *
 * The plugin should return a value to indicate how to proceed:
 *   - 0 => success; abort chain
 *   - (+)EAGAIN => success; proceed with next in chain
 *   - (+)ENOTSUP => don't handle this; proceed with next
 *   - negative error code => serious error; abort chain and report
 *   - positive error code => error; continue with next in chain, no report
 *
 * @param original_path The path to the actual file in the real VFS.
 * @param filename      The filename inside Insight.
 */
int plugin_process_import(const char *original_path, const char *filename) {
  DEBUG("Plugin processing: IMPORT");
  insight_plugin *p=NULL;
  int res;

  DEBUG("Starting plugin processing");
  for (p=insight.plugins; p; p=p->next) {
    DEBUG("Processing with plugin %s...", p->path);
    DEBUG("  p->caps.can_transduce_import = %d", p->caps.can_transduce_import);
    switch (p->caps.can_transduce_import) {
      case INSIGHT_CAP_CAN_ALL:
        FMSG(LOG_INFO, "Plugin %s can process all files, so \"%s\" is no problem", p->path, original_path);
        res = p->import(original_path, filename);
        break;
      case INSIGHT_CAP_CAN:
        /* do some checks first */
        FMSG(LOG_INFO, "Plugin %s can process file \"%s\"", p->path, original_path);
        res = p->import(original_path, filename);
        break;
      default:
        DEBUG("Plugin %s can't process files for import", p->path);
        res = ENOTSUP;
    }
    if (res==0) {
      /* success; abort chain */
      break;
    } else if (res==EAGAIN) {
      /* success; continue chain */
    } else if (res==ENOTSUP) {
      /* nothing done */
    } else if (res<0) {
      /* serious error; abort chain and report */
      FMSG(LOG_ERR, "Plugin %s fatally failed to process file \"%s\": %s", p->path, original_path, strerror(-res));
      return res;
    } else if (res>0) {
      /* error; continue with next in chain */
      FMSG(LOG_WARNING, "Plugin %s failed to process file \"%s\": %s", p->path, original_path, strerror(res));
    }
  }
  DEBUG("Plugin processing complete");
  return 0;
}


/**
 * Load a plugin, retrieve its function pointers, call its initialiser,
 * retrieve its capabilities, and add it to the global list.
 *
 * @param plugin_dir      The directory containing the plugin, without a trailing slash.
 * @param plugin_filename The filename for the plugin, to be appended to \a plugin_dir.
 * @return Zero on success, a negative error code on failure.
 */
int plugin_load(const char *plugin_dir, const char *plugin_filename) {
  char *fullpath = calloc(strlen(plugin_dir) + strlen(plugin_filename) + 2, sizeof(char));
  strcpy(fullpath, plugin_dir);
  strcat(fullpath, "/");
  strcat(fullpath, plugin_filename);

  insight_plugin *plugin = calloc(1, sizeof(insight_plugin));

  /* load the plugin */
  plugin->dlhandle = dlopen(fullpath, RTLD_NOW|RTLD_GLOBAL);
  ifree(fullpath);

  if (plugin->dlhandle == NULL) {
    FMSG(LOG_WARNING, "Error calling dlopen() on plugin %s: %s", plugin_filename, dlerror());
    ifree(plugin);
    return -ELIBACC;
  }

  /* search for init function */
  DEBUG("Searching for init function %s()", INSIGHT_PLUGIN_INIT);
  plugin->init = dlsym(plugin->dlhandle, INSIGHT_PLUGIN_INIT);
  if (!plugin->init) {
    FMSG(LOG_WARNING, "Failed to find initialisation function %s() in plugin %s; not loading it.", INSIGHT_PLUGIN_INIT, plugin_filename);
    return -ENOSYS;
  }

  int res = plugin->init(&plugin->caps, &insight.funcs);

  if (res<0) {
    FMSG(LOG_WARNING, "Plugin %s initialisation failed with an error: %s", plugin_filename, strerror(-res));
    return -ECANCELED;
  }

  plugin->chmod = dlsym(plugin->dlhandle, INSIGHT_PLUGIN_CHMOD);
  if (!plugin->chmod) plugin->chmod = _plugin_default_chmod;
  plugin->chown = dlsym(plugin->dlhandle, INSIGHT_PLUGIN_CHOWN);
  if (!plugin->chown) plugin->chown = _plugin_default_chown;
  plugin->write = dlsym(plugin->dlhandle, INSIGHT_PLUGIN_WRITE);
  if (!plugin->write) plugin->write = _plugin_default_write;
  plugin->rename = dlsym(plugin->dlhandle, INSIGHT_PLUGIN_RENAME);
  if (!plugin->rename) plugin->rename = _plugin_default_rename;
  plugin->import = dlsym(plugin->dlhandle, INSIGHT_PLUGIN_IMPORT);
  if (!plugin->import) plugin->import = _plugin_default_import;
  plugin->cleanup = dlsym(plugin->dlhandle, INSIGHT_PLUGIN_CLEANUP);
  if (!plugin->cleanup) plugin->cleanup = _plugin_default_cleanup;

  /* add to global linked list */
  plugin->path = strdup(plugin_filename);
  plugin->next = insight.plugins;
  insight.plugins = plugin;

  FMSG(LOG_INFO, "Loaded plugin %s", plugin_filename);

  return 0;
}

/**
 * Load all of the plugins in the given directory.
 *
 * @param plugin_dir The full path to the directory to process.
 * @return Zero on success, negative error code on failure.
 */
int plugin_load_all(const char *plugin_dir) {
  DIR *p = opendir(plugin_dir);
  if (p == NULL) {
    FMSG(LOG_WARNING, "Error opening plugin directory \"%s\": %s", plugin_dir, strerror(errno));
    return -ENOENT;
  }

  struct dirent *de = NULL;
  while ((de = readdir(p)) != NULL) {
    /* checking if file begins with plugin prefix */
    char *needle = strstr(de->d_name, INSIGHT_PLUGIN_PREFIX);
    if ((needle == NULL) || (needle != de->d_name))
      continue;

    needle = strstr(de->d_name, PLUGIN_EXT);
    if ((needle == NULL) || (needle != de->d_name + strlen(de->d_name) - strlen(PLUGIN_EXT)))
      continue;

    /* found something that really looks like a plugin... let's load it */
    int res = plugin_load(plugin_dir, de->d_name);

    if (res<0) {
      FMSG(LOG_WARNING, "Failed to load plugin %s/%s: %s", plugin_dir, de->d_name, strerror(-res));
    }
  }
  closedir(p);

  return 0;
}

/**
 * Unload all plugins, calling their cleanup functions, etc.
 */
void plugin_unload_all() {
  insight_plugin *p=insight.plugins;
  insight.plugins=NULL;
  while (p) {
    p->cleanup();
    insight_plugin *pnext=p->next;
    ifree(p->path);
    dlclose(p->dlhandle);
    ifree(p);
    p=pnext;
  }
}
