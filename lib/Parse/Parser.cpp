#include "Parse/Parser.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/SourceLocation.h"
#include "Basic/SourceManager.h"
#include "Lex/Token.h"
#include "Sema/DeclSpec.h"
#include "Sema/Sema.h"
#include "Support/Casting.h"
#include "Support/Unreachable.h"

#include <vector>

namespace rcc {

Parser::Parser(Token *CurTok, ASTContext &Ctx, Sema &S, SourceManager &SM)
    : CurTok(CurTok), Ctx(Ctx), S(S), SM(SM), Diag(Ctx.getDiagnostic()) {
  enterScope(Scope::DeclScope); // translation unit scope
}

Parser::~Parser() { exitScope(); }

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
  if (auto *Func = dynCast<FunctionDecl>(FirstDecl)) {
    TU->addDecl(parseFunctionBody(BegLoc, Func));
    return;
  }

  if (auto *Var = dynCast<VarDecl>(FirstDecl)) {
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
  if (getCurrScope()->getFlags() & Scope::FnScope)
    exitScope();
  return FD;
}

// var-decl: declspec { init-declarator-list } ';'
std::vector<VarDecl *> Parser::parseGlobalVarDecl(SourceLocation BegLoc,
                                                  DeclSpec &DS,
                                                  VarDecl *FirstVar) {
  std::vector<VarDecl *> Vars;
  parseVarInit(FirstVar);
  FirstVar->setGlobal(true);
  Vars.push_back(FirstVar);
  while (tryConsume(Token::TK_Comma)) {
    auto *Var = dynCast<VarDecl>(parseInitDeclarator(DS));
    if (!Var)
      Diag.fatalAt(BegLoc, "expect variable declaration");

    Var->setGlobal(true);
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
  auto *FD = dynCast<FunctionDecl>(S.actOnDeclarator(D));
  if (!FD)
    Diag.fatalAt(BegLoc, "expected function declaration");

  return parseFunctionBody(BegLoc, FD);
}

// struct-union-decl: struct-or-union '{' struct-decl-list '}'
//                  | struct-or-union ident
//                  | struct-or-union ident '{' struct-decl-list '}'
// struct-or-union: 'struct' | 'union'
RecordDecl *Parser::parseStructDecl() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Struct);
  RecordDecl *Struct = parseStructUnionDecl(BegLoc, DeclSpec::TST_Struct);
  if (!Struct)
    Diag.fatalAt(BegLoc, "expected struct declaration");

  std::size_t Offset = 0;
  std::size_t Align = 1;
  for (auto *Field : Struct->fields()) {
    std::size_t FieldAlign = Field->getType()->getAlign();
    Offset = alignTo(Offset, FieldAlign);
    Field->setOffset(Offset);
    Offset += Field->getType()->getSize();
    if (Align < FieldAlign)
      Align = FieldAlign;
  }

  std::size_t Size = alignTo(Offset, Align);
  QualType RT = Ctx.getRecordType(Struct, Size, Align);
  Struct->setTypeForDecl(RT.getTypePtr());
  return Struct;
}

RecordDecl *Parser::parseUnionDecl() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Union);
  RecordDecl *Union = parseStructUnionDecl(BegLoc, DeclSpec::TST_Union);
  if (!Union)
    Diag.fatalAt(BegLoc, "expected union declaration");

  std::size_t Size = 0;
  std::size_t Align = 1;
  for (auto *Field : Union->fields()) {
    std::size_t FieldAlign = Field->getType()->getAlign();
    std::size_t FieldSize = Field->getType()->getSize();
    if (Align < FieldAlign)
      Align = FieldAlign;
    if (Size < FieldSize)
      Size = FieldSize;
  }

  Size = alignTo(Size, Align);
  QualType RT = Ctx.getRecordType(Union, Size, Align);
  Union->setTypeForDecl(RT.getTypePtr());
  return Union;
}

RecordDecl *Parser::parseStructUnionDecl(SourceLocation BegLoc,
                                         unsigned TagKind) {
  auto Loc = SM.createBeginLocation(CurTok);
  std::string_view Ident;
  if (CurTok->is(Token::TK_Ident)) {
    Ident = CurTok->getIdentifer();
    skip();
  }

  if (!Ident.empty() && CurTok->isNot(Token::TK_LBrace)) {
    TagDecl *Tag = S.findTagDecl(Ident);
    if (!Tag)
      Diag.fatalAt(Loc, "unknown struct {} type", Ident);

    return dynCast<RecordDecl>(Tag);
  }

  if (tryConsume(Token::TK_LBrace)) {
    auto *Record = S.actOnRecordDecl(Loc, BegLoc, BegLoc, Ident, TagKind);
    enterScope(Scope::StructScope, Record);
    std::vector<FieldDecl *> Fields = parseFields();
    Record->setFields(std::move(Fields));
    exitScope();
    Record->setEndLoc(SM.createBeginLocation(CurTok));
    getCurrScope()->addTag(Record);
    skip(Token::TK_RBrace);
    return Record;
  }

  return nullptr;
}

std::vector<FieldDecl *> Parser::parseFields() {
  std::vector<FieldDecl *> Fields;
  while (CurTok->isNot(Token::TK_RBrace)) {
    DeclSpec DS;
    parseDeclSpec(DS);
    Fields.push_back(parseField(DS));
    while (tryConsume(Token::TK_Comma))
      Fields.push_back(parseField(DS));
    skip(Token::TK_Semicolon);
  }
  return Fields;
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
  case Token::TK_Char:
  case Token::TK_Short:
  case Token::TK_Int:
  case Token::TK_Long:
  case Token::TK_Struct:
  case Token::TK_Union:
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
  enterScope(Scope::CompoundScope);
  skip();
  std::vector<Stmt *> Stmts;
  while (CurTok->isNot(Token::TK_RBrace))
    Stmts.push_back(parseStmt());

  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_RBrace);
  exitScope();
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
  enterScope(Scope::BreakScope | Scope::ContinueScope | Scope::ControlScope);
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
  exitScope();
  return S.actOnForStmt(BegLoc, Init, Cond, Inc, Body);
}

// while-stmt: 'while' '(' cond-expr ')' stmt
Stmt *Parser::parseWhileStmt() {
  assert(CurTok->is(Token::TK_While));
  auto BegLoc = SM.createBeginLocation(CurTok);
  enterScope(Scope::BreakScope | Scope::ContinueScope | Scope::ControlScope);
  skip();
  skip(Token::TK_LParen);
  Expr *Cond = parseExpr();
  skip(Token::TK_RParen);
  Stmt *Body = parseStmt();
  exitScope();
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

static std::optional<DeclSpec::TypeSpecType>
getBuiltinTypeSpecType(Token::TokenKind TK) {
  switch (TK) {
  case Token::TK_Char:
    return DeclSpec::TST_Char;
  case Token::TK_Short:
    return DeclSpec::TST_Short;
  case Token::TK_Int:
    return DeclSpec::TST_Int;
  case Token::TK_Long:
    return DeclSpec::TST_Long;
  default:
    return std::nullopt;
  }
}

// declspec: char | short | int | long | structDecl | unionDecl
void Parser::parseDeclSpec(DeclSpec &DS) {
  auto TyLoc = SM.createBeginLocation(CurTok);
  auto TST = getBuiltinTypeSpecType(CurTok->getKind());
  if (TST) {
    skip();
    DS.setTypeSpecType(*TST, TyLoc);
    return;
  }

  if (CurTok->is(Token::TK_Struct)) {
    DS.setTypeSpecType(DeclSpec::TST_Struct, TyLoc);
    RecordDecl *Record = parseStructDecl();
    DS.setRepDecl(Record);
    return;
  }

  if (CurTok->is(Token::TK_Union)) {
    DS.setTypeSpecType(DeclSpec::TST_Union, TyLoc);
    RecordDecl *Record = parseUnionDecl();
    DS.setRepDecl(Record);
    return;
  }
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
  auto *Var = dynCast<VarDecl>(DR);
  if (!Var)
    Diag.fatalAt(D.getLocation(), "expect variable declarator");

  parseVarInit(Var);
  return Var;
}

void Parser::parseVarInit(VarDecl *Var) {
  if (tryConsume(Token::TK_Equal)) {
    Expr *E = parseInitExpr();
    Var->setInit(E);
    Var->setEndLoc(E->getEndLoc());
  }
}

// initializer: assign-expr
// TODO:      | { initializer-list }
// TODO:      | { initializer-list ',' }
Expr *Parser::parseInitExpr() { return parseAssign(); }

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
  if (CurTok->is(Token::TK_Ident)) {
    D.setIdent(std::string(CurTok->getIdentifer()));
    D.setEndLoc(SM.createEndLocation(CurTok));
    skip();
  }

  // TODO: direct-declarator: '(' declarator ')'

  while (true) {
    if (tryConsume(Token::TK_LParen)) {
      // Try parse function declarator.
      // Record function information in DeclaratorChunk.
      // params: param { ',' param }*
      // param: declspec declarator
      enterScope(Scope::FnScope);
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
      continue;
    }

    if (tryConsume(Token::TK_LSquare)) {
      // Try parse array declarator.
      Expr *LenExpr = parseAssign();
      D.setEndLoc(SM.createBeginLocation(CurTok));
      skip(Token::TK_RSquare);
      D.addDeclChunk(DeclaratorChunk::createArray(LenExpr));
      continue;
    }

    break;
  }
}

FieldDecl *Parser::parseField(DeclSpec &DS) {
  Declarator D(DS);
  parseDeclarator(D);
  Decl *DR = S.actOnDeclarator(D);
  auto *Field = dynCast<FieldDecl>(DR);
  if (!Field)
    Diag.fatalAt(D.getLocation(), "expect field declarator");

  return Field;
}

// expr-stmt: expr ';'
Stmt *Parser::parseExprStmt() {
  Expr *E = parseExpr();
  skip(Token::TK_Semicolon);
  return E;
}

// expr: assign-expr
//     | expr, assign-expr
Expr *Parser::parseExpr() {
  Expr *Assign = parseAssign();
  if (CurTok->is(Token::TK_Comma)) {
    skip();
    auto OpLoc = SM.createBeginLocation(CurTok);
    Expr *RHS = parseExpr();
    return S.actOnBinaryOperator(OpLoc, Assign, RHS, BinaryOperator::BO_Comma);
  }

  return Assign;
}

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
//             | postfix-expr '.' identifier
//             | postfix-expr '->' identifier
Expr *Parser::parsePostfixExpr() {
  Expr *LHS = parsePrimaryExpr();
  while (true) {
    if (tryConsume(Token::TK_LSquare)) {
      Expr *RHS = parseExpr();
      auto EndLoc = SM.createBeginLocation(CurTok);
      skip(Token::TK_RSquare);
      LHS = S.actOnArraySubscriptExpr(EndLoc, LHS, RHS);
      continue;
    }

    auto OpLoc = SM.createBeginLocation(CurTok);
    if (tryConsume(Token::TK_Dot)) {
      if (!CurTok->is(Token::TK_Ident)) {
        SourceLocation Loc = SM.createBeginLocation(CurTok);
        Diag.fatalAt(Loc, "expect identifier after '.'");
        return nullptr;
      }

      std::string_view Ident = CurTok->getIdentifer();
      auto EndLoc = SM.createEndLocation(CurTok);
      skip();
      LHS = S.actOnMemberAccessExpr(OpLoc, EndLoc, LHS, Ident, false);
      continue;
    }

    if (tryConsume(Token::TK_Arrow)) {
      if (!CurTok->is(Token::TK_Ident)) {
        SourceLocation Loc = SM.createBeginLocation(CurTok);
        Diag.fatalAt(Loc, "expect identifier after '->'");
        return nullptr;
      }

      std::string_view Ident = CurTok->getIdentifer();
      auto EndLoc = SM.createEndLocation(CurTok);
      skip();
      LHS = S.actOnMemberAccessExpr(OpLoc, EndLoc, LHS, Ident, true);
      continue;
    }

    return LHS;
  }
}

// primary-expr: paren-expr
//             | decl-ref-expr
//             | call-expr
//             | stmt-expr
//             | str
//             | num
// decl-ref-expr: ident
Expr *Parser::parsePrimaryExpr() {
  if (CurTok->is(Token::TK_LParen))
    return parseParenOrStmtExpr();

  if (CurTok->is(Token::TK_Str)) {
    auto SL = CurTok->lexStringLiteral(Diag);
    auto BegLoc = SM.createBeginLocation(CurTok);
    auto EndLoc = SM.createEndLocation(CurTok);
    skip();
    QualType CAT = Ctx.getConstantArrayType(Ctx.CharTy, SL.size() + 1);
    return S.actOnStringLiteral(BegLoc, EndLoc, CAT, std::move(SL));
  }

  if (CurTok->is(Token::TK_Num)) {
    auto Val = CurTok->getVal();
    auto BegLoc = SM.createBeginLocation(CurTok);
    auto EndLoc = SM.createEndLocation(CurTok);
    skip();
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

  SourceLocation Loc = SM.createBeginLocation(CurTok);
  Diag.fatalAt(Loc, "expect a primary expression");
  return nullptr;
}

// call-expr: ident '(' args? ')'
// args: assign-expr { ',' assign-expr }*
Expr *Parser::parseCallExpr(std::string_view Ident, SourceLocation IdentBegLoc,
                            SourceLocation IdentEndLoc) {
  // Parsed ident '(' already.
  std::vector<Expr *> Args;
  while (CurTok->isNot(Token::TK_RParen)) {
    if (!Args.empty())
      skip(Token::TK_Comma);

    Args.push_back(parseAssign());
  }

  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_RParen);
  return S.actOnCallExpr(IdentBegLoc, IdentEndLoc, EndLoc, Ident,
                         std::move(Args));
}

// paren-expr: '(' expr ')'
// stmt-expr: '(' compound-stmt ')'
Expr *Parser::parseParenOrStmtExpr() {
  assert(CurTok->is(Token::TK_LParen));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  if (CurTok->is(Token::TK_LBrace)) {
    Stmt *CS = parseCompoundStmt();
    auto EndLoc = SM.createBeginLocation(CurTok);
    skip(Token::TK_RParen);
    return S.actOnStmtExpr(BegLoc, EndLoc, CS);
  }
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

Scope *Parser::getCurrScope() const { return S.getCurrScope(); }

void Parser::enterScope(unsigned ScopeFlags, Decl *ScopeDecl) {
  S.CurrScope = new Scope(getCurrScope(), ScopeFlags, ScopeDecl);
}

void Parser::exitScope() {
  Scope *OldScope = getCurrScope();
  S.CurrScope = OldScope->getParent();
  delete OldScope;
}

void Parser::expect(Token::TokenKind Kind, const char *Prompt) {
  if (CurTok->isNot(Kind)) {
    SourceLocation Loc = SM.createBeginLocation(CurTok);
    Diag.fatalAt(Loc, "expect '{}'", Prompt);
  }
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