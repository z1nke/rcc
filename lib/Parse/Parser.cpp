#include "Parse/Parser.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/Casting.h"
#include "Basic/SourceManager.h"
#include "Basic/Unreachable.h"

namespace rcc {

Parser::Parser(Token *CurTok, ASTContext &Ctx, SourceManager &SM)
    : CurTok(CurTok), Ctx(Ctx), SM(SM), Diag(Ctx.getDiagnostic()) {}

Parser::~Parser() = default;

// expr eof
FunctionDecl *Parser::parse() {
  Stmt Head(Stmt::NoStmtKind);
  Stmt *CurStmt = &Head;

  auto BegLoc = SM.createBeginLocation(CurTok);
  while (CurTok->isNot(Token::TK_EOF)) {
    CurStmt->setNext(parseStmt());
    CurStmt = CurStmt->getNext();
  }

  auto EndLoc = CurStmt->getEndLoc();
  auto *FD = FunctionDecl::create(Ctx, BegLoc, EndLoc, Head.getNext());
  FD->setLocalVars(std::move(LocalVars));
  return FD;
}

// stmt: return-stmt
//     | compound-stmt
//     | if-stmt
//     | null-stmt
//     | expr-stmt
Stmt *Parser::parseStmt() {
  switch (CurTok->getKind()) {
  case Token::TK_Semicolon:
    return parseNullStmt();
  case Token::TK_Return:
    return parseReturnStmt();
  case Token::TK_LBrace:
    return parseCompoundStmt();
  case Token::TK_If:
    return parseIfStmt();
  case Token::TK_For:
    return parseForStmt();
  case Token::TK_While:
    return parseWhileStmt();
  default:
    break;
  }

  return parseExprStmt();
}

// null-stmt: ';'
Stmt *Parser::parseNullStmt() {
  assert(CurTok->is(Token::TK_Semicolon));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  return NullStmt::create(Ctx, BegLoc, BegLoc.getLocWithOffset(1));
}

// return-stmt: 'return' expr ';'
Stmt *Parser::parseReturnStmt() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  assert(CurTok->is(Token::TK_Return));
  skip();
  Expr *E = parseExpr();
  auto EndLoc = SM.createBeginLocation(CurTok + 1);
  skip(Token::TK_Semicolon);
  auto *RetStmt = ReturnStmt::create(Ctx, BegLoc, EndLoc, E);
  return RetStmt;
}

// compound-stmt: '{' stmt* '}'
Stmt *Parser::parseCompoundStmt() {
  assert(CurTok->is(Token::TK_LBrace));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  Stmt Head(Stmt::NoStmtKind);
  Stmt *CurStmt = &Head;
  while (CurTok->isNot(Token::TK_RBRace)) {
    CurStmt->setNext(parseStmt());
    CurStmt = CurStmt->getNext();
  }

  auto EndLoc = SM.createBeginLocation(CurTok + 1);
  skip(Token::TK_RBRace);
  return CompoundStmt::create(Ctx, BegLoc, EndLoc, Head.getNext());
}

// if-stmt: 'if' '(' expr ')' stmt { 'else' stmt }
Stmt *Parser::parseIfStmt() {
  assert(CurTok->is(Token::TK_If));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  skip(Token::TK_LParen);
  Expr *Cond = parseExpr();
  skip(Token::TK_RParen);
  Stmt *Then = parseStmt();
  Stmt *Else = nullptr;
  if (tryConsume(Token::TK_Else))
    Else = parseStmt();

  auto EndLoc = Else ? Else->getEndLoc() : Then->getEndLoc();
  return IfStmt::create(Ctx, BegLoc, EndLoc, Cond, Then, Else);
}

// for-stmt: 'for' '(' { init-stmt } { cond-expr } ';' { inc-expr } ')' stmt
Stmt *Parser::parseForStmt() {
  assert(CurTok->is(Token::TK_For));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  skip(Token::TK_LParen);
  Stmt *Init = parseStmt();
  if (isa<NullStmt>(Init))
    Init = nullptr;

  Expr *Cond = nullptr;
  if (CurTok->isNot(Token::TK_Semicolon))
    Cond = parseExpr();
  skip(Token::TK_Semicolon);

  Expr *Inc = nullptr;
  if (CurTok->isNot(Token::TK_RParen))
    Inc = parseExpr();
  skip(Token::TK_RParen);

  Stmt *Body = parseStmt();
  return ForStmt::create(Ctx, BegLoc, Body->getEndLoc(), Init, Cond, Inc, Body);
}

// while-stmt: 'while' '(' cond-expr ')' stmt
Stmt *Parser::parseWhileStmt() {
  assert(CurTok->is(Token::TK_While));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  skip(Token::TK_LParen);
  Expr *Cond = parseExpr();
  skip(Token::TK_RParen);
  Stmt *Body = parseStmt();
  return WhileStmt::create(Ctx, BegLoc, Body->getEndLoc(), Cond, Body);
}

// expr-stmt: expr ';'
Stmt *Parser::parseExprStmt() {
  Expr *E = parseExpr();
  skip(Token::TK_Semicolon);
  return E;
}

// expr: assign-expr
Expr *Parser::parseExpr() { return parseAssign(); }

// assign-expr: equality-expr { '=' assign-expr }
Expr *Parser::parseAssign() {
  Expr *LHS = parseEqualityExpr();
  auto OpLoc = SM.createBeginLocation(CurTok);
  if (tryConsume(Token::TK_Equal)) {
    Expr *RHS = parseAssign();
    LHS = BinaryOperator::create(Ctx, LHS->getBeginLoc(), RHS->getEndLoc(),
                                 OpLoc, LHS, RHS, BinaryOperator::BO_Assign);
  }
  return LHS;
}

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
  case Token::TK_Amp:
    return UnaryOperator::UO_Addrof;
  case Token::TK_Star:
    return UnaryOperator::UO_Deref;
  default:
    RCC_UNREACHABLE("Unknown binary operator");
  }
}

// unary-expr = ('+' | '-' | '*' | '&' ) (unary-expr | primary-expr)
Expr *Parser::parseUnaryExpr() {
  if (CurTok->isOneOf(Token::TK_Plus, Token::TK_Minus, Token::TK_Star,
                      Token::TK_Amp)) {
    auto Op = getUnaryOpcode(CurTok->getKind());
    auto BegLoc = SM.createBeginLocation(CurTok);
    CurTok = CurTok->getNext();
    Expr *SubExpr = parseUnaryExpr();
    auto EndLoc = SubExpr->getEndLoc();
    return UnaryOperator::create(Ctx, BegLoc, EndLoc, SubExpr, Op);
  }

  return parsePrimaryExpr();
}

// primary-expr: paren-expr | ident | num
Expr *Parser::parsePrimaryExpr() {
  if (CurTok->is(Token::TK_LParen))
    return parseParenExpr();

  if (CurTok->is(Token::TK_Num)) {
    auto Val = CurTok->getVal();
    auto BegLoc = SM.createBeginLocation(CurTok);
    auto EndLoc = SM.createEndLocation(CurTok);
    CurTok = CurTok->getNext();
    return IntergerLiteral::create(Ctx, BegLoc, EndLoc, Val);
  }

  if (CurTok->is(Token::TK_Ident)) {
    std::string_view Ident = CurTok->getIdentifer();
    auto BegLoc = SM.createBeginLocation(CurTok);
    auto EndLoc = SM.createEndLocation(CurTok);
    CurTok = CurTok->getNext();
    VarDecl *Var = findVar(Ident);
    if (!Var) {
      // FIXME: Invalid location.
      SourceLocation Loc;
      VarDecl *NewVar = VarDecl::create(Ctx, Loc, Loc, std::string(Ident));
      LocalVars.push_back(NewVar);
      return DeclRefExpr::create(Ctx, BegLoc, EndLoc, NewVar);
    }
    return DeclRefExpr::create(Ctx, BegLoc, EndLoc, Var);
  }

  Diag.fatalAt(CurTok->getLoc(), "expect a primary expression");
  return nullptr;
}

// paren-expr = '(' expr ')'
Expr *Parser::parseParenExpr() {
  assert(CurTok->is(Token::TK_LParen));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  Expr *E = parseExpr();
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_RParen);
  return ParenExpr::create(Ctx, BegLoc, EndLoc, E);
}

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

template <auto ParseOperand, Token::TokenKind... Tks>
Expr *Parser::parseBinaryOperator() {
  Expr *LHS = (this->*ParseOperand)();
  while (true) {
    if (CurTok->isOneOf(Tks...)) {
      auto OpLoc = SM.createBeginLocation(CurTok);
      auto Op = getBinaryOpcode(CurTok->getKind());
      CurTok = CurTok->getNext();
      Expr *RHS = (this->*ParseOperand)();
      LHS = BinaryOperator::create(Ctx, LHS->getBeginLoc(), RHS->getEndLoc(),
                                   OpLoc, LHS, RHS, Op);
      continue;
    }

    return LHS;
  }

  return nullptr;
}

VarDecl *Parser::findVar(std::string_view Ident) {
  for (VarDecl *Var : LocalVars) {
    if (Var->getName() == Ident)
      return Var;
  }

  return nullptr;
}

void Parser::expect(Token::TokenKind Kind, const char *Prompt) {
  if (CurTok->isNot(Kind))
    Diag.fatalAt(CurTok->getLoc(), "expect '%s'", Prompt);
}

void Parser::skip(Token::TokenKind Kind) {
  expect(Kind, Token::getKindStr(Kind));
  CurTok = CurTok->getNext();
}

void Parser::skip() { CurTok = CurTok->getNext(); }

bool Parser::tryConsume(Token::TokenKind Kind) {
  if (CurTok->is(Kind)) {
    CurTok = CurTok->getNext();
    return true;
  }

  return false;
}

} // namespace rcc