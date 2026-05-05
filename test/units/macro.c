// RUN: %check_rcc_pp_run %s %t

#include "Inputs/include1.h" extra tokens

void assert(int expected, int actual, char *code, int line);

int main() {
  // [160] Add #include "..."
  assert(5, include1, "include1", 8);
  assert(7, include2, "include2", 9);
  return 0;
}
