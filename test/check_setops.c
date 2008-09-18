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

#define UNION_TEST(type, format) do {\
  size_t i;\
  type ia[(sizeof(a)/sizeof(type))];\
  for (i=0; i<(sizeof(a)/sizeof(a[0])); i++) ia[i]=a[i];\
  type ib[(sizeof(a)/sizeof(type))];\
  for (i=0; i<(sizeof(b)/sizeof(b[0])); i++) ib[i]=b[i];\
  int r = UNION_TEST_abc(type);\
  for (i=0; i<(sizeof(a)/sizeof(a[0])); i++)\
    fail_if(a[i]!=ia[i], "Should not modify input; a[%d]=" format ", should be " format, i, a[i], ia[i]);\
  for (i=0; i<(sizeof(b)/sizeof(b[0])); i++)\
    fail_if(b[i]!=ib[i], "Should not modify input; b[%d]=" format ", should be " format, i, b[i], ib[i]);\
  fail_if(r<0, "Internal error: %d", -r);\
  fail_if(r!=(sizeof(xc)/sizeof(xc[0])), "Should output %d items, actually returned %d", (sizeof(xc)/sizeof(xc[0])), r);\
  for (i=0; i<(sizeof(xc)/sizeof(xc[0])); i++)\
    fail_if(c[i]!=xc[i], "Incorrect output: c[%d]=" format ", should be " format, i, c[i], xc[i]);\
} while (0)


START_TEST(test_setops_union_abc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
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
  int c[2] = { 0 };
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
  int c[2] = { 0 };
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
  int c[2] = { 0 };
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
  int c[2] = { 0 };
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
  int c[2] = { 0 };
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
  int c[2] = { 0 };
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
  int c[2] = { 0 };
  int xc[2] = { 1, 2 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_two_single_int2)
{
  int a[1] = { 2 };
  int b[1] = { 1 };
  int c[2] = { 0 };
  int xc[2] = { 1, 2 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_two_equal_int)
{
  int a[1] = { 1 };
  int b[1] = { 1 };
  int c[2] = { 0 };
  int xc[1] = { 1 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_many_equal_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[10] = { 0 };
  int xc[7] = { 1, 2, 3, 4, 5, 7, 9 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_large_sets_int)
{
  int a[7] = { 1, 2, 3, 4, 6, 7, 8 };
  int b[7] = { 1, 3, 5, 6, 8, 9, 10 };
  int c[14] = { 0 };
  int xc[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_large_unique_sets_int)
{
  int a[7] = { 1, 3, 5, 7,  9, 11, 13 };
  int b[7] = { 2, 4, 6, 8, 10, 12, 14 };
  int c[14] = { 0 };
  int xc[14] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_all_identical_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 2, 3, 4, 5 };
  int c[10] = { 0 };
  int xc[5] = { 1, 2, 3, 4, 5 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_first_identical_int)
{
  int a[5] = { 1, 2, 4, 6, 8 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[10] = { 0 };
  int xc[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_first2_identical_int)
{
  int a[5] = { 1, 2, 3, 5, 7 };
  int b[5] = { 1, 2, 4, 6, 8 };
  int c[10] = { 0 };
  int xc[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_last_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 9 };
  int b[5] = { 2, 4, 6, 8, 9 };
  int c[10] = { 0 };
  int xc[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  UNION_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_union_last2_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 8 };
  int b[5] = { 2, 4, 6, 7, 8 };
  int c[10] = { 0 };
  int xc[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  UNION_TEST(int, "%d");
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
