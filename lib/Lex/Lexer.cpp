#include "Lex/Lexer.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"
#include "Support/Unreachable.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

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
  AtStartOfLine = true;
  HasLeadingSpace = false;

  while (*P) {
    if (*P == '\n') {
      AtStartOfLine = true;
      HasLeadingSpace = true;
      ++P;
      continue;
    }

    // Skip whitespace characters.
    if (std::isspace(*P)) {
      HasLeadingSpace = true;
      ++P;
      continue;
    }

    if ((*P >= '0' && *P <= '9') || (*P == '.' && std::isdigit(*(P + 1)))) {
      lexNumericLiteral(Curr, P);
      continue;
    }

    if (*P == 'u' && *(P + 1) == '8' && *(P + 2) == '"') {
      lexStringLiteral(Curr, P);
      continue;
    }

    if (*P == 'u' && *(P + 1) == '"') {
      lexStringLiteral(Curr, P);
      continue;
    }

    if ((*P == 'L' || *P == 'u' || *P == 'U') && *(P + 1) == '\'') {
      lexCharLiteral(Curr, P);
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
        HasLeadingSpace = true;
        P += 2;
        while (*P && *P != '\n')
          ++P;
        continue;
      }

      if (*(P + 1) == '*') {
        HasLeadingSpace = true;
        const char *End = strstr(P + 2, "*/");
        if (!End)
          Diag.fatalAt(P, "unclosed block comment");
        if (std::memchr(P, '\n', End + 2 - P))
          AtStartOfLine = true;
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
      if (*(P + 1) == '.' && *(P + 2) == '.')
        lexPunctuator(Curr, Token::TK_Ellipsis, P, 3);
      else
        lexPunctuator(Curr, Token::TK_Dot, P);
      break;
    case '#':
      if (*(P + 1) == '#')
        lexPunctuator(Curr, Token::TK_HashHash, P, 2);
      else
        lexPunctuator(Curr, Token::TK_Hash, P);
      break;
    default:
      lexPunctuator(Curr, Token::TK_Unknown, P);
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
  // C preprocessing numbers are more permissive than C numeric constants so
  // that token pasting (e.g. 4 ## .57 or f ## 0zz) can form valid tokens.
  const char *Start = P++;
  while (true) {
    if (P[0] && P[1] && std::strchr("eEpP", P[0]) && std::strchr("+-", P[1]))
      P += 2;
    else if (std::isalnum(static_cast<unsigned char>(*P)) || *P == '.')
      ++P;
    else
      break;
  }
  Curr->setNext(newToken(Token::TK_PPNum, Start, P));
  Curr = Curr->getNext();
}

bool Lexer::convertPPInt(Token *Tok) {
  const char *P = Tok->getLoc();
  const char *End = Tok->getLoc() + Tok->getLen();

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
      Base = 8;
    }
  }

  char *ParseEnd = nullptr;
  std::uint64_t Val = std::strtoul(P, &ParseEnd, Base);
  P = ParseEnd;

  // Parse integer suffixes in any order, matching clang NumericLiteralParser:
  // - u/U -> unsigned;
  // - l/L -> long;
  // - ll/LL -> long long.
  bool IsUnsigned = false;
  bool IsLong = false;
  bool IsLongLong = false;
  bool HasSize = false;
  for (; P < End; ++P) {
    switch (*P) {
    case 'u':
    case 'U':
      if (IsUnsigned)
        return false;
      IsUnsigned = true;
      continue;
    case 'l':
    case 'L':
      if (HasSize)
        return false;
      HasSize = true;
      if (P + 1 < End && P[1] == *P) {
        IsLongLong = true;
        ++P; // Eat the second L/l.
      } else {
        IsLong = true;
      }
      continue;
    default:
      return false;
    }
  }

  if (P != End)
    return false;

  // Infer the literal type (C99 6.4.4.1 / LP64).
  using NLK = Token::NumericLiteralKind;
  NLK NumKind;
  bool IsUnsignedLongInt = IsUnsigned || (Base != 10 && (Val >> 63));
  if (IsLongLong)
    NumKind = IsUnsignedLongInt ? NLK::ULongLong : NLK::LongLong;
  else if (IsLong)
    NumKind = IsUnsignedLongInt ? NLK::ULong : NLK::Long;
  else if (IsUnsigned)
    NumKind = (Val >> 32) ? NLK::ULong : NLK::UInt;
  else if (Base == 10)
    NumKind = (Val >> 31) ? NLK::Long : NLK::Int;
  else if (Val >> 63)
    NumKind = NLK::ULong;
  else if (Val >> 32)
    NumKind = NLK::Long;
  else if (Val >> 31)
    NumKind = NLK::UInt;
  else
    NumKind = NLK::Int;

  Tok->becomeIntegerLiteral(static_cast<std::int64_t>(Val), NumKind);
  return true;
}

void Lexer::convertPPNumber(Token *Tok) {
  if (convertPPInt(Tok))
    return;

  char *End = nullptr;
  double Val = std::strtod(Tok->getLoc(), &End);

  using NLK = Token::NumericLiteralKind;
  NLK NumKind = NLK::Double;
  if (*End == 'f' || *End == 'F') {
    NumKind = NLK::Float;
    ++End;
  } else if (*End == 'l' || *End == 'L') {
    // long double is treated as double (RV64).
    NumKind = NLK::Double;
    ++End;
  }

  if (Tok->getLoc() + Tok->getLen() != End)
    Diag.fatalAt(Tok->getLoc(), "invalid numeric constant");

  Tok->becomeFloatingLiteral(Val, NumKind);
}

void Lexer::convertPPTokens(Token *Tok) {
  for (; Tok->isNot(Token::TK_EOF); Tok = Tok->getNext()) {
    if (Tok->is(Token::TK_PPNum))
      convertPPNumber(Tok);
  }
}

void Lexer::lexCharLiteral(Token *&Curr, const char *&P) {
  const char *Start = P;
  if (*P == 'L' || *P == 'u' || *P == 'U')
    ++P; // skip wide / UTF-16 / UTF-32 character prefix
  ++P;   // skip opening '\''

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
  if (*P == 'u' && *(P + 1) == '8')
    P += 2; // skip UTF-8 string prefix
  else if (*P == 'u')
    ++P; // skip UTF-16 string prefix
  ++P;   // skip opening '"'
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
                       const char *End, std::int64_t Val,
                       Token::NumericLiteralKind NumKind) {
  void *Mem = TokAlloc.allocate(sizeof(Token), alignof(Token));
  Token *Tok = new (Mem) Token(Kind, Start, End, Val, NumKind);
  Tok->setAtStartOfLine(AtStartOfLine);
  Tok->setHasLeadingSpace(HasLeadingSpace);
  AtStartOfLine = false;
  HasLeadingSpace = false;
  return Tok;
}

Token *Lexer::newToken(Token::TokenKind Kind, const char *Start,
                       const char *End, double FVal,
                       Token::NumericLiteralKind NumKind) {
  void *Mem = TokAlloc.allocate(sizeof(Token), alignof(Token));
  Token *Tok = new (Mem) Token(Kind, Start, End, FVal, NumKind);
  Tok->setAtStartOfLine(AtStartOfLine);
  Tok->setHasLeadingSpace(HasLeadingSpace);
  AtStartOfLine = false;
  HasLeadingSpace = false;
  return Tok;
}

} // namespace rcc