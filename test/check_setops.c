#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <check.h>

#include <set_ops.h>

Suite *setops_union_suite (void) {
  Suite *s = suite_create("set_ops union");

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
