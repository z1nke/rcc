#ifndef RCC_FRONTEND_COMPILERINVOCATION_H
#define RCC_FRONTEND_COMPILERINVOCATION_H

#include "Lex/MacroInfo.h"

#include <memory>
#include <string>
#include <vector>

namespace rcc {

/// Language / file kind forced by -x (driver), or inferred from extension.
enum class FileType {
  None,
  C,
  Assembler,
  Object,
  Archive,       // .a static library
  SharedObject,  // .so dynamic library
};

class CompilerInvocation {
public:
  static std::unique_ptr<CompilerInvocation> create(int Argc, char **Argv);

  // Append compiler-relative and system default include directories.
  // Call after parsing so user -I paths take precedence.
  void addDefaultIncludePaths(const char *Argv0);

  const std::vector<const char *> &getInputs() const { return Inputs; }
  const char *getCC1InputPath() const {
    return CC1InputPath.empty() ? Inputs[0] : CC1InputPath.c_str();
  }

  const std::string &getErrorMsg() const { return ErrMsg; }
  const std::string &getOutputPath() const { return OutputPath; }
  const std::vector<std::string> &getIncludePaths() const {
    return IncludePaths;
  }
  const std::vector<std::string> &getForcedIncludes() const {
    return ForcedIncludes;
  }
  const std::vector<CommandLineMacro> &getCommandLineMacros() const {
    return CommandLineMacros;
  }
  bool hasAstDump() const { return AstDump; }
  bool isCC1() const { return CC1; }
  bool shouldCompileOnly() const { return CompileOnly; }
  bool shouldEmitAssembly() const { return EmitAssembly; }
  bool shouldPreprocessOnly() const { return PreprocessOnly; }
  bool shouldPrintCommands() const { return PrintCommands; }
  /// Emit tentative definitions as common symbols (default true).
  bool shouldEmitCommon() const { return EmitCommon; }
  FileType getForcedFileType() const { return ForcedFileType; }
  /// Extra arguments forwarded to the linker (e.g. -s).
  const std::vector<std::string> &getLinkerExtraArgs() const {
    return LinkerExtraArgs;
  }

private:
  std::vector<const char *> Inputs;
  std::vector<std::string> IncludePaths;
  std::vector<std::string> ForcedIncludes;
  std::vector<CommandLineMacro> CommandLineMacros;
  std::vector<std::string> LinkerExtraArgs;
  std::string CC1InputPath;
  std::string OutputPath;
  std::string ErrMsg;
  bool AstDump = false;
  bool CC1 = false;
  bool CompileOnly = false;
  bool EmitAssembly = false;
  bool PreprocessOnly = false;
  bool PrintCommands = false;
  bool EmitCommon = true;
  FileType ForcedFileType = FileType::None;
};

} // namespace rcc

#endif
