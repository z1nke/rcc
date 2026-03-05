#ifndef RCC_SEMA_SEMA_H
#define RCC_SEMA_SEMA_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"
#include "Sema/Scope.h"

#include <string_view>
#include <vector>

namespace rcc {

class ASTContext;
class Diagnostic;
class Stmt;
class Expr;
class Decl;
class NamedDecl;
class VarDecl;
class ParamVarDecl;
class Declarator;
class FunctionDecl;
class FieldDecl;

class Sema {
public:
  friend class Parser;

  Sema(ASTContext &Ctx, Diagnostic &Diag) : Ctx(Ctx), Diag(Diag) {}

  ASTContext &getASTContext() const { return Ctx; }

  Decl *actOnDeclarator(Declarator &D);
  VarDecl *actOnVarDecl(Declarator &D, QualType T);
  FieldDecl *actOnFieldDecl(Declarator &D, QualType T, RecordDecl *Parent);
  ParamVarDecl *actOnParamVarDecl(Declarator &D, unsigned Index);
  FunctionDecl *actOnFunctionDecl(ASTContext &Ctx, SourceLocation Loc,
                                  SourceLocation BegLoc, SourceLocation EndLoc,
                                  std::string Name, QualType RetType,
                                  Stmt *Body);
  FunctionDecl *actOnFunctionDecl(Declarator &D, const FunctionType *FT,
                                  Stmt *Body);

  RecordDecl *actOnRecordDecl(SourceLocation Loc, SourceLocation BegLoc,
                              SourceLocation EndLoc, std::string_view Ident);

  void complete(FunctionDecl *FD);

  Stmt *actOnDeclStmt(ASTContext &Ctx, SourceLocation BegLoc,
                      SourceLocation EndLoc, std::vector<Decl *> Decls);
  Stmt *actOnNullStmt(SourceLocation SemiLoc);
  Stmt *actOnReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                        Expr *RetVal);
  Stmt *actOnCompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                          std::vector<Stmt *> Body);
  Stmt *actOnIfStmt(SourceLocation BegLoc, Expr *Cond, Stmt *Then, Stmt *Else);
  Stmt *actOnForStmt(SourceLocation BegLoc, Stmt *Init, Expr *Cond, Expr *Inc,
                     Stmt *Body);
  Stmt *actOnWhileStmt(ASTContext &Ctx, SourceLocation BegLoc, Expr *Cond,
                       Stmt *Body);
  Expr *actOnStringLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::string Str);
  Expr *actOnBinaryOperator(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                            unsigned Op);
  Expr *actOnUnaryOperator(SourceLocation OpLoc, Expr *SubExpr, unsigned Op);
  Expr *actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc, Expr *Ex);
  Expr *actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                      SourceLocation EndLoc, Type *Ty);
  Expr *actOnParenExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                       Expr *SubExpr);
  Expr *actOnDeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                         std::string_view Ident);
  Expr *actOnCallExpr(SourceLocation IdentBegLoc, SourceLocation IdentEndLoc,
                      SourceLocation EndLoc, std::string_view Name,
                      std::vector<Expr *> Args);
  Expr *actOnStmtExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                      Stmt *SubStmt);
  Expr *actOnArraySubscriptExpr(SourceLocation EndLoc, Expr *LHS, Expr *RHS);
  Expr *actOnMemberAccessExpr(SourceLocation OpLoc, SourceLocation EndLoc,
                              Expr *Base, std::string_view Ident, bool IsArrow);

public:
  Scope *getCurrScope() const { return CurrScope; }

private:
  VarDecl *findVar(std::string_view Ident) const;
  TagDecl *findTagDecl(std::string_view Ident) const;
  FunctionDecl *findFunction(std::string_view Name) const;

private:
  void checkScalarType(QualType T);
  void checkIntType(Expr *E);
  void checkArithmeticType(Expr *E);

  QualType getCommonArithmeticType(QualType LType, QualType RType);
  bool canCast(QualType LType, QualType RType);

  QualType getBinaryOperatorType(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                                 unsigned Op);

  QualType getUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
                                unsigned Op);

private:
  QualType getTypeForDeclarator(Declarator &D);
  QualType tryDecayArrayType(QualType T);
  std::size_t getArrayLength(const Expr *E) const;

private:
  ASTContext &Ctx;
  Diagnostic &Diag;
  Scope *CurrScope = nullptr;
  Decl *CurrScopeDecl = nullptr;

  std::vector<VarDecl *> LocalVars;
  std::vector<ParamVarDecl *> Params;
  std::vector<FunctionDecl *> Funcs;
};

} // namespace rcc

#endif