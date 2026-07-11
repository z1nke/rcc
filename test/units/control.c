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

  // [93] Add switch-case
  ASSERT(5, ({ int i=0; switch(0) { case 0:i=5;break; case 1:i=6;break; case 2:i=7;break; } i; }));
  ASSERT(6, ({ int i=0; switch(1) { case 0:i=5;break; case 1:i=6;break; case 2:i=7;break; } i; }));
  ASSERT(7, ({ int i=0; switch(2) { case 0:i=5;break; case 1:i=6;break; case 2:i=7;break; } i; }));
  ASSERT(0, ({ int i=0; switch(3) { case 0:i=5;break; case 1:i=6;break; case 2:i=7;break; } i; }));
  ASSERT(5, ({ int i=0; switch(0) { case 0:i=5;break; default:i=7; } i; }));
  ASSERT(7, ({ int i=0; switch(1) { case 0:i=5;break; default:i=7; } i; }));
  ASSERT(2, ({ int i=0; switch(1) { case 0: 0; case 1: 0; case 2: 0; i=2; } i; }));
  ASSERT(0, ({ int i=0; switch(3) { case 0: 0; case 1: 0; case 2: 0; i=2; } i; }));
  ASSERT(3, ({ int i=0; switch(-1) { case 0xffffffff: i=3; break; } i; }));

  // [124] Add do ... while
  ASSERT(7, ({ int i=0; int j=0; do { j++; } while (i++ < 6); j; }));
  ASSERT(4, ({ int i=0; int j=0; int k=0; do { if (++j > 3) break; continue; k++; } while (1); j; }));


  // [143] Handle floating-point number for if, while, do, !, ?:, || and &&
  ASSERT(0, 0.0 && 0.0);
  ASSERT(0, 0.0 && 0.1);
  ASSERT(0, 0.3 && 0.0);
  ASSERT(1, 0.3 && 0.5);
  ASSERT(0, 0.0 || 0.0);
  ASSERT(1, 0.0 || 0.1);
  ASSERT(1, 0.3 || 0.0);
  ASSERT(1, 0.3 || 0.5);
  ASSERT(5, ({ int x; if (0.0) x=3; else x=5; x; }));
  ASSERT(3, ({ int x; if (0.1) x=3; else x=5; x; }));
  ASSERT(5, ({ int x=5; if (0.0) x=3; x; }));
  ASSERT(3, ({ int x=5; if (0.1) x=3; x; }));
  ASSERT(10, ({ double i=10.0; int j=0; for (; i; i--, j++); j; }));
  ASSERT(10, ({ double i=10.0; int j=0; do j++; while(--i); j; }));

  // [GNU] Support case ranges
  ASSERT(2, ({ int i=0; switch(7) { case 0 ... 5: i=1; break; case 6 ... 20: i=2; break; } i; }));
  ASSERT(1, ({ int i=0; switch(7) { case 0 ... 7: i=1; break; case 8 ... 10: i=2; break; } i; }));
  ASSERT(1, ({ int i=0; switch(7) { case 0: i=1; break; case 7 ... 7: i=1; break; } i; }));

  printf("OK\n");
  return 0;
}

// CHECK: OK