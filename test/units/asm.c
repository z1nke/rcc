// RUN: %check_rcc_run %s %t %S

#include "test.h"

// [259] Add basic "asm" statement
char *asm_fn1(void) { asm("li a0, 50"); }

char *asm_fn2(void) { asm inline volatile("li a0, 55"); }

int main() {
  // [259] Add basic "asm" statement
  ASSERT(52, asm_fn1() + 2);
  ASSERT(55, asm_fn2());

  printf("OK\n");
  return 0;
}
