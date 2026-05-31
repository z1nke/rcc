// RUN: %check_rcc_run %s %t %S

#include "test.h"

int main() {
  // [65] Make sizeof to accept not only an expression but also a typename
  ASSERT(1, sizeof(char));
  ASSERT(2, sizeof(short));
  ASSERT(2, sizeof(short int));
  ASSERT(2, sizeof(int short));
  ASSERT(4, sizeof(int));
  ASSERT(8, sizeof(long));
  ASSERT(8, sizeof(long int));
  ASSERT(8, sizeof(long int));
  ASSERT(8, sizeof(char *));
  ASSERT(8, sizeof(int *));
  ASSERT(8, sizeof(long *));
  ASSERT(8, sizeof(int **));
  ASSERT(8, sizeof(int(*)[4]));
  ASSERT(32, sizeof(int*[4]));
  ASSERT(16, sizeof(int[4]));
  ASSERT(48, sizeof(int[3][4]));
  ASSERT(8, sizeof(struct {int a; int b;}));

  // [68] Implement usual arithmetic conversion
  ASSERT(8, sizeof(-10 + (long)5));
  ASSERT(8, sizeof(-10 - (long)5));
  ASSERT(8, sizeof(-10 * (long)5));
  ASSERT(8, sizeof(-10 / (long)5));
  ASSERT(8, sizeof((long)-10 + 5));
  ASSERT(8, sizeof((long)-10 - 5));
  ASSERT(8, sizeof((long)-10 * 5));
  ASSERT(8, sizeof((long)-10 / 5));

  // [78] Add pre ++ and --
  ASSERT(1, ({ char i; sizeof(++i); }));

  // [79] Add post ++ and --
  ASSERT(1, ({ char i; sizeof(i++); }));

  // [86] Add a notion of an incomplete array type
  ASSERT(8, sizeof(int(*)[10]));
  ASSERT(8, sizeof(int(*)[][10]));

  // [112] Add flexible array member
  ASSERT(4, sizeof(struct { int x, y[]; }));

  // [130] Add signed keyword
  ASSERT(1, sizeof(char));
  ASSERT(1, sizeof(signed char));

  ASSERT(2, sizeof(short));
  ASSERT(2, sizeof(int short));
  ASSERT(2, sizeof(short int));
  ASSERT(2, sizeof(signed short));
  ASSERT(2, sizeof(int short signed));

  ASSERT(4, sizeof(int));
  ASSERT(4, sizeof(signed int));
  ASSERT(4, sizeof(signed));

  ASSERT(8, sizeof(long));
  ASSERT(8, sizeof(signed long));
  ASSERT(8, sizeof(signed long int));

  ASSERT(8, sizeof(long long));
  ASSERT(8, sizeof(signed long long));
  ASSERT(8, sizeof(signed long long int));

  // [131] Add unsigned integral types
  ASSERT(1, sizeof(unsigned char));
  ASSERT(2, sizeof(unsigned short));
  ASSERT(2, sizeof(int short unsigned));
  ASSERT(4, sizeof(unsigned int));
  ASSERT(4, sizeof(unsigned));
  ASSERT(8, sizeof(unsigned long));
  ASSERT(8, sizeof(unsigned long int));
  ASSERT(8, sizeof(unsigned long long));
  ASSERT(8, sizeof(unsigned long long int));

  ASSERT(1, sizeof((char)1));
  ASSERT(2, sizeof((short)1));
  ASSERT(4, sizeof((int)1));
  ASSERT(8, sizeof((long)1));

  ASSERT(4, sizeof((char)1 + (char)1));
  ASSERT(4, sizeof((short)1 + (short)1));
  ASSERT(4, sizeof(1?2:3));
  ASSERT(4, sizeof(1?(short)2:(char)3));
  ASSERT(8, sizeof(1?(long)2:(char)3));

  // [133] Use long or ulong instead of int for some expressions
  ASSERT(1, sizeof(char) << 31 >> 31);
  ASSERT(1, sizeof(char) << 63 >> 63);

  // [140] Add "float" and "double" local variables and casts
  ASSERT(4, sizeof(float));
  ASSERT(8, sizeof(double));

  // [142] Add floating-pointer number +, -, * and /
  ASSERT(4, sizeof(1f + 2));
  ASSERT(8, sizeof(1.0+2));
  ASSERT(4, sizeof(1f-2));
  ASSERT(8, sizeof(1.0-2));
  ASSERT(4, sizeof(1f*2));
  ASSERT(8, sizeof(1.0*2));
  ASSERT(4, sizeof(1f/2));
  ASSERT(8, sizeof(1.0/2));

  // [149] Add "long double" as an alias for "double"
  ASSERT(8, sizeof(long double));

  // [257] [GNU] Allow sizeof(<function type>)
  ASSERT(1, sizeof(main));

  printf("OK\n");
  return 0;
}
