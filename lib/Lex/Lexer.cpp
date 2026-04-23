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
    case '\'':
      lexCharLiteral(Curr, P);
      break;
    case '"':
      lexStringLiteral(Curr, P);
      break;
    case '+':
      if (*(P + 1) == '+')
        lexPunctuator(Curr, Token::TK_PlusPlus, P, 2);
      else if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_PlusEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Plus, P);
      break;
    case '-':
      if (*(P + 1) == '-')
        lexPunctuator(Curr, Token::TK_MinusMinus, P, 2);
      else if (*(P + 1) == '>')
        lexPunctuator(Curr, Token::TK_Arrow, P, 2);
      else if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_MinusEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Minus, P);
      break;
    case '*':
      if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_StarEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Star, P);
      break;
    case '/':
      if (*(P + 1) == '=') {
        lexPunctuator(Curr, Token::TK_SlashEqual, P, 2);
        break;
      }

      if (*(P + 1) == '/') {
        P += 2;
        while (*P && *P != '\n')
          ++P;
        continue;
      }

      if (*(P + 1) == '*') {
        const char *End = strstr(P + 2, "*/");
        if (!End)
          Diag.fatalAt(P, "unclosed block comment");
        P = End + 2;
        continue;
      }

      lexPunctuator(Curr, Token::TK_Slash, P);
      break;
    case '%':
      if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_PercentEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Percent, P);
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
      if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_NotEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Exclaim, P);
      break;
    case '~':
      lexPunctuator(Curr, Token::TK_Tilde, P);
      break;
    case '<':
      if (*(P + 1) == '<' && *(P + 2) == '=')
        lexPunctuator(Curr, Token::TK_LessLessEqual, P, 3);
      else if (*(P + 1) == '<')
        lexPunctuator(Curr, Token::TK_LessLess, P, 2);
      else if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_LessEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Less, P);
      break;
    case '>':
      if (*(P + 1) == '>' && *(P + 2) == '=')
        lexPunctuator(Curr, Token::TK_GreaterGreaterEqual, P, 3);
      else if (*(P + 1) == '>')
        lexPunctuator(Curr, Token::TK_GreaterGreater, P, 2);
      else if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_GreaterEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Greater, P);
      break;
    case ';':
      lexPunctuator(Curr, Token::TK_Semicolon, P);
      break;
    case ':':
      lexPunctuator(Curr, Token::TK_Colon, P);
      break;
    case '?':
      lexPunctuator(Curr, Token::TK_Question, P);
      break;
    case '{':
      lexPunctuator(Curr, Token::TK_LBrace, P);
      break;
    case '}':
      lexPunctuator(Curr, Token::TK_RBrace, P);
      break;
    case '&':
      if (*(P + 1) == '&')
        lexPunctuator(Curr, Token::TK_AmpAmp, P, 2);
      else if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_AmpEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Amp, P);
      break;
    case '|':
      if (*(P + 1) == '|')
        lexPunctuator(Curr, Token::TK_PipePipe, P, 2);
      else if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_PipeEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Pipe, P);
      break;
    case '^':
      if (*(P + 1) == '=')
        lexPunctuator(Curr, Token::TK_CaretEqual, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Caret, P);
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
    case '.':
      lexPunctuator(Curr, Token::TK_Dot, P);
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
  int Base = 10;
  if (*P == '0') {
    char Next = *(P + 1);
    if (Next == 'x' || Next == 'X') {
      P += 2;
      Base = 16;
    } else if (Next == 'b' || Next == 'B') {
      P += 2;
      Base = 2;
    } else {
      ++P;
      Base = 8;
    }
  }

  std::int64_t Val = std::strtoul(P, &const_cast<char *&>(P), Base);
  if (std::isalnum(*P))
    Diag.fatalAt(P, "invalid numeric literal suffix: {}", *P);

  Curr->setNext(newToken(Token::TK_Num, Start, P, Val));
  Curr = Curr->getNext();
}

void Lexer::lexCharLiteral(Token *&Curr, const char *&P) {
  const char *Start = P;
  ++P; // skip opening '\''

  for (; *P != '\''; ++P) {
    if (*P == '\n' || *P == '\0')
      Diag.fatalAt(Start, "unclosed character literal");

    if (*P == '\\')
      ++P;
  }

  Curr->setNext(newToken(Token::TK_CharLiteral, Start, ++P));
  Curr = Curr->getNext();
}

void Lexer::lexStringLiteral(Token *&Curr, const char *&P) {
  const char *Start = P;
  ++P; // skip opening '"'
  for (; *P != '"'; ++P) {
    if (*P == '\n' || *P == '\0')
      Diag.fatalAt(Start, "unclosed string literal");

    if (*P == '\\')
      ++P;
  }

  Curr->setNext(newToken(Token::TK_StrLiteral, Start, ++P));
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