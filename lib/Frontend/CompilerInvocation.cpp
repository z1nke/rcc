#include "Frontend/CompilerInvocation.h"

#include <print>

namespace rcc {

[[noreturn]]
static void usage(int Status) {
  std::println(stderr, "{}", "rcc [-o <output>] input-files");
  std::exit(Status);
}

std::unique_ptr<CompilerInvocation> CompilerInvocation::create(int Argc,
                                                               char **Argv) {
  auto Invocation = std::make_unique<CompilerInvocation>();
  for (int Idx = 1; Idx < Argc; ++Idx) {
    std::string_view Arg = Argv[Idx];
    if (Arg == "--help")
      usage(0);

    if (Arg == "-cc1") {
      Invocation->CC1 = true;
      continue;
    }

    if (Arg == "-###") {
      Invocation->PrintCommands = true;
      continue;
    }

    if (Arg == "-S") {
      Invocation->EmitAssembly = true;
      continue;
    }

    if (Arg == "-cc1-input") {
      if (Idx + 1 >= Argc || !Argv[Idx + 1]) {
        Invocation->ErrMsg = "missing filename after '-cc1-input'";
        return Invocation;
      }
      Invocation->CC1InputPath = Argv[++Idx];
      continue;
    }

    if (Arg == "-cc1-output") {
      if (Idx + 1 >= Argc || !Argv[Idx + 1]) {
        Invocation->ErrMsg = "missing filename after '-cc1-output'";
        return Invocation;
      }
      Invocation->OutputPath = Argv[++Idx];
      continue;
    }

    if (Arg == "-o") {
      if (Idx + 1 >= Argc || !Argv[Idx + 1]) {
        Invocation->ErrMsg = "missing filename after '-o'";
        return Invocation;
      }

      Invocation->OutputPath = Argv[++Idx];
      continue;
    }

    if (Arg == "-ast-dump") {
      Invocation->AstDump = true;
      continue;
    }

    if (Arg.starts_with("-o")) {
      Invocation->OutputPath = Arg.substr(2);
      continue;
    }

    if (Arg[0] == '-' && Arg[1] != '\0') {
      Invocation->ErrMsg = std::format("unknown option: {}", Arg);
      return Invocation;
    }

    Invocation->Inputs.push_back(Argv[Idx]);
  }

  if (Invocation->Inputs.empty())
    Invocation->ErrMsg = "no input files";

  return Invocation;
}

} // namespace rcc