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
    if (i>0 && i!=count) printf(", ");\
    if (i==count) printf("}\033[1;31m{");\
    printf(format, set[i]);\
  }\
  printf("}");\
  if (count<alloc_count) printf("\033[m");\
}

void print_set_char(const char *set, size_t count, size_t alloc_count) {
  PRINT_SET(set, count, alloc_count, "%c");
}

void print_set_int(const int *set, size_t count, size_t alloc_count) {
  PRINT_SET(set, count, alloc_count, "%d");
}


/* ************************************************************************ */


#define array_size(arr) (sizeof(arr)/sizeof(arr[0]))

#define TEST_abc_sz(test, type, sa, sb, sc) set_##test (a, b, c, sa, sb, sc, sizeof(type), p##type##cmp)

#define UNION_TEST_abc_sz(type, sa, sb, sc) TEST_abc_sz(union, type, sa, sb, sc)
#define UNION_TEST_abc(type) UNION_TEST_abc_sz(type, array_size(a), array_size(b), array_size(c))

#define INTERSECT_TEST_abc_sz(type, sa, sb, sc) TEST_abc_sz(intersect, type, sa, sb, sc)
#define INTERSECT_TEST_abc(type) INTERSECT_TEST_abc_sz(type, array_size(a), array_size(b), array_size(c))

#define DIFF_TEST_abc_sz(type, sa, sb, sc) TEST_abc_sz(diff, type, sa, sb, sc)
#define DIFF_TEST_abc(type) DIFF_TEST_abc_sz(type, array_size(a), array_size(b), array_size(c))

#define UNIQ_TEST_abc_sz(type, sa, sb, sc) TEST_abc_sz(uniq, type, sa, sb, sc)
#define UNIQ_TEST_abc(type) UNIQ_TEST_abc_sz(type, array_size(a), array_size(b), array_size(c))

#define PRE_TEST(type) do {\
    type ia[array_size(a)];\
    type ib[array_size(b)];\
    size_t i;\
    do {\
      for (i=0; i<array_size(a); i++) ia[i]=a[i];\
      for (i=0; i<array_size(b); i++) ib[i]=b[i];\
  } while (0)

#define PRE_NOOP_TEST(type) do {\
    type ia[array_size(a)];\
    type ib[array_size(b)];\
    type ic[array_size(c)];\
    size_t i;\
    do {\
      for (i=0; i<array_size(a); i++) ia[i]=a[i];\
      for (i=0; i<array_size(b); i++) ib[i]=b[i];\
      for (i=0; i<array_size(c); i++) ic[i]=c[i];\
  } while (0)

#define POST_TEST(format) do {\
    for (i=0; i<array_size(a); i++)\
      fail_if(a[i]!=ia[i], "Should not modify input; a[%d]=" format ", should be " format, i, a[i], ia[i]);\
    for (i=0; i<array_size(b); i++)\
      fail_if(b[i]!=ib[i], "Should not modify input; b[%d]=" format ", should be " format, i, b[i], ib[i]);\
    fail_if(r<0, "Internal error: %d", -r);\
    fail_if(r!=array_size(xc), "Should output %d items, actually returned %d", array_size(xc), r);\
    for (i=0; i<array_size(xc); i++)\
      fail_if(c[i]!=xc[i], "Incorrect output: c[%d]=" format ", should be " format, i, c[i], xc[i]);\
  } while (0);\
} while (0)

#define POST_NOOP_TEST(format) do {\
    for (i=0; i<array_size(a); i++)\
      fail_if(a[i]!=ia[i], "Should not modify input; a[%d]=" format ", should be " format, i, a[i], ia[i]);\
    for (i=0; i<array_size(b); i++)\
      fail_if(b[i]!=ib[i], "Should not modify input; b[%d]=" format ", should be " format, i, b[i], ib[i]);\
    fail_if(r<0, "Internal error: %d", -r);\
    fail_if(r, "Should output 0 items, actually returned %d", r);\
    for (i=0; i<array_size(c); i++)\
      fail_if(c[i]!=ic[i], "Should not modify output: c[%d]=" format ", should be " format, i, c[i], ic[i]);\
  } while (0);\
} while (0)

#define POST_ERROR_TEST(format, err_code) do {\
    for (i=0; i<array_size(a); i++)\
      fail_if(a[i]!=ia[i], "Should not modify input; a[%d]=" format ", should be " format, i, a[i], ia[i]);\
    for (i=0; i<array_size(b); i++)\
      fail_if(b[i]!=ib[i], "Should not modify input; b[%d]=" format ", should be " format, i, b[i], ib[i]);\
    for (i=0; i<array_size(c); i++)\
      fail_if(c[i]!=ic[i], "Should not modify output: c[%d]=" format ", should be " format, i, c[i], ic[i]);\
    fail_if(r>=0, "Returned %d when expected error code %d", r, err_code);\
    fail_if(-r!=err_code, "Should return error code %d, actually returned error %d", err_code, -r);\
  } while (0);\
} while (0)



#define UNION_TEST(type, format) do {\
  PRE_TEST(type);\
  int r = UNION_TEST_abc(type);\
  POST_TEST(format);\
} while (0)

#define UNION_ERROR_TEST(type, format, err_code) do {\
  PRE_NOOP_TEST(type);\
  int r = UNION_TEST_abc(type);\
  POST_ERROR_TEST(format, err_code);\
} while (0)



#define INTERSECT_TEST(type, format) do {\
  PRE_TEST(type);\
  int r = INTERSECT_TEST_abc(type);\
  POST_TEST(format);\
} while (0)

#define INTERSECT_NOOP_TEST(type, format) do {\
  PRE_NOOP_TEST(type);\
  int r = INTERSECT_TEST_abc(type);\
  POST_NOOP_TEST(format);\
} while (0)

#define INTERSECT_ERROR_TEST(type, format, err_code) do {\
  PRE_NOOP_TEST(type);\
  int r = INTERSECT_TEST_abc(type);\
  POST_ERROR_TEST(format, err_code);\
} while (0)



#define DIFF_TEST(type, format) do {\
  PRE_TEST(type);\
  int r = DIFF_TEST_abc(type);\
  POST_TEST(format);\
} while (0)

#define DIFF_NOOP_TEST(type, format) do {\
  PRE_NOOP_TEST(type);\
  int r = DIFF_TEST_abc(type);\
  POST_NOOP_TEST(format);\
} while (0)

#define DIFF_ERROR_TEST(type, format, err_code) do {\
  PRE_NOOP_TEST(type);\
  int r = DIFF_TEST_abc(type);\
  POST_ERROR_TEST(format, err_code);\
} while (0)



#define UNIQ_TEST(type, format) do {\
  PRE_TEST(type);\
  int r = UNIQ_TEST_abc(type);\
  POST_TEST(format);\
} while (0)


/* ************************************************************************ */



START_TEST(test_setops_union_abc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = UNION_TEST_abc_sz(int, 0, 0, 0);
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_union_ab_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = UNION_TEST_abc_sz(int, 0, 0, array_size(c));
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_union_ac_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = UNION_TEST_abc_sz(int, 0, array_size(b), 0);
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_union_bc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = UNION_TEST_abc_sz(int, array_size(a), 0, 0);
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_union_a_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  int r = UNION_TEST_abc_sz(int, 0, array_size(b), array_size(c));
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
  int r = UNION_TEST_abc_sz(int, array_size(a), 0, array_size(c));
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
  int r = UNION_TEST_abc_sz(int, array_size(a), array_size(b), 0);
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


/* ************************************************************************ */


START_TEST(test_setops_intersect_abc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = INTERSECT_TEST_abc_sz(int, 0, 0, 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_intersect_ab_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = INTERSECT_TEST_abc_sz(int, 0, 0, array_size(c));
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_intersect_ac_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = INTERSECT_TEST_abc_sz(int, 0, array_size(b), 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_intersect_bc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = INTERSECT_TEST_abc_sz(int, array_size(a), 0, 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_intersect_a_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = INTERSECT_TEST_abc_sz(int, 0, array_size(b), array_size(c));
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_intersect_b_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = INTERSECT_TEST_abc_sz(int, array_size(a), 0, array_size(c));
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_intersect_c_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = INTERSECT_TEST_abc_sz(int, array_size(a), array_size(b), 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_intersect_two_single_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  INTERSECT_NOOP_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_two_single_int2)
{
  int a[1] = { 2 };
  int b[1] = { 1 };
  int c[2] = { 0 };
  INTERSECT_NOOP_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_two_equal_int)
{
  int a[1] = { 1 };
  int b[1] = { 1 };
  int c[2] = { 0 };
  int xc[1] = { 1 };
  INTERSECT_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_many_equal_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[5] = { 0 };
  int xc[3] = { 1, 3, 5 };
  INTERSECT_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_large_sets_int)
{
  int a[7] = { 1, 2, 3, 4, 6, 7, 8 };
  int b[7] = { 1, 3, 5, 6, 8, 9, 10 };
  int c[7] = { 0 };
  int xc[4] = { 1, 3, 6, 8 };
  INTERSECT_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_large_unique_sets_int)
{
  int a[7] = { 1, 3, 5, 7,  9, 11, 13 };
  int b[7] = { 2, 4, 6, 8, 10, 12, 14 };
  int c[7] = { 0 };
  INTERSECT_NOOP_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_all_identical_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 2, 3, 4, 5 };
  int c[5] = { 0 };
  int xc[5] = { 1, 2, 3, 4, 5 };
  INTERSECT_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_first_identical_int)
{
  int a[5] = { 1, 2, 4, 6, 8 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[5] = { 0 };
  int xc[1] = { 1 };
  INTERSECT_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_first2_identical_int)
{
  int a[5] = { 1, 2, 3, 5, 7 };
  int b[5] = { 1, 2, 4, 6, 8 };
  int c[5] = { 0 };
  int xc[2] = { 1, 2 };
  INTERSECT_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_last_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 9 };
  int b[5] = { 2, 4, 6, 8, 9 };
  int c[5] = { 0 };
  int xc[1] = { 9 };
  INTERSECT_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_intersect_last2_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 8 };
  int b[5] = { 2, 4, 6, 7, 8 };
  int c[5] = { 0 };
  int xc[2] = { 7, 8 };
  INTERSECT_TEST(int, "%d");
}
END_TEST


Suite *setops_intersect_suite (void) {
  Suite *s = suite_create("set_ops intersect");

  TCase *tc_empty = tcase_create("Empty sets");
  tcase_add_test(tc_empty, test_setops_intersect_abc_empty_int);
  tcase_add_test(tc_empty, test_setops_intersect_ab_empty_int);
  tcase_add_test(tc_empty, test_setops_intersect_ac_empty_int);
  tcase_add_test(tc_empty, test_setops_intersect_bc_empty_int);
  tcase_add_test(tc_empty, test_setops_intersect_a_empty_int);
  tcase_add_test(tc_empty, test_setops_intersect_b_empty_int);
  tcase_add_test(tc_empty, test_setops_intersect_c_empty_int);
  suite_add_tcase(s, tc_empty);

  TCase *tc_simple = tcase_create("Simple tests");
  tcase_add_test(tc_simple, test_setops_intersect_two_single_int);
  tcase_add_test(tc_simple, test_setops_intersect_two_single_int2);
  tcase_add_test(tc_simple, test_setops_intersect_two_equal_int);
  tcase_add_test(tc_simple, test_setops_intersect_many_equal_int);
  tcase_add_test(tc_simple, test_setops_intersect_large_sets_int);
  tcase_add_test(tc_simple, test_setops_intersect_large_unique_sets_int);
  suite_add_tcase(s, tc_simple);

  TCase *tc_edge = tcase_create("Edge cases");
  tcase_add_test(tc_edge, test_setops_intersect_all_identical_int);
  tcase_add_test(tc_edge, test_setops_intersect_first_identical_int);
  tcase_add_test(tc_edge, test_setops_intersect_first2_identical_int);
  tcase_add_test(tc_edge, test_setops_intersect_last_identical_int);
  tcase_add_test(tc_edge, test_setops_intersect_last2_identical_int);
  suite_add_tcase(s, tc_edge);

  return s;
}


/* ************************************************************************ */


/*
START_TEST(test_setops_diff_abc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = DIFF_TEST_abc_sz(int, 0, 0, 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_diff_ab_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = DIFF_TEST_abc_sz(int, 0, 0, array_size(c));
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_diff_ac_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = DIFF_TEST_abc_sz(int, 0, array_size(b), 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_diff_bc_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = DIFF_TEST_abc_sz(int, array_size(a), 0, 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_diff_a_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = DIFF_TEST_abc_sz(int, 0, array_size(b), array_size(c));
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_diff_b_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = DIFF_TEST_abc_sz(int, array_size(a), 0, array_size(c));
  POST_NOOP_TEST("%d");
}
END_TEST

START_TEST(test_setops_diff_c_empty_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  PRE_NOOP_TEST(int);
  int r = DIFF_TEST_abc_sz(int, array_size(a), array_size(b), 0);
  POST_ERROR_TEST("%d", EINVAL);
}
END_TEST

START_TEST(test_setops_diff_two_single_int)
{
  int a[1] = { 1 };
  int b[1] = { 2 };
  int c[2] = { 0 };
  DIFF_NOOP_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_two_single_int2)
{
  int a[1] = { 2 };
  int b[1] = { 1 };
  int c[2] = { 0 };
  DIFF_NOOP_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_two_equal_int)
{
  int a[1] = { 1 };
  int b[1] = { 1 };
  int c[2] = { 0 };
  int xc[1] = { 1 };
  DIFF_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_many_equal_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[5] = { 0 };
  int xc[3] = { 1, 3, 5 };
  DIFF_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_large_sets_int)
{
  int a[7] = { 1, 2, 3, 4, 6, 7, 8 };
  int b[7] = { 1, 3, 5, 6, 8, 9, 10 };
  int c[7] = { 0 };
  int xc[4] = { 1, 3, 6, 8 };
  DIFF_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_large_unique_sets_int)
{
  int a[7] = { 1, 3, 5, 7,  9, 11, 13 };
  int b[7] = { 2, 4, 6, 8, 10, 12, 14 };
  int c[7] = { 0 };
  DIFF_NOOP_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_all_identical_int)
{
  int a[5] = { 1, 2, 3, 4, 5 };
  int b[5] = { 1, 2, 3, 4, 5 };
  int c[5] = { 0 };
  int xc[5] = { 1, 2, 3, 4, 5 };
  DIFF_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_first_identical_int)
{
  int a[5] = { 1, 2, 4, 6, 8 };
  int b[5] = { 1, 3, 5, 7, 9 };
  int c[5] = { 0 };
  int xc[1] = { 1 };
  DIFF_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_first2_identical_int)
{
  int a[5] = { 1, 2, 3, 5, 7 };
  int b[5] = { 1, 2, 4, 6, 8 };
  int c[5] = { 0 };
  int xc[2] = { 1, 2 };
  DIFF_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_last_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 9 };
  int b[5] = { 2, 4, 6, 8, 9 };
  int c[5] = { 0 };
  int xc[1] = { 9 };
  DIFF_TEST(int, "%d");
}
END_TEST

START_TEST(test_setops_diff_last2_identical_int)
{
  int a[5] = { 1, 3, 5, 7, 8 };
  int b[5] = { 2, 4, 6, 7, 8 };
  int c[5] = { 0 };
  int xc[2] = { 7, 8 };
  DIFF_TEST(int, "%d");
}
END_TEST
*/


Suite *setops_diff_suite (void) {
  Suite *s = suite_create("set_ops diff");

  /*
  TCase *tc_empty = tcase_create("Empty sets");
  tcase_add_test(tc_empty, test_setops_diff_abc_empty_int);
  tcase_add_test(tc_empty, test_setops_diff_ab_empty_int);
  tcase_add_test(tc_empty, test_setops_diff_ac_empty_int);
  tcase_add_test(tc_empty, test_setops_diff_bc_empty_int);
  tcase_add_test(tc_empty, test_setops_diff_a_empty_int);
  tcase_add_test(tc_empty, test_setops_diff_b_empty_int);
  tcase_add_test(tc_empty, test_setops_diff_c_empty_int);
  suite_add_tcase(s, tc_empty);

  TCase *tc_simple = tcase_create("Simple tests");
  tcase_add_test(tc_simple, test_setops_diff_two_single_int);
  tcase_add_test(tc_simple, test_setops_diff_two_single_int2);
  tcase_add_test(tc_simple, test_setops_diff_two_equal_int);
  tcase_add_test(tc_simple, test_setops_diff_many_equal_int);
  tcase_add_test(tc_simple, test_setops_diff_large_sets_int);
  tcase_add_test(tc_simple, test_setops_diff_large_unique_sets_int);
  suite_add_tcase(s, tc_simple);

  TCase *tc_edge = tcase_create("Edge cases");
  tcase_add_test(tc_edge, test_setops_diff_all_identical_int);
  tcase_add_test(tc_edge, test_setops_diff_first_identical_int);
  tcase_add_test(tc_edge, test_setops_diff_first2_identical_int);
  tcase_add_test(tc_edge, test_setops_diff_last_identical_int);
  tcase_add_test(tc_edge, test_setops_diff_last2_identical_int);
  suite_add_tcase(s, tc_edge);
  */

  return s;
}


/* ************************************************************************ */


Suite *setops_uniq_suite (void) {
  Suite *s = suite_create("set_ops uniq");

  /*
  TCase *tc_empty = tcase_create("Empty sets");
  tcase_add_test(tc_empty, test_setops_uniq_abc_empty_int);
  tcase_add_test(tc_empty, test_setops_uniq_ab_empty_int);
  tcase_add_test(tc_empty, test_setops_uniq_ac_empty_int);
  tcase_add_test(tc_empty, test_setops_uniq_bc_empty_int);
  tcase_add_test(tc_empty, test_setops_uniq_a_empty_int);
  tcase_add_test(tc_empty, test_setops_uniq_b_empty_int);
  tcase_add_test(tc_empty, test_setops_uniq_c_empty_int);
  suite_add_tcase(s, tc_empty);

  TCase *tc_simple = tcase_create("Simple tests");
  tcase_add_test(tc_simple, test_setops_uniq_two_single_int);
  tcase_add_test(tc_simple, test_setops_uniq_two_single_int2);
  tcase_add_test(tc_simple, test_setops_uniq_two_equal_int);
  tcase_add_test(tc_simple, test_setops_uniq_many_equal_int);
  tcase_add_test(tc_simple, test_setops_uniq_large_sets_int);
  tcase_add_test(tc_simple, test_setops_uniq_large_unique_sets_int);
  suite_add_tcase(s, tc_simple);

  TCase *tc_edge = tcase_create("Edge cases");
  tcase_add_test(tc_edge, test_setops_uniq_all_identical_int);
  tcase_add_test(tc_edge, test_setops_uniq_first_identical_int);
  tcase_add_test(tc_edge, test_setops_uniq_first2_identical_int);
  tcase_add_test(tc_edge, test_setops_uniq_last_identical_int);
  tcase_add_test(tc_edge, test_setops_uniq_last2_identical_int);
  suite_add_tcase(s, tc_edge);
  */

  return s;
}


/* ************************************************************************ */


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
