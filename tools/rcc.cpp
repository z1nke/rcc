#include <cstdio>
#include <cstdlib>

#include "Basic/Diagnostic.h"
#include "Lex/Lexer.h"

using namespace rcc;

static int getNumber(const Token *Tok, Diagnostic &Diag) {
  if (Tok->isNot(Token::TK_NUM))
    Diag.fatalAt(Tok->getLoc(), "expect a number");
  return Tok->getVal();
}

static const Token *skip(const Token *Tok, Token::TokenKind Kind,
                         const char *Expect, Diagnostic &Diag) {
  if (Tok->isNot(Kind))
    Diag.fatalAt(Tok->getLoc(), "expect token kind is %s", Tok->getKindStr());

  if (!Tok->equals(Expect))
    Diag.fatalAt(Tok->getLoc(), "expect %s", Expect);

  return Tok->getNext();
}

int main(int Argc, char **Argv) {
  SourceManager SM;
  Diagnostic Diag(SM);

  if (Argc != 2) {
    Diag.fatal("%s: invalid number of arguments", Argv[0]);
    return 1;
  }

  Lexer TheLexer(Diag);
  SM.setStart(Argv[1]);
  auto TokList = TheLexer.tokenize();
  const Token *Tok = TokList.get();
  if (!Tok)
    Diag.fatal("tokenize failed");

  printf("  .global main\n");
  printf("main:\n");

  // add-expr: num { ('+' | '-') num }

  printf("  li a0, %d\n", getNumber(Tok, Diag));
  Tok = Tok->getNext();

  while (Tok && Tok->isNot(Token::TK_EOF)) {
    if (Tok->equals("+")) {
      Tok = Tok->getNext(); // Eat '+'.
      // addi rd, rs1, imm => rd = rs1 + imm.
      // Note: imm is a sign-extended 12-bit immediate.
      printf("  addi a0, a0, %d\n", Tok->getVal());
      Tok = Tok->getNext();
      continue;
    }

    Tok = skip(Tok, Token::TK_PUNCT, "-", Diag); // Eat '-'.
    // Note: No `subi` instruction.
    // Use `add rd, rs1, -imm` instruction instead of the `subi` instruction.
    printf("  addi a0, a0, -%d\n", Tok->getVal());
    Tok = Tok->getNext();
  }

  printf("  ret\n");
  return 0;
}