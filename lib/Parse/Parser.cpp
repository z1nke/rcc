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

// translation-unit: external-decl EOF
//                 | translation-unit external-decl
TranslationUnitDecl *Parser::parse() {
  auto *TU = TranslationUnitDecl::create(Ctx);
  S.TU = TU;
  while (CurTok->isNot(Token::TK_EOF))
    parseExternalDecl(TU);

  return TU;
}

// external-declaration: function-definition
//                     | declaration
// declaration: declaration-specifiers init-declarator-list?
// NOTE: declaration => function-decl | var-decl | typedef-decl
void Parser::parseExternalDecl(TranslationUnitDecl *TU) {
  auto BegLoc = SM.createBeginLocation(CurTok);
  DeclSpec DS(Diag);
  DS.setAlignasAllowed();
  parseDeclSpecs(DS);
  Declarator D(DS);
  unsigned Depth = S.getParamListDepth();
  parseDeclarator(D);
  Decl *FirstDecl = S.actOnDeclarator(D);
  // function-decl: function-definition
  //              | function-declaration
  if (auto *Func = dynCast<FunctionDecl>(FirstDecl)) {
    TU->addDecl(parseFunctionDecl(Func));
    return;
  }

  // Globals / typedefs are not completed as functions; drop ParamLists pushed
  // by function-pointer (or similar) declarators.
  S.finishParamListsTo(Depth);

  if (auto *Var = dynCast<VarDecl>(FirstDecl)) {
    std::vector<VarDecl *> Vars = parseRestVarDecl(BegLoc, DS, Var);
    for (VarDecl *Var : Vars)
      TU->addDecl(Var);
    return;
  }

  if (auto *Typedef = dynCast<TypedefDecl>(FirstDecl)) {
    auto Typedefs = parseRestTypedefDecl(BegLoc, DS, Typedef);
    for (auto *Typedef : Typedefs)
      TU->addDecl(Typedef);
    return;
  }

  Diag.fatalAt(BegLoc, "Unknown global declaration");
}

FunctionDecl *Parser::parseFunctionBody(FunctionDecl *FD) {
  Decl *OldScopeDecl = S.CurrScopeDecl;
  S.CurrScopeDecl = FD;
  S.CurrScope->setDeclContext(FD);
  S.actOnStartOfFunctionBody(FD);
  Stmt *Body = parseCompoundStmt();
  FD->setBody(Body);
  FD->setEndLoc(Body->getEndLoc());

  S.complete(FD);
  S.CurrScopeDecl = OldScopeDecl;
  if (getCurrScope()->getFlags() & Scope::FnScope)
    exitScope();
  return FD;
}

// var-decl: declspecs init-declarator-list? ';'
std::vector<VarDecl *> Parser::parseRestVarDecl(SourceLocation BegLoc,
                                                DeclSpec &DS,
                                                VarDecl *FirstVar) {
  std::vector<VarDecl *> Vars;
  tryParseVarInit(FirstVar);
  FirstVar->setGlobalStorage(true);
  // Non-static file-scope variables have external linkage.
  if (FirstVar->getLinkage() == Linkage::NoLinkage)
    FirstVar->setLinkage(Linkage::ExternalLinkage);
  Vars.push_back(FirstVar);
  while (tryConsume(Token::TK_Comma)) {
    auto *Var = dynCast<VarDecl>(parseInitDeclarator(DS));
    if (!Var)
      Diag.fatalAt(BegLoc, "expect variable declaration");

    Var->setGlobalStorage(true);
    if (Var->getLinkage() == Linkage::NoLinkage)
      Var->setLinkage(Linkage::ExternalLinkage);
    Vars.push_back(Var);
  }

  skip(Token::TK_Semicolon);
  return Vars;
}

std::vector<TypedefDecl *>
Parser::parseRestTypedefDecl(SourceLocation BegLoc, DeclSpec &DS,
                             TypedefDecl *FirstTypedef) {
  std::vector<TypedefDecl *> Typedefs;
  Typedefs.push_back(FirstTypedef);
  while (tryConsume(Token::TK_Comma)) {
    auto *Typedef = dynCast<TypedefDecl>(parseInitDeclarator(DS));
    if (!Typedef)
      Diag.fatalAt(BegLoc, "expect typedef declaration");

    Typedefs.push_back(Typedef);
  }
  skip(Token::TK_Semicolon);
  return Typedefs;
}

// function-decl: function-definition
//              | function-declaration
FunctionDecl *Parser::parseFunctionDecl(FunctionDecl *Func) {
  if (tryConsume(Token::TK_Semicolon)) {
    S.complete(Func);
    if (getCurrScope()->getFlags() & Scope::FnScope)
      exitScope();
    return Func;
  }

  if (CurTok->is(Token::TK_LBrace))
    return parseFunctionBody(Func);

  Diag.fatalAt(CurTok->getLoc(), "expect ';' or '{{'");
}

// struct-union-decl: struct-or-union '{' struct-decl-list '}'
//                  | struct-or-union ident
//                  | struct-or-union ident '{' struct-decl-list '}'
// struct-or-union: 'struct' | 'union'
RecordDecl *Parser::parseStructDecl() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Struct);
  RecordDecl *Struct = parseStructUnionDecl(BegLoc, RecordDecl::TK_Struct);
  if (!Struct)
    Diag.fatalAt(BegLoc, "expected struct declaration");
  return Struct;
}

RecordDecl *Parser::parseUnionDecl() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Union);
  RecordDecl *Union = parseStructUnionDecl(BegLoc, RecordDecl::TK_Union);
  if (!Union)
    Diag.fatalAt(BegLoc, "expected union declaration");
  return Union;
}

// enum-specifier: enum identifier? '{' enumerator-list '}'
//               | enum identifier? '{' enumerator-list ',' '}'
//               | enum identifier
EnumDecl *Parser::parseEnumDecl() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Enum);
  std::string_view Ident;
  SourceLocation Loc = BegLoc;
  if (CurTok->is(Token::TK_Ident)) {
    Loc = SM.createBeginLocation(CurTok);
    Ident = CurTok->getIdentifer();
    skip();
  }

  if (!Ident.empty() && CurTok->isNot(Token::TK_LBrace)) {
    TagDecl *Tag = S.findTagDecl(Ident);
    if (Tag) {
      auto *Enum = dynCast<EnumDecl>(Tag);
      if (!Enum)
        Diag.fatalAt(Loc, "use of tag '{}' with wrong kind", Ident);
      return Enum;
    }

    Tag = S.actOnTagDecl(Loc, BegLoc, BegLoc, Ident, TagDecl::TK_Enum);
    auto *Enum = cast<EnumDecl>(Tag);
    getCurrScope()->addTag(Enum);
    return Enum;
  }

  EnumDecl *Enum = cast<EnumDecl>(
      S.actOnTagDecl(Loc, BegLoc, SourceLocation(), Ident, TagDecl::TK_Enum));
  S.actOnTagStartDefinition(Loc, Enum);
  QualType ET = Enum->getType();
  if (!Ident.empty())
    getCurrScope()->addTag(Enum);

  if (tryConsume(Token::TK_LBrace)) {
    std::vector<EnumConstantDecl *> Constants = parseEnumeratorList(ET);
    Enum->setEnumerators(std::move(Constants));
    S.actOnTagFinishDefinition(Enum, SM.createBeginLocation(CurTok));
    skip(Token::TK_RBrace);
  }

  return Enum;
}

// enumerator-list: enumerator
//                | enumerator-list ',' enumerator
std::vector<EnumConstantDecl *> Parser::parseEnumeratorList(QualType ET) {
  int Val = 0;
  std::vector<EnumConstantDecl *> Constants;
  while (CurTok->isNot(Token::TK_RBrace)) {
    auto *ECD = parseEnumerator(ET, Val);
    Constants.push_back(ECD);
    if (tryConsume(Token::TK_Comma))
      continue;
    break;
  }
  return Constants;
}

// enumerator: enumeration-constant
//           | enumeration-constant '=' constant-expr
// enumeration-constant: identifier
EnumConstantDecl *Parser::parseEnumerator(QualType ET, int &Val) {
  auto BegLoc = SM.createBeginLocation(CurTok);
  if (CurTok->isNot(Token::TK_Ident))
    Diag.fatalAt(BegLoc, "expect identifier in enumeration-constant");

  std::string_view Ident = CurTok->getIdentifer();
  auto EndLoc = SM.createEndLocation(CurTok);
  skip();

  Expr *Init = nullptr;
  if (tryConsume(Token::TK_Equal)) {
    Init = parseConstantExpr();
    if (!Init->getType()->isIntegerType())
      Diag.fatalAt(Init->getBeginLoc(),
                   "enumerator initializer must be integer");
  }

  if (Init) {
    EndLoc = Init->getEndLoc();
    auto InitVal = Init->evaluateAsInt();
    if (!InitVal)
      Diag.fatalAt(Init->getBeginLoc(),
                   "enumerator initializer must be constant expression");

    Val = static_cast<int>(*InitVal);
  }

  auto *ECD = S.actOnEnumConstantDecl(BegLoc, BegLoc, EndLoc, ET,
                                      std::string(Ident), Val, Init);
  ++Val;
  return ECD;
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
    if (Tag) {
      auto *Record = dynCast<RecordDecl>(Tag);
      if (!Record ||
          Record->getTagKind() != static_cast<TagDecl::TagKind>(TagKind))
        Diag.fatalAt(Loc, "use of tag '{}' with wrong kind", Ident);
      return Record;
    }

    Tag = S.actOnTagDecl(Loc, BegLoc, BegLoc, Ident, TagKind);
    auto *Record = cast<RecordDecl>(Tag);
    getCurrScope()->addTag(Record);
    return Record;
  }

  if (tryConsume(Token::TK_LBrace)) {
    RecordDecl *Record =
        cast<RecordDecl>(S.actOnTagDecl(Loc, BegLoc, BegLoc, Ident, TagKind));
    S.actOnTagStartDefinition(Loc, Record);

    // Make a newly declared named tag visible while parsing fields so
    // self-referential members like `struct T *next;` are accepted.
    bool NeedLateAddTag = Ident.empty();
    if (!NeedLateAddTag)
      getCurrScope()->addTag(Record);

    enterScope(Scope::StructScope, Record);
    std::vector<FieldDecl *> Fields = parseFields();
    Record->setFields(std::move(Fields));
    exitScope();
    S.actOnTagFinishDefinition(Record, SM.createBeginLocation(CurTok));
    if (NeedLateAddTag)
      getCurrScope()->addTag(Record);
    skip(Token::TK_RBrace);
    return Record;
  }

  return nullptr;
}

std::vector<FieldDecl *> Parser::parseFields() {
  std::vector<FieldDecl *> Fields;
  while (CurTok->isNot(Token::TK_RBrace)) {
    DeclSpec DS(Diag);
    DS.setAlignasAllowed();
    parseDeclSpecs(DS);
    Fields.push_back(parseField(DS));
    while (tryConsume(Token::TK_Comma))
      Fields.push_back(parseField(DS));
    skip(Token::TK_Semicolon);
  }
  return Fields;
}

// stmt: labeled-stmt
//     | compound-stmt
//     | expr-stmt
//     | selection-stmt
//     | iteration-stmt
//     | jump-stmt
//     | null-stmt
//     | decl-stmt
// labeled-stmt: ident ':' stmt
//             | 'case' constant-expr ':' stmt
//             | 'default' ':' stmt
// selection-stmt: if-stmt
//               | switch-stmt
// iteration-stmt: for-stmt
//               | while-stmt
// jump-stmt: goto-stmt
//          | break-stmt
//          | continue-stmt
//          | return-stmt
Stmt *Parser::parseStmt() {
  if (CurTok->is(Token::TK_Ident)) {
    Token *Next = CurTok->getNext();
    if (Next && Next->is(Token::TK_Colon))
      return parseLabelStmt();
  }

  if (isStorageClassSpec(CurTok) || isTypeName(CurTok))
    return parseDeclStmt();

  switch (CurTok->getKind()) {
  case Token::TK_Semicolon:
    return parseNullStmt();
  case Token::TK_Return:
    return parseReturnStmt();
  case Token::TK_LBrace:
    return parseCompoundStmt();
  case Token::TK_If:
    return parseIfStmt();
  case Token::TK_Switch:
    return parseSwitchStmt();
  case Token::TK_Case:
    return parseCaseStmt();
  case Token::TK_Default:
    return parseDefaultStmt();
  case Token::TK_For:
    return parseForStmt();
  case Token::TK_While:
    return parseWhileStmt();
  case Token::TK_Do:
    return parseDoWhileStmt();
  case Token::TK_Break:
    return parseBreakStmt();
  case Token::TK_Continue:
    return parseContinueStmt();
  case Token::TK_Goto:
    return parseGotoStmt();
  default:
    break;
  }

  return parseExprStmt();
}

// switch-stmt: 'switch' '(' expr ')' stmt
Stmt *Parser::parseSwitchStmt() {
  assert(CurTok->is(Token::TK_Switch));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  skip(Token::TK_LParen);
  Expr *Cond = parseExpr();
  skip(Token::TK_RParen);
  enterScope(Scope::BreakScope | Scope::ControlScope | Scope::SwitchScope);
  S.actOnSwitchStmtStart();
  Stmt *Body = parseStmt();
  exitScope();
  return S.actOnSwitchStmt(BegLoc, Cond, Body);
}

// case-stmt: 'case' constant-expr ':' stmt
Stmt *Parser::parseCaseStmt() {
  assert(CurTok->is(Token::TK_Case));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  Expr *LHS = parseConstantExpr();
  skip(Token::TK_Colon);
  Stmt *SubStmt = parseStmt();
  return S.actOnCaseStmt(BegLoc, LHS, SubStmt);
}

// default-stmt: 'default' ':' stmt
Stmt *Parser::parseDefaultStmt() {
  assert(CurTok->is(Token::TK_Default));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  skip(Token::TK_Colon);
  Stmt *SubStmt = parseStmt();
  return S.actOnDefaultStmt(BegLoc, SubStmt);
}

// null-stmt: ';'
Stmt *Parser::parseNullStmt() {
  assert(CurTok->is(Token::TK_Semicolon));
  auto SemiLoc = SM.createBeginLocation(CurTok);
  skip();
  return S.actOnNullStmt(SemiLoc);
}

// return-stmt: 'return' expr? ';'
Stmt *Parser::parseReturnStmt() {
  auto BegLoc = SM.createBeginLocation(CurTok);
  assert(CurTok->is(Token::TK_Return));
  skip();

  Expr *E = nullptr;
  if (CurTok->isNot(Token::TK_Semicolon))
    E = parseExpr();

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
  while (CurTok->isNot(Token::TK_RBrace)) {
    if (Stmt *S = parseStmt())
      Stmts.push_back(S);
  }

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
  if (Init && isa<NullStmt>(Init))
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

// do-while-stmt: 'do' stmt 'while' '(' expr ')' ';'
Stmt *Parser::parseDoWhileStmt() {
  assert(CurTok->is(Token::TK_Do));
  auto BegLoc = SM.createBeginLocation(CurTok);
  enterScope(Scope::BreakScope | Scope::ContinueScope | Scope::ControlScope);
  skip();
  Stmt *Body = parseStmt();
  exitScope();
  skip(Token::TK_While);
  skip(Token::TK_LParen);
  Expr *Cond = parseExpr();
  skip(Token::TK_RParen);
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Semicolon);
  return S.actOnDoWhileStmt(BegLoc, EndLoc, Body, Cond);
}

// break-stmt: 'break' ';'
Stmt *Parser::parseBreakStmt() {
  assert(CurTok->is(Token::TK_Break));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Semicolon);
  return S.actOnBreakStmt(BegLoc, EndLoc);
}

// continue-stmt: 'continue' ';'
Stmt *Parser::parseContinueStmt() {
  assert(CurTok->is(Token::TK_Continue));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Semicolon);
  return S.actOnContinueStmt(BegLoc, EndLoc);
}

// goto-stmt: 'goto' ident ';'
Stmt *Parser::parseGotoStmt() {
  assert(CurTok->is(Token::TK_Goto));
  auto BegLoc = SM.createBeginLocation(CurTok);
  skip();
  if (CurTok->isNot(Token::TK_Ident))
    Diag.fatalAt(SM.createBeginLocation(CurTok), "expected label name");
  std::string_view LabelName = CurTok->getIdentifer();
  skip();
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Semicolon);
  return S.actOnGotoStmt(BegLoc, EndLoc, LabelName);
}

// label-stmt: ident ':' stmt
Stmt *Parser::parseLabelStmt() {
  assert(CurTok->is(Token::TK_Ident));
  auto BegLoc = SM.createBeginLocation(CurTok);
  std::string_view LabelName = CurTok->getIdentifer();
  skip();
  skip(Token::TK_Colon);
  Stmt *SubStmt = parseStmt();
  return S.actOnLabelStmt(BegLoc, SubStmt->getEndLoc(), LabelName, SubStmt);
}

// decl-stmt: declspecs init-declarator-list? ';'
Stmt *Parser::parseDeclStmt() {
  DeclSpec DS(Diag);
  DS.setAlignasAllowed();
  auto BegLoc = SM.createBeginLocation(CurTok);
  parseDeclSpecs(DS);
  std::vector<Decl *> Decls = parseInitDeclaratorList(DS);
  auto EndLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Semicolon);

  if (DS.getStorageClassSpec() == DeclSpec::SCS_Extern ||
      (!Decls.empty() && isa<FunctionDecl>(Decls.front())))
    return nullptr;

  return S.actOnDeclStmt(Ctx, BegLoc, EndLoc, std::move(Decls));
}

// declspecs: storage-class-spec declspecs?
//          | type-spec declspecs?
//          | '_Alignas' '(' type-name | constant-expr ')' declspecs?
// storage-class-spec: typedef | extern | static | auto | register
// typespec: void | _Bool | char | short | int | long | struct-or-union-spec
//         | enum-spec | typedef-name
void Parser::parseDeclSpecs(DeclSpec &DS) {
#define STORAGE_CLASS_SPEC_CASE(T)                                             \
  case Token::TK_##T:                                                          \
    DS.setStorageClassSpec(DeclSpec::SCS_##T, TyLoc);                          \
    skip();                                                                    \
    break

#define TYPE_SPEC_TYPE_CASE(T)                                                 \
  case Token::TK_##T:                                                          \
    DS.setTypeSpecType(DeclSpec::TST_##T, TyLoc);                              \
    skip();                                                                    \
    break
#define TYPE_SPEC_WIDTH_CASE(T)                                                \
  case Token::TK_##T:                                                          \
    DS.setTypeSpecWidth(DeclSpec::TSW_##T, TyLoc);                             \
    skip();                                                                    \
    break

  while (true) {
    auto TyLoc = SM.createBeginLocation(CurTok);
    switch (CurTok->getKind()) {
      STORAGE_CLASS_SPEC_CASE(Typedef);
      STORAGE_CLASS_SPEC_CASE(Static);
      STORAGE_CLASS_SPEC_CASE(Extern);
      TYPE_SPEC_TYPE_CASE(Void);
      TYPE_SPEC_TYPE_CASE(UnderlineBool);
      TYPE_SPEC_TYPE_CASE(Char);
      TYPE_SPEC_TYPE_CASE(Int);
      TYPE_SPEC_WIDTH_CASE(Short);
      TYPE_SPEC_WIDTH_CASE(Long);
    case Token::TK_Alignas: {
      if (!DS.isAlignasAllowed())
        Diag.fatalAt(TyLoc, "_Alignas is not allowed in this context");
      skip();
      skip(Token::TK_LParen);
      if (isTypeName(CurTok)) {
        QualType T = parseTypeName();
        DS.setAlign(T->getAlign());
      } else {
        Expr *E = parseConstantExpr();
        auto Val = E->evaluateAsInt();
        if (!Val)
          Diag.fatalAt(E->getBeginLoc(),
                       "_Alignas argument must be a constant expression");
        DS.setAlign(static_cast<std::size_t>(*Val));
      }
      skip(Token::TK_RParen);
      break;
    }
    case Token::TK_Struct:
      DS.setTypeSpecType(DeclSpec::TST_Struct, TyLoc);
      DS.setRepDecl(parseStructDecl());
      break;
    case Token::TK_Union:
      DS.setTypeSpecType(DeclSpec::TST_Union, TyLoc);
      DS.setRepDecl(parseUnionDecl());
      break;
    case Token::TK_Enum:
      DS.setTypeSpecType(DeclSpec::TST_Enum, TyLoc);
      DS.setRepDecl(parseEnumDecl());
      break;
    case Token::TK_Ident: {
      if (DS.hasTypeSpecifier())
        return;

      std::string_view Ident = CurTok->getIdentifer();
      auto *Typedef = S.findTypedef(Ident);
      if (!Typedef) {
        // Support C89-style implicit-int typedef declarations like:
        //   typedef t;
        if (DS.getStorageClassSpec() == DeclSpec::SCS_Typedef)
          return;
        Diag.fatalAt(TyLoc, "use of undeclared identifier '{}'", Ident);
      }

      DS.setTypeSpecType(DeclSpec::TST_Typename, TyLoc);
      DS.setRepDecl(Typedef);
      skip();
      break;
    }
    default:
      return;
    }
  }
#undef TYPE_SPEC_TYPE_CASE
#undef TYPE_SPEC_WIDTH_CASE
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
  unsigned Depth = S.getParamListDepth();
  parseDeclarator(D);
  Decl *DR = S.actOnDeclarator(D);

  if (auto *Var = dynCast<VarDecl>(DR)) {
    // Function-pointer declarators push Params but are not completed as
    // functions; restore the enclosing parameter frame.
    S.finishParamListsTo(Depth);
    tryParseVarInit(Var);
    return Var;
  }

  if (auto *Typedef = dynCast<TypedefDecl>(DR)) {
    S.finishParamListsTo(Depth);
    return Typedef;
  }

  if (auto *Func = dynCast<FunctionDecl>(DR)) {
    S.complete(Func);
    if (getCurrScope()->getFlags() & Scope::FnScope)
      exitScope();
    return Func;
  }

  Diag.fatalAt(D.getLocation(), "expect variable or typedef declarator");
}

void Parser::tryParseVarInit(VarDecl *Var) {
  if (tryConsume(Token::TK_Equal)) {
    Expr *Init = parseInitExpr();
    S.complete(Var, Init);
  }
}

// initializer: assign-expr
//            | { initializer-list }
//            | { initializer-list ',' }
Expr *Parser::parseInitExpr() {
  if (CurTok->isNot(Token::TK_LBrace))
    return parseAssign();

  SourceLocation BegLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_LBrace);
  std::vector<Expr *> Inits;
  while (CurTok->isNot(Token::TK_RBrace)) {
    Inits.push_back(parseInitExpr());
    if (!tryConsume(Token::TK_Comma)) {
      if (CurTok->is(Token::TK_RBrace))
        break;
      Diag.fatalAt(SM.createBeginLocation(CurTok), "expect ',' or '}}'");
    }
  }
  SourceLocation EndLoc = SM.createEndLocation(CurTok);
  skip(Token::TK_RBrace);
  return InitListExpr::create(Ctx, BegLoc, EndLoc, QualType(), std::move(Inits));
}

// declarator: '*'* direct-declarator
void Parser::parseDeclarator(Declarator &D) {
  while (tryConsume(Token::TK_Star)) {
    // Recursively parse the declarator.
    // Type suffixes have higher priority than stars.
    parseDeclarator(D);
    auto Chunk = DeclaratorChunk::createPointer();
    D.addDeclChunk(Chunk);
  }

  if (CurTok->isOneOf(Token::TK_Ident, Token::TK_LParen))
    parseDirectDeclarator(D);
}

// direct-declarator: ident
//                  | ( declarator )
//                  | direct-declarator type-suffix
void Parser::parseDirectDeclarator(Declarator &D) {
  if (CurTok->is(Token::TK_Ident)) {
    D.setIdent(std::string(CurTok->getIdentifer()));
    D.setEndLoc(SM.createEndLocation(CurTok));
    skip();
  } else if (tryConsume(Token::TK_LParen)) {
    parseDeclarator(D);
    skip(Token::TK_RParen);
  } else {
    Diag.fatalAt(CurTok->getLoc(), "expect identifier or '('");
  }

  parseTypeSuffix(D);
}

// [type-suffix]: '(' param-type-list ')'
//              | '[' { assign-expr } ']'
// param-type-list: param-list
//                | param-list, '...'
// param-list: param-decl
//           | param-list, param-decl
// param: declspec declarator
void Parser::parseTypeSuffix(Declarator &D) {
  while (true) {
    if (tryConsume(Token::TK_LParen)) {
      // Try parse function declarator.
      // Record function information in DeclaratorChunk.
      S.enterParamList();
      enterScope(Scope::FnScope);
      unsigned Idx = 0;
      bool IsVariadic = false;
      while (CurTok->isNot(Token::TK_RParen)) {
        if (Idx > 0)
          skip(Token::TK_Comma);

        if (CurTok->is(Token::TK_Ellipsis)) {
          IsVariadic = true;
          skip();
          break;
        }

        DeclSpec ParamDS(Diag);
        parseDeclSpecs(ParamDS);
        Declarator ParamD(ParamDS);
        parseDeclarator(ParamD);
        (void)S.actOnParamVarDecl(ParamD, Idx);
        ++Idx;
      }

      // Empty parameter list is an old-style (unprototyped) parameter list:
      // treat as variadic so callers may pass arguments. (void) is not empty.
      if (Idx == 0)
        IsVariadic = true;

      D.setEndLoc(SM.createEndLocation(CurTok));
      skip(Token::TK_RParen);
      D.addDeclChunk(DeclaratorChunk::createFunction(IsVariadic));
      continue;
    }

    // Try parse array declarator.
    if (tryConsume(Token::TK_LSquare)) {
      // Parse '[' ']'.
      if (tryConsume(Token::TK_RSquare)) {
        D.addDeclChunk(DeclaratorChunk::createArray(nullptr));
        continue;
      }

      // Parse '[' assign-expr ']'.
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
  unsigned Depth = S.getParamListDepth();
  parseDeclarator(D);
  Decl *DR = S.actOnDeclarator(D);
  S.finishParamListsTo(Depth);
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

static std::optional<BinaryOperator::Opcode> getAssignOpcode(const Token &Tok) {
  switch (Tok.getKind()) {
  case Token::TK_Equal:
    return BinaryOperator::BO_Assign;
  case Token::TK_PlusEqual:
    return BinaryOperator::BO_AddAssign;
  case Token::TK_MinusEqual:
    return BinaryOperator::BO_SubAssign;
  case Token::TK_StarEqual:
    return BinaryOperator::BO_MulAssign;
  case Token::TK_SlashEqual:
    return BinaryOperator::BO_DivAssign;
  case Token::TK_PercentEqual:
    return BinaryOperator::BO_RemAssign;
  case Token::TK_AmpEqual:
    return BinaryOperator::BO_AndAssign;
  case Token::TK_PipeEqual:
    return BinaryOperator::BO_OrAssign;
  case Token::TK_CaretEqual:
    return BinaryOperator::BO_XorAssign;
  case Token::TK_LessLessEqual:
    return BinaryOperator::BO_ShlAssign;
  case Token::TK_GreaterGreaterEqual:
    return BinaryOperator::BO_ShrAssign;
  default:
    return std::nullopt;
  }
}

// assign-expr: conditional-expr
//            | unary-expr assign-op assign-expr
// assign-op: '=' | '+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^='
//          | '<<=' | '>>='
Expr *Parser::parseAssign() {
  Expr *LHS = parseConditionalExpr();
  auto OpLoc = SM.createBeginLocation(CurTok);
  auto Opcode = getAssignOpcode(*CurTok);
  if (!Opcode)
    return LHS;

  skip();
  Expr *RHS = parseAssign();
  return S.actOnBinaryOperator(OpLoc, LHS, RHS, *Opcode);
}

// conditional-expr: logical-or-expr
//                 | logical-or-expr ? expr : conditional-expr
Expr *Parser::parseConditionalExpr() {
  Expr *Cond = parseLogicalOrExpr();
  if (CurTok->isNot(Token::TK_Question))
    return Cond;

  auto QLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Question);
  Expr *TrueExpr = parseExpr();
  auto ColonLoc = SM.createBeginLocation(CurTok);
  skip(Token::TK_Colon);
  Expr *FalseExpr = parseConditionalExpr();
  return S.actOnConditionalOperator(QLoc, ColonLoc, Cond, TrueExpr, FalseExpr);
}

// equality-expr: relational-expr
//              | equality-expr '==' relational-expr
//              | equality-expr '!=' relational-expr
// Left Recursion Elimination =>
// equality-expr: relational-expr { ('==' | '!=') relational-expr }
Expr *Parser::parseEqualityExpr() {
  return parseBinaryExpr<&Parser::parseRelationalExpr, Token::TK_EqualEqual,
                         Token::TK_NotEqual>();
}

// constant-expr: conditional-expr
Expr *Parser::parseConstantExpr() { return parseConditionalExpr(); }

// logical-or-expr: logical-and-expr
//                | logical-or-expr '||' logical-and-expr
Expr *Parser::parseLogicalOrExpr() {
  return parseBinaryExpr<&Parser::parseLogicalAndExpr, Token::TK_PipePipe>();
}

// logical-and-expr: inclusive-or-expr
//                 | logical-and-expr '&&' inclusive-or-expr
Expr *Parser::parseLogicalAndExpr() {
  return parseBinaryExpr<&Parser::parseBitwiseOrExpr, Token::TK_AmpAmp>();
}

// inclusive-or-expr: exclusive-or-expr
//                  | inclusive-or-expr '|' exclusive-or-expr
Expr *Parser::parseBitwiseOrExpr() {
  return parseBinaryExpr<&Parser::parseBitwiseXorExpr, Token::TK_Pipe>();
}

// exclusive-or-expr: and-expr
//                  | exclusive-or-expr '^' and-expr
Expr *Parser::parseBitwiseXorExpr() {
  return parseBinaryExpr<&Parser::parseBitwiseAndExpr, Token::TK_Caret>();
}

// and-expr: equality-expr
//         | and-expr & equality-expr
Expr *Parser::parseBitwiseAndExpr() {
  return parseBinaryExpr<&Parser::parseEqualityExpr, Token::TK_Amp>();
}

// relational-expr: shift-expr { ('<' | '<=' | '>' | '>=') shift-expr }
Expr *Parser::parseRelationalExpr() {
  return parseBinaryExpr<&Parser::parseShiftExpr, Token::TK_Less,
                         Token::TK_LessEqual, Token::TK_Greater,
                         Token::TK_GreaterEqual>();
}

// shift-expr: add-expr { ('<<' | '>>') add-expr }
Expr *Parser::parseShiftExpr() {
  return parseBinaryExpr<&Parser::parseAddExpr, Token::TK_LessLess,
                         Token::TK_GreaterGreater>();
}

// add-expr: mul-expr { ('+' | '-') mul-expr }
Expr *Parser::parseAddExpr() {
  return parseBinaryExpr<&Parser::parseMulExpr, Token::TK_Plus,
                         Token::TK_Minus>();
}

// mul-expr: cast-expr { ('*' | '/' | '%') cast-expr }
Expr *Parser::parseMulExpr() {
  return parseBinaryExpr<&Parser::parseCastExpr, Token::TK_Star,
                         Token::TK_Slash, Token::TK_Percent>();
}

// cast-expr: unary-expr | '(' type-name ')' cast-expr
Expr *Parser::parseCastExpr() {
  if (CurTok->is(Token::TK_LParen) && isTypeName(CurTok->getNext())) {
    Token *Start = CurTok;
    auto BegLoc = SM.createBeginLocation(CurTok);
    skip();
    QualType T = parseTypeName();
    skip(Token::TK_RParen);

    // Compound literal: ( type-name ) { initializer-list }
    if (CurTok->is(Token::TK_LBrace)) {
      CurTok = Start;
      return parseUnaryExpr();
    }

    auto *SubExpr = parseCastExpr();
    return S.actOnCastExpr(BegLoc, SubExpr->getEndLoc(), T, SubExpr, false);
  }

  return parseUnaryExpr();
}

static UnaryOperator::Opcode getUnaryOpcode(Token::TokenKind Kind) {
  switch (Kind) {
  case Token::TK_Plus:
    return UnaryOperator::UO_Plus;
  case Token::TK_Minus:
    return UnaryOperator::UO_Minus;
  case Token::TK_Exclaim:
    return UnaryOperator::UO_LNot;
  case Token::TK_Tilde:
    return UnaryOperator::UO_Not;
  case Token::TK_Amp:
    return UnaryOperator::UO_Addrof;
  case Token::TK_Star:
    return UnaryOperator::UO_Deref;
  case Token::TK_PlusPlus:
    return UnaryOperator::UO_PreInc;
  case Token::TK_MinusMinus:
    return UnaryOperator::UO_PreDec;
  default:
    RCC_UNREACHABLE("Unknown binary operator");
  }
}

// unary-expr: postfix-expr
//           | '++' unary-expr
//           | '--' unary-expr
//           | unary-operator cast-expr
//           | sizeof unary-expr
//           | sizeof '(' type-name ')'
//           | '_Alignof' '(' type-name ')'
//           | '_Alignof' unary-expr
Expr *Parser::parseUnaryExpr() {
  if (CurTok->isOneOf(Token::TK_PlusPlus, Token::TK_MinusMinus)) {
    auto Op = getUnaryOpcode(CurTok->getKind());
    auto OpLoc = SM.createBeginLocation(CurTok);
    skip();
    Expr *SubExpr = parseUnaryExpr();
    return S.actOnUnaryOperator(OpLoc, SubExpr, Op);
  }

  // unary-operator: '&' | '*' | '+' | '-' | '~' | '!'
  if (CurTok->isOneOf(Token::TK_Amp, Token::TK_Star, Token::TK_Plus,
                      Token::TK_Minus, Token::TK_Tilde, Token::TK_Exclaim)) {
    auto Op = getUnaryOpcode(CurTok->getKind());
    auto OpLoc = SM.createBeginLocation(CurTok);
    skip();
    Expr *SubExpr = parseCastExpr();
    return S.actOnUnaryOperator(OpLoc, SubExpr, Op);
  }

  if (CurTok->is(Token::TK_Sizeof)) {
    auto BegLoc = SM.createBeginLocation(CurTok);
    skip();
    if (CurTok->is(Token::TK_LParen) && isTypeName(CurTok->getNext())) {
      skip(Token::TK_LParen);
      QualType T = parseTypeName();
      SourceLocation EndLoc = SM.createBeginLocation(CurTok);
      skip(Token::TK_RParen);
      return S.actOnUnaryExprOrTypeTraitExpr(BegLoc, EndLoc, T.getTypePtr());
    }

    Expr *Ex = parseUnaryExpr();
    return S.actOnUnaryExprOrTypeTraitExpr(BegLoc, Ex);
  }

  // "_Alignof" "(" type-name ")"
  // "_Alignof" unary-expr
  if (CurTok->is(Token::TK_Alignof)) {
    auto BegLoc = SM.createBeginLocation(CurTok);
    skip();
    if (CurTok->is(Token::TK_LParen) && isTypeName(CurTok->getNext())) {
      skip(Token::TK_LParen);
      QualType T = parseTypeName();
      SourceLocation EndLoc = SM.createBeginLocation(CurTok);
      skip(Token::TK_RParen);
      return IntegerLiteral::create(Ctx, BegLoc, EndLoc, Ctx.IntTy,
                                    T->getAlign());
    }

    Expr *Ex = parseUnaryExpr();
    return IntegerLiteral::create(Ctx, BegLoc, Ex->getEndLoc(), Ctx.IntTy,
                                  Ex->getType()->getAlign());
  }

  return parsePostfixExpr();
}

// abstract-declarator: pointer
//                    | pointer? direct-abstract-declarator
void Parser::parseAbstractDeclarator(Declarator &D) {
  while (tryConsume(Token::TK_Star)) {
    // Type suffixes have higher priority than stars.
    parseAbstractDeclarator(D);
    D.addDeclChunk(DeclaratorChunk::createPointer());
    return;
  }

  parseDirectAbstractDeclarator(D);
}

// direct-abstract-declarator: '(' abstract-declarator ')'
//                           | direct-abstract-declarator type-suffix
void Parser::parseDirectAbstractDeclarator(Declarator &D) {
  if (tryConsume(Token::TK_LParen)) {
    parseAbstractDeclarator(D);
    skip(Token::TK_RParen);
  }

  parseTypeSuffix(D);
}

// type-name: specifier-qualifier-list abstract-declarator?
QualType Parser::parseTypeName() {
  DeclSpec DS(Diag);
  parseDeclSpecs(DS);
  Declarator D(DS);
  // Only discard ParamLists pushed for this type-name (e.g. function types in
  // casts/sizeof). Do not pop the enclosing function's parameter frame.
  unsigned Depth = S.getParamListDepth();
  parseAbstractDeclarator(D);
  QualType T = S.getTypeForDeclarator(D);
  S.finishParamListsTo(Depth);
  return T;
}

bool Parser::isStorageClassSpec(const Token *Tok) {
  switch (Tok->getKind()) {
  case Token::TK_Typedef:
  case Token::TK_Static:
  case Token::TK_Extern:
    return true;
  default:
    return false;
  }
}

bool Parser::isTypeName(const Token *Tok) {
  switch (Tok->getKind()) {
  case Token::TK_Void:
  case Token::TK_UnderlineBool:
  case Token::TK_Char:
  case Token::TK_Short:
  case Token::TK_Int:
  case Token::TK_Long:
  case Token::TK_Struct:
  case Token::TK_Union:
  case Token::TK_Enum:
  case Token::TK_Alignas:
  case Token::TK_Typedef:
  case Token::TK_Static:
  case Token::TK_Extern:
    return true;
  case Token::TK_Ident:
    return S.findTypedef(Tok->getIdentifer());
  default:
    return false;
  }
}

// postfix-expr: '(' type-name ')' '{' initializer-list '}'
//             | primary-expr
//             | postfix-expr '[' expr ']'
//             | postfix-expr '.' identifier
//             | postfix-expr '->' identifier
//             | postfix-expr '++'
//             | postfix-expr '--'
Expr *Parser::parsePostfixExpr() {
  // Compound literal: ( type-name ) { initializer-list }
  if (CurTok->is(Token::TK_LParen) && isTypeName(CurTok->getNext())) {
    auto BegLoc = SM.createBeginLocation(CurTok);
    skip();
    QualType T = parseTypeName();
    skip(Token::TK_RParen);
    Expr *Init = parseInitExpr();
    return S.actOnCompoundLiteral(BegLoc, Init->getEndLoc(), T, Init);
  }

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

    if (tryConsume(Token::TK_PlusPlus)) {
      LHS = S.actOnUnaryOperator(OpLoc, LHS, UnaryOperator::UO_PostInc);
      continue;
    }

    if (tryConsume(Token::TK_MinusMinus)) {
      LHS = S.actOnUnaryOperator(OpLoc, LHS, UnaryOperator::UO_PostDec);
      continue;
    }

    return LHS;
  }
}

// primary-expr: paren-expr
//             | decl-ref-expr
//             | call-expr
//             | stmt-expr
//             | character-literal
//             | string-literal
//             | numeric-constant
// decl-ref-expr: ident
Expr *Parser::parsePrimaryExpr() {
  if (CurTok->is(Token::TK_LParen))
    return parseParenOrStmtExpr();

  if (CurTok->is(Token::TK_CharLiteral)) {
    unsigned Val = CurTok->getCharLiteral(Diag);
    auto BegLoc = SM.createBeginLocation(CurTok);
    auto EndLoc = SM.createEndLocation(CurTok);
    skip();
    return S.actOnCharacterLiteral(BegLoc, EndLoc, Ctx.CharTy, Val);
  }

  if (CurTok->is(Token::TK_StrLiteral)) {
    auto SL = CurTok->getStringLiteral(Diag);
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
  case Token::TK_Percent:
    return BinaryOperator::BO_Rem;
  case Token::TK_Amp:
    return BinaryOperator::BO_And;
  case Token::TK_Pipe:
    return BinaryOperator::BO_Or;
  case Token::TK_Caret:
    return BinaryOperator::BO_Xor;
  case Token::TK_LessLess:
    return BinaryOperator::BO_Shl;
  case Token::TK_GreaterGreater:
    return BinaryOperator::BO_Shr;
  case Token::TK_AmpAmp:
    return BinaryOperator::BO_LAnd;
  case Token::TK_PipePipe:
    return BinaryOperator::BO_LOr;
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
    if ((CurTok->is(Tks) || ...)) {
      auto OpLoc = SM.createBeginLocation(CurTok);
      auto Op = getBinaryOpcode(CurTok->getKind());
      skip();
      Expr *RHS = (this->*ParseOperand)();
      LHS = S.actOnBinaryOperator(OpLoc, LHS, RHS, Op);
      continue;
    }

    return LHS;
  }
}

Scope *Parser::getCurrScope() const { return S.getCurrScope(); }

void Parser::enterScope(unsigned ScopeFlags, Decl *ScopeDecl) {
  S.CurrScope = new Scope(getCurrScope(), ScopeFlags, ScopeDecl);
}

void Parser::exitScope() {
  Scope *OldScope = getCurrScope();
  S.CurrScope = OldScope->getParent();
  if (OldScope)
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