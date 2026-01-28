#ifndef RCC_PARSE_PARSER_H
#define RCC_PARSE_PARSER_H

#include "Basic/SourceLocation.h"
#include "Lex/Token.h"
#include "Sema/Scope.h"

#include <vector>

namespace rcc {

class Stmt;
class Expr;
class Decl;
class TranslationUnitDecl;
class FunctionDecl;
class VarDecl;

class ASTContext;
class Lexer;
class Sema;
class SourceManager;
class Diagnostic;
class DeclSpec;
class Declarator;

class Parser {
public:
  Parser(Token *CurTok, ASTContext &Ctx, Sema &S, SourceManager &SM);

  ~Parser();

  TranslationUnitDecl *parse();

private:
  void parseGlobalDecl(TranslationUnitDecl *TU);
  FunctionDecl *parseFunctionDecl();
  FunctionDecl *parseFunctionBody(SourceLocation BegLoc, FunctionDecl *Func);
  std::vector<VarDecl *> parseGlobalVarDecl(SourceLocation BegLoc, DeclSpec &DS,
                                            VarDecl *FirstVar);

private:
  Stmt *parseStmt();
  Stmt *parseNullStmt();
  Stmt *parseReturnStmt();
  Stmt *parseCompoundStmt();
  Stmt *parseIfStmt();
  Stmt *parseForStmt();
  Stmt *parseWhileStmt();
  Stmt *parseDeclStmt();
  Stmt *parseExprStmt();
  Expr *parseExpr();
  Expr *parseAssign();
  Expr *parseEqualityExpr();
  Expr *parseRelationalExpr();
  Expr *parseAddExpr();
  Expr *parseMulExpr();
  Expr *parseUnaryExpr();
  Expr *parsePrimaryExpr();
  Expr *parseParenOrStmtExpr();
  Expr *parseCallExpr(std::string_view Ident, SourceLocation IdentBegLoc,
                      SourceLocation IdentEndLoc);
  Expr *parsePostfixExpr();

public:
  Scope *getCurrScope() const;
  void enterScope(unsigned ScopeFlags);
  void exitScope();

private:
  void parseDeclSpec(DeclSpec &DS);
  std::vector<Decl *> parseInitDeclaratorList(DeclSpec &DS);
  Decl *parseInitDeclarator(DeclSpec &DS);
  void parseVarInit(VarDecl *Var);
  void parseDeclarator(Declarator &D);
  void parseDirectDeclarator(Declarator &D);

private:
  template <auto ParseOperand, Token::TokenKind... TKS> Expr *parseBinaryExpr();

  Token::TokenKind getKeyword(std::string_view Ident) const;

private:
  void expect(Token::TokenKind Kind, const char *Prompt);
  void skip(Token::TokenKind Kind);
  void skip();
  bool tryConsume(Token::TokenKind Kind);

private:
  Token *CurTok;
  ASTContext &Ctx;
  Sema &S;
  SourceManager &SM;
  Diagnostic &Diag;
};

} // namespace rcc

#endif