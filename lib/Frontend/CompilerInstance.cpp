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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace rcc {

CompilerInstance::CompilerInstance() = default;
CompilerInstance::~CompilerInstance() = default;

static void printTokens(Token *Toks, FILE *Fp) {
  bool IsFirst = true;
  for (; Toks->isNot(Token::TK_EOF); Toks = Toks->getNext()) {
    if (!IsFirst && Toks->isAtStartOfLine())
      std::fputc('\n', Fp);
    std::fprintf(Fp, " %.*s", Toks->getLen(), Toks->getLoc());
    IsFirst = false;
  }
  std::fputc('\n', Fp);
}

static FILE *openOutputFile(Diagnostic &Diag, const char *Output) {
  if (std::strcmp(Output, "-") == 0)
    return stdout;

  FILE *Fp = std::fopen(Output, "w");
  if (!Fp)
    Diag.fatal("open {} failed", Output);
  return Fp;
}

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
    Output = Invocation->shouldPreprocessOnly() ? "-" : "a.out";

  Lexer TheLexer(*Diag);
  const char *Input = Invocation->getCC1InputPath();
  Token *Toks = TheLexer.tokenizeFile(Input);
  if (!Toks)
    Diag->fatal("tokenize failed");
  Preprocessor PP(*Diag, TheLexer, Invocation->getIncludePaths(),
                  Invocation->getCommandLineMacros());
  Toks = PP.preprocess(Toks);
  if (!Toks)
    Diag->fatal("preprocess failed");
  if (Invocation->shouldPreprocessOnly()) {
    FILE *Fp = openOutputFile(*Diag, Output);
    printTokens(Toks, Fp);
    if (Fp != stdout)
      std::fclose(Fp);
    return;
  }
  Sema S(*ACtx, *Diag);
  Parser P(Toks, *ACtx, S, *SM);
  TranslationUnitDecl *TU = P.parse();
  if (Invocation->hasAstDump()) {
    TU->dump();
    return;
  }

  // Buffer assembly in memory first so a mid-compile abort does not leave a
  // partial output file.
  char *Buf = nullptr;
  std::size_t BufLen = 0;
  FILE *OutputBuf = open_memstream(&Buf, &BufLen);
  if (!OutputBuf)
    Diag->fatal("open_memstream failed");

  CodeGen CG(*Diag, OutputBuf);
  CG.codegen(TU, Input);
  std::fclose(OutputBuf);

  FILE *Out = openOutputFile(*Diag, Output);
  if (BufLen != 0)
    std::fwrite(Buf, 1, BufLen, Out);
  if (Out != stdout)
    std::fclose(Out);
  std::free(Buf);
}

} // namespace rcc
