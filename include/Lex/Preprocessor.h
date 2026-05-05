#ifndef RCC_LEX_PREPROCESSOR_H
#define RCC_LEX_PREPROCESSOR_H

#include <cstdint>

namespace rcc {

class Diagnostic;
class Lexer;
class Token;

class Preprocessor {
public:
  Preprocessor(Diagnostic &Diag, Lexer &Lex) : Diag(Diag), Lex(Lex) {}

  Token *preprocess(Token *Toks);

private:
  std::int64_t evaluateDirectiveExpression(Token *&Rest, Token *Toks);

  Diagnostic &Diag;
  Lexer &Lex;
};

} // namespace rcc

#endif
