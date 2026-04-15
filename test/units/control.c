// RUN: %check_rcc_run %s %t %S

#include "test.h"

/*
 * This is a block comment.
 */

int main() {
  // [15] Support if/else statement
  ASSERT(3, ({ int x; if (0) x=2; else x=3; x; }));
  ASSERT(3, ({ int x; if (1-1) x=2; else x=3; x; }));
  ASSERT(2, ({ int x; if (1) x=2; else x=3; x; }));
  ASSERT(2, ({ int x; if (2-1) x=2; else x=3; x; }));

  // [16] Support for statement
  ASSERT(55, ({ int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; j; }));

  // [17] Support while statement
  ASSERT(10, ({ int i=0; while(i<10) i=i+1; i; }));
  ASSERT(10, ({ int i=0; while(i<10) i=i+1; i; }));
  ASSERT(55, ({ int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} j; }));

  // [13] Support {...}
  ASSERT(3, ({ 1; {2;} 3; }));

  // [14] Support null statement
  ASSERT(5, ({ ;;; 5; }));

  // [48] Add comma operator
  ASSERT(3, (1,2,3));
  ASSERT(5, ({ int i=2, j=3; (i=5,j)=6; i; }));
  ASSERT(6, ({ int i=2, j=3; (i=5,j)=6; j; }));

  // [76] Allow for-loops to define local variables
  ASSERT(55, ({ int j=0; for (int i=0; i<=10; i=i+1) j=j+i; j; }));
  ASSERT(3, ({ int i=3; int j=0; for (int i=0; i<=10; i=i+1) j=j+i; i; }));

  // [85] Add && and || operator
  ASSERT(1, 0||1);
  ASSERT(1, 0||(2-2)||5);
  ASSERT(0, 0||0);
  ASSERT(0, 0||(2-2));

  ASSERT(0, 0&&1);
  ASSERT(0, (2-2)&&5);
  ASSERT(1, 1&&5);
  ASSERT(0, ({ int x=0; 0 && (x=1); x; }));
  ASSERT(0, ({ int x=0; (2-2) && (x=1); x; }));
  ASSERT(1, ({ int x=0; 1 && (x=1); x; }));

  ASSERT(0, ({ int x=0; 0 || (x=0); x; }));
  ASSERT(0, ({ int x=0; 1 || (x=1); x; }));
  ASSERT(1, ({ int x=0; 0 || (x=1); x; }));

  printf("OK\n");
  return 0;
}

// CHECK: OK