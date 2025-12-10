#ifndef RCC_PARSE_PARSER_H
#define RCC_PARSE_PARSER_H

#include "Lex/Token.h"

namespace rcc {

class Stmt;
class Expr;
class Lexer;
class ASTContext;
class Diagnostic;

class Parser {
public:
  Parser(std::unique_ptr<Token> CurTok, ASTContext &Ctx);

  ~Parser();

  Stmt *parse();

private:
  Stmt *parseStmt();
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

private:
  std::unique_ptr<Token> CurTok;
  ASTContext &Ctx;
  Diagnostic &Diag;
};

} // namespace rcc

#endif