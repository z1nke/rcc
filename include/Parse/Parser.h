#ifndef RCC_PARSE_PARSER_H
#define RCC_PARSE_PARSER_H

#include "Lex/Token.h"

#include <vector>

namespace rcc {

class Stmt;
class Expr;
class Lexer;
class FunctionDecl;
class VarDecl;
class ASTContext;
class Diagnostic;

class Parser {
public:
  Parser(std::unique_ptr<Token> CurTok, ASTContext &Ctx);

  ~Parser();

  FunctionDecl *parse();

private:
  Stmt *parseStmt();
  Stmt *parseCompoundStmt();
  Stmt *parseExprStmt();
  Expr *parseExpr();
  Expr *parseAssign();
  Expr *parseEqualityExpr();
  Expr *parseRalationalExpr();
  Expr *parseAddExpr();
  Expr *parseMulExpr();
  Expr *parseUnaryExpr();
  Expr *parsePrimaryExpr();
  Expr *parseParenExpr();

private:
  template <auto ParseOperand, Token::TokenKind... TKS>
  Expr *parseBinaryOperator();

  VarDecl *findVar(std::string_view Ident);
  Token::TokenKind getKeyword(std::string_view Ident) const;

  void expect(Token::TokenKind Kind, const char *Prompt);
  void skip(Token::TokenKind Kind, const char *Prompt);
  bool tryConsume(Token::TokenKind Kind);

private:
  std::unique_ptr<Token> CurTok;
  ASTContext &Ctx;
  Diagnostic &Diag;
  std::vector<VarDecl *> LocalVars;
};

} // namespace rcc

#endif