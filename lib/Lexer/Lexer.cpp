#include "Lex/Lexer.h"
#include "Basic/Diagnostic.h"

#include <cctype>
#include <cstdlib>

namespace rcc {

std::unique_ptr<Token> Lexer::tokenize() {
  char *P = Diag.getSourceManager().getStart();
  Token Dummy;
  Token *Curr = &Dummy;

  while (*P) {
    // Skip whitespace characters.
    if (std::isspace(*P)) {
      ++P;
      continue;
    }

    // Numeric literal.
    if (std::isdigit(*P)) {
      const char *Start = P;
      int Val = std::strtoul(P, &P, 10);
      Curr->newNext(Token::TK_NUM, Start, P, Val);
      Curr = Curr->getNext();
      continue;
    }

    // Punctuator.
    if (*P == '+' || *P == '-') {
      Curr->newNext(Token::TK_PUNCT, P, P + 1);
      Curr = Curr->getNext();
      ++P;
      continue;
    }

    Diag.fatalAt(P, "invalid character: %c", *P);
  }

  Curr->newNext(Token::TK_EOF, P, P);
  return Dummy.takeNext();
}

} // namespace rcc