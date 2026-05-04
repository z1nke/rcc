#include "Frontend/CompilerInstance.h"
#include "Frontend/CompilerInvocation.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
  if (Inputs.size() > 1 && !RequestedOutput.empty()) {
    std::println(stderr, "cannot specify '-o' with multiple files");
    return 1;
  }

  bool PrintCommand = Invocation->shouldPrintCommands();
  for (const char *Input : Inputs) {
    std::string Output = RequestedOutput;
    if (Output.empty()) {
      std::string_view Extension =
          Invocation->shouldEmitAssembly() ? ".s" : ".o";
      Output = replaceExtension(Input, Extension);
    }

    if (Invocation->shouldEmitAssembly() || Invocation->hasAstDump()) {
      int Status = runCC1(Argc, Argv, Input, Output, PrintCommand);
      if (Status != 0)
        return Status;
      continue;
    }

    TempFile AssemblyFile;
    if (!AssemblyFile.isValid())
      return 1;

    int Status =
        runCC1(Argc, Argv, Input, AssemblyFile.getPath(), PrintCommand);
    if (Status != 0)
      return Status;

    Status = assemble(AssemblyFile.getPath(), Output, PrintCommand);
    if (Status != 0)
      return Status;
  }

  return 0;
}
