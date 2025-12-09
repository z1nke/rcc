#include "Lex/Token.h"
#include "Basic/Unreachable.h"

#include <cassert>
#include <string_view>

namespace rcc {

void Token::newNext(TokenKind Kind, const char *Start, const char *End,
                    int Val) {
  Next = std::make_unique<Token>(Kind, Start, End, Val);
}

int Token::getVal() const {
  assert(Kind == TK_Num && "expect a number");
  return Val;
}

bool Token::equals(const char *Tok) const {
  return std::string_view(Loc, Len) == Tok;
}

const char *Token::getKindStr() const {
  switch (Kind) {
  case TK_EOF:
    return "eof";
  case TK_Plus:
    return "+";
  case TK_Minus:
    return "-";
  case TK_Star:
    return "*";
  case TK_Slash:
    return "/";
  case TK_LParen:
    return "(";
  case TK_RParen: 
    return ")";
  case TK_Num:
    return "number";
  case TK_Unknown:
    return "unknown";
  default:
    RCC_UNREACHABLE("Unknown token kind");
  }
}

} // namespace rcc