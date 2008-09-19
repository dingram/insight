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

int _output_keys(const char *key, const fileptr ignore, void *d) {
  (void)ignore;
  (void)d;
  //printf("%s\n", key);
  return 0;
}

START_TEST(test_bplus_ins_simple_ins)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  snprintf(sid, TREEKEY_SIZE, "k%03d", 0);
  initDataNode(&datan);
  strncpy(datan.name, sid, TREEKEY_SIZE);
  datan.parent = 0;
  int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
  fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins10)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<10; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins12)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<12; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins13)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<13; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins14)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<14; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins15)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<15; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins25)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<25; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins50)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<50; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
}
END_TEST

START_TEST(test_bplus_ins_simple_ins175)
{
  tdata datan;
  char sid[TREEKEY_SIZE] = { 0 };
  int i;
  for (i=0; i<175; i++) {
    snprintf(sid, TREEKEY_SIZE, "k%03d", i);
    initDataNode(&datan);
    strncpy(datan.name, sid, TREEKEY_SIZE);
    datan.parent = 0;
    int r = tree_sub_insert(tree_get_root(), sid, (tblock*)&datan);
    fail_unless(r, "Tree insertion failed with error number %d (%s)", errno, strerror(errno));
  }
  tree_map_keys(tree_get_root(), _output_keys, NULL);
  //printf("\n");
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

Suite *bplus_insert_suite (void) {
  Suite *s = suite_create("bplus insertion");

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
  suite_add_tcase(s, tc_ins_simple);

#if 0
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
  suite_add_tcase(s, tc_ins_check_map);
#endif

  return s;
}

// TODO: random-order insertion
// TODO: reverse-order insertion

int main (void) {
  int number_failed;
  printf("\n\033[1;32m>>>\033[m BEGIN TESTS \033[1;37m==========================================================================\033[m\n\n");

  SRunner *sr = srunner_create( bplus_core_suite() );
  srunner_add_suite(sr, bplus_insert_suite() );

  srunner_run_all(sr, CK_NORMAL);

  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  printf("\n\033[1;32m>>>\033[m END OF TESTS \033[1;37m=========================================================================\033[m\n\n");

  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
