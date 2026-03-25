// RUN: %check_rcc_run %s %t %S

#include "test.h"

int main() {
  // [1] Return value
  ASSERT(0, 0);
  ASSERT(42, 42);
  // [2] Support add and sub
  ASSERT(21, 5+20-4);
  // [3] Support whitespace
  ASSERT(41,  12 + 34 - 5 );
  // [5] Support mul, div and paren
  ASSERT(47, 5+6*7);
  ASSERT(15, 5*(9-6));
  ASSERT(4, (3+5)/2);
  // [6] Support unary plus and minus
  ASSERT(10, - -10);
  ASSERT(10, - - +10);

  // [7] Support compare operator
  ASSERT(0, 0==1);
  ASSERT(1, 42==42);
  ASSERT(1, 0!=1);
  ASSERT(0, 42!=42);

  ASSERT(1, 0<1);
  ASSERT(0, 1<1);
  ASSERT(0, 2<1);
  ASSERT(1, 0<=1);
  ASSERT(1, 1<=1);
  ASSERT(0, 2<=1);

  ASSERT(1, 1>0);
  ASSERT(0, 1>1);
  ASSERT(0, 1>2);
  ASSERT(1, 1>=0);
  ASSERT(1, 1>=1);
  ASSERT(0, 1>=2);

  // [9] Accept multiple statements
  ASSERT(3, ({1; 2; 3;}));
  ASSERT(12, ({12+23;12+99/3;78-66;}));

  // [22] Support int keyword
  ASSERT(8, ({int x, y; x=3; y=5; x+y;}));
  ASSERT(8, ({int x=3, y=5; x+y;}));

  // [68] Implement usual arithmetic conversion
  ASSERT(0, 1073741824 * 100 / 100);

  printf("OK\n");
  return 0;
}

// CHECK: OK
