// RUN: %check_rcc_run %s %t %S

#include "test.h"
#include <stddef.h>

// [264] Add offsetof
typedef struct {
  int a;
  char b;
  int c;
  double d;
} T;

int main() {
  // [264] Add offsetof
  ASSERT(0, offsetof(T, a));
  ASSERT(4, offsetof(T, b));
  ASSERT(8, offsetof(T, c));
  ASSERT(16, offsetof(T, d));

  printf("OK\n");
  return 0;
}
