#include "Frontend/CompilerInstance.h"

using namespace rcc;

int main(int Argc, char **Argv) {
  auto CI = rcc::CompilerInstance::create(Argc, Argv);
  if (!CI)
    return 1;

  CI->run();
  return 0;
}