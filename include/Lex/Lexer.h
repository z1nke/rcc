#ifndef RCC_LEX_LEXER_H
#define RCC_LEX_LEXER_H

#include "Basic/Diagnostic.h"
#include "Lex/Token.h"

namespace rcc {

class Lexer {
public:
  Lexer(Diagnostic &Diag) : Diag(Diag) {}

  std::unique_ptr<Token> tokenize();

  void lexPunctuator(Token *&Curr, Token::TokenKind Kind, char *&P,
                     int Len = 1);

  void lexNumericLiteral(Token *&Curr, char *&P);

private:
  Diagnostic &Diag;
};

} // namespace rcc

#endif