#include "Lex/Lexer.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"

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

    switch (*P) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      ++P;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      lexNumericLiteral(Curr, P);
      break;
    case '+':
      lexPunctuator(Curr, Token::TK_Plus, P);
      break;
    case '-':
      lexPunctuator(Curr, Token::TK_Minus, P);
      break;
    case '*':
      lexPunctuator(Curr, Token::TK_Mul, P);
      break;
    case '/':
      lexPunctuator(Curr, Token::TK_Div, P);
      break;
    case '(':
      lexPunctuator(Curr, Token::TK_LParen, P);
      break;
    case ')':
      lexPunctuator(Curr, Token::TK_RParen, P);
      break;
    default:
      Diag.fatalAt(P, "invalid character: %c", *P);
      break;
    }
  }

  Curr->newNext(Token::TK_EOF, P, P);
  return Dummy.takeNext();
}

void Lexer::lexNumericLiteral(Token *&Curr, char *&P) {
  const char *Start = P;
  int Val = std::strtoul(P, &P, 10);
  Curr->newNext(Token::TK_NUM, Start, P, Val);
  Curr = Curr->getNext();
}

void Lexer::lexPunctuator(Token *&Curr, Token::TokenKind Kind, char *&P) {
  Curr->newNext(Kind, P, P + 1);
  Curr = Curr->getNext();
  ++P;
}

} // namespace rcc