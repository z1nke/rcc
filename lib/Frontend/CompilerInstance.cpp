#include "Frontend/CompilerInstance.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "Basic/FileManager.h"
#include "CodeGen/CodeGen.h"
#include "Frontend/CompilerInvocation.h"
#include "Lex/Lexer.h"
#include "Lex/Preprocessor.h"
#include "Lex/Token.h"
#include "Parse/Parser.h"
#include "Sema/Sema.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace rcc {

CompilerInstance::CompilerInstance() = default;
CompilerInstance::~CompilerInstance() = default;

static Token *appendTokens(Token *Tok1, Token *Tok2) {
  if (!Tok1 || Tok1->is(Token::TK_EOF))
    return Tok2;

  Token *Last = Tok1;
  while (Last->getNext()->isNot(Token::TK_EOF))
    Last = Last->getNext();
  Last->setNext(Tok2);
  return Tok1;
}

static void printTokens(Token *Toks, FILE *Fp) {
  bool IsFirst = true;
  for (; Toks->isNot(Token::TK_EOF); Toks = Toks->getNext()) {
    if (!IsFirst && Toks->isAtStartOfLine())
      std::fputc('\n', Fp);
    if (Toks->hasLeadingSpace() && !Toks->isAtStartOfLine())
      std::fputc(' ', Fp);
    std::fprintf(Fp, "%.*s", Toks->getLen(), Toks->getLoc());
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
  Preprocessor PP(*Diag, TheLexer, Invocation->getIncludePaths(),
                  Invocation->getCommandLineMacros(), Input);

  // -include: tokenize forced headers before the main input (like #include).
  Token *Toks = nullptr;
  for (const std::string &Incl : Invocation->getForcedIncludes()) {
    std::string Path;
    std::error_code EC;
    if (std::filesystem::exists(Incl, EC) && !EC) {
      Path = Incl;
    } else {
      Path = PP.searchIncludePaths(Incl);
      if (Path.empty())
        Diag->fatal("-include: {}: No such file or directory", Incl);
    }

    Token *Included = TheLexer.tokenizeFile(Path.c_str());
    if (!Included)
      Diag->fatal("tokenize failed: {}", Path);
    Toks = appendTokens(Toks, Included);
  }

  Token *MainToks = TheLexer.tokenizeFile(Input);
  if (!MainToks)
    Diag->fatal("tokenize failed");
  Toks = appendTokens(Toks, MainToks);

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
  CG.setEmitCommon(Invocation->shouldEmitCommon());
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
