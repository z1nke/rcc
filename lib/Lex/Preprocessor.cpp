#include "Lex/Preprocessor.h"
#include "Basic/Diagnostic.h"
#include "Lex/Token.h"

namespace rcc {

Token *Preprocessor::preprocess(Token *Toks) {
  Token Head;
  Token *Curr = &Head;

  while (Toks->isNot(Token::TK_EOF)) {
    if (Toks->is(Token::TK_Hash) && Toks->isAtStartOfLine()) {
      Toks = Toks->getNext();
      if (Toks->is(Token::TK_EOF) || Toks->isAtStartOfLine())
        continue;
      Diag.fatalAt(Toks->getLoc(), "invalid preprocessor directive");
    }

    Curr->setNext(Toks);
    Curr = Toks;
    Toks = Toks->getNext();
  }

  Curr->setNext(Toks);
  return Head.getNext();
}

} // namespace rcc
