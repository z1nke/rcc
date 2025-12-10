#include <cstdio>
#include <cstdlib>

#include "AST/AST.h"
#include "AST/ASTContext.h"
#include "Basic/Diagnostic.h"
#include "Basic/SourceManager.h"
#include "CodeGen/CodeGen.h"
#include "Lex/Lexer.h"
#include "Parse/Parser.h"

using namespace rcc;

int main(int Argc, char **Argv) {
  SourceManager SM;
  Diagnostic Diag(SM);

  if (Argc != 2) {
    Diag.fatal("%s: invalid number of arguments", Argv[0]);
    return 1;
  }

  Lexer TheLexer(Diag);
  SM.setStart(Argv[1]);
  auto TokList = TheLexer.tokenize();
  const Token *Tok = TokList.get();
  if (!Tok)
    Diag.fatal("tokenize failed");

  ASTContext Ctx(Diag);
  Parser P(std::move(TokList), Ctx);
  Stmt *S = P.parse();
  CodeGen CG(Diag);
  CG.codegen(S);
  return 0;
}