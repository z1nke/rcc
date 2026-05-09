// RUN: %check_rcc_pp_run %s %t

#include "Inputs/include1.h" extra tokens

void assert(int expected, int actual, char *code, int line);
int strcmp(char *p, char *q);

int ret3(void) { return 3; }
int dbl(int x) { return x * x; }

int main() {
  // [160] Add #include "..."
  assert(5, include1, "include1", 9);
  assert(7, include2, "include2", 10);

  // [163] Add #if and #endif
#if 0
#include "/no/such/file"
  assert(0, 1, "1", 15);

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
  assert(7, m, "m", 32);

  // [165] Add #else
  int n = 0;
#if 1
  n = 2;
#else
  n = 3;
#endif
  assert(2, n, "n", 41);

#if 0
  n = 4;
#else
#if 1
  n = 5;
#else
  n = 6;
#endif
#endif
  assert(5, n, "n", 52);

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
  assert(3, n, "n", 64);

#if 1
  n = 4;
#elif 1 / 0
  n = 5;
#else
  n = 6;
#endif
  assert(4, n, "n", 73);

  // [167] Add object-like #define
  int M1 = 5;
#define M1 3
  assert(3, M1, "M1", 0);
#define M1 4
  assert(4, M1, "M1", 0);

#define M1 3 + 4 +
  assert(12, M1 5, "M1 5", 0);

#define M1 3 + 4
  assert(23, M1 * 5, "M1 * 5", 0);

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
  assert(5, M1, "M1", 0);

  // [169] Expand macros in #if and #elif arguments
#define M 5
#if M
  m = 5;
#else
  m = 6;
#endif
  assert(5, m, "m", 0);

#if M - 5
  m = 6;
#elif M
  m = 5;
#endif
  assert(5, m, "m", 0);

  // [170] Expand each token only once for the same macro
  int M2 = 6;
#define M2 M2 + 3
  assert(9, M2, "M2", 0);

#define M3 M2 + 3
  assert(12, M3, "M3", 0);

  int M4 = 3;
#define M4 M5 * 5
#define M5 M4 + 2
  assert(13, M4, "M4", 0);

  // [171] Add #ifdef and #ifndef
#ifdef M6
  m = 5;
#else
  m = 3;
#endif
  assert(3, m, "m", 0);

#define M6
#ifdef M6
  m = 5;
#else
  m = 3;
#endif
  assert(5, m, "m", 0);

#ifndef M7
  m = 3;
#else
  m = 5;
#endif
  assert(3, m, "m", 0);

#define M7
#ifndef M7
  m = 3;
#else
  m = 5;
#endif
  assert(5, m, "m", 0);

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
  assert(1, M7(), "M7()", 0);
  assert(5, M7, "M7", 0);

#define M7 ()
  assert(3, ret3 M7, "ret3 M7", 0);

  // [173] Add multi-arity funclike #define
#define M8(x, y) x + y
  assert(7, M8(3, 4), "M8(3, 4)");

#define M8(x, y) x *y
  assert(24, M8(3 + 4, 4 + 5), "M8(3+4, 4+5)");

#define M8(x, y) (x) * (y)
  assert(63, M8(3 + 4, 4 + 5), "M8(3+4, 4+5)");

  // [174] Allow empty macro arguments
#define M8(x, y) x y
  assert(9, M8(, 4 + 5), "M8(, 4+5)");

  // [175] Allow parenthesized expressions as macro arguments
#define M8(x, y) x *y
  assert(20, M8((2 + 3), 4), "M8((2+3), 4)", 0);

#define M8(x, y) x *y
  assert(12, M8((2, 3), 4), "M8((2,3), 4)", 0);

  // [176] Do not expand a token more than once for the same function-like macro
#define dbl(x) M10(x) * x
#define M10(x) dbl(x) + 3
  assert(10, dbl(2), "dbl(2)", 0);

  // [177] Add macro stringizing operator (#)
#define M11(x) #x
  assert('a', M11( a!b  `""c)[0], "M11( a!b  `\"\"c)[0]", 0);
  assert('!', M11( a!b  `""c)[1], "M11( a!b  `\"\"c)[1]", 0);
  assert('b', M11( a!b  `""c)[2], "M11( a!b  `\"\"c)[2]", 0);
  assert(' ', M11( a!b  `""c)[3], "M11( a!b  `\"\"c)[3]", 0);
  assert('`', M11( a!b  `""c)[4], "M11( a!b  `\"\"c)[4]", 0);
  assert('"', M11( a!b  `""c)[5], "M11( a!b  `\"\"c)[5]", 0);
  assert('"', M11( a!b  `""c)[6], "M11( a!b  `\"\"c)[6]", 0);
  assert('c', M11( a!b  `""c)[7], "M11( a!b  `\"\"c)[7]", 0);
  assert(0, M11( a!b  `""c)[8], "M11( a!b  `\"\"c)[8]", 0);

  // [178] Add macro token-pasting operator (##)
#define paste(x, y) x##y
  assert(15, paste(1, 5), "paste(1,5)", 0);
  assert(255, paste(0, xff), "paste(0,xff)", 0);
  assert(3, ({ int foobar = 3; paste(foo, bar); }),
         "({ int foobar=3; paste(foo,bar); })", 0);
  assert(5, paste(5, ), "paste(5,)", 0);
  assert(5, paste(, 5), "paste(,5)", 0);

#define i 5
  assert(101, ({ int i3 = 100; paste(1 + i, 3); }),
         "({ int i3=100; paste(1+i,3); })", 0);
#undef i

#define paste2(x) x##5
  assert(26, paste2(1 + 2), "paste2(1+2)", 0);

#define paste3(x) 2##x
  assert(23, paste3(1 + 2), "paste3(1+2)", 0);

#define paste4(x, y, z) x##y##z
  assert(123, paste4(1, 2, 3), "paste4(1,2,3)", 0);

  // [180] Add defined() macro operator
#define M12
#if defined(M12)
  m = 3;
#else
  m = 4;
#endif
  assert(3, m, "m", 0);

#define M12
#if defined M12
  m = 3;
#else
  m = 4;
#endif
  assert(3, m, "m", 0);

#if defined(M12) - 1
  m = 3;
#else
  m = 4;
#endif
  assert(4, m, "m", 0);

#if defined(NO_SUCH_MACRO)
  m = 3;
#else
  m = 4;
#endif
  assert(4, m, "m", 0);

  // [181] Replace remaining identifiers with 0 in macro constexpr
#if no_such_symbol == 0
  m = 5;
#else
  m = 6;
#endif
  assert(5, m, "m", 0);

  // [182] Preserve newline and space during macro expansion
#define STR(x) #x
#define M12(x) STR(x)
#define M13(x) M12(foo.x)
  assert(0, strcmp(M13(bar), "foo.bar"), "strcmp(M13(bar), \"foo.bar\")", 0);

#define M13(x) M12(foo. x)
  assert(0, strcmp(M13(bar), "foo. bar"),
         "strcmp(M13(bar), \"foo. bar\")", 0);

#define M12 foo
#define M13(x) STR(x)
#define M14(x) M13(x.M12)
  assert(0, strcmp(M14(bar), "bar.foo"), "strcmp(M14(bar), \"bar.foo\")", 0);

#define M14(x) M13(x. M12)
  assert(0, strcmp(M14(bar), "bar. foo"),
         "strcmp(M14(bar), \"bar. foo\")", 0);

  // [184] Add #include <...>
#include "include3.h"
  assert(3, foo, "foo", 0);

#include "include4.h"
  assert(4, foo, "foo", 0);

#define M13 "include3.h"
#include M13
  assert(3, foo, "foo", 0);

#define M13 < include4.h
#include M13 >
  assert(4, foo, "foo", 0);

#undef foo

  // [187] Add #error
#if 0
#error "this should be ignored"
#endif

  return 0;
}
