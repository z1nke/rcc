#include "Parse/Parser.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/Casting.h"
#include "Basic/SourceManager.h"
#include "Basic/Unreachable.h"
#include "Sema/DeclSpec.h"
#include "Sema/Sema.h"

namespace rcc {

Parser::Parser(Token *CurTok, ASTContext &Ctx, Sema &S, SourceManager &SM)
    : CurTok(CurTok), Ctx(Ctx), S(S), SM(SM), Diag(Ctx.getDiagnostic()) {}

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
  Stmt *Body = Head.getNext();
  return S.actOnFunctionDecl(Ctx, BegLoc, EndLoc, "main", Body);
}

// stmt: return-stmt
//     | compound-stmt
//     | if-stmt
//     | for-stmt
//     | while-stmt
//     | null-stmt
//     | decl-stmt
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
  case Token::TK_Int:
    return parseDeclStmt();
  default:
    break;
  }

  return parseExprStmt();
}

// null-stmt: ';'
Stmt *Parser::parseNullStmt() {
  assert(CurTok->is(Token::TK_Semicolon));
  auto SemiLoc = SM.createBeginLocation(CurTok);
  skip();
  return S.actOnNullStmt(SemiLoc);
}

// return-stmt: 'return' expr ';'
Stmt *Parser::parseReturnStmt() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  assert(CurTok->is(Token::TK_Return));
  skip();
  Expr *E = parseExpr();
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Semicolon);
  return S.actOnReturnStmt(BegLoc, EndLoc, E);
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
  return S.actOnCompoundStmt(BegLoc, EndLoc, Head.getNext());
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

  return S.actOnIfStmt(BegLoc, Cond, Then, Else);
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
  return S.actOnForStmt(BegLoc, Init, Cond, Inc, Body);
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
  return S.actOnWhileStmt(Ctx, BegLoc, Cond, Body);
}

// decl-stmt: declspec { init-declarator-list } ';'
Stmt *Parser::parseDeclStmt() {
  DeclSpec DS;
  auto BegLoc = SM.createBeginLocation(CurTok);
  parseDeclSpec(DS);
  std::vector<Decl *> Decls = parseInitDeclaratorList(DS);
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Semicolon);
  return S.actOnDeclStmt(Ctx, BegLoc, EndLoc, std::move(Decls));
}

// declspec: 'int'
void Parser::parseDeclSpec(DeclSpec &DS) {
  auto TyLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Int);
  DS.setTypeSpecType(DeclSpec::TST_Int, TyLoc);
}

// init-declarator-list: init-declarator { ',' init-declarator }*
std::vector<Decl *> Parser::parseInitDeclaratorList(DeclSpec &DS) {
  std::vector<Decl *> Decls;
  Decls.push_back(parseInitDeclarator(DS));
  while (tryConsume(Token::TK_Comma))
    Decls.push_back(parseInitDeclarator(DS));

  return Decls;
}

// init-declarator: declarator { '=' expr }
Decl *Parser::parseInitDeclarator(DeclSpec &DS) {
  Declarator D(DS);
  parseDeclarator(D);
  Decl *DR = S.actOnDeclarator(D);
  auto *Var = dyn_cast<VarDecl>(DR);
  if (!Var)
    Diag.fatalAt(D.getLocation(), "expect variable declarator");

  if (tryConsume(Token::TK_Equal)) {
    Expr *E = parseExpr();
    Var->setInit(E);
    Var->setEndLoc(E->getEndLoc());
    return Var;
  }

  return Var;
}

// declarator: '*'* ident
void Parser::parseDeclarator(Declarator &D) {
  while (tryConsume(Token::TK_Star)) {
    auto Chunk = DeclaratorChunk::createPointer();
    D.addDeclChunk(Chunk);
  }

  D.setLocation(SM.createBeginLocation(CurTok));
  if (!CurTok->is(Token::TK_Ident)) {
    Diag.fatalAt(CurTok->getLoc(), "expect identifier");
    return;
  }

  D.setIdent(std::string(CurTok->getIdentifer()));
  D.setEndLoc(SM.createEndLocation(CurTok));
  skip();
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
    LHS = S.actOnBinaryOperator(OpLoc, LHS, RHS, BinaryOperator::BO_Assign);
  }
  return LHS;
}

// equality-expr: relational-expr { ('==' | '!=') relational-expr }
Expr *Parser::parseEqualityExpr() {
  return parseBinaryOperator<&Parser::parseRelationalExpr, Token::TK_EqualEqual,
                             Token::TK_NotEqual>();
}

// relational-expr: add-expr { ('<' | '<=' | '>' | '>=') add-expr }
Expr *Parser::parseRelationalExpr() {
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
  return parseBinaryOperator<&Parser::parseUnaryOperator, Token::TK_Star,
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
Expr *Parser::parseUnaryOperator() {
  if (CurTok->isOneOf(Token::TK_Plus, Token::TK_Minus, Token::TK_Star,
                      Token::TK_Amp)) {
    auto Op = getUnaryOpcode(CurTok->getKind());
    auto OpLoc = SM.createBeginLocation(CurTok);
    CurTok = CurTok->getNext();
    Expr *SubExpr = parseUnaryOperator();
    return S.actOnUnaryOperator(OpLoc, SubExpr, Op);
  }

  return parsePrimaryExpr();
}

// primary-expr: paren-expr | decl-ref-expr | call-expr | num
// decl-ref-expr: ident
// call-expr: ident '(' ')'
Expr *Parser::parsePrimaryExpr() {
  if (CurTok->is(Token::TK_LParen))
    return parseParenExpr();

  if (CurTok->is(Token::TK_Num)) {
    auto Val = CurTok->getVal();
    auto BegLoc = SM.createBeginLocation(CurTok);
    auto EndLoc = SM.createEndLocation(CurTok);
    CurTok = CurTok->getNext();
    return IntegerLiteral::create(Ctx, BegLoc, EndLoc, Ctx.IntTy, Val);
  }

  if (CurTok->is(Token::TK_Ident)) {
    std::string_view Ident = CurTok->getIdentifer();
    auto IdentBegLoc = SM.createBeginLocation(CurTok);
    auto IdentEndLoc = SM.createEndLocation(CurTok);
    skip();
    if (CurTok->is(Token::TK_LParen)) {
      skip();
      SourceLocation EndLoc = SM.createBeginLocation(CurTok);
      skip(Token::TK_RParen);
      return S.actOnCallExpr(IdentBegLoc, IdentEndLoc, EndLoc, Ident, {});
    }

    return S.actOnDeclRefExpr(IdentBegLoc, IdentEndLoc, Ident);
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
  return S.actOnParenExpr(BegLoc, EndLoc, E);
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
      LHS = S.actOnBinaryOperator(OpLoc, LHS, RHS, Op);
      continue;
    }

    return LHS;
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