#include "Frontend/CompilerInstance.h"
#include "Frontend/CompilerInvocation.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace rcc;

static int runSubprocess(char **Argv, bool PrintCommand) {
  if (PrintCommand) {
    std::fprintf(stderr, "%s", Argv[0]);
    for (unsigned I = 1; Argv[I]; ++I)
      std::fprintf(stderr, " %s", Argv[I]);
    std::fputc('\n', stderr);
  }

  pid_t PID = fork();
  if (PID < 0) {
    std::fprintf(stderr, "fork failed: %s\n", std::strerror(errno));
    return 1;
  }

  if (PID == 0) {
    execvp(Argv[0], Argv);
    std::fprintf(stderr, "exec failed: %s: %s\n", Argv[0],
                 std::strerror(errno));
    _exit(1);
  }

  int Status = 0;
  pid_t Result;
  do {
    Result = waitpid(PID, &Status, 0);
  } while (Result < 0 && errno == EINTR);

  if (Result < 0) {
    std::fprintf(stderr, "waitpid failed: %s\n", std::strerror(errno));
    return 1;
  }

  if (WIFEXITED(Status))
    return WEXITSTATUS(Status);
  return 1;
}

static int runCC1(int Argc, char **Argv, bool PrintCommand) {
  std::vector<char *> Args(Argv, Argv + Argc);
  Args.push_back(const_cast<char *>("-cc1"));
  Args.push_back(nullptr);
  return runSubprocess(Args.data(), PrintCommand);
}

int main(int Argc, char **Argv) {
  auto Invocation = CompilerInvocation::create(Argc, Argv);
  const auto &ErrMsg = Invocation->getErrorMsg();
  if (!ErrMsg.empty()) {
    std::fprintf(stderr, "%s\n", ErrMsg.c_str());
    return 1;
  }

  if (!Invocation->isCC1())
    return runCC1(Argc, Argv, Invocation->shouldPrintCommands());

  auto CI = CompilerInstance::create(std::move(Invocation));
  if (!CI)
    return 1;

  CI->run();
  return 0;
}