#include "Lex/Token.h"
#include "Basic/Unreachable.h"

#include <cassert>
#include <cstdio>

namespace rcc {

void Token::newNext(TokenKind Kind, const char *Start, const char *End,
                    int Val) {
  Next = std::make_unique<Token>(Kind, Start, End, Val);
}

int Token::getVal() const {
  assert(Kind == TK_Num && "expect a number");
  return Val;
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
  case TK_Equal:
    return "=";
  case TK_EqualEqual:
    return "==";
  case TK_NotEqual:
    return "!=";
  case TK_Less:
    return "<";
  case TK_LessEqual:
    return "<=";
  case TK_Greater:
    return ">";
  case TK_GreaterEqual:
    return ">=";
  case TK_Semicolon:
    return ";";
  case TK_Ident:
    return "identifier";
  default:
    RCC_UNREACHABLE("Unknown token kind");
    break;
  }
}

std::string_view Token::getIdentifer() const {
  assert(Kind == TK_Ident);
  return std::string_view(Loc, Len);
}

void Token::dump() const {
  printf("%s \n", getKindStr());
  if (Next) {
    Next->dump();
  }
}

} // namespace rcc