// RUN: %check_rcc_run %s %t %S

#include "test.h"

int main() {
  // [73] Add character literal
  ASSERT(97, 'a');
  ASSERT(10, '\n');
  ASSERT(127, '\x7f');

  printf("OK\n");
  return 0;
}
