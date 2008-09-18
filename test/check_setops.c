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
  suite_add_tcase(s, tc_simple);

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
