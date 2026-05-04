#include "Frontend/CompilerInstance.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "Basic/FileManager.h"
#include "CodeGen/CodeGen.h"
#include "Frontend/CompilerInvocation.h"
#include "Lex/Lexer.h"
#include "Lex/Preprocessor.h"
#include "Parse/Parser.h"
#include "Sema/Sema.h"

#include <cstring>
#include <memory>

namespace rcc {

CompilerInstance::CompilerInstance() = default;
CompilerInstance::~CompilerInstance() = default;

std::unique_ptr<CompilerInstance>
CompilerInstance::create(std::unique_ptr<CompilerInvocation> Invocation) {
  auto CI = std::make_unique<CompilerInstance>();
  CI->Invocation = std::move(Invocation);

  CI->FileMgr = std::make_unique<FileManager>();
  CI->SM = std::make_unique<SourceManager>(*CI->FileMgr);
  CI->Diag = std::make_unique<Diagnostic>(*CI->SM);
  CI->ACtx = std::make_unique<ASTContext>(*CI->Diag);
  CI->ACtx->initBuiltinTypes();
  return CI;
}

void CompilerInstance::run() {
  const char *Output = Invocation->getOutputPath().c_str();
  if (Output == nullptr || Output[0] == '\0')
    Output = "a.out";

  FILE *Fp = stdout;
  if (std::strcmp(Output, "-") != 0) {
    Fp = std::fopen(Output, "w");
    if (!Fp)
      Diag->fatal("open {} failed", Output);
  }

  Lexer TheLexer(*Diag);
  const char *Input = Invocation->getCC1InputPath();
  Token *Toks = TheLexer.tokenizeFile(Input);
  if (!Toks)
    Diag->fatal("tokenize failed");
  Preprocessor PP;
  Toks = PP.preprocess(Toks);
  if (!Toks)
    Diag->fatal("preprocess failed");
  Sema S(*ACtx, *Diag);
  Parser P(Toks, *ACtx, S, *SM);
  TranslationUnitDecl *TU = P.parse();
  if (Invocation->hasAstDump()) {
    TU->dump();
    return;
  }
  CodeGen CG(*Diag, Fp);
  CG.codegen(TU, Input);
}

} // namespace rcc