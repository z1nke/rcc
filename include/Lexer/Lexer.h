#ifndef RCC_LEXER_LEXER_H
#define RCC_LEXER_LEXER_H

#include "Lexer/Token.h"

namespace rcc {

class Lexer {
public:
  std::unique_ptr<Token> tokenize(char *P);
};

} // namespace

#endif