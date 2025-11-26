#ifndef RCC_LEX_TOKEN_H
#define RCC_LEX_TOKEN_H

#include <memory>

namespace rcc {

class Token {
public:
  enum TokenKind {
    TK_PUNCT,
    TK_NUM,
    TK_EOF,
    TK_UNKNOWN,
  };

  constexpr Token() = default;

  Token(TokenKind Kind, const char *Start, const char *End, int Val = 0)
      : Loc(Start), Kind(Kind), Val(Val), Len(End - Start) {}

  void newNext(TokenKind Kind, const char *Start, const char *End, int Val = 0);

  Token *getNext() const { return Next.get(); }

  std::unique_ptr<Token> takeNext() { return std::move(Next); }

  int getVal() const;

  bool is(TokenKind TK) const { return TK == Kind; }
  bool isNot(TokenKind TK) const { return TK != Kind; }
  bool equals(const char *Tok) const;

  const char *getLoc() const { return Loc; }
  TokenKind getKind() const { return Kind; }
  const char *getKindStr() const;

private:
  const char *Loc = nullptr;
  std::unique_ptr<Token> Next;
  TokenKind Kind = TK_UNKNOWN;
  int Val = 0;
  int Len = 0;
};

} // namespace rcc

#endif