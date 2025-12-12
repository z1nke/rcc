#ifndef RCC_LEX_TOKEN_H
#define RCC_LEX_TOKEN_H

#include <memory>
#include <string_view>

namespace rcc {

class Token {
public:
  enum TokenKind {
    TK_EOF,
    TK_Plus,
    TK_Minus,
    TK_Star,
    TK_Slash,
    TK_LParen,
    TK_RParen,
    TK_Equal,
    TK_EqualEqual,
    TK_NotEqual,
    TK_Less,
    TK_LessEqual,
    TK_Greater,
    TK_GreaterEqual,
    TK_Semicolon,
    TK_LBrace,
    TK_RBRace,
    TK_Num,
    TK_Ident,
    TK_Return,
    TK_Unknown,
  };

  constexpr Token() = default;

  Token(TokenKind Kind, const char *Start, const char *End, int Val = 0)
      : Loc(Start), Kind(Kind), Val(Val), Len(End - Start) {}

  void newNext(TokenKind Kind, const char *Start, const char *End, int Val = 0);

  Token *getNext() const { return Next.get(); }

  std::string_view getIdentifer() const;

  std::unique_ptr<Token> takeNext() { return std::move(Next); }

  int getVal() const;

  bool is(TokenKind TK) const { return TK == Kind; }

  bool isOneOf(TokenKind K1, TokenKind K2) const { return is(K1) || is(K2); }
  template <typename... Ts> bool isOneOf(TokenKind K1, Ts... Ks) const {
    return is(K1) || isOneOf(Ks...);
  }

  bool isNot(TokenKind TK) const { return TK != Kind; }
  const char *getLoc() const { return Loc; }
  TokenKind getKind() const { return Kind; }
  const char *getKindStr() const;

  void dump() const;

private:
  const char *Loc = nullptr;
  std::unique_ptr<Token> Next;
  TokenKind Kind = TK_Unknown;
  int Val = 0;
  int Len = 0;
};

} // namespace rcc

#endif