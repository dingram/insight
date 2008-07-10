#include <insight_plugin.h>

static insight_funcs *insight;

int insight_plugin_init(insight_plugin_caps *caps, insight_funcs *funcs) {
  caps->can_transduce_import=INSIGHT_CAP_CAN_ALL;
  caps->can_transduce_chown=INSIGHT_CAP_CAN_ALL;
  insight = funcs;
  return 0;
}

int insight_plugin_import(const char *realpath, const char *filename) {
  insight->log(LOG_INFO, "auto_owner: import()");
  return EAGAIN;
}

int insight_plugin_chown(const char *realpath, const char *filename, uid_t uid, gid_t gid) {
  insight->log(LOG_INFO, "auto_owner: chown()");
  return EAGAIN;
}
