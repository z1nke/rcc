#include <cstdio>
#include <cstdlib>

#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "CodeGen/CodeGen.h"
#include "Lex/Lexer.h"
#include "Parse/Parser.h"

using namespace rcc;

int main(int Argc, char **Argv) {
  if (Argc != 2) {
    fprintf(stderr, "%s: invalid number of arguments", Argv[0]);
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
  Parser P(Toks, Ctx, SM);
  FunctionDecl *S = P.parse();
  CodeGen CG(Diag);
  CG.codegen(S);
  return 0;
}