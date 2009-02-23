#include <insight_plugin.h>
#include <insight_consts.h>
#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <tag_c.h>

static insight_funcs *insight;

int insight_plugin_init(insight_plugin_caps *caps, insight_funcs *funcs) {
  caps->can_transduce_import=INSIGHT_CAP_CAN;
  caps->transduce_exts = calloc(2,sizeof(char*));
  caps->transduce_exts[0] = calloc(4,sizeof(char));
  strncpy(caps->transduce_exts[0], "mp3", 4);
  insight = funcs;

  taglib_set_strings_unicode(0);

  insight->log(LOG_DEBUG, "auto_mp3: init()");
  return 0;
}

int insight_plugin_import(const char *realpath, const char *filename) {
  insight->log(LOG_DEBUG, "auto_mp3: import(\"%s\", \"%s\");", realpath, filename);

  int seconds;
  int minutes;
  TagLib_File *file;
  TagLib_Tag *tag;
  const TagLib_AudioProperties *properties;

  file = taglib_file_new(realpath);

  if(file == NULL) {
    insight->log(LOG_INFO, "Cannot import MP3 file: %s", realpath);
    // nothing to clear up
    return EAGAIN;
  }

  fileptr target = insight->get_inode(filename);
  char attr[255];

  tag = taglib_file_tag(file);
  properties = taglib_file_audioproperties(file);

  seconds = taglib_audioproperties_length(properties) % 60;
  minutes = (taglib_audioproperties_length(properties) - seconds) / 60;

  // NOTE: string cleanup handled by taglib_tag_free_strings() later
  char *field = taglib_tag_artist(tag);
  if (field) {
    snprintf(attr, 254, "music%cartist%c%s", INSIGHT_SUBKEY_SEP_C, INSIGHT_SUBKEY_SEP_C, field);
    insight->log(LOG_DEBUG, "Tagging: \"%s\"", attr);
    insight->add_tag(target, attr);
  }
  field = taglib_tag_title(tag);
  if (field) {
    snprintf(attr, 254, "music%ctitle%c%s", INSIGHT_SUBKEY_SEP_C, INSIGHT_SUBKEY_SEP_C, field);
    insight->log(LOG_DEBUG, "Tagging: \"%s\"", attr);
    insight->add_tag(target, attr);
  }
  field = taglib_tag_album(tag);
  if (field) {
    snprintf(attr, 254, "music%calbum%c%s", INSIGHT_SUBKEY_SEP_C, INSIGHT_SUBKEY_SEP_C, field);
    insight->log(LOG_DEBUG, "Tagging: \"%s\"", attr);
    insight->add_tag(target, attr);
  }
  unsigned int field_i = taglib_tag_track(tag);
  if (field_i) {
    snprintf(attr, 254, "music%ctrack%c%02u", INSIGHT_SUBKEY_SEP_C, INSIGHT_SUBKEY_SEP_C, field_i);
    insight->log(LOG_DEBUG, "Tagging: \"%s\"", attr);
    insight->add_tag(target, attr);
  }
  field=NULL;

  insight->log(LOG_DEBUG, "Would tag: \"music`length`%i:%02i\"", minutes, seconds);

  /*
  printf("title   - \"%s\"\n", taglib_tag_title(tag));
  printf("artist  - \"%s\"\n", taglib_tag_artist(tag));
  printf("album   - \"%s\"\n", taglib_tag_album(tag));
  printf("year    - \"%i\"\n", taglib_tag_year(tag));
  printf("comment - \"%s\"\n", taglib_tag_comment(tag));
  printf("track   - \"%i\"\n", taglib_tag_track(tag));
  printf("genre   - \"%s\"\n", taglib_tag_genre(tag));

  printf("-- AUDIO --\n");
  printf("bitrate     - %i\n", taglib_audioproperties_bitrate(properties));
  printf("sample rate - %i\n", taglib_audioproperties_samplerate(properties));
  printf("channels    - %i\n", taglib_audioproperties_channels(properties));
  printf("length      - %i:%02i\n", minutes, seconds);
  */

  taglib_tag_free_strings();
  taglib_file_free(file);

  //insight->add_tag(insight->get_inode(filename), attr);

  return EAGAIN;
}

int insight_plugin_rename(const char *realpath, const char *oldname, const char *newname) {
  (void) realpath;
  (void) oldname;
  (void) newname;
  insight->log(LOG_DEBUG, "auto_mp3: rename()");
  return EAGAIN;
}
