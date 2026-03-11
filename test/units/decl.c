// RUN: %check_rcc_run %s %t %S

#include "test.h"

int main() {
  // [62] Handle complex type declarations correctly
  ASSERT(1, ({ char x; sizeof(x); }));
  ASSERT(2, ({ short int x; sizeof(x); }));
  ASSERT(2, ({ int short x; sizeof(x); }));
  ASSERT(4, ({ int x; sizeof(x); }));
  ASSERT(8, ({ long int x; sizeof(x); }));
  ASSERT(8, ({ int long x; sizeof(x); }));

  // [63] Add long long
  ASSERT(8, ({ long long x; sizeof(x); }));

  printf("OK\n");
  return 0;
}
