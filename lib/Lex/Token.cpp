#include "Lex/Token.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceLocation.h"
#include "Support/Unreachable.h"

#include <cassert>
#include <print>

namespace rcc {

int Token::getVal() const {
  assert(Kind == TK_Num && "expect a number");
  return Val;
}

static int fromHex(char C) {
  if ('0' <= C && C <= '9')
    return C - '0';
  if ('a' <= C && C <= 'f')
    return C - 'a' + 10;
  if ('A' <= C && C <= 'F')
    return C - 'A' + 10;

  RCC_UNREACHABLE("invalid hex character");
}

static int escapeHex(const char *&P, Diagnostic &Diag) {
  if (!std::isxdigit(*P))
    Diag.fatalAt(P, "invalid hex escape sequence");
  // \xWXYZ = ((16 * W + X) * 16 + Y) * 16 + Z
  int C = fromHex(*P++);
  unsigned Count = 1;
  while (std::isxdigit(*P)) {
    C = (C << 4) + fromHex(*P);
    ++P;
    if (++Count >= 4)
      break;
  }
  return C;
}

static int escapeChar(const char *&P, Diagnostic &Diag) {
  if ('0' <= *P && *P <= '7') {
    // Octal escape sequence \abc <=> (a*8+b)*8+c
    int C = *P++ - '0';
    if ('0' <= *P && *P <= '7') {
      C = (C << 3) + (*P++ - '0');
      if ('0' <= *P && *P <= '7')
        C = (C << 3) + (*P++ - '0');
    }
    return C;
  }

  int C = *P;
  switch (*P++) {
  case 'a':
    return '\a';
  case 'b':
    return '\b';
  case 'f':
    return '\f';
  case 'n':
    return '\n';
  case 'r':
    return '\r';
  case 't':
    return '\t';
  case 'v':
    return '\v';
  case '\\':
    return '\\';
  case '\'':
    return '\'';
  case '"':
    return '"';
  case 'e':
    return 27;
  case 'x':
    return escapeHex(P, Diag);
  default:
    return C;
  }
}

std::string Token::lexStringLiteral(Diagnostic &Diag) const {
  assert(Kind == TK_Str && "expect a string literal");
  std::string Result;
  Result.reserve(Len - 2);
  for (const char *P = Loc + 1; P < Loc + Len - 1;) {
    if (*P != '\\') {
      Result += *P++;
      continue;
    }

    ++P; // Skip the '\'.
    Result += escapeChar(P, Diag);
  }
  return Result;
}

const char *Token::getKindStr() const { return getKindStr(Kind); }

std::string_view Token::getIdentifer() const {
  assert(Kind == TK_Ident);
  return std::string_view(Loc, Len);
}

void Token::dump() const {
  std::println("{} ", getKindStr());
  if (Next) {
    Next->dump();
  }
}

const char *Token::getKindStr(TokenKind Kind) {
  switch (Kind) {
  case TK_EOF:
    return "eof";
#define TOKEN(KIND, STR)                                                       \
  case TK_##KIND:                                                              \
    return STR;
#include "Lex/Token.def"
  case TK_Unknown:
    return "unknown";
  default:
    RCC_UNREACHABLE("Unknown token kind");
    break;
  }
}

} // namespace rcc