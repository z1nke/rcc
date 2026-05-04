#include "Frontend/CompilerInstance.h"
#include "Frontend/CompilerInvocation.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace rcc;

static int runSubprocess(char **Argv, bool PrintCommand) {
  if (PrintCommand) {
    std::print(stderr, "{}", Argv[0]);
    for (unsigned I = 1; Argv[I]; ++I)
      std::print(stderr, " {}", Argv[I]);
    std::println(stderr);
  }

  pid_t PID = fork();
  if (PID < 0) {
    std::println(stderr, "fork failed: {}", std::strerror(errno));
    return 1;
  }

  if (PID == 0) {
    execvp(Argv[0], Argv);
    std::println(stderr, "exec failed: {}: {}", Argv[0], std::strerror(errno));
    _exit(1);
  }

  int Status = 0;
  pid_t Result;
  do {
    Result = waitpid(PID, &Status, 0);
  } while (Result < 0 && errno == EINTR);

  if (Result < 0) {
    std::println(stderr, "waitpid failed: {}", std::strerror(errno));
    return 1;
  }

  if (WIFEXITED(Status))
    return WEXITSTATUS(Status);
  return 1;
}

static int runCC1(int Argc, char **Argv, const char *Input,
                  const std::string &Output, bool PrintCommand) {
  std::vector<char *> Args(Argv, Argv + Argc);
  Args.push_back(const_cast<char *>("-cc1"));
  Args.push_back(const_cast<char *>("-cc1-input"));
  Args.push_back(const_cast<char *>(Input));
  Args.push_back(const_cast<char *>("-cc1-output"));
  Args.push_back(const_cast<char *>(Output.c_str()));
  Args.push_back(nullptr);
  return runSubprocess(Args.data(), PrintCommand);
}

static int assemble(const std::string &Input, const std::string &Output,
                    bool PrintCommand) {
  char *Args[] = {
      const_cast<char *>("riscv64-unknown-linux-gnu-as"),
      const_cast<char *>("-c"),
      const_cast<char *>(Input.c_str()),
      const_cast<char *>("-o"),
      const_cast<char *>(Output.c_str()),
      nullptr,
  };
  return runSubprocess(Args, PrintCommand);
}

static std::filesystem::path findProgram(std::string_view Name) {
  const char *PathEnv = std::getenv("PATH");
  if (!PathEnv)
    return {};

  std::string_view Paths(PathEnv);
  while (!Paths.empty()) {
    std::size_t Colon = Paths.find(':');
    std::string_view Directory = Paths.substr(0, Colon);
    std::filesystem::path Candidate = std::filesystem::path(Directory) / Name;
    if (access(Candidate.c_str(), X_OK) == 0)
      return std::filesystem::weakly_canonical(Candidate);

    if (Colon == std::string_view::npos)
      break;
    Paths.remove_prefix(Colon + 1);
  }

  return {};
}

static std::filesystem::path
findGCCLibPath(const std::filesystem::path &ToolchainRoot) {
  std::filesystem::path GCCRoot =
      ToolchainRoot / "lib/gcc/riscv64-unknown-linux-gnu";
  std::error_code EC;
  if (!std::filesystem::is_directory(GCCRoot, EC))
    return {};

  std::filesystem::path Result;
  for (const auto &Entry : std::filesystem::directory_iterator(GCCRoot, EC)) {
    if (EC)
      break;
    if (std::filesystem::exists(Entry.path() / "crtbegin.o", EC))
      Result = Entry.path();
  }
  return Result;
}

static int runLinker(const std::vector<std::string> &Inputs,
                     const std::string &Output, bool PrintCommand) {
  std::filesystem::path Linker = findProgram("riscv64-unknown-linux-gnu-ld");
  if (Linker.empty()) {
    std::println(stderr, "riscv64-unknown-linux-gnu-ld not found");
    return 1;
  }

  std::filesystem::path ToolchainRoot = Linker.parent_path().parent_path();
  std::filesystem::path GCCLibPath = findGCCLibPath(ToolchainRoot);
  std::filesystem::path Sysroot = ToolchainRoot / "sysroot";
  std::filesystem::path LibPath = Sysroot / "usr/lib";
  std::filesystem::path DynamicLinker =
      Sysroot / "lib/ld-linux-riscv64-lp64d.so.1";
  if (GCCLibPath.empty() || !std::filesystem::exists(LibPath / "crt1.o") ||
      !std::filesystem::exists(DynamicLinker)) {
    std::println(stderr, "RISC-V runtime library path is not found");
    return 1;
  }

  std::vector<std::string> Args = {
      Linker.string(),
      "-o",
      Output,
      "-m",
      "elf64lriscv",
      "-dynamic-linker",
      DynamicLinker.string(),
      (LibPath / "crt1.o").string(),
      (GCCLibPath / "crti.o").string(),
      (GCCLibPath / "crtbegin.o").string(),
      "-L" + GCCLibPath.string(),
      "-L" + LibPath.string(),
      "-L" + (Sysroot / "lib").string(),
      "-L" + (ToolchainRoot / "riscv64-unknown-linux-gnu/lib").string(),
  };
  Args.insert(Args.end(), Inputs.begin(), Inputs.end());
  Args.insert(Args.end(), {
                              "-lc",
                              "-lgcc",
                              "--as-needed",
                              "-lgcc_s",
                              "--no-as-needed",
                              (GCCLibPath / "crtend.o").string(),
                              (GCCLibPath / "crtn.o").string(),
                          });

  std::vector<char *> Argv;
  Argv.reserve(Args.size() + 1);
  for (std::string &Arg : Args)
    Argv.push_back(Arg.data());
  Argv.push_back(nullptr);
  return runSubprocess(Argv.data(), PrintCommand);
}

static std::string replaceExtension(const char *Input,
                                    std::string_view Extension) {
  std::filesystem::path Filename = std::filesystem::path(Input).filename();
  Filename.replace_extension(Extension);
  return Filename.string();
}

class TempFile {
public:
  TempFile() {
    std::string Template = "/tmp/rcc-XXXXXX";
    std::vector<char> PathBuffer(Template.begin(), Template.end());
    PathBuffer.push_back('\0');
    int FD = mkstemp(PathBuffer.data());
    if (FD < 0) {
      std::println(stderr, "mkstemp failed: {}", std::strerror(errno));
      return;
    }

    close(FD);
    Path = PathBuffer.data();
  }

  ~TempFile() {
    if (!Path.empty())
      unlink(Path.c_str());
  }

  TempFile(const TempFile &) = delete;
  TempFile &operator=(const TempFile &) = delete;

  const std::string &getPath() const { return Path; }
  bool isValid() const { return !Path.empty(); }

private:
  std::string Path;
};

static TempFile *
createTempFile(std::vector<std::unique_ptr<TempFile>> &TempFiles) {
  auto File = std::make_unique<TempFile>();
  if (!File->isValid())
    return nullptr;

  TempFile *Result = File.get();
  TempFiles.push_back(std::move(File));
  return Result;
}

int main(int Argc, char **Argv) {
  auto Invocation = CompilerInvocation::create(Argc, Argv);
  const auto &ErrMsg = Invocation->getErrorMsg();
  if (!ErrMsg.empty()) {
    std::println(stderr, "{}", ErrMsg);
    return 1;
  }

  if (Invocation->isCC1()) {
    auto CI = CompilerInstance::create(std::move(Invocation));
    if (!CI)
      return 1;

    CI->run();
    return 0;
  }

  const auto &Inputs = Invocation->getInputs();
  const std::string &RequestedOutput = Invocation->getOutputPath();
  bool CompileOnly = Invocation->shouldCompileOnly();
  bool EmitAssembly = Invocation->shouldEmitAssembly();
  if (Inputs.size() > 1 && !RequestedOutput.empty() &&
      (CompileOnly || EmitAssembly)) {
    std::println(stderr,
                 "cannot specify '-o' with '-c' or '-S' with multiple files");
    return 1;
  }

  bool PrintCommand = Invocation->shouldPrintCommands();
  std::vector<std::unique_ptr<TempFile>> TempFiles;
  std::vector<std::string> LinkerInputs;

  for (const char *Input : Inputs) {
    std::filesystem::path InputPath(Input);
    std::string Extension = InputPath.extension().string();

    if (Extension == ".o") {
      LinkerInputs.emplace_back(Input);
      continue;
    }

    if (Extension == ".s") {
      if (EmitAssembly)
        continue;

      std::string Output;
      if (CompileOnly) {
        Output = RequestedOutput.empty() ? replaceExtension(Input, ".o")
                                         : RequestedOutput;
      } else {
        TempFile *ObjectFile = createTempFile(TempFiles);
        if (!ObjectFile)
          return 1;
        Output = ObjectFile->getPath();
        LinkerInputs.push_back(Output);
      }

      int Status = assemble(Input, Output, PrintCommand);
      if (Status != 0)
        return Status;
      continue;
    }

    if (Extension != ".c" && std::strcmp(Input, "-") != 0) {
      std::println(stderr, "unknown file extension: {}", Input);
      return 1;
    }

    if (EmitAssembly || Invocation->hasAstDump()) {
      std::string Output = RequestedOutput.empty()
                               ? replaceExtension(Input, ".s")
                               : RequestedOutput;
      int Status = runCC1(Argc, Argv, Input, Output, PrintCommand);
      if (Status != 0)
        return Status;
      continue;
    }

    TempFile *AssemblyFile = createTempFile(TempFiles);
    if (!AssemblyFile)
      return 1;

    int Status =
        runCC1(Argc, Argv, Input, AssemblyFile->getPath(), PrintCommand);
    if (Status != 0)
      return Status;

    std::string ObjectOutput;
    if (CompileOnly) {
      ObjectOutput = RequestedOutput.empty() ? replaceExtension(Input, ".o")
                                             : RequestedOutput;
    } else {
      TempFile *ObjectFile = createTempFile(TempFiles);
      if (!ObjectFile)
        return 1;
      ObjectOutput = ObjectFile->getPath();
      LinkerInputs.push_back(ObjectOutput);
    }

    Status = assemble(AssemblyFile->getPath(), ObjectOutput, PrintCommand);
    if (Status != 0)
      return Status;
  }

  if (CompileOnly || EmitAssembly || Invocation->hasAstDump())
    return 0;

  std::string LinkOutput = RequestedOutput.empty() ? "a.out" : RequestedOutput;
  return runLinker(LinkerInputs, LinkOutput, PrintCommand);
}
