#include "Frontend/CompilerInvocation.h"

#include <filesystem>
#include <format>
#include <print>
#include <string_view>
#include <system_error>

namespace rcc {

[[noreturn]]
static void usage(int Status) {
  std::println(stderr, "{}", "rcc [-o <output>] input-files");
  std::exit(Status);
}

void CompilerInvocation::addDefaultIncludePaths(const char *Argv0) {
  // dirname(argv[0])/include — used by tests and local overrides.
  std::filesystem::path ArgvPath = Argv0;
  std::filesystem::path Dir = ArgvPath.parent_path();
  if (Dir.empty())
    Dir = ".";
  IncludePaths.push_back((Dir / "include").string());

  // Resolve the real executable so freestanding headers are found even when
  // argv[0] is a bare name from PATH.
  // bin/rcc  ->  lib/rcc/include.
  std::error_code EC;
  std::filesystem::path Exe = ArgvPath;
  if (!ArgvPath.has_parent_path())
    Exe = std::filesystem::read_symlink("/proc/self/exe", EC);
  else
    Exe = std::filesystem::weakly_canonical(Exe, EC);

  if (!EC && !Exe.empty()) {
    auto ResourceInclude = Exe.parent_path() / ".." / "lib" / "rcc" / "include";
    ResourceInclude = std::filesystem::weakly_canonical(ResourceInclude, EC);
    if (!EC)
      IncludePaths.push_back(ResourceInclude.string());
  }

  // Standard system include paths.
  IncludePaths.emplace_back("/usr/local/include");
  IncludePaths.emplace_back("/usr/include/riscv64-linux-gnu");
  IncludePaths.emplace_back("/usr/include");
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

    if (Arg == "-c") {
      Invocation->CompileOnly = true;
      continue;
    }

    if (Arg == "-E") {
      Invocation->PreprocessOnly = true;
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

    if (Arg == "-I") {
      if (Idx + 1 >= Argc || !Argv[Idx + 1]) {
        Invocation->ErrMsg = "missing path after '-I'";
        return Invocation;
      }
      Invocation->IncludePaths.emplace_back(Argv[++Idx]);
      continue;
    }

    if (Arg == "-D") {
      if (Idx + 1 >= Argc || !Argv[Idx + 1]) {
        Invocation->ErrMsg = "missing macro after '-D'";
        return Invocation;
      }
      Invocation->CommandLineMacros.push_back(
          {CommandLineMacro::Define, Argv[++Idx]});
      continue;
    }

    if (Arg == "-U") {
      if (Idx + 1 >= Argc || !Argv[Idx + 1]) {
        Invocation->ErrMsg = "missing macro after '-U'";
        return Invocation;
      }
      Invocation->CommandLineMacros.push_back(
          {CommandLineMacro::Undef, Argv[++Idx]});
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

    if (Arg.starts_with("-I")) {
      Invocation->IncludePaths.emplace_back(Arg.substr(2));
      continue;
    }

    if (Arg.starts_with("-D")) {
      Invocation->CommandLineMacros.push_back(
          {CommandLineMacro::Define, std::string(Arg.substr(2))});
      continue;
    }

    if (Arg.starts_with("-U")) {
      Invocation->CommandLineMacros.push_back(
          {CommandLineMacro::Undef, std::string(Arg.substr(2))});
      continue;
    }

    // Ignore common GCC/Clang flags that we do not implement yet.
    if (Arg.starts_with("-O") || Arg.starts_with("-W") ||
        Arg.starts_with("-g") || Arg.starts_with("-std=") ||
        Arg == "-ffreestanding" || Arg == "-fno-builtin" ||
        Arg == "-fno-omit-frame-pointer" || Arg == "-fno-stack-protector" ||
        Arg == "-fno-strict-aliasing" || Arg == "-m64" ||
        Arg == "-mno-red-zone" || Arg == "-w" || Arg == "-march=native")
      continue;

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