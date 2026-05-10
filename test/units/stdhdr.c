// RUN: %check_rcc_run %s %t %S

#include "test.h"
#include <float.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdnoreturn.h>

int main() {
  // [195] Add stdarg.h, stdbool.h, stddef.h, stdalign.h and float.h
  printf("OK\n");
  return 0;
}

// CHECK: OK
