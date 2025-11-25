#include <cstdio>
#include <cstdlib>

int main(int Argc, char **Argv) {
  if (Argc != 2) {
    fprintf(stderr, "%s: invalid number of arguments\n", Argv[0]);
    return 1;
  }

  //   .global main
  // main:
  //   li a0, Argv[1]
  //   ret
  printf("  .global main\n");
  printf("main:\n");

  // add-expr: num { ('+' | '-') num }
  char *P = Argv[1];
  printf("  li a0, %ld\n", strtol(P, &P, 10));

  while (*P) {
    if (*P == '+') {
      ++P; // Eat '+'.
      // addi rd, rs1, imm => rd = rs1 + imm.
      // Note: imm is a sign-extended 12-bit immediate.
      printf("  addi a0, a0, %ld\n", strtol(P, &P, 10));
      continue;
    }

    if (*P == '-') {
      ++P; // Eat '-'.
      // Note: No `subi` instruction.
      // Use `add rd, rs1, -imm` instruction instead of the `subi` instruction.
      printf("  addi a0, a0, -%ld\n", strtol(P, &P, 10));
      continue;
    }

    fprintf(stderr, "unexpected character: '%c'\n", *P);
    return 1;
  }

  printf("  ret\n");
  return 0;
}