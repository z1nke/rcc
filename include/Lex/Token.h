#ifndef RCC_LEX_TOKEN_H
#define RCC_LEX_TOKEN_H

#include <cstdint>
#include <string>
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

  bool hasLeadingSpace() const { return HasLeadingSpace; }
  void setHasLeadingSpace(bool Value = true) { HasLeadingSpace = Value; }

  bool isExpandDisabled() const { return DisableExpand; }
  void disableExpand() { DisableExpand = true; }

  const char *getSourceLoc() const { return SourceLoc ? SourceLoc : Loc; }
  int getSourceLen() const { return SourceLoc ? SourceLen : Len; }
  void setSourceRange(const Token &Tok) {
    SourceLoc = Tok.getSourceLoc();
    SourceLen = Tok.getSourceLen();
  }

  Token *getOrigin() const { return Origin; }
  void setOrigin(Token *Tok) { Origin = Tok; }

  std::string_view getIdentifer() const;
  std::int64_t getVal() const;
  double getFVal() const;
  NumericLiteralKind getNumericLiteralKind() const;
  bool isFloatingLiteral() const;
  unsigned getCharLiteral(Diagnostic &Diag) const;
  std::string getStringLiteral(Diagnostic &Diag) const;

  /// Encoding prefix of a string literal token.
  enum class StringLiteralKind : unsigned char {
    Narrow,
    UTF8,
    UTF16,
    UTF32,
    Wide,
  };

  StringLiteralKind getStringLiteralKind() const;

  /// Decode this token's contents using \p Kind (used when concatenating a
  /// narrow literal with an L/u/U literal).
  std::string getStringLiteralAs(Diagnostic &Diag,
                                 StringLiteralKind Kind) const;

  /// Attach precomputed encoded string bytes (without the trailing NUL).
  void setStringLiteralData(const char *Data, int Length);

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

  /// Convert a TK_PPNum token into a TK_Num integer literal.
  void becomeIntegerLiteral(std::int64_t Value, NumericLiteralKind Kind);

  /// Convert a TK_PPNum token into a TK_Num floating literal.
  void becomeFloatingLiteral(double Value, NumericLiteralKind Kind);

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
  const char *SourceLoc = nullptr;
  int SourceLen = 0;
  Token *Origin = nullptr;
  const char *StrData = nullptr;
  int StrLen = 0;
  bool AtStartOfLine = false;
  bool HasLeadingSpace = false;
  bool DisableExpand = false;
};

} // namespace rcc

#endif
