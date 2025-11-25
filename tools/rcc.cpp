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
  printf("  li a0, %d\n", std::atoi(Argv[1]));
  printf("  ret\n");
  return 0;
}