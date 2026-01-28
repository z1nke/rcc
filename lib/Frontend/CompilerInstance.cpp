#include "Frontend/CompilerInstance.h"
#include "AST/ASTContext.h"
#include "Basic/FileManager.h"
#include "CodeGen/CodeGen.h"
#include "Frontend/CompilerInvocation.h"
#include "Lex/Lexer.h"
#include "Parse/Parser.h"
#include "Sema/Sema.h"

#include <memory>

namespace rcc {

CompilerInstance::CompilerInstance() = default;
CompilerInstance::~CompilerInstance() = default;

std::unique_ptr<CompilerInstance> CompilerInstance::create(int Argc,
                                                           char **Argv) {
  auto CI = std::make_unique<CompilerInstance>();
  CI->Invocation = CompilerInvocation::create(Argc, Argv);
  const auto &ErrMsg = CI->Invocation->getErrorMsg();
  if (!ErrMsg.empty()) {
    std::println(stderr, "{}", ErrMsg);
    return nullptr;
  }

  CI->FileMgr = std::make_unique<FileManager>();
  CI->SM = std::make_unique<SourceManager>(*CI->FileMgr);
  CI->Diag = std::make_unique<Diagnostic>(*CI->SM);
  CI->ACtx = std::make_unique<ASTContext>(*CI->Diag);
  CI->ACtx->initBuiltinTypes();
  return CI;
}

void CompilerInstance::run() {
  const char *Output = Invocation->getOutputPath().c_str();
  FILE *Fp = std::fopen(Output, "w");
  if (!Fp)
    Diag->fatal("open {} failed", Output);

  Lexer TheLexer(*Diag);
  Token *Toks = TheLexer.tokenizeFile(Invocation->getInputs()[0]);
  if (!Toks)
    Diag->fatal("tokenize failed");
  Sema S(*ACtx, *Diag);
  Parser P(Toks, *ACtx, S, *SM);
  TranslationUnitDecl *TU = P.parse();
  CodeGen CG(*Diag, Fp);
  CG.codegen(TU);
}

} // namespace rcc