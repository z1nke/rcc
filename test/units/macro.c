// RUN: %check_rcc_pp_run %s %t

#include "Inputs/include1.h" extra tokens
#include "test.h"

void assert(int expected, int actual, char *code, int line);
int strcmp(char *p, char *q);
char *strstr(char *p, char *q);

// [189] Add __FILE__ and __LINE__
char *main_filename1 = __FILE__;
int main_line1 = __LINE__;
#define LINE() __LINE__
int main_line2 = LINE();

int ret3(void) { return 3; }
int dbl(int x) { return x * x; }

// [190] Add __VA_ARGS__
int add2(int x, int y) { return x + y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

int main() {
  // [160] Add #include "..."
  ASSERT(5, include1);
  ASSERT(7, include2);

  // [163] Add #if and #endif
#if 0
#include "/no/such/file"
  ASSERT(0, 1);

  // [164] Skip nested #if in a skipped #if-clause
#if nested
#endif
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
  ASSERT(7, m);

  // [165] Add #else
  int n = 0;
#if 1
  n = 2;
#else
  n = 3;
#endif
  ASSERT(2, n);

#if 0
  n = 4;
#else
#if 1
  n = 5;
#else
  n = 6;
#endif
#endif
  ASSERT(5, n);

  // [166] Add #elif
#if 0
  n = 1;
#elif 0
  n = 2;
#elif 3 + 5
  n = 3;
#elif 1 / 0
  n = 4;
#endif
  ASSERT(3, n);

#if 1
  n = 4;
#elif 1 / 0
  n = 5;
#else
  n = 6;
#endif
  ASSERT(4, n);

  // [167] Add object-like #define
  int M1 = 5;
#define M1 3
  ASSERT(3, M1);
#define M1 4
  ASSERT(4, M1);

#define M1 3 + 4 +
  ASSERT(12, M1 5);

#define M1 3 + 4
  ASSERT(23, M1 * 5);

#define ASSERT_ assert(
#define if 5
#define five "5"
#define END )
  ASSERT_ 5, if, five, 0 END;

  // [168] Add #undef
#undef ASSERT_
#undef if
#undef five
#undef END
#undef M1
  if (0);
  ASSERT(5, M1);

  // [169] Expand macros in #if and #elif arguments
#define M 5
#if M
  m = 5;
#else
  m = 6;
#endif
  ASSERT(5, m);

#if M - 5
  m = 6;
#elif M
  m = 5;
#endif
  ASSERT(5, m);

  // [170] Expand each token only once for the same macro
  int M2 = 6;
#define M2 M2 + 3
  ASSERT(9, M2);

#define M3 M2 + 3
  ASSERT(12, M3);

  int M4 = 3;
#define M4 M5 * 5
#define M5 M4 + 2
  ASSERT(13, M4);

  // [171] Add #ifdef and #ifndef
#ifdef M6
  m = 5;
#else
  m = 3;
#endif
  ASSERT(3, m);

#define M6
#ifdef M6
  m = 5;
#else
  m = 3;
#endif
  ASSERT(5, m);

#ifndef M7
  m = 3;
#else
  m = 5;
#endif
  ASSERT(3, m);

#define M7
#ifndef M7
  m = 3;
#else
  m = 5;
#endif
  ASSERT(5, m);

#if 0
#ifdef NO_SUCH_MACRO
#endif
#ifndef NO_SUCH_MACRO
#endif
#else
#endif

  // [172] Add zero-arity function-like #define
#define M7() 1
  int M7 = 5;
  ASSERT(1, M7());
  ASSERT(5, M7);

#define M7 ()
  ASSERT(3, ret3 M7);

  // [173] Add multi-arity funclike #define
#define M8(x, y) x + y
  ASSERT(7, M8(3, 4));

#define M8(x, y) x *y
  ASSERT(24, M8(3 + 4, 4 + 5));

#define M8(x, y) (x) * (y)
  ASSERT(63, M8(3 + 4, 4 + 5));

  // [174] Allow empty macro arguments
#define M8(x, y) x y
  ASSERT(9, M8(, 4 + 5));

  // [175] Allow parenthesized expressions as macro arguments
#define M8(x, y) x *y
  ASSERT(20, M8((2 + 3), 4));

#define M8(x, y) x *y
  ASSERT(12, M8((2, 3), 4));

  // [176] Do not expand a token more than once for the same function-like macro
#define dbl(x) M10(x) * x
#define M10(x) dbl(x) + 3
  ASSERT(10, dbl(2));

  // [177] Add macro stringizing operator (#)
#define M11(x) #x
  ASSERT('a', M11( a!b  `""c)[0]);
  ASSERT('!', M11( a!b  `""c)[1]);
  ASSERT('b', M11( a!b  `""c)[2]);
  ASSERT(' ', M11( a!b  `""c)[3]);
  ASSERT('`', M11( a!b  `""c)[4]);
  ASSERT('"', M11( a!b  `""c)[5]);
  ASSERT('"', M11( a!b  `""c)[6]);
  ASSERT('c', M11( a!b  `""c)[7]);
  ASSERT(0, M11( a!b  `""c)[8]);

  // [178] Add macro token-pasting operator (##)
#define paste(x, y) x##y
  ASSERT(15, paste(1, 5));
  ASSERT(255, paste(0, xff));
  ASSERT(3, ({ int foobar = 3; paste(foo, bar); }));
  ASSERT(5, paste(5, ));
  ASSERT(5, paste(, 5));

#define i 5
  ASSERT(101, ({ int i3 = 100; paste(1 + i, 3); }));
#undef i

#define paste2(x) x##5
  ASSERT(26, paste2(1 + 2));

#define paste3(x) 2##x
  ASSERT(23, paste3(1 + 2));

#define paste4(x, y, z) x##y##z
  ASSERT(123, paste4(1, 2, 3));

  // [180] Add defined() macro operator
#define M12
#if defined(M12)
  m = 3;
#else
  m = 4;
#endif
  ASSERT(3, m);

#define M12
#if defined M12
  m = 3;
#else
  m = 4;
#endif
  ASSERT(3, m);

#if defined(M12) - 1
  m = 3;
#else
  m = 4;
#endif
  ASSERT(4, m);

#if defined(NO_SUCH_MACRO)
  m = 3;
#else
  m = 4;
#endif
  ASSERT(4, m);

  // [181] Replace remaining identifiers with 0 in macro constexpr
#if no_such_symbol == 0
  m = 5;
#else
  m = 6;
#endif
  ASSERT(5, m);

  // [182] Preserve newline and space during macro expansion
#define STR(x) #x
#define M12(x) STR(x)
#define M13(x) M12(foo.x)
  ASSERT(0, strcmp(M13(bar), "foo.bar"));

#define M13(x) M12(foo. x)
  ASSERT(0, strcmp(M13(bar), "foo. bar"));

#define M12 foo
#define M13(x) STR(x)
#define M14(x) M13(x.M12)
  ASSERT(0, strcmp(M14(bar), "bar.foo"));

#define M14(x) M13(x. M12)
  ASSERT(0, strcmp(M14(bar), "bar. foo"));

  // [184] Add #include <...>
#include "include3.h"
  ASSERT(3, foo);

#include "include4.h"
  ASSERT(4, foo);

#define M13 "include3.h"
#include M13
  ASSERT(3, foo);

#define M13 < include4.h
#include M13 >
  ASSERT(4, foo);

#undef foo

  // [187] Add #error
#if 0
#error "this should be ignored"
#endif

  // [188] Add predefined macros such as __STDC__
  ASSERT(1, __STDC__);
  ASSERT(1, __STDC_HOSTED__);
  ASSERT(1, __rcc__);
  ASSERT(1, __riscv);
  ASSERT(64, __riscv_xlen);
  ASSERT(8, __SIZEOF_LONG__);
  ASSERT(8, __SIZEOF_POINTER__);
  ASSERT(4, __SIZEOF_INT__);
  ASSERT(1, _LP64);
  ASSERT(1, __LP64__);

  // [189] Add __FILE__ and __LINE__
  ASSERT(1, strstr(main_filename1, "macro.c") != 0);
  ASSERT(12, main_line1);
  ASSERT(14, main_line2);
  ASSERT(1, strstr(include1_filename, "include1.h") != 0);
  ASSERT(5, include1_line);

  // [190] Add __VA_ARGS__
#define M14(...) 3
  ASSERT(3, M14());

#define M14(...) __VA_ARGS__
  ASSERT(2, M14() 2);
  ASSERT(5, M14(5));

#define M14(...) add2(__VA_ARGS__)
  ASSERT(8, M14(2, 6));

#define M14(...) add6(1,2,__VA_ARGS__,6)
  ASSERT(21, M14(3,4,5));

#define M14(x, ...) add6(1,2,x,__VA_ARGS__,6)
  ASSERT(21, M14(3,4,5));

#define M14(x, ...) x
  ASSERT(5, M14(5));

  // [207] Tokenize numeric tokens as pp-numbers
#define CONCAT(x,y) x##y
  ASSERT(5, ({ int f0zz=5; CONCAT(f,0zz); }));
  ASSERT(5, ({ CONCAT(4,.57) + 0.5; }));

  // [221] Add __DATE__ and __TIME__ macros
  ASSERT(11, strlen(__DATE__));
  ASSERT(8, strlen(__TIME__));

  // [222] [GNU] Add __COUNTER__ macro
  ASSERT(0, __COUNTER__);
  ASSERT(1, __COUNTER__);
  ASSERT(2, __COUNTER__);

  return 0;
}
