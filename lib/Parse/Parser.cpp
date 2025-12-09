#include "Parse/Parser.h"
#include "AST/AST.h"
#include "AST/ASTContext.h"
#include "Basic/Unreachable.h"

namespace rcc {

Parser::Parser(std::unique_ptr<Token> CurTok, ASTContext &Ctx)
    : CurTok(std::move(CurTok)), Ctx(Ctx), Diag(Ctx.getDiagnostic()) {}

Parser::~Parser() = default;

static BinaryOperator::Opcode getBinaryOpcode(Token::TokenKind Kind) {
  switch (Kind) {
  case Token::TK_Plus:
    return BinaryOperator::BO_Add;
  case Token::TK_Minus:
    return BinaryOperator::BO_Sub;
  case Token::TK_Mul:
    return BinaryOperator::BO_Mul;
  case Token::TK_Div:
    return BinaryOperator::BO_Div;
  default:
    RCC_UNREACHABLE("Unknown binary operator");
  }
}

// expr eof
Expr *Parser::parse() {
  Expr *E = parseExpression();
  if (CurTok->isNot(Token::TK_EOF))
    Diag.fatalAt(CurTok->getLoc(), "extra token");
  return E;
}

// expr: mul-expr { ('+' | '-' mul-expr) }
Expr *Parser::parseExpression() {
  Expr *LHS = parseMulExpression();
  while (true) {
    if (CurTok->isOneOf(Token::TK_Plus, Token::TK_Minus)) {
      auto Op = getBinaryOpcode(CurTok->getKind());
      CurTok = CurTok->takeNext(); // Eat '+' or '-'.
      Expr *RHS = parseMulExpression();
      LHS = BinaryOperator::create(Ctx, LHS, RHS, Op);
      continue;
    }

    return LHS;
  }
  return nullptr;
}

// mul-expr: unary-expr { ('*' | '/') unary-expr }
Expr *Parser::parseMulExpression() {
  Expr *LHS = parseUnaryExpression();
  while (true) {
    if (CurTok->isOneOf(Token::TK_Mul, Token::TK_Div)) {
      auto Op = getBinaryOpcode(CurTok->getKind());
      CurTok = CurTok->takeNext(); // Eat '*' or '/'.
      Expr *RHS = parseUnaryExpression();
      LHS = BinaryOperator::create(Ctx, LHS, RHS, Op);
      continue;
    }

    return LHS;
  }
  return nullptr;
}

static UnaryOperator::Opcode getUnaryOpcode(Token::TokenKind Kind) {
  switch (Kind) {
  case Token::TK_Plus:
    return UnaryOperator::UO_Plus;
  case Token::TK_Minus:
    return UnaryOperator::UO_Minus;
  default:
    RCC_UNREACHABLE("Unknown binary operator");
  }
}

// unary-expr = ('+' | '-') (unary-expr | primary-expr)
Expr *Parser::parseUnaryExpression() {
  if (CurTok->isOneOf(Token::TK_Plus, Token::TK_Minus)) {
    auto Op = getUnaryOpcode(CurTok->getKind());
    CurTok = CurTok->takeNext();
    Expr *SubExpr = parseUnaryExpression();
    return UnaryOperator::create(Ctx, SubExpr, Op);
  }

  return parsePrimaryExpression();
}

// primary-expr: paren-expr | num
Expr *Parser::parsePrimaryExpression() {
  if (CurTok->is(Token::TK_LParen))
    return parseParenExpression();

  if (CurTok->is(Token::TK_Num)) {
    auto Val = CurTok->getVal();
    CurTok = CurTok->takeNext(); // Eat the number.
    return IntergerLiteral::create(Ctx, Val);
  }

  Diag.fatalAt(CurTok->getLoc(), "expect a primary expression");
  return nullptr;
}

// paren-expr = '(' expr ')'
Expr *Parser::parseParenExpression() {
  if (CurTok->isNot(Token::TK_LParen))
    Diag.fatalAt(CurTok->getLoc(), "expect '('");

  CurTok = CurTok->takeNext(); // Eat '('.
  Expr *E = parseExpression();

  if (CurTok->isNot(Token::TK_RParen))
    Diag.fatalAt(CurTok->getLoc(), "expect ')'");
  CurTok = CurTok->takeNext(); // Eat ')'.
  return E;
}

} // namespace rcc