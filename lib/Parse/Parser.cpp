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
  case Token::TK_Star:
    return BinaryOperator::BO_Mul;
  case Token::TK_Slash:
    return BinaryOperator::BO_Div;
  case Token::TK_EqualEqual:
    return BinaryOperator::BO_EQ;
  case Token::TK_NotEqual:
    return BinaryOperator::BO_NE;
  case Token::TK_Less:
    return BinaryOperator::BO_LT;
  case Token::TK_LessEqual:
    return BinaryOperator::BO_LE;
  case Token::TK_Greater:
    return BinaryOperator::BO_GT;
  case Token::TK_GreaterEqual:
    return BinaryOperator::BO_GE;
  default:
    RCC_UNREACHABLE("Unknown binary operator");
  }
}

// expr eof
Expr *Parser::parse() {
  Expr *E = parseExpr();
  if (CurTok->isNot(Token::TK_EOF))
    Diag.fatalAt(CurTok->getLoc(), "extra token");
  return E;
}

// expr: equality-expr
Expr *Parser::parseExpr() { return parseEqualityExpr(); }

// equality-expr: relational-expr { ('==' | '!=') relational-expr }
Expr *Parser::parseEqualityExpr() {
  return parseBinaryOperator<&Parser::parseRalationalExpr, Token::TK_EqualEqual,
                             Token::TK_NotEqual>();
}

// relational-expr: add-expr { ('<' | '<=' | '>' | '>=') add-expr }
Expr *Parser::parseRalationalExpr() {
  return parseBinaryOperator<&Parser::parseAddExpr, Token::TK_Less,
                             Token::TK_LessEqual, Token::TK_Greater,
                             Token::TK_GreaterEqual>();
}

// add-expr: mul-expr { ('+' | '-') mul-expr }
Expr *Parser::parseAddExpr() {
  return parseBinaryOperator<&Parser::parseMulExpr, Token::TK_Plus,
                             Token::TK_Minus>();
}

// mul-expr: unary-expr { ('*' | '/') unary-expr }
Expr *Parser::parseMulExpr() {
  return parseBinaryOperator<&Parser::parseUnaryExpr, Token::TK_Star,
                             Token::TK_Slash>();
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
Expr *Parser::parseUnaryExpr() {
  if (CurTok->isOneOf(Token::TK_Plus, Token::TK_Minus)) {
    auto Op = getUnaryOpcode(CurTok->getKind());
    CurTok = CurTok->takeNext();
    Expr *SubExpr = parseUnaryExpr();
    return UnaryOperator::create(Ctx, SubExpr, Op);
  }

  return parsePrimaryExpr();
}

// primary-expr: paren-expr | num
Expr *Parser::parsePrimaryExpr() {
  if (CurTok->is(Token::TK_LParen))
    return parseParenExpr();

  if (CurTok->is(Token::TK_Num)) {
    auto Val = CurTok->getVal();
    CurTok = CurTok->takeNext(); // Eat the number.
    return IntergerLiteral::create(Ctx, Val);
  }

  Diag.fatalAt(CurTok->getLoc(), "expect a primary expression");
  return nullptr;
}

// paren-expr = '(' expr ')'
Expr *Parser::parseParenExpr() {
  if (CurTok->isNot(Token::TK_LParen))
    Diag.fatalAt(CurTok->getLoc(), "expect '('");

  CurTok = CurTok->takeNext(); // Eat '('.
  Expr *E = parseExpr();

  if (CurTok->isNot(Token::TK_RParen))
    Diag.fatalAt(CurTok->getLoc(), "expect ')'");
  CurTok = CurTok->takeNext(); // Eat ')'.
  return E;
}

template <auto ParseOperand, Token::TokenKind... Tks>
Expr *Parser::parseBinaryOperator() {
  Expr *LHS = (this->*ParseOperand)();
  while (true) {
    if (CurTok->isOneOf(Tks...)) {
      auto Op = getBinaryOpcode(CurTok->getKind());
      CurTok = CurTok->takeNext();
      Expr *RHS = (this->*ParseOperand)();
      LHS = BinaryOperator::create(Ctx, LHS, RHS, Op);
      continue;
    }

    return LHS;
  }

  return nullptr;
}

} // namespace rcc