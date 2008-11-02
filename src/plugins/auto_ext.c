#include <insight_plugin.h>
#include <insight_consts.h>
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>

static insight_funcs *insight;

int insight_plugin_init(insight_plugin_caps *caps, insight_funcs *funcs) {
  caps->can_transduce_import=INSIGHT_CAP_CAN_ALL;
  caps->can_transduce_rename=INSIGHT_CAP_CAN_ALL;
  insight = funcs;
  return 0;
}

int insight_plugin_import(const char *realpath, const char *filename) {
  char *basepath = strdup(realpath);
  char *basename_s = basename(basepath);
  free(basepath);
  char *ext = rindex(basename_s, '.');
  char attr[255];
  memset(attr, 0, 255);
  if (ext) {
    ext = strdup(ext+1);
  } else {
    ext=malloc(sizeof(char));
    *ext='\0';
  }
  insight->log(LOG_DEBUG, "  File extension: \"%s\"", ext);
  if (*ext) {
    snprintf(attr, 254, "ext%s%s", INSIGHT_SUBKEY_SEP, ext);
    insight->log(LOG_DEBUG, "  ... add_auto_tag(\"%s\", \"%s\");", filename, attr);
    insight->add_auto_tag(insight->get_inode(filename), attr);
  }

  if (ext) free(ext);

  return EAGAIN;
}

int insight_plugin_rename(const char *realpath, const char *oldname, const char *newname) {
  (void) realpath;
  (void) oldname;
  (void) newname;
  insight->log(LOG_INFO, "auto_ext: rename()");
  return EAGAIN;
}
