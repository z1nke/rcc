#include "Lex/Lexer.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"
#include "Support/Unreachable.h"

#include <cctype>
#include <cstdlib>

namespace rcc {

static bool isIdent0(char C) { return std::isalpha(C) || C == '_'; }

static bool isIdent1(char C) { return isIdent0(C) || std::isdigit(C); }

Lexer::Lexer(Diagnostic &Diag) : Diag(Diag), SM(Diag.getSourceManager()) {
  Keywords = {
#define KEYWORD(KIND, STR) {STR, Token::TK_##KIND},
#include "Lex/Token.def"
  };
}

Token *Lexer::tokenize(const char *P) {
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
      Curr->setNext(newToken(Kind, Start, P));
      Curr = Curr->getNext();
      continue;
    }

    switch (*P) {
    case '"':
      lexStringLiteral(Curr, P);
      break;
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
    case '&':
      lexPunctuator(Curr, Token::TK_Amp, P);
      break;
    case ',':
      lexPunctuator(Curr, Token::TK_Comma, P);
      break;
    case '[':
      lexPunctuator(Curr, Token::TK_LSquare, P);
      break;
    case ']':
      lexPunctuator(Curr, Token::TK_RSquare, P);
      break;
    default:
      Diag.fatalAt(P, "invalid character: {}", *P);
      break;
    }
  }

  Curr->setNext(newToken(Token::TK_EOF, P, P));
  return Dummy.getNext();
}

Token *Lexer::tokenizeFile(const char *Path) {
  FileID FID = SM.createFileID(Path);
  SourceLocation Loc = SM.getLocForStartOfFile(FID);
  CurrStart = SM.getLoc(Loc);
  return tokenize(CurrStart);
}

void Lexer::lexNumericLiteral(Token *&Curr, const char *&P) {
  const char *Start = P;
  int Val = std::strtoul(P, &const_cast<char *&>(P), 10);
  Curr->setNext(newToken(Token::TK_Num, Start, P, Val));
  Curr = Curr->getNext();
}

void Lexer::lexStringLiteral(Token *&Curr, const char *&P) {
  const char *Start = P;
  ++P; // skip opening '"'
  while (*P != '"') {
    if (*P == '\n' || *P == '\0')
      Diag.fatalAt(Start, "unclosed string literal");

    ++P;
  }

  Curr->setNext(newToken(Token::TK_Str, Start, ++P));
  Curr = Curr->getNext();
}

void Lexer::lexPunctuator(Token *&Curr, Token::TokenKind Kind, const char *&P,
                          int Len) {
  Curr->setNext(newToken(Kind, P, P + Len));
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

Token *Lexer::newToken(Token::TokenKind Kind, const char *Start,
                       const char *End, int Val) {
  void *Mem = TokAlloc.allocate(sizeof(Token), alignof(Token));
  return new (Mem) Token(Kind, Start, End, Val);
}

} // namespace rcc