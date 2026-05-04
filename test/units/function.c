// RUN: %check_rcc_run %s %t %S

#include "test.h"

int ret3(void) { // [114] Accept `void` as a parameter list
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

// [70] Handle return type conversion
int g1;

int *g1_ptr(void) { return &g1; } // [114] Accept `void` as a parameter list
char int_to_char(int x) { return x; }

// {71] Handle function argument type conversion
int div_long(long a, long b) {
  return a / b;
}

// [72] Add _Bool type
_Bool bool_fn_add(_Bool x) { return x + 1; }
_Bool bool_fn_sub(_Bool x) { return x - 1; }

// [75] Support file-scope functions
// [114] Accept `void` as a parameter list
static int static_fn(void) { return 3; }

// [87] Decay an array to a pointer in the func param context
int param_decay(int x[]) { return x[0]; }

// [120] Add static local variables
int counter() {
  static int i;
  static int j = 1+1;
  return i++ + j++;
}

// [122] Add return that doesn't take any value
void ret_none() { return; }

// [126] Handle a function returning bool, char or short
_Bool true_fn();
_Bool false_fn();
char char_fn();
short short_fn();

// [127] Allow to call a variadic function
int add_all(int n, ...);

// [128] Add va_start to support variadic functions
typedef void *va_list;

int sprintf(char *buf, char *fmt, ...);
int vsprintf(char *buf, char *fmt, va_list ap);

char *fmt(char *buf, char *fmt, ...) {
  va_list ap = __va_area__;
  vsprintf(buf, fmt, ap);
}

// [129] Check the number of function arguments
int nullParam() { return 123; }

// [131] Add unsigned integral types
unsigned char uchar_fn();
unsigned short ushort_fn();

signed char schar_fn();
short sshort_fn();

// [138] Allow to omit parameter name in function declaration
int add2_omit(int, int);
int ptr_omit(int *);
int arr_omit(int [3]);

// [144] Allow to call a function that takes/returns floating-point number
double add_double(double x, double y);
float add_float(float x, float y);

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

  // [70] Handle return type conversion
  g1 = 3;
  ASSERT(3, *g1_ptr());
  ASSERT(5, int_to_char(261));

  // [71] Handle function argument type conversion
  ASSERT(-5, div_long(-10, 2));

  // [72] Add _Bool type
  ASSERT(1, bool_fn_add(3));
  ASSERT(0, bool_fn_sub(3));
  ASSERT(1, bool_fn_add(-3));
  ASSERT(0, bool_fn_sub(-3));
  ASSERT(1, bool_fn_add(0));
  ASSERT(1, bool_fn_sub(0));

  // [75] Support file-scope functions
  ASSERT(3, static_fn());

  // [87] Decay an array to a pointer in the func param context
  ASSERT(3, ({ int x[2]; x[0]=3; param_decay(x); }));

  // [120] Add static local variables
  ASSERT(2, counter());
  ASSERT(4, counter());
  ASSERT(6, counter());

  // [122] Add return that doesn't take any value
  ret_none();

  // [126] Handle a function returning bool, char or short
  ASSERT(1, true_fn());
  ASSERT(0, false_fn());
  ASSERT(3, char_fn());
  ASSERT(5, short_fn());

  // [127] Allow to call a variadic function
  ASSERT(6, add_all(3,1,2,3));
  ASSERT(5, add_all(4,1,2,3,-1));

  // [128] Add va_start to support variadic functions
  { char buf[100]; fmt(buf, "%d %d %s", 1, 2, "foo"); printf("%s\n", buf); }

  ASSERT(0, ({ char buf[100]; sprintf(buf, "%d %d %s", 1, 2, "foo"); strcmp("1 2 foo", buf); }));

  ASSERT(0, ({ char buf[100]; fmt(buf, "%d %d %s", 1, 2, "foo"); strcmp("1 2 foo", buf); }));

  // [129] Check the number of function arguments
  ASSERT(123, ({ nullParam(); }));

  // [131] Add unsigned integral types
  ASSERT(251, uchar_fn());
  ASSERT(65528, ushort_fn());
  ASSERT(-5, schar_fn());
  ASSERT(-8, sshort_fn());

  // [144] Allow to call a function that takes/returns floating-point number
  ASSERT(6, add_float(2.3, 3.8));
  ASSERT(6, add_double(2.3, 3.8));

  printf("OK\n");
  return 0;
}

// CHECK: OK
