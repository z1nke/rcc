#include "Lex/Token.h"
#include "Basic/Diagnostic.h"
#include "Support/Unicode.h"
#include "Support/Unreachable.h"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <print>

namespace rcc {

std::int64_t Token::getVal() const {
  assert(Kind == TK_Num && "expect a number");
  assert(!isFloatingLiteral() && "expect an integer literal");
  return Val;
}

double Token::getFVal() const {
  assert(Kind == TK_Num && "expect a number");
  assert(isFloatingLiteral() && "expect a floating literal");
  return FVal;
}

Token::NumericLiteralKind Token::getNumericLiteralKind() const {
  assert(Kind == TK_Num && "expect a number");
  return NumKind;
}

bool Token::isFloatingLiteral() const {
  return NumKind == NumericLiteralKind::Float ||
         NumKind == NumericLiteralKind::Double;
}

void Token::becomeIntegerLiteral(std::int64_t Value, NumericLiteralKind Kind) {
  this->Kind = TK_Num;
  this->Val = Value;
  this->NumKind = Kind;
}

void Token::becomeFloatingLiteral(double Value, NumericLiteralKind Kind) {
  this->Kind = TK_Num;
  this->FVal = Value;
  this->NumKind = Kind;
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

static unsigned escapeHex(const char *&P, Diagnostic &Diag) {
  if (!std::isxdigit(*P))
    Diag.fatalAt(P, "invalid hex escape sequence");
  // \xWXYZ = ((16 * W + X) * 16 + Y) * 16 + Z
  unsigned C = fromHex(*P++);
  while (std::isxdigit(*P)) {
    C = (C << 4) + fromHex(*P);
    ++P;
  }
  return C;
}

static unsigned escapeChar(const char *&P, Diagnostic &Diag) {
  if ('0' <= *P && *P <= '7') {
    // Octal escape sequence \abc <=> (a*8+b)*8+c
    unsigned C = *P++ - '0';
    if ('0' <= *P && *P <= '7') {
      C = (C << 3) + (*P++ - '0');
      if ('0' <= *P && *P <= '7')
        C = (C << 3) + (*P++ - '0');
    }
    return C;
  }

  unsigned C = *P;
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

unsigned Token::getCharLiteral(Diagnostic &Diag) const {
  assert(Kind == TK_CharLiteral && "expect a character literal");
  const char *P = Loc;
  enum { Narrow, Wide, UTF16, UTF32 } LitKind = Narrow;
  if (*P == 'L') {
    LitKind = Wide;
    ++P; // skip wide-character prefix
  } else if (*P == 'u') {
    LitKind = UTF16;
    ++P; // skip UTF-16 character prefix
  } else if (*P == 'U') {
    LitKind = UTF32;
    ++P; // skip UTF-32 character prefix
  }
  ++P; // Skip the opening '\''.
  assert(P < Loc + Len - 1);

  unsigned C;
  if (*P != '\\') {
    const char *Next = nullptr;
    C = decodeUTF8(&Next, P, Diag);
  } else {
    ++P; // Skip the '\'.
    C = escapeChar(P, Diag);
  }

  switch (LitKind) {
  case Narrow:
    // Narrow character literals are truncated to char.
    return static_cast<unsigned>(static_cast<char>(C));
  case UTF16:
    // UTF-16 character literals keep the low 16 bits.
    return C & 0xffff;
  case Wide:
  case UTF32:
    return C;
  }
  RCC_UNREACHABLE("invalid character literal kind");
}

std::string Token::getStringLiteral(Diagnostic &Diag) const {
  assert(Kind == TK_StrLiteral && "expect a string literal");
  if (StrData)
    return std::string(StrData, StrData + StrLen);
  return getStringLiteralAs(Diag, getStringLiteralKind());
}

Token::StringLiteralKind Token::getStringLiteralKind() const {
  assert(Kind == TK_StrLiteral && "expect a string literal");
  const char *P = Loc;
  if (*P == 'u' && *(P + 1) == '8')
    return StringLiteralKind::UTF8;
  switch (*P) {
  case '"':
    return StringLiteralKind::Narrow;
  case 'u':
    return StringLiteralKind::UTF16;
  case 'U':
    return StringLiteralKind::UTF32;
  case 'L':
    return StringLiteralKind::Wide;
  default:
    RCC_UNREACHABLE("invalid string literal prefix");
  }
}

void Token::setStringLiteralData(const char *Data, int Length) {
  assert(Kind == TK_StrLiteral && "expect a string literal");
  StrData = Data;
  StrLen = Length;
}

std::string Token::getStringLiteralAs(Diagnostic &Diag,
                                      StringLiteralKind LitKind) const {
  assert(Kind == TK_StrLiteral && "expect a string literal");
  const char *P = Loc;
  if (*P == 'u' && *(P + 1) == '8')
    P += 2;
  else if (*P == 'u' || *P == 'U' || *P == 'L')
    ++P;
  ++P; // Skip the opening '"'.

  const bool IsNarrow = LitKind == StringLiteralKind::Narrow ||
                        LitKind == StringLiteralKind::UTF8;
  const bool IsUTF16 = LitKind == StringLiteralKind::UTF16;

  std::string Result;
  Result.reserve(Loc + Len - P - 1);
  for (; P < Loc + Len - 1;) {
    std::uint32_t C;
    if (*P != '\\') {
      if (IsNarrow) {
        Result += *P++;
        continue;
      }
      C = decodeUTF8(&P, P, Diag);
    } else {
      ++P; // Skip the '\'.
      C = escapeChar(P, Diag);
      if (IsNarrow) {
        Result += static_cast<char>(C);
        continue;
      }
    }

    if (IsUTF16) {
      // Encode as little-endian UTF-16 code units.
      if (C < 0x10000) {
        Result += static_cast<char>(C & 0xff);
        Result += static_cast<char>((C >> 8) & 0xff);
      } else {
        C -= 0x10000;
        std::uint16_t Hi = 0xd800 + ((C >> 10) & 0x3ff);
        std::uint16_t Lo = 0xdc00 + (C & 0x3ff);
        Result += static_cast<char>(Hi & 0xff);
        Result += static_cast<char>((Hi >> 8) & 0xff);
        Result += static_cast<char>(Lo & 0xff);
        Result += static_cast<char>((Lo >> 8) & 0xff);
      }
    } else {
      // Encode as little-endian UTF-32 code units.
      Result += static_cast<char>(C & 0xff);
      Result += static_cast<char>((C >> 8) & 0xff);
      Result += static_cast<char>((C >> 16) & 0xff);
      Result += static_cast<char>((C >> 24) & 0xff);
    }
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