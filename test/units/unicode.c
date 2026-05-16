// RUN: %check_rcc_run %s %t %S

#include "test.h"

// [228] Add UTF-8 string literal
#define STR(x) #x

int main() {
  // [224] Add \u and \U escape sequences
  ASSERT(4, sizeof(L'\0'));
  ASSERT(97, L'a');

  ASSERT(0, strcmp("αβγ", "\u03B1\u03B2\u03B3"));
  ASSERT(0, strcmp("日本語", "\u65E5\u672C\u8A9E"));
  ASSERT(0, strcmp("日本語", "\U000065E5\U0000672C\U00008A9E"));
  ASSERT(0, strcmp("中文", "\u4E2D\u6587"));
  ASSERT(0, strcmp("中文", "\U00004E2D\U00006587"));
  ASSERT(0, strcmp("🌮", "\U0001F32E"));

  // [225] Accept multibyte character as wide character literal
  ASSERT(-1, L'\xffffffff' >> 31);
  ASSERT(946, L'β');
  ASSERT(12354, L'あ');
  ASSERT(127843, L'🍣');

  // [228] Add UTF-8 string literal
  ASSERT(4, sizeof(u8"abc"));
  ASSERT(5, sizeof(u8"😀"));
  ASSERT(7, sizeof(u8"汉语"));
  ASSERT(0, strcmp(u8"abc", "abc"));

  ASSERT(0, strcmp(STR(u8"a"), "u8\"a\""));

  printf("OK\n");
  return 0;
}
