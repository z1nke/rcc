#include "Basic/Diagnostic.h"
#include "Lex/MacroInfo.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"

#include <cctype>
#include <string>
#include <utility>

namespace rcc {

bool Preprocessor::isMacroIdentifier(const Token *Tok) {
  if (Tok->getLen() == 0)
    return false;

  unsigned char First = static_cast<unsigned char>(Tok->getLoc()[0]);
  if (!std::isalpha(First) && First != '_')
    return false;

  for (int I = 1; I < Tok->getLen(); ++I) {
    unsigned char C = static_cast<unsigned char>(Tok->getLoc()[I]);
    if (!std::isalnum(C) && C != '_')
      return false;
  }
  return true;
}

void Preprocessor::handleDefineDirective(Token *&Rest, Token *NameTok) {
  if (!isMacroIdentifier(NameTok))
    Diag.fatalAt(NameTok->getLoc(), "macro name must be an identifier");

  MacroInfo MI;
  Token *Tok = NameTok->getNext();
  while (Tok->isNot(Token::TK_EOF) && !Tok->isAtStartOfLine()) {
    MI.addTokenToBody(*Tok);
    Tok = Tok->getNext();
  }

  std::string Name(NameTok->getLoc(), NameTok->getLen());
  Macros.insert_or_assign(std::move(Name), std::move(MI));
  Rest = Tok;
}

void Preprocessor::handleUndefDirective(Token *&Rest, Token *NameTok) {
  if (!isMacroIdentifier(NameTok))
    Diag.fatalAt(NameTok->getLoc(), "macro name must be an identifier");

  std::string Name(NameTok->getLoc(), NameTok->getLen());
  Macros.erase(Name);

  Token *Tok = NameTok->getNext();
  if (Tok->isNot(Token::TK_EOF) && !Tok->isAtStartOfLine()) {
    Diag.warnAt(Tok->getLoc(), "extra token");
    do {
      Tok = Tok->getNext();
    } while (Tok->isNot(Token::TK_EOF) && !Tok->isAtStartOfLine());
  }
  Rest = Tok;
}

} // namespace rcc
