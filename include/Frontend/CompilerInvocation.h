#ifndef RCC_FRONTEND_COMPILERINVOCATION_H
#define RCC_FRONTEND_COMPILERINVOCATION_H

#include <memory>
#include <vector>

namespace rcc {

class CompilerInvocation {
public:
  static std::unique_ptr<CompilerInvocation> create(int Argc, char **Argv);

  const std::vector<const char *> &getInputs() const { return Inputs; }
  const char *getCC1InputPath() const {
    return CC1InputPath.empty() ? Inputs[0] : CC1InputPath.c_str();
  }

  const std::string &getErrorMsg() const { return ErrMsg; }
  const std::string &getOutputPath() const { return OutputPath; }
  bool hasAstDump() const { return AstDump; }
  bool isCC1() const { return CC1; }
  bool shouldCompileOnly() const { return CompileOnly; }
  bool shouldEmitAssembly() const { return EmitAssembly; }
  bool shouldPrintCommands() const { return PrintCommands; }

private:
  std::vector<const char *> Inputs;
  std::string CC1InputPath;
  std::string OutputPath;
  std::string ErrMsg;
  bool AstDump = false;
  bool CC1 = false;
  bool CompileOnly = false;
  bool EmitAssembly = false;
  bool PrintCommands = false;
};

} // namespace rcc

#endif