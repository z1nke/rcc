#include "Lex/Token.h"
#include "Basic/Unreachable.h"

#include <cassert>
#include <print>

namespace rcc {

int Token::getVal() const {
  assert(Kind == TK_Num && "expect a number");
  return Val;
}

static char escapeChar(const char *&P) {
  if ('0' <= *P && *P <= '7') {
    // Octal escape sequence \abc <=> (a*8+b)*8+c
    char C = *P++ - '0';
    if ('0' <= *P && *P <= '7') {
      C = (C << 3) + (*P++ - '0');
      if ('0' <= *P && *P <= '7')
        C = (C << 3) + (*P++ - '0');
    }
    return C;
  }

  char C = *P;
  switch (*P++) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '"':
    return '"';
  case 'e':
    return 27;
  default:
    return C;
  }
}

std::string Token::getStringLiteral() const {
  assert(Kind == TK_Str && "expect a string literal");
  std::string Result;
  Result.reserve(Len - 2);
  for (const char *P = Loc + 1; P < Loc + Len - 1;) {
    if (*P != '\\') {
      Result += *P++;
      continue;
    }

    ++P; // Skip the '\'.
    Result += escapeChar(P);
  }
  return Result;
}

const char *Token::getKindStr() const { return getKindStr(Kind); }

std::string_view Token::getIdentifer() const {
  assert(Kind == TK_Ident);
  return std::string_view(Loc, Len);
}

void Token::dump() const {
  std::println("{} ", getKindStr());
  if (Next) {
    Next->dump();
  }
}

const char *Token::getKindStr(TokenKind Kind) {
  switch (Kind) {
  case TK_EOF:
    return "eof";
#define TOKEN(KIND, STR)                                                       \
  case TK_##KIND:                                                              \
    return STR;
#include "Lex/Token.def"
  case TK_Unknown:
    return "unknown";
  default:
    RCC_UNREACHABLE("Unknown token kind");
    break;
  }
}

} // namespace rcc