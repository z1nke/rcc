#ifndef RCC_PARSE_PARSER_H
#define RCC_PARSE_PARSER_H

#include "Lex/Token.h"

#include <vector>

namespace rcc {

class Stmt;
class Expr;
class Decl;
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

  FunctionDecl *parse();

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
  Expr *parseUnaryOperator();
  Expr *parsePrimaryExpr();
  Expr *parseParenExpr();

private:
  void parseDeclSpec(DeclSpec &DS);
  std::vector<Decl *> parseInitDeclaratorList(DeclSpec &DS);
  Decl *parseInitDeclarator(DeclSpec &DS);
  void parseDeclarator(Declarator &D);

private:
  template <auto ParseOperand, Token::TokenKind... TKS>
  Expr *parseBinaryOperator();

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