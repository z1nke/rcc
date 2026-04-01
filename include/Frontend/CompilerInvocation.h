#ifndef RCC_FRONTEND_COMPILERINVOCATION_H
#define RCC_FRONTEND_COMPILERINVOCATION_H

#include <memory>
#include <vector>

namespace rcc {

class CompilerInvocation {
public:
  static std::unique_ptr<CompilerInvocation> create(int Argc, char **Argv);

  const std::vector<const char *> &getInputs() const { return Inputs; }

  const std::string &getErrorMsg() const { return ErrMsg; }
  const std::string &getOutputPath() const { return OutputPath; }
  bool hasAstDump() const { return AstDump; }

private:
  std::vector<const char *> Inputs;
  std::string OutputPath;
  std::string ErrMsg;
  bool AstDump = false;
};

} // namespace rcc

#endif