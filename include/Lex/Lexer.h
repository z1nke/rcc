#ifndef RCC_LEX_LEXER_H
#define RCC_LEX_LEXER_H

#include "Basic/Diagnostic.h"
#include "Lex/Token.h"
#include "Support/Allocator.h"

#include <unordered_map>
#include <vector>

namespace rcc {

class Lexer {
public:
  Lexer(Diagnostic &Diag);

  Token *tokenize(const char *P);
  Token *tokenizeFile(const char *Path);

private:
  void lexPunctuator(Token *&Curr, Token::TokenKind Kind, const char *&P,
                     int Len = 1);

  void lexNumericLiteral(Token *&Curr, const char *&P);
  void lexCharLiteral(Token *&Curr, const char *&P);
  void lexStringLiteral(Token *&Curr, const char *&P);

  Token::TokenKind getTokenKindOfIdent(std::string_view Ident);
  Token::TokenKind getTokenKindOfIdent(const char *Start, const char *End);

  Token *newToken(Token::TokenKind Kind, const char *Start, const char *End,
                  std::int64_t Val = 0,
                  Token::NumericLiteralKind NumKind =
                      Token::NumericLiteralKind::Int);

private:
  Diagnostic &Diag;
  SourceManager &SM;
  std::vector<Token *> CachedToks;
  BumpPtrAllocator TokAlloc;
  std::unordered_map<std::string_view, Token::TokenKind> Keywords;
  const char *CurrStart = nullptr;
};

} // namespace rcc

#endif