#ifndef RCC_LEX_LEXER_H
#define RCC_LEX_LEXER_H

#include "Basic/Allocator.h"
#include "Basic/Diagnostic.h"
#include "Lex/Token.h"

#include <unordered_map>
#include <vector>

namespace rcc {

class Lexer {
public:
  Lexer(Diagnostic &Diag);

  Token *tokenize(char *P);

private:
  void lexPunctuator(Token *&Curr, Token::TokenKind Kind, char *&P,
                     int Len = 1);

  void lexNumericLiteral(Token *&Curr, char *&P);
  void lexStringLiteral(Token *&Curr, char *&P);

  Token::TokenKind getTokenKindOfIdent(std::string_view Ident);
  Token::TokenKind getTokenKindOfIdent(const char *Start, const char *End);

  Token *newToken(Token::TokenKind Kind, const char *Start, const char *End,
                  int Val = 0);

private:
  Diagnostic &Diag;
  std::vector<Token *> CachedToks;
  BumpPtrAllocator TokAlloc;
  std::unordered_map<std::string_view, Token::TokenKind> Keywords;
};

} // namespace rcc

#endif