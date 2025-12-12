#ifndef RCC_LEX_LEXER_H
#define RCC_LEX_LEXER_H

#include "Basic/Diagnostic.h"
#include "Lex/Token.h"

#include <unordered_map>
#include <optional>

namespace rcc {

class Lexer {
public:
  Lexer(Diagnostic &Diag);

  std::unique_ptr<Token> tokenize();

private:
  void lexPunctuator(Token *&Curr, Token::TokenKind Kind, char *&P,
                     int Len = 1);

  void lexNumericLiteral(Token *&Curr, char *&P);

  Token::TokenKind getTokenKindOfIdent(std::string_view Ident);
  Token::TokenKind getTokenKindOfIdent(const char *Start, const char *End);

private:
  Diagnostic &Diag;
  std::unordered_map<std::string_view, Token::TokenKind> Keywords;
};

} // namespace rcc

#endif