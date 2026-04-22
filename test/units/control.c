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

  
  // [89] Add goto and labeled statement
  ASSERT(3, ({ int i=0; goto a; a: i++; b: i++; c: i++; i; }));
  ASSERT(2, ({ int i=0; goto e; d: i++; e: i++; f: i++; i; }));
  ASSERT(1, ({ int i=0; goto i; g: i++; h: i++; i: i++; i; }));

  // [90] Resolve conflict between labels and typedefs
  ASSERT(1, ({ typedef int foo; goto foo; foo:; 1; }));

  // [91] Add break statement
  ASSERT(3, ({ int i=0; for(;i<10;i++) { if (i == 3) break; } i; }));
  ASSERT(4, ({ int i=0; while (1) { if (i++ == 3) break; } i; }));
  ASSERT(3, ({ int i=0; for(;i<10;i++) { for (;;) break; if (i == 3) break; } i; }));
  ASSERT(4, ({ int i=0; while (1) { while(1) break; if (i++ == 3) break; } i; }));

  // [92] Add continue statement
  ASSERT(10, ({ int i=0; int j=0; for (;i<10;i++) { if (i>5) continue; j++; } i; }));
  ASSERT(6, ({ int i=0; int j=0; for (;i<10;i++) { if (i>5) continue; j++; } j; }));
  ASSERT(10, ({ int i=0; int j=0; for(;!i;) { for (;j!=10;j++) continue; break; } j; }));
  ASSERT(11, ({ int i=0; int j=0; while (i++<10) { if (i>5) continue; j++; } i; }));
  ASSERT(5, ({ int i=0; int j=0; while (i++<10) { if (i>5) continue; j++; } j; }));
  ASSERT(11, ({ int i=0; int j=0; while(!i) { while (j++!=10) continue; break; } j; }));

  printf("OK\n");
  return 0;
}

// CHECK: OK