#ifndef RCC_FRONTEND_COMPILERINSTANCE_H
#define RCC_FRONTEND_COMPILERINSTANCE_H

#include <memory>

namespace rcc {

class ASTContext;
class CompilerInvocation;
class Diagnostic;
class FileManager;
class SourceManager;

class CompilerInstance {
public:
  static std::unique_ptr<CompilerInstance>
  create(std::unique_ptr<CompilerInvocation> Invocation);

  void run();

public:
  CompilerInstance();
  ~CompilerInstance();

private:
  std::unique_ptr<CompilerInvocation> Invocation;
  std::unique_ptr<Diagnostic> Diag;
  std::unique_ptr<FileManager> FileMgr;
  std::unique_ptr<SourceManager> SM;
  std::unique_ptr<ASTContext> ACtx;
};

} // namespace rcc

#endif