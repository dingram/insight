#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <debug.h>
#include <bplus.h>

#define TEST_TREE_PATH "./test-tree"
#define BEGIN_TEST(x, ...) printf("\033[1;34m***\033[m BEGIN: " x, ## __VA_ARGS__);
#define SUCCEED(x, ...)    printf("  \033[1;32m>>>\033[m " x, ## __VA_ARGS__);
#define DEBUG(x, ...)      printf("  \033[1;37m###\033[m " x, ## __VA_ARGS__);
#define FAIL(x, ...)       printf("  \033[1;31m!!!\033[m " x, ## __VA_ARGS__);
#define END_TEST(x, ...)   printf("\033[1;34m***\033[m END:   " x, ## __VA_ARGS__);

void test_unlink_tree() {
}

void test_open_close_new() {
}

void test_open_close_existing() {
}

void test_insert() {
}

void test_delete() {
}

void test_insert_10() {
}

void test_delete_10() {
}

void test_insert_50() {
}

void test_delete_50() {
}

void test_insert_100() {
}

void test_delete_100() {
}

/*
 * Insert just enough nodes so that the next insertion will force a split
 */
void test_insert_almost_split() {
}

/*
 * Delete just enough nodes so that the next deletion will force a merge
 */
void test_delete_almost_merge() {
}

/*
 * Insert just enough nodes to cause a split
 */
void test_insert_just_split() {
}

/*
 * Delete just enough nodes to cause a merge
 */
void test_delete_just_merge() {
}

void test_insert_500() {
}

void test_delete_500() {
}

/*
 * Test that we can find a node
 */
void test_find() {
}

/*
 * Test that we cannot find a node
 */
void test_notfind() {
}

int main(int argc, char **argv) {
  return 0;
}
