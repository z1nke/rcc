#include "Lex/Lexer.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "Basic/Unreachable.h"

#include <cctype>
#include <cstdlib>

namespace rcc {

static bool isIdent0(char C) { return std::isalpha(C) || C == '_'; }

static bool isIdent1(char C) { return isIdent0(C) || std::isdigit(C); }

Lexer::Lexer(Diagnostic &Diag) : Diag(Diag) {
  Keywords = {{"return", Token::TK_Return},
              {"if", Token::TK_If},
              {"else", Token::TK_Else},
              {"for", Token::TK_For}};
}

std::unique_ptr<Token> Lexer::tokenize() {
  char *P = Diag.getSourceManager().getStart();
  Token Dummy;
  Token *Curr = &Dummy;

  while (*P) {
    // Skip whitespace characters.
    if (std::isspace(*P)) {
      ++P;
      continue;
    }

    if (*P >= '0' && *P <= '9') {
      lexNumericLiteral(Curr, P);
      continue;
    }

    if (isIdent0(*P)) {
      const char *Start = P;
      do {
        ++P;
      } while (isIdent1(*P));
      auto Kind = getTokenKindOfIdent(Start, P);
      Curr->newNext(Kind, Start, P);
      Curr = Curr->getNext();
      continue;
    }

    switch (*P) {
    case '+':
      lexPunctuator(Curr, Token::TK_Plus, P);
      break;
    case '-':
      lexPunctuator(Curr, Token::TK_Minus, P);
      break;
    case '*':
      lexPunctuator(Curr, Token::TK_Star, P);
      break;
    case '/':
      lexPunctuator(Curr, Token::TK_Slash, P);
      break;
    case '(':
      lexPunctuator(Curr, Token::TK_LParen, P);
      break;
    case ')':
      lexPunctuator(Curr, Token::TK_RParen, P);
      break;
    case '=':
      if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_EqualEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Equal, P);
      break;
    case '!':
      if (*(P + 1) == '=') {
        lexPunctuator(Curr, Token::TK_NotEqual, P, 2);
        break;
      }
      RCC_UNREACHABLE("Not supported 'not' token");
      break;
    case '<':
      if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_LessEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Less, P);
      break;
    case '>':
      if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_GreaterEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Greater, P);
      break;
    case ';':
      lexPunctuator(Curr, Token::TK_Semicolon, P);
      break;
    case '{':
      lexPunctuator(Curr, Token::TK_LBrace, P);
      break;
    case '}':
      lexPunctuator(Curr, Token::TK_RBRace, P);
      break;
    default:
      Diag.fatalAt(P, "invalid character: %c", *P);
      break;
    }
  }

  Curr->newNext(Token::TK_EOF, P, P);
  return Dummy.takeNext();
}

void Lexer::lexNumericLiteral(Token *&Curr, char *&P) {
  const char *Start = P;
  int Val = std::strtoul(P, &P, 10);
  Curr->newNext(Token::TK_Num, Start, P, Val);
  Curr = Curr->getNext();
}

void Lexer::lexPunctuator(Token *&Curr, Token::TokenKind Kind, char *&P,
                          int Len) {
  Curr->newNext(Kind, P, P + Len);
  Curr = Curr->getNext();
  P += Len;
}

Token::TokenKind Lexer::getTokenKindOfIdent(std::string_view Ident) {
  auto Iter = Keywords.find(Ident);
  if (Iter == Keywords.end())
    return Token::TK_Ident;

  return Iter->second;
}

Token::TokenKind Lexer::getTokenKindOfIdent(const char *Start,
                                            const char *End) {
  std::string_view Ident(Start, End - Start);
  return getTokenKindOfIdent(Ident);
}

} // namespace rcc