#ifndef RCC_PARSE_PARSER_H
#define RCC_PARSE_PARSER_H

#include "Lex/Token.h"

namespace rcc {

class Expr;
class Lexer;
class ASTContext;
class Diagnostic;

class Parser {
public:
  Parser(std::unique_ptr<Token> CurTok, ASTContext &Ctx);

  ~Parser();

  Expr *parse();

  Expr *parseExpression();

  Expr *parseMulExpression();

  Expr *parsePrimaryExpression();

  Expr *parseParenExpression();

private:
  std::unique_ptr<Token> CurTok;
  ASTContext &Ctx;
  Diagnostic &Diag;
};

} // namespace rcc

#endif