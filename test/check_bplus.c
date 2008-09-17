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

void bplus_core_new_teardown(void) {
  struct stat s;
  if (stat(TEST_TREE_FILENAME, &s)!=-1 || errno != ENOENT) {
    unlink(TEST_TREE_FILENAME);
  } else {
    fprintf(stderr, "Could not find tree to remove\n");
  }
  if(stat(TEST_TREE_FILENAME, &s)!=-1 || errno!=ENOENT) {
    fprintf(stderr, "Unlinking tree failed: %s\n", strerror(errno));
  }
}


START_TEST (test_bplus_core_new_open)
{
  int ret = tree_open(TEST_TREE_FILENAME);
  fail_if(ret, "Creating tree failed");
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


Suite * bplus_core_suite (void) {
  Suite *s = suite_create("bplus core");

  TCase *tc_core_new = tcase_create("Core (new)");
  tcase_add_checked_fixture(tc_core_new, bplus_core_new_setup, bplus_core_new_teardown);
  tcase_add_test(tc_core_new, test_bplus_core_new_open);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_sb);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_root);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_free_head);
  tcase_add_test(tc_core_new, test_bplus_core_new_check_inode_root);
  suite_add_tcase(s, tc_core_new);

  TCase *tc_core_existing = tcase_create("Core (existing)");
  //tcase_add_checked_fixture(tc_core, bplus_core_existing_setup, bplus_core_existing_teardown);
  tcase_add_test(tc_core_existing, test_bplus_core_existing_open);
  suite_add_tcase(s, tc_core_existing);

  return s;
}

Suite *bplus_insert_suite (void) {
  Suite *s = suite_create("bplus insertion");

  return s;
}

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
