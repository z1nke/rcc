// RUN: %check_rcc_run %s %t %S

#include "test.h"

int main() {
  // [73] Add character literal
  ASSERT(97, 'a');
  ASSERT(10, '\n');
  ASSERT(127, '\x7f');

  // [80] Add hexadecimal, octal and binary number literals
  ASSERT(511, 0777);
  ASSERT(0, 0x0);
  ASSERT(10, 0xa);
  ASSERT(10, 0XA);
  ASSERT(48879, 0xbeef);
  ASSERT(48879, 0xBEEF);
  ASSERT(48879, 0XBEEF);
  ASSERT(0, 0b0);
  ASSERT(1, 0b1);
  ASSERT(47, 0b101111);
  ASSERT(47, 0B101111);

  printf("OK\n");
  return 0;
}
