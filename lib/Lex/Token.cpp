#include "Lex/Token.h"
#include "Basic/Unreachable.h"

#include <cassert>
#include <cstdio>
#include <print>

namespace rcc {

int Token::getVal() const {
  assert(Kind == TK_Num && "expect a number");
  return Val;
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