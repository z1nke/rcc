#ifndef RCC_PARSE_PARSER_H
#define RCC_PARSE_PARSER_H

#include "AST/Type.h"
#include "Lex/Token.h"

#include <vector>

namespace rcc {

class Stmt;
class Expr;
class Lexer;
class FunctionDecl;
class VarDecl;
class ASTContext;
class Sema;
class SourceManager;
class Diagnostic;

class Parser {
public:
  Parser(Token *CurTok, ASTContext &Ctx, Sema &S, SourceManager &SM);

  ~Parser();

  FunctionDecl *parse();

private:
  Stmt *parseStmt();
  Stmt *parseNullStmt();
  Stmt *parseReturnStmt();
  Stmt *parseCompoundStmt();
  Stmt *parseIfStmt();
  Stmt *parseForStmt();
  Stmt *parseWhileStmt();
  Stmt *parseExprStmt();
  Expr *parseExpr();
  Expr *parseAssign();
  Expr *parseEqualityExpr();
  Expr *parseRalationalExpr();
  Expr *parseAddExpr();
  Expr *parseMulExpr();
  Expr *parseUnaryOperator();
  Expr *parsePrimaryExpr();
  Expr *parseParenExpr();

private:
  template <auto ParseOperand, Token::TokenKind... TKS>
  Expr *parseBinaryOperator();

  VarDecl *findVar(std::string_view Ident);
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
  std::vector<VarDecl *> LocalVars;
};

} // namespace rcc

#endif