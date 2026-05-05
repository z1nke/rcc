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

} // namespace rcc
