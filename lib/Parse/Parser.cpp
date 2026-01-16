#include "Parse/Parser.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/Casting.h"
#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"
#include "Basic/Unreachable.h"
#include "Lex/Token.h"
#include "Sema/DeclSpec.h"
#include "Sema/Sema.h"
#include <vector>

namespace rcc {

Parser::Parser(Token *CurTok, ASTContext &Ctx, Sema &S, SourceManager &SM)
    : CurTok(CurTok), Ctx(Ctx), S(S), SM(SM), Diag(Ctx.getDiagnostic()) {}

Parser::~Parser() = default;

// program: (function-decl | var-decl) EOF
TranslationUnitDecl *Parser::parse() {
  auto *TU = TranslationUnitDecl::create(Ctx);
  while (CurTok->isNot(Token::TK_EOF))
    parseGlobalDecl(TU);

  return TU;
}

void Parser::parseGlobalDecl(TranslationUnitDecl *TU) {
  auto BegLoc = SM.createBeginLocation(CurTok);
  DeclSpec DS;
  parseDeclSpec(DS);
  Declarator D(DS);
  parseDeclarator(D);
  Decl *FirstDecl = S.actOnDeclarator(D);
  // function-decl: function-definition
  // TODO:        | function-declaration
  if (auto *Func = dyn_cast<FunctionDecl>(FirstDecl)) {
    TU->addDecl(parseFunctionBody(BegLoc, Func));
    return;
  }

  if (auto *Var = dyn_cast<VarDecl>(FirstDecl)) {
    std::vector<VarDecl *> Vars = parseGlobalVarDecl(BegLoc, DS, Var);
    for (VarDecl *Var : Vars)
      TU->addDecl(Var);
    return;
  }

  Diag.fatalAt(BegLoc, "Unknown global declaration");
}

FunctionDecl *Parser::parseFunctionBody(SourceLocation BegLoc,
                                        FunctionDecl *FD) {
  if (CurTok->is(Token::TK_LBrace)) {
    Stmt *Body = parseCompoundStmt();
    FD->setBody(Body);
    FD->setEndLoc(Body->getEndLoc());
  }

  S.complete(FD);
  return FD;
}

// var-decl: declspec { init-declarator-list } ';'
std::vector<VarDecl *> Parser::parseGlobalVarDecl(SourceLocation BegLoc,
                                                  DeclSpec &DS,
                                                  VarDecl *FirstVar) {
  std::vector<VarDecl *> Vars;
  parseVarInit(FirstVar);
  Vars.push_back(FirstVar);
  while (tryConsume(Token::TK_Comma)) {
    auto *Var = dyn_cast<VarDecl>(parseInitDeclarator(DS));
    if (!Var)
      Diag.fatalAt(BegLoc, "expect variable declaration");

    Vars.push_back(Var);
  }

  skip(Token::TK_Semicolon);
  return Vars;
}

FunctionDecl *Parser::parseFunctionDecl() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  DeclSpec DS;
  parseDeclSpec(DS);
  Declarator D(DS);
  D.setLocation(SM.createBeginLocation(CurTok));
  parseDeclarator(D);
  auto *FD = dyn_cast<FunctionDecl>(S.actOnDeclarator(D));
  if (!FD)
    Diag.fatalAt(BegLoc, "expected function declaration");

  return parseFunctionBody(BegLoc, FD);
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
  std::vector<Stmt *> Stmts;
  while (CurTok->isNot(Token::TK_RBRace))
    Stmts.push_back(parseStmt());

  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_RBRace);
  return S.actOnCompoundStmt(BegLoc, EndLoc, std::move(Stmts));
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

  parseVarInit(Var);
  return Var;
}

void Parser::parseVarInit(VarDecl *Var) {
  if (tryConsume(Token::TK_Equal)) {
    Expr *E = parseExpr();
    Var->setInit(E);
    Var->setEndLoc(E->getEndLoc());
  }
}

// declarator: '*'* direct-declarator
void Parser::parseDeclarator(Declarator &D) {
  while (tryConsume(Token::TK_Star)) {
    auto Chunk = DeclaratorChunk::createPointer();
    D.addDeclChunk(Chunk);
  }

  parseDirectDeclarator(D);
}

// direct-declarator: ident
//                  | direct-declarator '(' { params } ')'
//                  | direct-declarator '[' { assign-expr } ']'
void Parser::parseDirectDeclarator(Declarator &D) {
  if (!CurTok->is(Token::TK_Ident)) {
    Diag.fatalAt(CurTok->getLoc(), "expect identifier");
    return;
  }

  D.setIdent(std::string(CurTok->getIdentifer()));
  D.setEndLoc(SM.createEndLocation(CurTok));
  skip();

  if (tryConsume(Token::TK_LParen)) {
    // Try parse function declarator.
    // Record function information in DeclaratorChunk.
    // params: param { ',' param }*
    // param: declspec declarator
    unsigned Idx = 0;
    while (CurTok->isNot(Token::TK_RParen)) {
      if (Idx > 0)
        skip(Token::TK_Comma);

      DeclSpec ParamDS;
      parseDeclSpec(ParamDS);
      Declarator ParamD(ParamDS);
      parseDeclarator(ParamD);
      (void)S.actOnParamVarDecl(ParamD, Idx);
      ++Idx;
    }

    D.setEndLoc(SM.createEndLocation(CurTok));
    skip();
    D.addDeclChunk(DeclaratorChunk::createFunction());
    return;
  }

  while (tryConsume(Token::TK_LSquare)) {
    // Try parse array declarator.
    Expr *LenExpr = parseAssign();
    D.setEndLoc(SM.createBeginLocation(CurTok));
    skip(Token::TK_RSquare);
    D.addDeclChunk(DeclaratorChunk::createArray(LenExpr));
  }
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
  return parseBinaryExpr<&Parser::parseRelationalExpr, Token::TK_EqualEqual,
                         Token::TK_NotEqual>();
}

// relational-expr: add-expr { ('<' | '<=' | '>' | '>=') add-expr }
Expr *Parser::parseRelationalExpr() {
  return parseBinaryExpr<&Parser::parseAddExpr, Token::TK_Less,
                         Token::TK_LessEqual, Token::TK_Greater,
                         Token::TK_GreaterEqual>();
}

// add-expr: mul-expr { ('+' | '-') mul-expr }
Expr *Parser::parseAddExpr() {
  return parseBinaryExpr<&Parser::parseMulExpr, Token::TK_Plus,
                         Token::TK_Minus>();
}

// mul-expr: unary-expr { ('*' | '/') unary-expr }
Expr *Parser::parseMulExpr() {
  return parseBinaryExpr<&Parser::parseUnaryExpr, Token::TK_Star,
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

// unary-expr: postfix-expr
//           | unary-operator unary-expr
//           | sizeof unary-expr
//           | sizeof '(' type-name ')'
Expr *Parser::parseUnaryExpr() {
  // unary-operator: '+' | '-' | '*' | '&'
  if (CurTok->isOneOf(Token::TK_Plus, Token::TK_Minus, Token::TK_Star,
                      Token::TK_Amp)) {
    auto Op = getUnaryOpcode(CurTok->getKind());
    auto OpLoc = SM.createBeginLocation(CurTok);
    skip();
    Expr *SubExpr = parseUnaryExpr();
    return S.actOnUnaryOperator(OpLoc, SubExpr, Op);
  }

  if (CurTok->is(Token::TK_Sizeof)) {
    auto BegLoc = SM.createBeginLocation(CurTok);
    skip();
    // FIXME: Support sizeof '(' type-name ')'
    Expr *Ex = parseUnaryExpr();
    return S.actOnUnaryExprOrTypeTraitExpr(BegLoc, Ex);
  }

  return parsePostfixExpr();
}

// postfix-expr: primary-expr
//             | postfix-expr '[' expr ']'
Expr *Parser::parsePostfixExpr() {
  Expr *LHS = parsePrimaryExpr();
  while (tryConsume(Token::TK_LSquare)) {
    Expr *RHS = parseExpr();
    auto EndLoc = SM.createBeginLocation(CurTok);
    skip(Token::TK_RSquare);
    LHS = S.actOnArraySubscriptExpr(EndLoc, LHS, RHS);
  }

  return LHS;
}

// primary-expr: paren-expr | decl-ref-expr | call-expr | num
// decl-ref-expr: ident
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
    if (tryConsume(Token::TK_LParen)) {
      return parseCallExpr(Ident, IdentBegLoc, IdentEndLoc);

      SourceLocation EndLoc = SM.createBeginLocation(CurTok);
      skip(Token::TK_RParen);
      return S.actOnCallExpr(IdentBegLoc, IdentEndLoc, EndLoc, Ident, {});
    }

    return S.actOnDeclRefExpr(IdentBegLoc, IdentEndLoc, Ident);
  }

  Diag.fatalAt(CurTok->getLoc(), "expect a primary expression");
  return nullptr;
}

// call-expr: ident '(' args ')'
// args: expr { ',' expr }*
Expr *Parser::parseCallExpr(std::string_view Ident, SourceLocation IdentBegLoc,
                            SourceLocation IdentEndLoc) {
  // Parsed ident '(' already.
  std::vector<Expr *> Args;
  while (CurTok->isNot(Token::TK_RParen)) {
    if (!Args.empty())
      skip(Token::TK_Comma);

    Args.push_back(parseExpr());
  }

  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_RParen);
  return S.actOnCallExpr(IdentBegLoc, IdentEndLoc, EndLoc, Ident,
                         std::move(Args));
}

// paren-expr: '(' expr ')'
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
Expr *Parser::parseBinaryExpr() {
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
    Diag.fatalAt(CurTok->getLoc(), "expect '{}'", Prompt);
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