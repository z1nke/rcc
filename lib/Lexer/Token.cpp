#include "Lexer/Token.h"

#include "Support/Error.h"

namespace rcc {

void Token::newNext(TokenKind Kind, const char *Start, const char *End,
                    int Val) {
  Next = std::make_unique<Token>(Kind, Start, End, Val);
}

int Token::getNumber() const {
  if (Kind != TK_NUM)
    errorf("expect a number");
  return Val;
}

} // namespace rcc