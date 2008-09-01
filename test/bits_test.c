#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string_helpers.h>
#include <bplus.h>
#include <set_ops.h>
#include <insight.h>
#include <path_helpers.h>

#if (!defined _DEBUG && defined _DEBUG_MAIN)
# define _DEBUG
#endif
#include <debug.h>

#define INFO(f, ...) printf("\033[1;32m>>>\033[1;37m " f "\033[m\n", ## __VA_ARGS__)


static int tag_ensure_create(const char *tag) {
  INFO("  %s", tag);
  fileptr attrid;
  size_t len = strlen(tag);
  char *curtag=calloc(len+1, sizeof(char)); /* TODO: check for failure */
  strcpy(curtag, tag);

  /* loop backwards to find the closest parent tag that doesn't exist */
  /* i.e. check foo`bar`qux then foo`bar then foo */
  //DEBUG("Checking \"%s\"", curtag);
  int found=1;
  while (!(attrid=get_tag(curtag))) {
    char *t = rindex(curtag, INSIGHT_SUBKEY_SEP_C);
    if (t) {
      *t='\0';
    } else {
      found=0;
      break;
    }
    //DEBUG("Checking \"%s\"", curtag);
  }

  if (!attrid) {
    attrid = tree_get_root();
  }

  do {
    /* if found is set then curtag exists... so we skip it and go on to its child */
    if (!found) {
      tdata datan;

      /* create curtag */
      //DEBUG("Creating tag \"%s\" as a child of block %lu", curtag, attrid);

      char *the_tag = strlast(curtag, INSIGHT_SUBKEY_SEP_C);
      initDataNode(&datan);
      if (!tree_sub_insert(attrid, the_tag, (tblock*)&datan)) {
        PMSG(LOG_ERR, "Tree insertion failed");
        return -ENOSPC;
      }
      //DEBUG("Successfully inserted \"%s\" into parent", curtag);
      ifree(the_tag);

      attrid = get_tag(curtag);
      if (!attrid) {
        PMSG(LOG_ERR, "Could not find the tag I just created!");
        ifree(curtag);
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
  return attrid;
}

static int _sibcallback(const char *key, const fileptr val, void *data) {
  DEBUG("_sibcallback(\"%s\", %lu, ...)", key, val);
  struct {
    char **sibs;
    char *prefix;
    int cur;
    int sibcount;
  } *thing = data;
  if (thing->cur >= thing->sibcount) {
    DEBUG("SERIOUS ERROR");
    return -1;
  }
  thing->sibs[thing->cur] = calloc(strlen(thing->prefix)+strlen(key)+1, sizeof(char));
  strcpy(thing->sibs[thing->cur], thing->prefix);
  strcat(thing->sibs[thing->cur], key);
  thing->cur++;
  DEBUG("_sibcallback() // done");
  return 0;
}

void test_path(char *canon_path) {
  unsigned int i;

  int count;
  char **bits = strsplit(canon_path, '/', &count);

  INFO("Testing path \"%s\"...", canon_path);

  /* TODO: new algorithm:
   *         - for each path component (each item in bits):
   *           - add (with all ancestors) to PATHS set
   *         - for each element in PATHS:
   *           - union set of all (fully-qualified) siblings with TAGS set
   *         - result = TAGS - PATHS
   */

  unsigned int alloc_count=0, pathcount=0;
  /* ********** NEW ALGORITHM BEGIN ********** */
  // count amount of space we'll need in PATHS set
  alloc_count+=strcount(canon_path, INSIGHT_SUBKEY_SEP_C);
  alloc_count+=strcount(canon_path, '/');
  DEBUG("Path set count: %u", alloc_count);

  char **tag_set=calloc(alloc_count, sizeof(char*));
  char **path_set=calloc(alloc_count, sizeof(char*));
  unsigned int p=0;
  for (i=0; i<count; i++) {
    char *tmp;
    if (!*bits[i])
      continue;
    tag_set[p++]=strdup(bits[i]);
    while ((tmp=rindex(bits[i], INSIGHT_SUBKEY_SEP_C))) {
      *tmp='\0';
      tag_set[p++]=strdup(bits[i]);
    }
  }

  DEBUG("Freeing bits");

  for (count--;count>=0; count--) {
    ifree(bits[count]);
  }
  ifree(bits);

  DEBUG("Preparing set");
  pathcount = set_uniq(tag_set, path_set, alloc_count, alloc_count, sizeof(char*), pstrcmp);

  DEBUG("PATH set:");
  for (i=0; i<pathcount; i++) {
    path_set[i] = strdup(path_set[i]);
    DEBUG("  path_set[%u] = \"%s\"", i, path_set[i]);
  }

  for (i=0; i<alloc_count; i++) {
    ifree(tag_set[i]);
  }
  ifree(tag_set);

  /*
   * for each element in PATHS:
   *  - union set of all (fully-qualified) siblings with TAGS set
   */
  alloc_count=pathcount;
  char **outset = malloc(alloc_count*sizeof(char*));
  char **tmpset = NULL;
  memcpy(outset, path_set, alloc_count*sizeof(char*));
  char *curprefix=NULL;
  for (i=0; i<pathcount; i++) {
    /* get parent tag */
    fileptr tree_root;
    char *tmp=rindex(path_set[i], INSIGHT_SUBKEY_SEP_C);
    if (!tmp) {
      tree_root=0;
      curprefix=strdup("");
    } else {
      *tmp='\0';
      tree_root=get_tag(path_set[i]);
      curprefix=calloc(strlen(path_set[i])+2, sizeof(char));
      strcat(curprefix, path_set[i]);
      strcat(curprefix, INSIGHT_SUBKEY_SEP);
      *tmp=INSIGHT_SUBKEY_SEP_C;
    }
    INFO("Current prefix: \"%s\"", curprefix);
    /* get set of all siblings */
    unsigned int sibcount = tree_sub_key_count(tree_root);
    printf("%d siblings of tag \"%s\", including itself\n", sibcount, path_set[i]);

    char **sibset=calloc(sibcount+1, sizeof(char*));
    struct {
      char **sibs;
      char *prefix;
      int cur;
      int sibcount;
    } thing;
    thing.sibs = sibset;
    thing.cur=0;
    thing.sibcount=sibcount;
    thing.prefix=curprefix;
    DEBUG("tree_map_keys();");
    int ret = tree_map_keys(tree_root, _sibcallback, (void*)&thing);
    DEBUG("tree_map_keys(); // done");

    unsigned int k;
    for (k=0; k<sibcount; k++) {
      DEBUG("  sibset[%u] = \"%s\"", k, sibset[k]);
    }

    DEBUG("Realloc()ing tmpset");
    tmpset=realloc(tmpset, alloc_count*sizeof(char*)); /* TODO: check for failure */
    DEBUG("memcpy()");
    memcpy(tmpset, outset, alloc_count*sizeof(char*));
    DEBUG("Realloc()ing outset");
    outset=realloc(outset, (alloc_count+sibcount)*sizeof(char*));

    DEBUG("Set union");
    alloc_count = set_union(sibset, tmpset, outset, sibcount, alloc_count, alloc_count+(sibcount-1), sizeof(char*), pstrcmp);
    //DEBUG("Freeing sibset");
    ifree(sibset);
    ifree(curprefix);
  }
  tmpset=realloc(tmpset, alloc_count*sizeof(char*));
  memcpy(tmpset, outset, alloc_count*sizeof(char*));

  int out_count = set_diff(tmpset, path_set, outset, alloc_count, pathcount, alloc_count, sizeof(char*), pstrcmp);

  INFO("Out set: ");
  for (i=0; i<out_count; i++) {
    INFO("  %s", outset[i]);
  }

  INFO("Path set: ");
  for (i=0; i<pathcount; i++) {
    INFO("  %s", path_set[i]);
  }

  for (i=0; i<pathcount; i++) {
    ifree(path_set[i]);
  }
  ifree(path_set);
  ifree(outset);
  ifree(tmpset);
  /* **********  NEW ALGORITHM END  ********** */
}

int main(int argc, char ** argv) {
  INFO("Opening tree...");
  tree_open("test.tree");
  INFO("Done.");

  printf("\n");

  INFO("Populating tree...");
  tag_ensure_create("document");
  tag_ensure_create("film");
  tag_ensure_create("photo");
  tag_ensure_create("tv`show`Chuck");
  tag_ensure_create("tv`show`Trick or Treat");
  tag_ensure_create("tv`show`Lost");
  tag_ensure_create("tv`season`1");
  tag_ensure_create("tv`season`2");
  tag_ensure_create("tv`episode`1");
  tag_ensure_create("tv`episode`2");
  tag_ensure_create("tv`episode`3");
  tag_ensure_create("tv`episode`4");
  tag_ensure_create("tv`episode`5");
  tag_ensure_create("tv`episode`6");
  tag_ensure_create("tv`episode`7");
  tag_ensure_create("tv`episode`8");
  tag_ensure_create("tv`episode`9");
  INFO("Done.");

  printf("\n"); test_path("/tv");
  printf("\n"); test_path("/tv`show");
  printf("\n"); test_path("/tv`show`Chuck");
  printf("\n"); test_path("/tv`show`Chuck/tv`episode");
  printf("\n"); test_path("/tv`show`Chuck/tv`show`Trick or Treat");

  printf("\n");

  INFO("Closing tree");
  tree_close();
  INFO("Done.");

  printf("\n");

  INFO("Unlinking tree");
  unlink("test.tree");
  INFO("Done.");

  return 0;
}
