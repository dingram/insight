#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <check.h>

#include <set_ops.h>

int pcharcmp(const void *p1, const void *p2) {
  return (*(const char*)p1>*(const char*)p2) ?  1 :
         (*(const char*)p1<*(const char*)p2) ? -1 :
         0;
}

int pintcmp(const void *p1, const void *p2) {
  return (*(const int*)p1>*(const int*)p2) ?  1 :
         (*(const int*)p1<*(const int*)p2) ? -1 :
         0;
}

#define PRINT_SET(set, count, alloc_count, format) {\
  size_t i;\
  printf("{");\
  for (i=0; i<alloc_count; i++) {\
    if (i>0 && i!=count)\
      printf(", ");\
    if (i==count)\
      printf("}\033[1;31m{");\
    printf(format, set[i]);\
  }\
  printf("}");\
  if (count<alloc_count)\
    printf("\033[m");\
}

void print_set_char(const char *set, size_t count, size_t alloc_count) {
  PRINT_SET(set, count, alloc_count, "%c");
}

void print_set_int(const int *set, size_t count, size_t alloc_count) {
  PRINT_SET(set, count, alloc_count, "%d");
}

#define UNION_TEST_abc(type) set_union(a, b, c, sizeof(a)/sizeof(type), sizeof(b)/sizeof(type), sizeof(c)/sizeof(type), sizeof(type), p##type##cmp)

START_TEST(test_setops_union_abc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = set_union(a, b, c, 0, 0, 0, sizeof(int), pintcmp);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(c[0]!=0, "Should not modify output");
  fail_if(c[1]!=0, "Should not modify output");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=0, "Should do nothing");
}
END_TEST

START_TEST(test_setops_union_ab_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = set_union(a, b, c, 0, 0, sizeof(c)/sizeof(c[0]), sizeof(int), pintcmp);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(c[0]!=0, "Should not modify output");
  fail_if(c[1]!=0, "Should not modify output");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=0, "Should do nothing");
}
END_TEST

START_TEST(test_setops_union_ac_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = set_union(a, b, c, 0, sizeof(b)/sizeof(b[0]), 0, sizeof(int), pintcmp);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(c[0]!=0, "Should not modify output");
  fail_if(c[1]!=0, "Should not modify output");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=0, "Should do nothing");
}
END_TEST

START_TEST(test_setops_union_bc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = set_union(a, b, c, sizeof(a)/sizeof(a[0]), 0, 0, sizeof(int), pintcmp);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(c[0]!=0, "Should not modify output");
  fail_if(c[1]!=0, "Should not modify output");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=0, "Should do nothing");
}
END_TEST

START_TEST(test_setops_union_a_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = set_union(a, b, c, 0, sizeof(b)/sizeof(b[0]), sizeof(c)/sizeof(c[0]), sizeof(int), pintcmp);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=1, "Should only provide one output item: output returned was %d", r);
  fail_if(c[0]!=2, "Incorrect output");
  fail_if(c[1]!=0, "Incorrect output");
}
END_TEST

START_TEST(test_setops_union_b_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = set_union(a, b, c, sizeof(a)/sizeof(a[0]), 0, sizeof(c)/sizeof(c[0]), sizeof(int), pintcmp);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=1, "Should only provide one output item: output returned was %d", r);
  fail_if(c[0]!=1, "Incorrect output");
  fail_if(c[1]!=0, "Incorrect output");
}
END_TEST

START_TEST(test_setops_union_c_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = set_union(a, b, c, sizeof(a)/sizeof(a[0]), sizeof(b)/sizeof(b[0]), 0, sizeof(int), pintcmp);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(c[0]!=0, "Should not modify output");
  fail_if(c[1]!=0, "Should not modify output");
  fail_if(r!=-EINVAL, "Should return -EINVAL, but actually returned %d", r);
}
END_TEST

START_TEST(test_setops_union_two_single_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=2, "Should output 2 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
}
END_TEST

START_TEST(test_setops_union_two_single_int2)
{
  int a[1] = { 2 };
  int b[1] = { 1 };
  int c[2] = { 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=2, "Should not modify input");
  fail_if(b[0]!=1, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=2, "Should output 2 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
}
END_TEST

START_TEST(test_setops_union_two_equal_int)
{
  int a[1] = { 1 };
  int b[1] = { 1 };
  int c[2] = { 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(b[0]!=1, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=1, "Should output 1 item, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=1, "Incorrect output: c[1]=%d, should be 1", c[1]);
}
END_TEST

START_TEST(test_setops_union_many_equal_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(a[1]!=2, "Should not modify input");
  fail_if(a[2]!=3, "Should not modify input");
  fail_if(a[3]!=4, "Should not modify input");
  fail_if(a[4]!=5, "Should not modify input");
  fail_if(b[0]!=1, "Should not modify input");
  fail_if(b[1]!=3, "Should not modify input");
  fail_if(b[2]!=5, "Should not modify input");
  fail_if(b[3]!=7, "Should not modify input");
  fail_if(b[4]!=9, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=7, "Should output 7 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
  fail_if(c[2]!=3, "Incorrect output: c[2]=%d, should be 3", c[2]);
  fail_if(c[3]!=4, "Incorrect output: c[3]=%d, should be 4", c[3]);
  fail_if(c[4]!=5, "Incorrect output: c[4]=%d, should be 5", c[4]);
  fail_if(c[5]!=7, "Incorrect output: c[5]=%d, should be 7", c[5]);
  fail_if(c[6]!=9, "Incorrect output: c[6]=%d, should be 9", c[6]);
}
END_TEST

START_TEST(test_setops_union_large_sets_int)
{
  int a[7] = { 1, 2, 3, 4, 6, 7, 8 };
  int b[7] = { 1, 3, 5, 6, 8, 9, 10 };
  int c[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!= 1, "Should not modify input");
  fail_if(a[1]!= 2, "Should not modify input");
  fail_if(a[2]!= 3, "Should not modify input");
  fail_if(a[3]!= 4, "Should not modify input");
  fail_if(a[4]!= 6, "Should not modify input");
  fail_if(a[5]!= 7, "Should not modify input");
  fail_if(a[6]!= 8, "Should not modify input");
  fail_if(b[0]!= 1, "Should not modify input");
  fail_if(b[1]!= 3, "Should not modify input");
  fail_if(b[2]!= 5, "Should not modify input");
  fail_if(b[3]!= 6, "Should not modify input");
  fail_if(b[4]!= 8, "Should not modify input");
  fail_if(b[5]!= 9, "Should not modify input");
  fail_if(b[6]!=10, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=10, "Should output 10 items, actually returned %d", r);
  fail_if(c[0]!= 1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!= 2, "Incorrect output: c[1]=%d, should be 2", c[1]);
  fail_if(c[2]!= 3, "Incorrect output: c[2]=%d, should be 3", c[2]);
  fail_if(c[3]!= 4, "Incorrect output: c[3]=%d, should be 4", c[3]);
  fail_if(c[4]!= 5, "Incorrect output: c[4]=%d, should be 5", c[4]);
  fail_if(c[5]!= 6, "Incorrect output: c[5]=%d, should be 6", c[5]);
  fail_if(c[6]!= 7, "Incorrect output: c[6]=%d, should be 7", c[6]);
  fail_if(c[7]!= 8, "Incorrect output: c[7]=%d, should be 8", c[7]);
  fail_if(c[8]!= 9, "Incorrect output: c[8]=%d, should be 9", c[8]);
  fail_if(c[9]!=10, "Incorrect output: c[9]=%d, should be 10", c[9]);
}
END_TEST

START_TEST(test_setops_union_large_unique_sets_int)
{
  int a[7] = { 1, 3, 5, 7,  9, 11, 13 };
  int b[7] = { 2, 4, 6, 8, 10, 12, 14 };
  int c[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!= 1, "Should not modify input");
  fail_if(a[1]!= 3, "Should not modify input");
  fail_if(a[2]!= 5, "Should not modify input");
  fail_if(a[3]!= 7, "Should not modify input");
  fail_if(a[4]!= 9, "Should not modify input");
  fail_if(a[5]!=11, "Should not modify input");
  fail_if(a[6]!=13, "Should not modify input");
  fail_if(b[0]!= 2, "Should not modify input");
  fail_if(b[1]!= 4, "Should not modify input");
  fail_if(b[2]!= 6, "Should not modify input");
  fail_if(b[3]!= 8, "Should not modify input");
  fail_if(b[4]!=10, "Should not modify input");
  fail_if(b[5]!=12, "Should not modify input");
  fail_if(b[6]!=14, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=14, "Should output 14 items, actually returned %d", r);
  fail_if(c[ 0]!= 1, "Incorrect output: c[0]=%d, should be 1",  c[ 0]);
  fail_if(c[ 1]!= 2, "Incorrect output: c[1]=%d, should be 2",  c[ 1]);
  fail_if(c[ 2]!= 3, "Incorrect output: c[2]=%d, should be 3",  c[ 2]);
  fail_if(c[ 3]!= 4, "Incorrect output: c[3]=%d, should be 4",  c[ 3]);
  fail_if(c[ 4]!= 5, "Incorrect output: c[4]=%d, should be 5",  c[ 4]);
  fail_if(c[ 5]!= 6, "Incorrect output: c[5]=%d, should be 6",  c[ 5]);
  fail_if(c[ 6]!= 7, "Incorrect output: c[6]=%d, should be 7",  c[ 6]);
  fail_if(c[ 7]!= 8, "Incorrect output: c[7]=%d, should be 8",  c[ 7]);
  fail_if(c[ 8]!= 9, "Incorrect output: c[8]=%d, should be 9",  c[ 8]);
  fail_if(c[ 9]!=10, "Incorrect output: c[9]=%d, should be 10", c[ 9]);
  fail_if(c[10]!=11, "Incorrect output: c[10]=%d, should be 11", c[10]);
  fail_if(c[11]!=12, "Incorrect output: c[11]=%d, should be 12", c[11]);
  fail_if(c[12]!=13, "Incorrect output: c[12]=%d, should be 13", c[12]);
  fail_if(c[13]!=14, "Incorrect output: c[13]=%d, should be 14", c[13]);
}
END_TEST

START_TEST(test_setops_union_all_identical_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 2, 3, 4, 5 };
  int c[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(a[1]!=2, "Should not modify input");
  fail_if(a[2]!=3, "Should not modify input");
  fail_if(a[3]!=4, "Should not modify input");
  fail_if(a[4]!=5, "Should not modify input");
  fail_if(b[0]!=1, "Should not modify input");
  fail_if(b[1]!=2, "Should not modify input");
  fail_if(b[2]!=3, "Should not modify input");
  fail_if(b[3]!=4, "Should not modify input");
  fail_if(b[4]!=5, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=5, "Should output 5 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
  fail_if(c[2]!=3, "Incorrect output: c[2]=%d, should be 3", c[2]);
  fail_if(c[3]!=4, "Incorrect output: c[3]=%d, should be 4", c[3]);
  fail_if(c[4]!=5, "Incorrect output: c[4]=%d, should be 5", c[4]);
}
END_TEST

START_TEST(test_setops_union_first_identical_int)
{
  int a[5] = { 1, 2, 4, 6, 8 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(a[1]!=2, "Should not modify input");
  fail_if(a[2]!=4, "Should not modify input");
  fail_if(a[3]!=6, "Should not modify input");
  fail_if(a[4]!=8, "Should not modify input");
  fail_if(b[0]!=1, "Should not modify input");
  fail_if(b[1]!=3, "Should not modify input");
  fail_if(b[2]!=5, "Should not modify input");
  fail_if(b[3]!=7, "Should not modify input");
  fail_if(b[4]!=9, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=9, "Should output 9 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
  fail_if(c[2]!=3, "Incorrect output: c[2]=%d, should be 3", c[2]);
  fail_if(c[3]!=4, "Incorrect output: c[3]=%d, should be 4", c[3]);
  fail_if(c[4]!=5, "Incorrect output: c[4]=%d, should be 5", c[4]);
  fail_if(c[5]!=6, "Incorrect output: c[5]=%d, should be 6", c[5]);
  fail_if(c[6]!=7, "Incorrect output: c[6]=%d, should be 7", c[6]);
  fail_if(c[7]!=8, "Incorrect output: c[7]=%d, should be 8", c[7]);
  fail_if(c[8]!=9, "Incorrect output: c[8]=%d, should be 9", c[8]);
}
END_TEST

START_TEST(test_setops_union_first2_identical_int)
{
  int a[5] = { 1, 2, 3, 5, 7 };
  int b[5] = { 1, 2, 4, 6, 8 };
  int c[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(a[1]!=2, "Should not modify input");
  fail_if(a[2]!=3, "Should not modify input");
  fail_if(a[3]!=5, "Should not modify input");
  fail_if(a[4]!=7, "Should not modify input");
  fail_if(b[0]!=1, "Should not modify input");
  fail_if(b[1]!=2, "Should not modify input");
  fail_if(b[2]!=4, "Should not modify input");
  fail_if(b[3]!=6, "Should not modify input");
  fail_if(b[4]!=8, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=8, "Should output 8 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
  fail_if(c[2]!=3, "Incorrect output: c[2]=%d, should be 3", c[2]);
  fail_if(c[3]!=4, "Incorrect output: c[3]=%d, should be 4", c[3]);
  fail_if(c[4]!=5, "Incorrect output: c[4]=%d, should be 5", c[4]);
  fail_if(c[5]!=6, "Incorrect output: c[5]=%d, should be 6", c[5]);
  fail_if(c[6]!=7, "Incorrect output: c[6]=%d, should be 7", c[6]);
  fail_if(c[7]!=8, "Incorrect output: c[7]=%d, should be 8", c[7]);
}
END_TEST

START_TEST(test_setops_union_last_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 9 };
  int b[5] = { 2, 4, 6, 8, 9 };
  int c[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(a[1]!=3, "Should not modify input");
  fail_if(a[2]!=5, "Should not modify input");
  fail_if(a[3]!=7, "Should not modify input");
  fail_if(a[4]!=9, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(b[1]!=4, "Should not modify input");
  fail_if(b[2]!=6, "Should not modify input");
  fail_if(b[3]!=8, "Should not modify input");
  fail_if(b[4]!=9, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=9, "Should output 9 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
  fail_if(c[2]!=3, "Incorrect output: c[2]=%d, should be 3", c[2]);
  fail_if(c[3]!=4, "Incorrect output: c[3]=%d, should be 4", c[3]);
  fail_if(c[4]!=5, "Incorrect output: c[4]=%d, should be 5", c[4]);
  fail_if(c[5]!=6, "Incorrect output: c[5]=%d, should be 6", c[5]);
  fail_if(c[6]!=7, "Incorrect output: c[6]=%d, should be 7", c[6]);
  fail_if(c[7]!=8, "Incorrect output: c[7]=%d, should be 8", c[7]);
  fail_if(c[8]!=9, "Incorrect output: c[8]=%d, should be 9", c[8]);
}
END_TEST

START_TEST(test_setops_union_last2_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 8 };
  int b[5] = { 2, 4, 6, 7, 8 };
  int c[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  int r = UNION_TEST_abc(int);
  fail_if(a[0]!=1, "Should not modify input");
  fail_if(a[1]!=3, "Should not modify input");
  fail_if(a[2]!=5, "Should not modify input");
  fail_if(a[3]!=7, "Should not modify input");
  fail_if(a[4]!=8, "Should not modify input");
  fail_if(b[0]!=2, "Should not modify input");
  fail_if(b[1]!=4, "Should not modify input");
  fail_if(b[2]!=6, "Should not modify input");
  fail_if(b[3]!=7, "Should not modify input");
  fail_if(b[4]!=8, "Should not modify input");
  fail_if(r<0, "Internal error: %d", -r);
  fail_if(r!=8, "Should output 8 items, actually returned %d", r);
  fail_if(c[0]!=1, "Incorrect output: c[0]=%d, should be 1", c[0]);
  fail_if(c[1]!=2, "Incorrect output: c[1]=%d, should be 2", c[1]);
  fail_if(c[2]!=3, "Incorrect output: c[2]=%d, should be 3", c[2]);
  fail_if(c[3]!=4, "Incorrect output: c[3]=%d, should be 4", c[3]);
  fail_if(c[4]!=5, "Incorrect output: c[4]=%d, should be 5", c[4]);
  fail_if(c[5]!=6, "Incorrect output: c[5]=%d, should be 6", c[5]);
  fail_if(c[6]!=7, "Incorrect output: c[6]=%d, should be 7", c[6]);
  fail_if(c[7]!=8, "Incorrect output: c[7]=%d, should be 8", c[7]);
}
END_TEST

Suite *setops_union_suite (void) {
  Suite *s = suite_create("set_ops union");

  TCase *tc_empty = tcase_create("Empty sets");
  tcase_add_test(tc_empty, test_setops_union_abc_empty_int);
  tcase_add_test(tc_empty, test_setops_union_ab_empty_int);
  tcase_add_test(tc_empty, test_setops_union_ac_empty_int);
  tcase_add_test(tc_empty, test_setops_union_bc_empty_int);
  tcase_add_test(tc_empty, test_setops_union_a_empty_int);
  tcase_add_test(tc_empty, test_setops_union_b_empty_int);
  tcase_add_test(tc_empty, test_setops_union_c_empty_int);
  suite_add_tcase(s, tc_empty);

  TCase *tc_simple = tcase_create("Simple tests");
  tcase_add_test(tc_simple, test_setops_union_two_single_int);
  tcase_add_test(tc_simple, test_setops_union_two_single_int2);
  tcase_add_test(tc_simple, test_setops_union_two_equal_int);
  tcase_add_test(tc_simple, test_setops_union_many_equal_int);
  tcase_add_test(tc_simple, test_setops_union_large_sets_int);
  tcase_add_test(tc_simple, test_setops_union_large_unique_sets_int);
  suite_add_tcase(s, tc_simple);

  TCase *tc_edge = tcase_create("Edge cases");
  tcase_add_test(tc_edge, test_setops_union_all_identical_int);
  tcase_add_test(tc_edge, test_setops_union_first_identical_int);
  tcase_add_test(tc_edge, test_setops_union_first2_identical_int);
  tcase_add_test(tc_edge, test_setops_union_last_identical_int);
  tcase_add_test(tc_edge, test_setops_union_last2_identical_int);
  suite_add_tcase(s, tc_edge);

  return s;
}

Suite *setops_intersect_suite (void) {
  Suite *s = suite_create("set_ops intersect");

  return s;
}

Suite *setops_diff_suite (void) {
  Suite *s = suite_create("set_ops diff");

  return s;
}

Suite *setops_uniq_suite (void) {
  Suite *s = suite_create("set_ops uniq");

  return s;
}

int main (void) {
  int number_failed;
  printf("\n\033[1;32m>>>\033[m BEGIN TESTS \033[1;37m==========================================================================\033[m\n\n");

  SRunner *sr = srunner_create( setops_union_suite() );
  srunner_add_suite(sr, setops_intersect_suite() );
  srunner_add_suite(sr, setops_diff_suite() );
  srunner_add_suite(sr, setops_uniq_suite() );

  srunner_run_all(sr, CK_NORMAL);

  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  printf("\n\033[1;32m>>>\033[m END OF TESTS \033[1;37m=========================================================================\033[m\n\n");

  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
