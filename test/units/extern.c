// RUN: %check_rcc_run %s %t %S

#include "test.h"

// [116] Add extern
extern int ext1;
extern int *ext2;

int main() {
  // [116] Add extern
  ASSERT(5, ext1);
  ASSERT(5, *ext2);

  printf("OK\n");
  return 0;
}
