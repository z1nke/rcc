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
  assert(Kind == TK_NUM && "expect a number");
  return Val;
}

bool Token::equals(const char *Tok) const {
  return std::string_view(Loc, Len) == Tok;
}

const char *Token::getKindStr() const {
  switch (Kind) {
  case TK_PUNCT:
    return "punct";
  case TK_NUM:
    return "number";
  case TK_EOF:
    return "eof";
  case TK_UNKNOWN:
    return "unknown";
  default:
    RCC_UNREACHABLE("Unknown token kind");
  }
}

} // namespace rcc