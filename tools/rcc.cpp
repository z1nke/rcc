#include <cstdio>
#include <cstdlib>

#include "Lexer/Lexer.h"
#include "Support/Error.h"

using namespace rcc;

int main(int Argc, char **Argv) {
  if (Argc != 2) {
    errorf("%s: invalid number of arguments", Argv[0]);
    return 1;
  }

  Lexer TheLexer;
  auto TokList = TheLexer.tokenize(Argv[1]);
  const Token *Tok = TokList.get();
  if (!Tok)
    errorf("tokenize failed");

  printf("  .global main\n");
  printf("main:\n");

  // add-expr: num { ('+' | '-') num }
  char *P = Argv[1];
  printf("  li a0, %d\n", Tok->getNumber());
  Tok = Tok->getNext();

  while (Tok && Tok->isNot(Token::TK_EOF)) {
    if (Tok->equals("+")) {
      Tok = Tok->getNext(); // Eat '+'.
      // addi rd, rs1, imm => rd = rs1 + imm.
      // Note: imm is a sign-extended 12-bit immediate.
      printf("  addi a0, a0, %d\n", Tok->getNumber());
      Tok = Tok->getNext();
      continue;
    }

    if (Tok->equals("-")) {
      Tok = Tok->getNext(); // Eat '-'.
      // Note: No `subi` instruction.
      // Use `add rd, rs1, -imm` instruction instead of the `subi` instruction.
      printf("  addi a0, a0, -%d\n", Tok->getNumber());
      Tok = Tok->getNext();
      continue;
    }

    errorf("unexpected character: '%c'", *P);
    return 1;
  }

  printf("  ret\n");
  return 0;
}