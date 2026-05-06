#include "Lex/MacroInfo.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"

#include <new>
#include <string>

namespace rcc {

bool Preprocessor::expandMacro(Token *&Rest, Token *Tok) {
  if (!isMacroIdentifier(Tok))
    return false;

  std::string Name(Tok->getLoc(), Tok->getLen());
  auto Iter = Macros.find(Name);
  if (Iter == Macros.end())
    return false;

  Token Head;
  Token *Curr = &Head;
  for (const Token &Replacement : Iter->second.tokens()) {
    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Expanded = new (Mem) Token(Replacement);
    Expanded->setNext(nullptr);
    Curr->setNext(Expanded);
    Curr = Expanded;
  }

  Curr->setNext(Tok->getNext());
  Rest = Head.getNext();
  return true;
}

Token *Preprocessor::expandMacroExpression(Token *&Rest, Token *Toks) {
  Token LineHead;
  Token *LineCurr = &LineHead;
  while (Toks->isNot(Token::TK_EOF) && !Toks->isAtStartOfLine()) {
    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Copy = new (Mem) Token(*Toks);
    Copy->setNext(nullptr);
    LineCurr->setNext(Copy);
    LineCurr = Copy;
    Toks = Toks->getNext();
  }
  Rest = Toks;

  void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
  Token *EOFToken =
      new (Mem) Token(Token::TK_EOF, Toks->getLoc(), Toks->getLoc());
  LineCurr->setNext(EOFToken);

  Token ResultHead;
  Token *ResultCurr = &ResultHead;
  Toks = LineHead.getNext();
  while (Toks->isNot(Token::TK_EOF)) {
    Token *Tok = Toks;
    if (expandMacro(Toks, Tok))
      continue;

    ResultCurr->setNext(Toks);
    ResultCurr = Toks;
    Toks = Toks->getNext();
  }
  ResultCurr->setNext(Toks);
  return ResultHead.getNext();
}

} // namespace rcc
