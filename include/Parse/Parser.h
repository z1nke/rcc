#ifndef RCC_PARSE_PARSER_H
#define RCC_PARSE_PARSER_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"
#include "Lex/Token.h"
#include "Sema/Scope.h"

#include <vector>

namespace rcc {

class Stmt;
class Expr;
class Decl;
class TranslationUnitDecl;
class FunctionDecl;
class RecordDecl;
class EnumDecl;
class EnumConstantDecl;
class FieldDecl;
class VarDecl;
class TypedefDecl;

class ASTContext;
class Lexer;
class Sema;
class SourceManager;
class Diagnostic;
class DeclSpec;
class Declarator;

class Parser {
public:
  Parser(Token *CurTok, ASTContext &Ctx, Sema &S, SourceManager &SM);

  ~Parser();

  TranslationUnitDecl *parse();

private:
  void parseExternalDecl(TranslationUnitDecl *TU);
  RecordDecl *parseStructDecl();
  RecordDecl *parseUnionDecl();
  RecordDecl *parseStructUnionDecl(SourceLocation BegLoc, unsigned TagKind);
  std::vector<FieldDecl *> parseFields();

  EnumDecl *parseEnumDecl();
  std::vector<EnumConstantDecl *> parseEnumeratorList(QualType ET);
  EnumConstantDecl *parseEnumerator(QualType ET, int &Val);

  FunctionDecl *parseFunctionBody(FunctionDecl *Func);
  FunctionDecl *parseFunctionDecl(FunctionDecl *Func);
  std::vector<VarDecl *> parseRestVarDecl(SourceLocation BegLoc, DeclSpec &DS,
                                          VarDecl *FirstVar);
  std::vector<TypedefDecl *> parseRestTypedefDecl(SourceLocation BegLoc,
                                                  DeclSpec &DS,
                                                  TypedefDecl *FirstTypedef);

private:
  Stmt *parseStmt();
  Stmt *parseNullStmt();
  Stmt *parseReturnStmt();
  Stmt *parseCompoundStmt();
  Stmt *parseIfStmt();
  Stmt *parseForStmt();
  Stmt *parseWhileStmt();
  Stmt *parseDoWhileStmt();
  Stmt *parseSwitchStmt();
  Stmt *parseCaseStmt();
  Stmt *parseDefaultStmt();
  Stmt *parseBreakStmt();
  Stmt *parseContinueStmt();
  Stmt *parseGotoStmt();
  Stmt *parseLabelStmt();
  Stmt *parseDeclStmt();
  Stmt *parseExprStmt();
  Expr *parseExpr();
  Expr *parseAssign();
  Expr *parseConditionalExpr();
  Expr *parseLogicalOrExpr();
  Expr *parseLogicalAndExpr();
  Expr *parseBitwiseOrExpr();
  Expr *parseBitwiseXorExpr();
  Expr *parseBitwiseAndExpr();
  Expr *parseEqualityExpr();
  Expr *parseConstantExpr();
  Expr *parseRelationalExpr();
  Expr *parseShiftExpr();
  Expr *parseAddExpr();
  Expr *parseMulExpr();
  Expr *parseCastExpr();
  Expr *parseUnaryExpr();
  Expr *parsePrimaryExpr();
  Expr *parseParenOrStmtExpr();
  Expr *parseCallExpr(std::string_view Ident, SourceLocation IdentBegLoc,
                      SourceLocation IdentEndLoc);
  Expr *parsePostfixExpr();

public:
  Scope *getCurrScope() const;
  void enterScope(unsigned ScopeFlags, Decl *ScopeDecl = nullptr);
  void exitScope();

private:
  void parseDeclSpecs(DeclSpec &DS);

  std::vector<Decl *> parseInitDeclaratorList(DeclSpec &DS);
  Decl *parseInitDeclarator(DeclSpec &DS);
  void tryParseVarInit(VarDecl *Var);
  void parseDeclarator(Declarator &D);
  void parseDirectDeclarator(Declarator &D);
  void parseAbstractDeclarator(Declarator &D);
  void parseDirectAbstractDeclarator(Declarator &D);
  void skipPointerQualifiers();
  QualType parseTypeName();
  bool isStorageClassSpec(const Token *Tok);
  bool isTypeName(const Token *Tok);
  void parseTypeSuffix(Declarator &D);
  Expr *parseInitExpr();
  FieldDecl *parseField(DeclSpec &DS);

private:
  template <auto ParseOperand, Token::TokenKind... TKS> Expr *parseBinaryExpr();

  Token::TokenKind getKeyword(std::string_view Ident) const;

private:
  void expect(Token::TokenKind Kind, const char *Prompt);
  void skip(Token::TokenKind Kind);
  void skip();
  bool tryConsume(Token::TokenKind Kind);

private:
  Token *CurTok;
  ASTContext &Ctx;
  Sema &S;
  SourceManager &SM;
  Diagnostic &Diag;
};

} // namespace rcc

#endif