// RUN: %check_rcc_pp_run %s %t

#include "Inputs/include1.h" extra tokens

void assert(int expected, int actual, char *code, int line);

int main() {
  // [160] Add #include "..."
  assert(5, include1, "include1", 9);
  assert(7, include2, "include2", 10);

  // [163] Add #if and #endif
#if 0
#include "/no/such/file"
  assert(0, 1, "1", 15);
#endif

  int m = 0;
#if 1
  m = 5;
#endif
#if 1 || (1 / 0)
  m = m + 2;
#endif
#if UNDEFINED_IDENTIFIER
  m = 0;
#endif
  assert(7, m, "m", 28);
  return 0;
}
