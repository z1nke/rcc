#include "Basic/Diagnostic.h"
#include "Lex/MacroInfo.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"

#include <new>
#include <string>
#include <string_view>

namespace rcc {

bool Preprocessor::expandMacro(Token *&Rest, Token *Tok) {
  if (!isMacroIdentifier(Tok) || Tok->isExpandDisabled())
    return false;

  std::string_view NameView(Tok->getLoc(), Tok->getLen());
  std::string Name(NameView);
  auto Iter = Macros.find(Name);
  if (Iter == Macros.end())
    return false;

  MacroInfo &MI = Iter->second;
  Token *ExpansionEnd = Tok->getNext();
  if (MI.isFunctionLike()) {
    if (ExpansionEnd->isNot(Token::TK_LParen))
      return false;

    Token *RParen = ExpansionEnd->getNext();
    if (RParen->isNot(Token::TK_RParen))
      Diag.fatalAt(RParen->getLoc(), "expected ')'");
    ExpansionEnd = RParen->getNext();
  }

  if (MI.isDisabled()) {
    Tok->disableExpand();
    return false;
  }

  MI.disableMacro();
  Token Head;
  Token *Curr = &Head;
  for (const Token &Replacement : MI.tokens()) {
    void *Mem = MacroTokenAlloc.allocate(sizeof(Token), alignof(Token));
    Token *Expanded = new (Mem) Token(Replacement);
    Expanded->setNext(nullptr);
    Curr->setNext(Expanded);
    Curr = Expanded;
  }

  Curr->setNext(ExpansionEnd);
  MacroExpansionStack.push_back(MacroExpansionFrame{&MI, ExpansionEnd});
  Rest = Head.getNext();
  return true;
}

void Preprocessor::finishMacroExpansions(Token *Tok) {
  while (!MacroExpansionStack.empty() &&
         MacroExpansionStack.back().End == Tok) {
    MacroExpansionStack.back().MI->enableMacro();
    MacroExpansionStack.pop_back();
  }
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
    finishMacroExpansions(Toks);
    Token *Tok = Toks;
    if (expandMacro(Toks, Tok))
      continue;

    ResultCurr->setNext(Toks);
    ResultCurr = Toks;
    Toks = Toks->getNext();
  }
  finishMacroExpansions(Toks);
  ResultCurr->setNext(Toks);
  return ResultHead.getNext();
}

} // namespace rcc
