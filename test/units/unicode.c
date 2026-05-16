// RUN: %check_rcc_run %s %t %S

#include "test.h"

// [228] Add UTF-8 string literal
#define STR(x) #x

// [232] Add UTF-16 string literal initializer
typedef unsigned short char16_t;

// [233] Add UTF-32 string literal initializer
typedef unsigned int char32_t;
typedef int wchar_t;

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

  // [229] Add UTF-16 string literal
  ASSERT(2, sizeof(u""));
  ASSERT(10, sizeof(u"\xffzzz"));
  ASSERT(0, memcmp(u"", "\0\0", 2));
  ASSERT(0, memcmp(u"abc", "a\0b\0c\0\0\0", 8));
  ASSERT(0, memcmp(u"日本語", "\345e,g\236\212\0\0", 8));
  ASSERT(0, memcmp(u"🍣", "<\330c\337\0\0", 6));
  ASSERT(u'β', u"βb"[0]);
  ASSERT(u'b', u"βb"[1]);
  ASSERT(0, u"βb"[2]);

  ASSERT(0, strcmp(STR(u"a"), "u\"a\""));

  // [230] Add UTF-32 string literal
  ASSERT(4, sizeof(U""));
  ASSERT(20, sizeof(U"\xffzzz"));
  ASSERT(0, memcmp(U"", "\0\0\0\0", 4));
  ASSERT(0, memcmp(U"abc", "a\0\0\0b\0\0\0c\0\0\0\0\0\0\0", 16));
  ASSERT(0, memcmp(U"日本語", "\345e\0\0,g\0\0\236\212\0\0\0\0\0\0", 16));
  ASSERT(0, memcmp(U"🍣", "c\363\001\0\0\0\0\0", 8));
  ASSERT(u'β', U"βb"[0]);
  ASSERT(u'b', U"βb"[1]);
  ASSERT(0, U"βb"[2]);
  ASSERT(1, U"\xffffffff"[0] >> 31);

  ASSERT(0, strcmp(STR(U"a"), "U\"a\""));

  // [231] Add wide string literal
  ASSERT(4, sizeof(L""));
  ASSERT(20, sizeof(L"\xffzzz"));
  ASSERT(0, memcmp(L"", "\0\0\0\0", 4));
  ASSERT(0, memcmp(L"abc", "a\0\0\0b\0\0\0c\0\0\0\0\0\0\0", 16));
  ASSERT(0, memcmp(L"日本語", "\345e\0\0,g\0\0\236\212\0\0\0\0\0\0", 16));
  ASSERT(0, memcmp(L"🍣", "c\363\001\0\0\0\0\0", 8));
  ASSERT(u'β', L"βb"[0]);
  ASSERT(u'b', L"βb"[1]);
  ASSERT(0, L"βb"[2]);
  ASSERT(-1, L"\xffffffff"[0] >> 31);

  ASSERT(0, strcmp(STR(L"a"), "L\"a\""));


  // [232] Add UTF-16 string literal initializer
  ASSERT(u'α', ({ char16_t x[] = u"αβ"; x[0]; }));
  ASSERT(u'β', ({ char16_t x[] = u"αβ"; x[1]; }));
  ASSERT(6, ({ char16_t x[] = u"αβ"; sizeof(x); }));

  // [233] Add UTF-32 string literal initializer
  ASSERT(U'🤔', ({ char32_t x[] = U"🤔x"; x[0]; }));
  ASSERT(U'x', ({ char32_t x[] = U"🤔x"; x[1]; }));
  ASSERT(12, ({ char32_t x[] = U"🤔x"; sizeof(x); }));

  ASSERT(L'🤔', ({ wchar_t x[] = L"🤔x"; x[0]; }));
  ASSERT(L'x', ({ wchar_t x[] = L"🤔x"; x[1]; }));
  ASSERT(12, ({ wchar_t x[] = L"🤔x"; sizeof(x); }));

  printf("OK\n");
  return 0;
}
