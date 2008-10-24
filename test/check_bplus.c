#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <check.h>

#include <bplus.h>

#define TEST_TREE_FILENAME "my-test-tree"

static inline void DUMPDATA(tdata *node) {
  unsigned int i;
  printf(" [DATA NODE]\n");
  printf("  inodecount:     %d\n", node->inodecount);
  printf("  flags: %x\n", node->flags);
  printf("  subkeys: %lu\n", node->subkeys);
  printf("  inodes:");
  for(i=0;i<DATA_INODE_MAX;i++) printf((i>=node->inodecount?" [\033[4m%08lX\033[m]":" [%08lX]"), node->inodes[i]);
  printf("\n");
  printf("  name: %s\n", node->name);
  printf("  parent: %lu\n", node->parent);
  printf("  next_inodes: %lu\n", node->next_inodes);
}

void bplus_core_new_setup(void) {
  struct stat s;
  if (stat(TEST_TREE_FILENAME, &s)!=-1 || errno != ENOENT) {
    unlink(TEST_TREE_FILENAME);
  }
  fail_if(stat(TEST_TREE_FILENAME, &s)!=-1 || errno!=ENOENT, "Unlinking tree failed: %s", strerror(errno));
}

void bplus_core_existing_setup(void) {
  struct stat s;
  if (stat(TEST_TREE_FILENAME, &s)!=-1 || errno != ENOENT) {
    unlink(TEST_TREE_FILENAME);
  }
  fail_if(stat(TEST_TREE_FILENAME, &s)!=-1 || errno!=ENOENT, "Unlinking tree failed: %s", strerror(errno));
  // create sample tree
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
  tree_close();
}

void bplus_core_open_setup(void) {
  struct stat s;
  if (stat(TEST_TREE_FILENAME, &s)!=-1 || errno != ENOENT) {
    unlink(TEST_TREE_FILENAME);
  }
  fail_if(stat(TEST_TREE_FILENAME, &s)!=-1 || errno!=ENOENT, "Unlinking tree failed: %s", strerror(errno));
  // create sample tree
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
}

void bplus_null_teardown(void) {
}

void bplus_teardown(void) {
  struct stat s;
  if (stat(TEST_TREE_FILENAME, &s)!=-1 || errno != ENOENT) {
    unlink(TEST_TREE_FILENAME);
    if(stat(TEST_TREE_FILENAME, &s)!=-1 || errno!=ENOENT) {
      fprintf(stderr, "Unlinking tree failed: %s\n", strerror(errno));
    }
  }
}


START_TEST (test_bplus_core_new_open)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
  tree_close();
}
END_TEST

START_TEST (test_bplus_core_new_open2)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
  ret = tree_open(TEST_TREE_FILENAME);
  fail_unless(ret==EMFILE, "Opening tree again; didn't get expcted EMFILE error: %s", strerror(ret));
  tree_close();
}
END_TEST

START_TEST (test_bplus_core_new_check_sb)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");

  struct stat st;
  fail_if(stat(TEST_TREE_FILENAME, &st)==-1, "Could not stat tree file \"%s\": %s", TEST_TREE_FILENAME, strerror(errno));

  tsblock s;
  tree_read_sb(&s);
  fail_unless(s.magic == MAGIC_SUPERBLOCK, "Superblock magic wrong: %08x instead of %08x", s.magic,       MAGIC_SUPERBLOCK);
  fail_unless(s.root_index  == 1,          "Root index wrong: %lu instead of %lu",         s.root_index,  1);
  fail_unless(s.free_head   == 3,          "Free head index wrong: %lu instead of %lu",    s.free_head,   3);
  fail_unless(s.inode_limbo == 0,          "Inode limbo index wrong: %lu instead of %lu",  s.inode_limbo, 0);
  fail_unless(s.limbo_count == 0,          "Inode limbo count wrong: %lu instead of %lu",  s.limbo_count, 0);
  fail_unless(s.inode_root  == 2,          "Inode root index wrong: %lu instead of %lu",   s.inode_root,  2);
  fail_if    (s.max_size    == 0,          "Maximum size is wrong (zero)"                                  );
  fail_unless((1 + s.max_size) * TREEBLOCK_SIZE == st.st_size,
      "Max size (%lu blocks, %lu bytes) != filesize (%lu bytes)", (1 + s.max_size), (1 + s.max_size) * TREEBLOCK_SIZE, st.st_size);

  tree_close();
}
END_TEST

START_TEST (test_bplus_core_new_check_root)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
  tnode n;
  tree_read(1, (tblock*)&n);
  fail_unless(n.magic == MAGIC_TREENODE, "Root magic wrong: %08x instead of %08x",    n.magic,    MAGIC_TREENODE);
  fail_unless(n.leaf  == 1,              "Leaf flag wrong: %u instead of %u",         n.leaf,     1);
  fail_unless(n.keycount == 0,           "Key count wrong: %u instead of %u",         n.keycount, 0);
  fail_unless(n.ptrs[0] == 0,            "First pointer wrong: %lu instead of %lu",   n.ptrs[0],  0);
  fail_unless(*n.keys[0] == '\0',        "First key wrong: \"%s\" instead of \"%s\"", n.keys[0],  "");

  tree_close();
}
END_TEST

START_TEST (test_bplus_core_new_check_free_head)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
  freeblock b;
  tree_read(3, (tblock*)&b);
  fail_unless(b.magic == MAGIC_FREEBLOCK, "Free block magic wrong: %08x instead of %08x", b.magic, MAGIC_FREEBLOCK);
  fail_unless(b.next  == 4,               "Next index wrong: %lu instead of %lu",         b.next,  4);

  tree_close();
}
END_TEST

START_TEST (test_bplus_core_new_check_inode_root)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
  tnode n;
  tree_read(2, (tblock*)&n);
  fail_unless(n.magic == MAGIC_TREENODE, "Inode root magic wrong: %08x instead of %08x", n.magic,    MAGIC_TREENODE);
  fail_unless(n.leaf  == 1,              "Leaf flag wrong: %u instead of %u",            n.leaf,     1);
  fail_unless(n.keycount == 0,           "Key count wrong: %u instead of %u",            n.keycount, 0);
  fail_unless(n.ptrs[0] == 0,            "First pointer wrong: %lu instead of %lu",      n.ptrs[0],  0);
  fail_unless(*n.keys[0] == '\0',        "First key wrong: \"%s\" instead of \"%s\"",    n.keys[0],  "");

  tree_close();
}
END_TEST


START_TEST (test_bplus_core_existing_open)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Opening tree failed");
  tree_close();
}
END_TEST

START_TEST (test_bplus_core_existing_open2)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Opening tree failed");
  ret = tree_open(TEST_TREE_FILENAME);
  fail_unless(ret==EMFILE, "Opening tree again; didn't get expcted EMFILE error: %s", strerror(ret));
  tree_close();
}
END_TEST

START_TEST (test_bplus_core_existing_check_default_sb)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Opening tree failed");

  struct stat st;
  fail_if(stat(TEST_TREE_FILENAME, &st)==-1, "Could not stat tree file \"%s\": %s", TEST_TREE_FILENAME, strerror(errno));

  tsblock s;
  tree_read_sb(&s);
  fail_unless(s.magic == MAGIC_SUPERBLOCK, "Superblock magic wrong: %08x instead of %08x", s.magic,       MAGIC_SUPERBLOCK);
  fail_unless(s.root_index  == 1,          "Root index wrong: %lu instead of %lu",         s.root_index,  1);
  fail_unless(s.free_head   == 3,          "Free head index wrong: %lu instead of %lu",    s.free_head,   3);
  fail_unless(s.inode_limbo == 0,          "Inode limbo index wrong: %lu instead of %lu",  s.inode_limbo, 0);
  fail_unless(s.limbo_count == 0,          "Inode limbo count wrong: %lu instead of %lu",  s.limbo_count, 0);
  fail_unless(s.inode_root  == 2,          "Inode root index wrong: %lu instead of %lu",   s.inode_root,  2);
  fail_if    (s.max_size    == 0,          "Maximum size is wrong (zero)"                                  );
  fail_unless((1 + s.max_size) * TREEBLOCK_SIZE == st.st_size,
      "Max size (%lu blocks, %lu bytes) != filesize (%lu bytes)", (1 + s.max_size), (1 + s.max_size) * TREEBLOCK_SIZE, st.st_size);

  tree_close();
}
END_TEST

#if 0
int _output_keys(const char *key, const fileptr ignore, void *d) {
  (void)ignore;
  (void)d;
  (void)key;
  //printf("%s\n", key);
  return 0;
}
#endif

inline void _insert_n(const int n) {
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
}

START_TEST(test_bplus_ins_simple_ins)
{
  printf("   Simple insertions ");
  _insert_n(1);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins10)
{
  _insert_n(10);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins12)
{
  _insert_n(12);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins13)
{
  _insert_n(13);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins14)
{
  _insert_n(14);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins15)
{
  _insert_n(15);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins25)
{
  _insert_n(25);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins50)
{
  _insert_n(50);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins175)
{
  _insert_n(175);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins1000)
{
  _insert_n(1000);
  printf(".\n");
}
END_TEST

inline void _insert_check_immed_n(const int n) {
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion of key %s failed with error number %d (%s)", sid, errno, strerror(errno));
    fileptr p = tree_sub_search(tree_get_root(), sid);
    fail_unless(p, "Tree search for key %s failed with error number %d (%s)", sid, errno, strerror(errno));
  }
}

START_TEST(test_bplus_ins_check_immed_ins)
{
  printf("   Immediate-check insertions ");
  _insert_check_immed_n(1);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins10)
{
  _insert_check_immed_n(10);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins12)
{
  _insert_check_immed_n(12);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins13)
{
  _insert_check_immed_n(13);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins14)
{
  _insert_check_immed_n(14);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins15)
{
  _insert_check_immed_n(15);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins25)
{
  _insert_check_immed_n(25);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins50)
{
  _insert_check_immed_n(50);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins175)
{
  _insert_check_immed_n(175);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_immed_ins1000)
{
  _insert_check_immed_n(1000);
  printf(".\n");
}
END_TEST

inline void _insert_check_defer_n(const int n) {
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion of key %s failed with error number %d (%s)", sid, errno, strerror(errno));
  }
  for (i=0; i<n; i++) {
    fileptr r = tree_sub_search(tree_get_root(), sid);
    fail_unless(r, "Tree search for key %s failed with error number %d (%s)", sid, errno, strerror(errno));
  }
}

START_TEST(test_bplus_ins_check_defer_ins)
{
  printf("   Deferred-check insertions ");
  _insert_check_defer_n(1);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins10)
{
  _insert_check_defer_n(10);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins12)
{
  _insert_check_defer_n(12);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins13)
{
  _insert_check_defer_n(13);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins14)
{
  _insert_check_defer_n(14);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins15)
{
  _insert_check_defer_n(15);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins25)
{
  _insert_check_defer_n(25);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins50)
{
  _insert_check_defer_n(50);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins175)
{
  _insert_check_defer_n(175);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_defer_ins1000)
{
  _insert_check_defer_n(1000);
  printf(".\n");
}
END_TEST

inline void _insert_check_map_n(const int n) {
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion of key %s failed with error number %d (%s)", sid, errno, strerror(errno));
  }
  tkey keys[n];
  int g = tree_get_all_keys(tree_get_root(), keys, n);
  fail_if(g<0, "Fetching all keys failed with error number %d (%s)", -g, strerror(-g));
  fail_unless(g==n, "Fetching all keys failed: returned %d when we expected %d", g, n);
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    fail_if(strncmp(sid, keys[i], TREEKEY_SIZE), "Tree key match failed for key %d (\"%s\"), should be \"%s\"", i, keys[i], sid);
  }
}

START_TEST(test_bplus_ins_check_map_ins)
{
  printf("   Map-check insertions ");
  _insert_check_map_n(1);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins10)
{
  _insert_check_map_n(10);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins12)
{
  _insert_check_map_n(12);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins13)
{
  _insert_check_map_n(13);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins14)
{
  _insert_check_map_n(14);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins15)
{
  _insert_check_map_n(15);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins25)
{
  _insert_check_map_n(25);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins50)
{
  _insert_check_map_n(50);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins175)
{
  _insert_check_map_n(175);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_map_ins1000)
{
  _insert_check_map_n(1000);
  printf(".\n");
}
END_TEST

inline void _insert_check_data_immed_n(const int n) {
  tdata datan;
  tdata checkn;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion of key %s failed with error number %d (%s)", sid, errno, strerror(errno));
    fileptr p = tree_sub_get(tree_get_root(), sid, (tblock*)&checkn);
    fail_if(p, "Tree get for key %s failed with error number %d (%s)", sid, -p, strerror(-p));
    fail_if(memcmp(&datan, &checkn, sizeof(tdata)), "Data block verification for key %d (\"%s\") failed", i, sid);
  }
}

START_TEST(test_bplus_ins_check_data_immed_ins)
{
  printf("   Immediate-check insertions + data ");
  _insert_check_data_immed_n(1);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins10)
{
  _insert_check_data_immed_n(10);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins12)
{
  _insert_check_data_immed_n(12);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins13)
{
  _insert_check_data_immed_n(13);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins14)
{
  _insert_check_data_immed_n(14);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins15)
{
  _insert_check_data_immed_n(15);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins25)
{
  _insert_check_data_immed_n(25);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins50)
{
  _insert_check_data_immed_n(50);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins175)
{
  _insert_check_data_immed_n(175);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_immed_ins1000)
{
  _insert_check_data_immed_n(1000);
  printf(".\n");
}
END_TEST

inline void _insert_check_data_defer_n(const int n) {
  tdata datan[n];
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    initDataNode(&datan[i]);
    strncpy(datan[i].name, sid, TREEKEY_SIZE);
    datan[i].parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan[i]);
    fail_unless(r, "Tree insertion of key %s failed with error number %d (%s)", sid, errno, strerror(errno));
  }
  for (i=0; i<n; i++) {
    tdata checkn;
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    fileptr p = tree_sub_get(tree_get_root(), sid, (tblock*)&checkn);
    fail_if(p, "Tree get for key %s failed with error number %d (%s)", sid, -p, strerror(-p));
    p = memcmp(&datan[i], &checkn, sizeof(tdata));
    if (p) DUMPDATA(&checkn);
    fail_if(p, "Data block verification for key %d (\"%s\") failed", i, sid);
  }
}

START_TEST(test_bplus_ins_check_data_defer_ins)
{
  printf("   Deferred-check insertions + data ");
  _insert_check_data_defer_n(1);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins10)
{
  _insert_check_data_defer_n(10);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins12)
{
  _insert_check_data_defer_n(12);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins13)
{
  _insert_check_data_defer_n(13);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins14)
{
  _insert_check_data_defer_n(14);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins15)
{
  _insert_check_data_defer_n(15);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins25)
{
  _insert_check_data_defer_n(25);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins50)
{
  _insert_check_data_defer_n(50);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins175)
{
  _insert_check_data_defer_n(175);
  printf(".");
}
END_TEST

START_TEST(test_bplus_ins_check_data_defer_ins1000)
{
  _insert_check_data_defer_n(1000);
  printf(".\n");
}
END_TEST

#define doInsert(d, k, p, n) do {\
  initDataNode(&(d));\
  strncpy((d).name, (k), TREEKEY_SIZE);\
  (d).parent = (p);\
  n = tree_sub_insert(((p)==0)?tree_get_root():(p), (k), (tblock*)&(d));\
  fail_unless((n), "Tree insertion of key %s failed with error number %d (%s)", (k), errno, strerror(errno));\
} while (0)

START_TEST(test_bplus_ins_complex_df)
{
  fileptr newaddr[4];
  tdata datan;

  doInsert(datan, "attributes", 0, newaddr[0]);
  doInsert(datan,            "bronzed", newaddr[0], newaddr[1]);
  doInsert(datan,                    "huge",   newaddr[1], newaddr[2]);
  doInsert(datan,                    "large",  newaddr[1], newaddr[2]);
  doInsert(datan,                    "normal", newaddr[1], newaddr[2]);
  doInsert(datan,                    "tiny",   newaddr[1], newaddr[2]);
  doInsert(datan,            "ethnicity", newaddr[0], newaddr[1]);
  doInsert(datan,                      "asian",  newaddr[1], newaddr[2]);
  doInsert(datan,                      "black",  newaddr[1], newaddr[2]);
  doInsert(datan,                      "latina", newaddr[1], newaddr[2]);
  doInsert(datan,                      "mixed",  newaddr[1], newaddr[2]);
  doInsert(datan,                      "white",  newaddr[1], newaddr[2]);
  doInsert(datan,            "goth", newaddr[0], newaddr[1]);
  doInsert(datan,            "hair", newaddr[0], newaddr[1]);
  doInsert(datan,                 "black",    newaddr[1], newaddr[2]);
  doInsert(datan,                 "blonde",   newaddr[1], newaddr[2]);
  doInsert(datan,                 "brunette", newaddr[1], newaddr[2]);
  doInsert(datan,                 "redhead",  newaddr[1], newaddr[2]);
  doInsert(datan,            "matter",   newaddr[0], newaddr[1]);
  doInsert(datan,            "previous", newaddr[0], newaddr[1]);
  doInsert(datan,            "temp",     newaddr[0], newaddr[1]);
  doInsert(datan,            "twist",    newaddr[0], newaddr[1]);
  doInsert(datan,            "viking",   newaddr[0], newaddr[1]);
  doInsert(datan,            "young",    newaddr[0], newaddr[1]);
  doInsert(datan, "candle", 0, newaddr[0]);
  doInsert(datan,        "downstairs",  newaddr[0], newaddr[1]);
  doInsert(datan,        "nitric-skip", newaddr[0], newaddr[1]);
  doInsert(datan,        "upstair",     newaddr[0], newaddr[1]);
  doInsert(datan, "cartoon",  0, newaddr[0]);
  doInsert(datan, "cleaning", 0, newaddr[0]);
  doInsert(datan,          "backless", newaddr[0], newaddr[1]);
  doInsert(datan,          "bikini",   newaddr[0], newaddr[1]);
  doInsert(datan,                 "bottoms", newaddr[1], newaddr[2]);
  doInsert(datan,                 "full",    newaddr[1], newaddr[2]);
  doInsert(datan,                 "top",     newaddr[1], newaddr[2]);
  doInsert(datan,          "denim",       newaddr[0], newaddr[1]);
  doInsert(datan,          "dress",       newaddr[0], newaddr[1]);
  doInsert(datan,          "hotpants",    newaddr[0], newaddr[1]);
  doInsert(datan,          "jeans",       newaddr[0], newaddr[1]);
  doInsert(datan,          "provocative", newaddr[0], newaddr[1]);
  doInsert(datan,          "see-through", newaddr[0], newaddr[1]);
  doInsert(datan,          "shorts",      newaddr[0], newaddr[1]);
  doInsert(datan,          "skirt",       newaddr[0], newaddr[1]);
  doInsert(datan,                "knee",    newaddr[1], newaddr[2]);
  doInsert(datan,                "long",    newaddr[1], newaddr[2]);
  doInsert(datan,                "micro",   newaddr[1], newaddr[2]);
  doInsert(datan,                "mini",    newaddr[1], newaddr[2]);
  doInsert(datan,                "short",   newaddr[1], newaddr[2]);
  doInsert(datan,          "tight",     newaddr[0], newaddr[1]);
  doInsert(datan,          "underwear", newaddr[0], newaddr[1]);
  doInsert(datan,                    "bra",        newaddr[1], newaddr[2]);
  doInsert(datan,                    "corset",     newaddr[1], newaddr[2]);
  doInsert(datan,                    "panties",    newaddr[1], newaddr[2]);
  doInsert(datan,                    "stockings",  newaddr[1], newaddr[2]);
  doInsert(datan,                    "suspenders", newaddr[1], newaddr[2]);
  doInsert(datan,                    "thong",      newaddr[1], newaddr[2]);
  doInsert(datan, "compilation", 0, newaddr[0]);
  doInsert(datan, "favourite",   0, newaddr[0]);
  doInsert(datan, "girls",       0, newaddr[0]);
  doInsert(datan,       "1",        newaddr[0], newaddr[1]);
  doInsert(datan,       "2",        newaddr[0], newaddr[1]);
  doInsert(datan,       "3",        newaddr[0], newaddr[1]);
  doInsert(datan,       "4",        newaddr[0], newaddr[1]);
  doInsert(datan,       "5",        newaddr[0], newaddr[1]);
  doInsert(datan,       "6",        newaddr[0], newaddr[1]);
  doInsert(datan,       "7",        newaddr[0], newaddr[1]);
  doInsert(datan, "guys", 0, newaddr[0]);
  doInsert(datan,      "1", newaddr[0], newaddr[1]);
  doInsert(datan,      "2", newaddr[0], newaddr[1]);
  doInsert(datan, "location", 0, newaddr[0]);
  doInsert(datan,          "beach",         newaddr[0], newaddr[1]);
  doInsert(datan,          "chanting-room", newaddr[0], newaddr[1]);
  doInsert(datan,          "club",          newaddr[0], newaddr[1]);
  doInsert(datan,          "house",         newaddr[0], newaddr[1]);
  doInsert(datan,          "park",          newaddr[0], newaddr[1]);
  doInsert(datan,          "pool",          newaddr[0], newaddr[1]);
  doInsert(datan,          "public",        newaddr[0], newaddr[1]);
  doInsert(datan,          "school",        newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush", 0, newaddr[0]);
  doInsert(datan,           "bookdealer", newaddr[0], newaddr[1]);
  doInsert(datan,           "clothed",    newaddr[0], newaddr[1]);
  doInsert(datan,           "flooding",   newaddr[0], newaddr[1]);
  doInsert(datan,           "handbar",    newaddr[0], newaddr[1]);
  doInsert(datan,           "linguist",   newaddr[0], newaddr[1]);
  doInsert(datan,           "named",      newaddr[0], newaddr[1]);
  doInsert(datan,           "topsoil",    newaddr[0], newaddr[1]);
  doInsert(datan, "photo",  0, newaddr[0]);
  doInsert(datan, "series", 0, newaddr[0]);
  doInsert(datan,        "RealignKites", newaddr[0], newaddr[1]);
  doInsert(datan,                     "8thSidersLeaners",   newaddr[1], newaddr[2]);
  doInsert(datan,                     "BagNaggings",        newaddr[1], newaddr[2]);
  doInsert(datan,                     "CobFixate",          newaddr[1], newaddr[2]);
  doInsert(datan,                     "EvilSadPlanets",     newaddr[1], newaddr[2]);
  doInsert(datan,                     "FirstTileArrangers", newaddr[1], newaddr[2]);
  doInsert(datan,                     "FlowerTopic",        newaddr[1], newaddr[2]);
  doInsert(datan,                     "MileInBasins",       newaddr[1], newaddr[2]);
  doInsert(datan,                     "MimeNextDoor",       newaddr[1], newaddr[2]);
  doInsert(datan,                     "MoneyTicks",         newaddr[1], newaddr[2]);
  doInsert(datan,                     "MonsterClears",      newaddr[1], newaddr[2]);
  doInsert(datan,                     "Pour42",             newaddr[1], newaddr[2]);
  doInsert(datan,                     "StreetBlogJack",     newaddr[1], newaddr[2]);
  doInsert(datan, "sub", 0, newaddr[0]);
  doInsert(datan,     ".athlete", newaddr[0], newaddr[1]);
  doInsert(datan,              "dud", newaddr[1], newaddr[2]);
  doInsert(datan,              "dud`1",   newaddr[2], newaddr[3]);
  doInsert(datan,              "dud`2",   newaddr[2], newaddr[3]);
  doInsert(datan,              "hyper", newaddr[1], newaddr[2]);
  doInsert(datan,              "hyper`1", newaddr[2], newaddr[3]);
  doInsert(datan,     "a2ogm",    newaddr[0], newaddr[1]);
  doInsert(datan,     "avow",     newaddr[0], newaddr[1]);
  doInsert(datan,     "crackpot", newaddr[0], newaddr[1]);
  doInsert(datan,     "do",       newaddr[0], newaddr[1]);
  doInsert(datan,     "fulcrum",  newaddr[0], newaddr[1]);
  doInsert(datan,             "avow",    newaddr[1], newaddr[2]);
  doInsert(datan,             "self",    newaddr[1], newaddr[2]);
  doInsert(datan,             "vaccine", newaddr[1], newaddr[2]);
  doInsert(datan,     "gardener",  newaddr[0], newaddr[1]);
  doInsert(datan,     "invisible", newaddr[0], newaddr[1]);
  doInsert(datan,               "carrot",     newaddr[1], newaddr[2]);
  doInsert(datan,               "cucumber",   newaddr[1], newaddr[2]);
  doInsert(datan,               "golf-ball",  newaddr[1], newaddr[2]);
  doInsert(datan,               "hush-diner", newaddr[1], newaddr[2]);
  doInsert(datan,     "longbow", newaddr[0], newaddr[1]);
  doInsert(datan,             "62",        newaddr[1], newaddr[2]);
  doInsert(datan,             "fictional", newaddr[1], newaddr[2]);
  doInsert(datan,             "kinsman",   newaddr[1], newaddr[2]);
  doInsert(datan,             "opus",      newaddr[1], newaddr[2]);
  doInsert(datan,             "toys",      newaddr[1], newaddr[2]);
  doInsert(datan,                  "diner",    newaddr[2], newaddr[3]);
  doInsert(datan,                  "vocation", newaddr[2], newaddr[3]);
  doInsert(datan,     "opus", newaddr[0], newaddr[1]);
  doInsert(datan,          "2giftby", newaddr[1], newaddr[2]);
  doInsert(datan,                  "sugared",  newaddr[2], newaddr[3]);
  doInsert(datan,                  "toneless", newaddr[2], newaddr[3]);
  doInsert(datan,          "bp", newaddr[1], newaddr[2]);
  doInsert(datan,     "oven",             newaddr[0], newaddr[1]);
  doInsert(datan,     "p2ogm",            newaddr[0], newaddr[1]);
  doInsert(datan,     "reverse-gardener", newaddr[0], newaddr[1]);
  doInsert(datan,     "solo",             newaddr[0], newaddr[1]);
  doInsert(datan,          "fictional",      newaddr[1], newaddr[2]);
  doInsert(datan,          "toys",           newaddr[1], newaddr[2]);
  doInsert(datan,               "diner",        newaddr[2], newaddr[3]);
  doInsert(datan,               "vocation",     newaddr[2], newaddr[3]);
  doInsert(datan,     "threshold", newaddr[0], newaddr[1]);
  doInsert(datan,               "aaa", newaddr[1], newaddr[2]);
  doInsert(datan,               "baa", newaddr[1], newaddr[2]);
  doInsert(datan,               "bba", newaddr[1], newaddr[2]);
  doInsert(datan,     "vaccine", newaddr[0], newaddr[1]);
  doInsert(datan, "video", 0, newaddr[0]);

  tree_dump_dot(0);

  /*
  tkey keys[n];
  int g = tree_get_all_keys(tree_get_root(), keys, n);
  fail_if(g<0, "Fetching all keys failed with error number %d (%s)", -g, strerror(-g));
  fail_unless(g==n, "Fetching all keys failed: returned %d when we expected %d", g, n);
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    fail_if(strncmp(sid, keys[i], TREEKEY_SIZE), "Tree key match failed for key %d (\"%s\"), should be \"%s\"", i, keys[i], sid);
  }
  */
}
END_TEST

START_TEST(test_bplus_ins_complex_bf)
{
  fileptr newaddr[4];
  tdata datan;

  // TODO: this is mostly wrong

  doInsert(datan, "attributes",  0, newaddr[0]);
  doInsert(datan, "candle",      0, newaddr[0]);
  doInsert(datan, "cartoon",     0, newaddr[0]);
  doInsert(datan, "cleaning",    0, newaddr[0]);
  doInsert(datan, "compilation", 0, newaddr[0]);
  doInsert(datan, "favourite",   0, newaddr[0]);
  doInsert(datan, "girls",       0, newaddr[0]);
  doInsert(datan, "guys",        0, newaddr[0]);
  doInsert(datan, "location",    0, newaddr[0]);
  doInsert(datan, "nailbrush",   0, newaddr[0]);
  doInsert(datan, "photo",       0, newaddr[0]);
  doInsert(datan, "series",      0, newaddr[0]);
  doInsert(datan, "sub",         0, newaddr[0]);
  doInsert(datan, "video",       0, newaddr[0]);

  doInsert(datan, "attributes`bronzed",     newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`ethnicity",   newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`goth",        newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`hair",        newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`matter",      newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`previous",    newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`temp",        newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`twist",       newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`viking",      newaddr[0], newaddr[1]);
  doInsert(datan, "attributes`young",       newaddr[0], newaddr[1]);
  doInsert(datan, "candle`downstairs",      newaddr[0], newaddr[1]);
  doInsert(datan, "candle`nitric-skip",     newaddr[0], newaddr[1]);
  doInsert(datan, "candle`upstair",         newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`backless",      newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`bikini",        newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`denim",         newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`dress",         newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`hotpants",      newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`jeans",         newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`provocative",   newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`see-through",   newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`shorts",        newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`skirt",         newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`tight",         newaddr[0], newaddr[1]);
  doInsert(datan, "cleaning`underwear",     newaddr[0], newaddr[1]);
  doInsert(datan, "girls`1",                newaddr[0], newaddr[1]);
  doInsert(datan, "girls`2",                newaddr[0], newaddr[1]);
  doInsert(datan, "girls`3",                newaddr[0], newaddr[1]);
  doInsert(datan, "girls`4",                newaddr[0], newaddr[1]);
  doInsert(datan, "girls`5",                newaddr[0], newaddr[1]);
  doInsert(datan, "girls`6",                newaddr[0], newaddr[1]);
  doInsert(datan, "girls`7",                newaddr[0], newaddr[1]);
  doInsert(datan, "guys`1",                 newaddr[0], newaddr[1]);
  doInsert(datan, "guys`2",                 newaddr[0], newaddr[1]);
  doInsert(datan, "location`beach",         newaddr[0], newaddr[1]);
  doInsert(datan, "location`chanting-room", newaddr[0], newaddr[1]);
  doInsert(datan, "location`club",          newaddr[0], newaddr[1]);
  doInsert(datan, "location`house",         newaddr[0], newaddr[1]);
  doInsert(datan, "location`park",          newaddr[0], newaddr[1]);
  doInsert(datan, "location`pool",          newaddr[0], newaddr[1]);
  doInsert(datan, "location`public",        newaddr[0], newaddr[1]);
  doInsert(datan, "location`school",        newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush`bookdealer",   newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush`clothed",      newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush`flooding",     newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush`handbar",      newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush`linguist",     newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush`named",        newaddr[0], newaddr[1]);
  doInsert(datan, "nailbrush`topsoil",      newaddr[0], newaddr[1]);
  doInsert(datan, "series`RealignKites",    newaddr[0], newaddr[1]);
  doInsert(datan, "sub`.athlete",           newaddr[0], newaddr[1]);
  doInsert(datan, "sub`a2ogm",              newaddr[0], newaddr[1]);
  doInsert(datan, "sub`avow",               newaddr[0], newaddr[1]);
  doInsert(datan, "sub`crackpot",           newaddr[0], newaddr[1]);
  doInsert(datan, "sub`do",                 newaddr[0], newaddr[1]);
  doInsert(datan, "sub`fulcrum",            newaddr[0], newaddr[1]);
  doInsert(datan, "sub`gardener",           newaddr[0], newaddr[1]);
  doInsert(datan, "sub`invisible",          newaddr[0], newaddr[1]);
  doInsert(datan, "sub`longbow",            newaddr[0], newaddr[1]);
  doInsert(datan, "sub`opus",               newaddr[0], newaddr[1]);
  doInsert(datan, "sub`oven",               newaddr[0], newaddr[1]);
  doInsert(datan, "sub`p2ogm",              newaddr[0], newaddr[1]);
  doInsert(datan, "sub`reverse-gardener",   newaddr[0], newaddr[1]);
  doInsert(datan, "sub`solo",               newaddr[0], newaddr[1]);
  doInsert(datan, "sub`threshold",          newaddr[0], newaddr[1]);
  doInsert(datan, "sub`vaccine",            newaddr[0], newaddr[1]);

  doInsert(datan, "attributes`bronzed`huge",                newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`bronzed`large",               newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`bronzed`normal",              newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`bronzed`tiny",                newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`ethnicity`asian",             newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`ethnicity`black",             newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`ethnicity`latina",            newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`ethnicity`mixed",             newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`ethnicity`white",             newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`hair`black",                  newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`hair`blonde",                 newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`hair`brunette",               newaddr[1], newaddr[2]);
  doInsert(datan, "attributes`hair`redhead",                newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`bikini`bottoms",                newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`bikini`full",                   newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`bikini`top",                    newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`skirt`knee",                    newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`skirt`long",                    newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`skirt`micro",                   newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`skirt`mini",                    newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`skirt`short",                   newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`underwear`bra",                 newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`underwear`corset",              newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`underwear`panties",             newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`underwear`stockings",           newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`underwear`suspenders",          newaddr[1], newaddr[2]);
  doInsert(datan, "cleaning`underwear`thong",               newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`8thSidersLeaners",   newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`BagNaggings",        newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`CobFixate",          newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`EvilSadPlanets",     newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`FirstTileArrangers", newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`FlowerTopic",        newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`MileInBasins",       newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`MimeNextDoor",       newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`MoneyTicks",         newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`MonsterClears",      newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`Pour42",             newaddr[1], newaddr[2]);
  doInsert(datan, "series`RealignKites`StreetBlogJack",     newaddr[1], newaddr[2]);
  doInsert(datan, "sub`.athlete`dud",                       newaddr[1], newaddr[2]);
  doInsert(datan, "sub`.athlete`hyper",                     newaddr[1], newaddr[2]);
  doInsert(datan, "sub`fulcrum`avow",                       newaddr[1], newaddr[2]);
  doInsert(datan, "sub`fulcrum`self",                       newaddr[1], newaddr[2]);
  doInsert(datan, "sub`fulcrum`vaccine",                    newaddr[1], newaddr[2]);
  doInsert(datan, "sub`invisible`carrot",                   newaddr[1], newaddr[2]);
  doInsert(datan, "sub`invisible`cucumber",                 newaddr[1], newaddr[2]);
  doInsert(datan, "sub`invisible`golf-ball",                newaddr[1], newaddr[2]);
  doInsert(datan, "sub`invisible`hush-diner",               newaddr[1], newaddr[2]);
  doInsert(datan, "sub`longbow`62",                         newaddr[1], newaddr[2]);
  doInsert(datan, "sub`longbow`fictional",                  newaddr[1], newaddr[2]);
  doInsert(datan, "sub`longbow`kinsman",                    newaddr[1], newaddr[2]);
  doInsert(datan, "sub`longbow`opus",                       newaddr[1], newaddr[2]);
  doInsert(datan, "sub`longbow`toys",                       newaddr[1], newaddr[2]);
  doInsert(datan, "sub`opus`2giftby",                       newaddr[1], newaddr[2]);
  doInsert(datan, "sub`opus`bp",                            newaddr[1], newaddr[2]);
  doInsert(datan, "sub`solo`fictional",                     newaddr[1], newaddr[2]);
  doInsert(datan, "sub`solo`toys",                          newaddr[1], newaddr[2]);
  doInsert(datan, "sub`threshold`aaa",                      newaddr[1], newaddr[2]);
  doInsert(datan, "sub`threshold`baa",                      newaddr[1], newaddr[2]);
  doInsert(datan, "sub`threshold`bba",                      newaddr[1], newaddr[2]);

  doInsert(datan, "sub`.athlete`dud`1",         newaddr[2], newaddr[3]);
  doInsert(datan, "sub`.athlete`dud`2",         newaddr[2], newaddr[3]);
  doInsert(datan, "sub`.athlete`hyper`1",       newaddr[2], newaddr[3]);
  doInsert(datan, "sub`longbow`toys`diner",     newaddr[2], newaddr[3]);
  doInsert(datan, "sub`longbow`toys`vocation",  newaddr[2], newaddr[3]);
  doInsert(datan, "sub`opus`2giftby`sugared",   newaddr[2], newaddr[3]);
  doInsert(datan, "sub`opus`2giftby`toneless",  newaddr[2], newaddr[3]);
  doInsert(datan, "sub`solo`toys`diner",        newaddr[2], newaddr[3]);
  doInsert(datan, "sub`solo`toys`vocation",     newaddr[2], newaddr[3]);

  /*
  tkey keys[n];
  int g = tree_get_all_keys(tree_get_root(), keys, n);
  fail_if(g<0, "Fetching all keys failed with error number %d (%s)", -g, strerror(-g));
  fail_unless(g==n, "Fetching all keys failed: returned %d when we expected %d", g, n);
  for (i=0; i<n; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%04d", i);
    fail_if(strncmp(sid, keys[i], TREEKEY_SIZE), "Tree key match failed for key %d (\"%s\"), should be \"%s\"", i, keys[i], sid);
  }
  */
}
END_TEST


Suite * bplus_core_suite (void) {
  Suite *s = suite_create("bplus core");

  TCase *tc_core_new = tcase_create("Core (new)");
  tcase_add_checked_fixture(tc_core_new, bplus_core_new_setup, bplus_teardown);
  tcase_add_test(tc_core_new, test_bplus_core_new_open);
  tcase_add_test(tc_core_new, test_bplus_core_new_open2);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_sb);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_root);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_free_head);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_inode_root);
  suite_add_tcase(s, tc_core_new);

  TCase *tc_core_existing = tcase_create("Core (existing)");
  tcase_add_checked_fixture(tc_core_new, bplus_core_existing_setup, bplus_teardown);
  tcase_add_test(tc_core_existing, test_bplus_core_existing_open);
  tcase_add_test(tc_core_existing, test_bplus_core_existing_open2);
  tcase_add_test(tc_core_existing, test_bplus_core_existing_check_default_sb);
  suite_add_tcase(s, tc_core_existing);

  return s;
}

Suite *bplus_simple_insert_suite (void) {
  Suite *s = suite_create("bplus simple insertion");

  TCase *tc_ins_simple = tcase_create("Simple insertion");
  tcase_add_checked_fixture(tc_ins_simple, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins10);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins12);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins13);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins14);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins15);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins25);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins50);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins175);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins1000);
  suite_add_tcase(s, tc_ins_simple);

  /* i.e. insert and then search for the key within the same loop */
  TCase *tc_ins_check_immed = tcase_create("Insertion and immediate key check");
  tcase_add_checked_fixture(tc_ins_check_immed, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins10);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins12);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins13);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins14);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins15);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins25);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins50);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins175);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins1000);
  suite_add_tcase(s, tc_ins_check_immed);

  /* i.e. insert and then search for the key within a separate loop */
  TCase *tc_ins_check_defer = tcase_create("Insertion and deferred key check");
  tcase_add_checked_fixture(tc_ins_check_defer, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins10);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins12);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins13);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins14);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins15);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins25);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins50);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins175);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins1000);
  suite_add_tcase(s, tc_ins_check_defer);

  /* i.e. insert and then check the array of keys matches what's expected */
  TCase *tc_ins_check_map = tcase_create("Insertion and map_keys check");
  tcase_add_checked_fixture(tc_ins_check_map, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins10);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins12);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins13);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins14);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins15);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins25);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins50);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins175);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins1000);
  suite_add_tcase(s, tc_ins_check_map);

  /* i.e. insert and then search for the key within the same loop and check data */
  TCase *tc_ins_check_data_immed = tcase_create("Insertion and immediate key + data check");
  tcase_add_checked_fixture(tc_ins_check_data_immed, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins10);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins12);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins13);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins14);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins15);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins25);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins50);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins175);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins1000);
  suite_add_tcase(s, tc_ins_check_data_immed);

  /* i.e. insert and then search for the key within a separate loop and check data */
  TCase *tc_ins_check_data_defer = tcase_create("Insertion and deferred key + data check");
  tcase_add_checked_fixture(tc_ins_check_data_defer, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins10);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins12);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins13);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins14);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins15);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins25);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins50);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins175);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins1000);
  suite_add_tcase(s, tc_ins_check_data_defer);

  return s;
}

Suite *bplus_sub_insert_single_suite (void) {
  Suite *s = suite_create("bplus subtree insertion");

  TCase *tc_ins_simple = tcase_create("Simple insertion");
  tcase_add_checked_fixture(tc_ins_simple, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins10);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins12);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins13);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins14);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins15);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins25);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins50);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins175);
  tcase_add_test(tc_ins_simple, test_bplus_ins_simple_ins1000);
  suite_add_tcase(s, tc_ins_simple);

  /* i.e. insert and then search for the key within the same loop */
  TCase *tc_ins_check_immed = tcase_create("Insertion and immediate key check");
  tcase_add_checked_fixture(tc_ins_check_immed, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins10);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins12);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins13);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins14);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins15);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins25);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins50);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins175);
  tcase_add_test(tc_ins_check_immed, test_bplus_ins_check_immed_ins1000);
  suite_add_tcase(s, tc_ins_check_immed);

  /* i.e. insert and then search for the key within a separate loop */
  TCase *tc_ins_check_defer = tcase_create("Insertion and deferred key check");
  tcase_add_checked_fixture(tc_ins_check_defer, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins10);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins12);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins13);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins14);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins15);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins25);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins50);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins175);
  tcase_add_test(tc_ins_check_defer, test_bplus_ins_check_defer_ins1000);
  suite_add_tcase(s, tc_ins_check_defer);

  /* i.e. insert and then check the array of keys matches what's expected */
  TCase *tc_ins_check_map = tcase_create("Insertion and map_keys check");
  tcase_add_checked_fixture(tc_ins_check_map, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins10);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins12);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins13);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins14);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins15);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins25);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins50);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins175);
  tcase_add_test(tc_ins_check_map, test_bplus_ins_check_map_ins1000);
  suite_add_tcase(s, tc_ins_check_map);

  /* i.e. insert and then search for the key within the same loop and check data */
  TCase *tc_ins_check_data_immed = tcase_create("Insertion and immediate key + data check");
  tcase_add_checked_fixture(tc_ins_check_data_immed, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins10);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins12);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins13);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins14);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins15);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins25);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins50);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins175);
  tcase_add_test(tc_ins_check_data_immed, test_bplus_ins_check_data_immed_ins1000);
  suite_add_tcase(s, tc_ins_check_data_immed);

  /* i.e. insert and then search for the key within a separate loop and check data */
  TCase *tc_ins_check_data_defer = tcase_create("Insertion and deferred key + data check");
  tcase_add_checked_fixture(tc_ins_check_data_defer, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins10);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins12);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins13);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins14);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins15);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins25);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins50);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins175);
  tcase_add_test(tc_ins_check_data_defer, test_bplus_ins_check_data_defer_ins1000);
  suite_add_tcase(s, tc_ins_check_data_defer);

  return s;
}

Suite *bplus_complex_insert_suite (void) {
  Suite *s = suite_create("bplus complex insertion");

  TCase *tc_depth_first = tcase_create("Complex (depth-first)");
  tcase_add_checked_fixture(tc_depth_first, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_depth_first, test_bplus_ins_complex_df);
  suite_add_tcase(s, tc_depth_first);

  TCase *tc_breadth_first = tcase_create("Complex (breadth-first)");
  tcase_add_checked_fixture(tc_breadth_first, bplus_core_open_setup, bplus_teardown);
  tcase_add_test(tc_breadth_first, test_bplus_ins_complex_bf);
  suite_add_tcase(s, tc_breadth_first);

  return s;
}

// TODO: reverse-order insertion
// TODO: random-order insertion

int main (void) {
  int number_failed;
  printf("\n\033[1;32m>>>\033[m BEGIN TESTS \033[1;37m==========================================================================\033[m\n\n");

  SRunner *sr = srunner_create( bplus_core_suite() );
  srunner_add_suite(sr, bplus_simple_insert_suite() );
  srunner_add_suite(sr, bplus_complex_insert_suite() );

  srunner_run_all(sr, CK_NORMAL);

  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  printf("\n\033[1;32m>>>\033[m END OF TESTS \033[1;37m=========================================================================\033[m\n\n");

  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
