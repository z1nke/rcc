#ifndef RCC_LEX_TOKEN_H
#define RCC_LEX_TOKEN_H

#include <cstdint>
#include <string_view>

namespace rcc {

class Diagnostic;

class Token {
public:
  enum TokenKind {
    TK_EOF,
#define TOKEN(KIND, STR) TK_##KIND,
#include "Lex/Token.def"
    TK_Unknown,
  };

  /// Resolved type of a numeric literal.
  enum class NumericLiteralKind : unsigned char {
    Int,
    UInt,
    Long,
    ULong,
    LongLong,
    ULongLong,
    Float,
    Double,
  };

  constexpr Token() = default;

  Token(TokenKind Kind, const char *Start, const char *End,
        std::int64_t Val = 0,
        NumericLiteralKind NumKind = NumericLiteralKind::Int)
      : Loc(Start), Kind(Kind), Val(Val), Len(End - Start), NumKind(NumKind) {}

  Token(TokenKind Kind, const char *Start, const char *End, double FVal,
        NumericLiteralKind NumKind)
      : Loc(Start), Kind(Kind), FVal(FVal), Len(End - Start), NumKind(NumKind) {
  }

  Token *getNext() const { return Next; }
  void setNext(Token *Next) { this->Next = Next; }

  bool isAtStartOfLine() const { return AtStartOfLine; }
  void setAtStartOfLine(bool Value = true) { AtStartOfLine = Value; }

  bool isExpandDisabled() const { return DisableExpand; }
  void disableExpand() { DisableExpand = true; }

  std::string_view getIdentifer() const;
  std::int64_t getVal() const;
  double getFVal() const;
  NumericLiteralKind getNumericLiteralKind() const;
  bool isFloatingLiteral() const;
  unsigned getCharLiteral(Diagnostic &Diag) const;
  std::string getStringLiteral(Diagnostic &Diag) const;

  bool is(TokenKind TK) const { return TK == Kind; }

  bool isOneOf(TokenKind K1, TokenKind K2) const { return is(K1) || is(K2); }
  template <typename... Ts> bool isOneOf(TokenKind K1, Ts... Ks) const {
    return is(K1) || isOneOf(Ks...);
  }

  bool isNot(TokenKind TK) const { return TK != Kind; }
  const char *getLoc() const { return Loc; }
  int getLen() const { return Len; }

  TokenKind getKind() const { return Kind; }
  const char *getKindStr() const;
  static const char *getKindStr(TokenKind Kind);

  void dump() const;

private:
  const char *Loc = nullptr;
  Token *Next = nullptr;
  TokenKind Kind = TK_Unknown;
  union {
    std::int64_t Val = 0;
    double FVal;
  };
  int Len = 0;
  NumericLiteralKind NumKind = NumericLiteralKind::Int;
  bool AtStartOfLine = false;
  bool DisableExpand = false;
};

} // namespace rcc

#endif
