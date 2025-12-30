#include <cstdio>
#include <cstdlib>

#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "CodeGen/CodeGen.h"
#include "Lex/Lexer.h"
#include "Parse/Parser.h"
#include "Sema/Sema.h"

#include <print>

using namespace rcc;

int main(int Argc, char **Argv) {
  if (Argc != 2) {
    std::println(stderr, "{}: invalid number of arguments", Argv[0]);
    return 1;
  }

  char *CodeStr = Argv[1];
  SourceManager SM(CodeStr);
  Diagnostic Diag(SM);
  Lexer TheLexer(Diag);
  Token *Toks = TheLexer.tokenize(CodeStr);
  if (!Toks)
    Diag.fatal("tokenize failed");

  ASTContext Ctx(Diag);
  Ctx.initBuiltinTypes();
  Sema S(Ctx, Diag);
  Parser P(Toks, Ctx, S, SM);
  TranslationUnitDecl *TU = P.parse();
  CodeGen CG(Diag);
  CG.codegen(TU);
  return 0;
}