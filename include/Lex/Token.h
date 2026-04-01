#ifndef RCC_LEX_TOKEN_H
#define RCC_LEX_TOKEN_H

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

  constexpr Token() = default;

  Token(TokenKind Kind, const char *Start, const char *End, int Val = 0)
      : Loc(Start), Kind(Kind), Val(Val), Len(End - Start) {}

  Token *getNext() const { return Next; }
  void setNext(Token *Next) { this->Next = Next; }

  std::string_view getIdentifer() const;
  std::int64_t getVal() const;
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
  std::int64_t Val = 0;
  int Len = 0;
};

} // namespace rcc

#endif