// RUN: %check_rcc_run %s %t %S

#include "test.h"

int ret3() {
  return 3;
  return 5;
}

int ret5() {
  return 5;
}

int add2(int x, int y) {
  return x + y;
}

int sub2(int x, int y) {
  return x - y;
}

int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }

int add6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

int addx(int *x, int y) {
  return *x + y;
}

int fib(int x) {
  if (x<=1)
    return 1;
  return fib(x-1) + fib(x-2);
}

int main1() { return 1; 2; 3; }
int main2() { 1; return 2; 3; }
int main3() { 1; 2; return 3; }

int ret32() { return 32; }

int stmtexpr1() { return ({ 0; }); }
int stmtexpr2() { return ({ 0; 1; 2; }); }
int stmtexpr3() { ({ 0; return 1; 2; }); return 3; }
int stmtexpr4() { return ({ 1; }) + ({ 2; }) + ({ 3; }); }
int stmtexpr5() { return ({ int x=3; x; }); }

int comment1() {
  /* return 1; */
  return 2;
}

int comment2() {
  // return 1;
  return 2;
}

// [57] Add long type
int sub_long(long a, long b, long c) {
  return a - b - c;
}

// [58] Add short type
int sub_short(short a, short b, short c) {
  return a - b - c;
}

int main() {
  // [12] Support return
  ASSERT(1, main1());
  ASSERT(2, main2());
  ASSERT(3, main3());

  // [23] Support zero-arity function calls
  ASSERT(3, ret3());
  ASSERT(5, ret5());
  ASSERT(8, ret3()+ret5());

  // [24] 支持最多6个参数的函数定义
  ASSERT(8, add(3, 5));
  ASSERT(2, sub(5, 3));
  ASSERT(21, add6(1,2,3,4,5,6));
  ASSERT(66, add6(1,2,add6(3,4,5,6,7,8),9,10,11));
  ASSERT(136, add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16));

  // [25] Support zero-arity function definition
  ASSERT(32, ret32());

  // [26] Support function definition up to 6 parameters
  ASSERT(7, add2(3, 4));
  ASSERT(1, sub(4,3));
  ASSERT(55, fib(9));

  // [39] Add GNU statement expression
  ASSERT(0, stmtexpr1());
  ASSERT(2, stmtexpr2());
  ASSERT(1, stmtexpr3());
  ASSERT(6, stmtexpr4());
  ASSERT(3, stmtexpr5());

  // [43] Add line and block comments
  ASSERT(2, comment1());
  ASSERT(2, comment2());

  printf("OK\n");
  return 0;
}

// CHECK: OK
