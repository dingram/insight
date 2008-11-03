/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */
/** Default implementation - returns <tt>ENOTSUP</tt>, thereby skipping this plugin. */

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
/**
 * Load a plugin, retrieve its function pointers, call its initialiser,
 * retrieve its capabilities, and add it to the global list.
 *
 * @param plugin_dir      The directory containing the plugin, without a trailing slash.
 * @param plugin_filename The filename for the plugin, to be appended to \a plugin_dir.
 * @return Zero on success, a negative error code on failure.
 */
/**
 * Load all of the plugins in the given directory.
 *
 * @param plugin_dir The full path to the directory to process.
 * @return Zero on success, negative error code on failure.
 */
    return -ENOENT;
    if ((needle == NULL) || (needle != de->d_name))
      continue;
/**
 * Unload all plugins, calling their cleanup functions, etc.
 */
void plugin_unload_all() {