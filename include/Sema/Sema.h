#ifndef RCC_SEMA_SEMA_H
#define RCC_SEMA_SEMA_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"

#include <string_view>
#include <vector>

namespace rcc {

class ASTContext;
class Diagnostic;
class Stmt;
class Expr;
class Decl;
class VarDecl;
class ParamVarDecl;
class Declarator;
class FunctionDecl;

class Sema {
public:
  Sema(ASTContext &Ctx, Diagnostic &Diag) : Ctx(Ctx), Diag(Diag) {}

  ASTContext &getASTContext() const { return Ctx; }

  Decl *actOnDeclarator(Declarator &D);
  VarDecl *actOnVarDecl(Declarator &D, QualType T);
  ParamVarDecl *actOnParamVarDecl(Declarator &D, unsigned Index);
  FunctionDecl *actOnFunctionDecl(ASTContext &Ctx, SourceLocation Loc,
                                  SourceLocation BegLoc, SourceLocation EndLoc,
                                  std::string Name, QualType RetType,
                                  Stmt *Body);
  FunctionDecl *actOnFunctionDecl(Declarator &D, const FunctionType *FT,
                                  Stmt *Body);

  void complete(FunctionDecl *FD);

  Stmt *actOnDeclStmt(ASTContext &Ctx, SourceLocation BegLoc,
                      SourceLocation EndLoc, std::vector<Decl *> Decls);
  Stmt *actOnNullStmt(SourceLocation SemiLoc);
  Stmt *actOnReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                        Expr *RetVal);
  Stmt *actOnCompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                          Stmt *Body);
  Stmt *actOnIfStmt(SourceLocation BegLoc, Expr *Cond, Stmt *Then, Stmt *Else);
  Stmt *actOnForStmt(SourceLocation BegLoc, Stmt *Init, Expr *Cond, Expr *Inc,
                     Stmt *Body);
  Stmt *actOnWhileStmt(ASTContext &Ctx, SourceLocation BegLoc, Expr *Cond,
                       Stmt *Body);
  Expr *actOnBinaryOperator(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                            unsigned Op);
  Expr *actOnUnaryOperator(SourceLocation OpLoc, Expr *SubExpr, unsigned Op);
  Expr *actOnParenExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                       Expr *SubExpr);
  Expr *actOnDeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                         std::string_view Ident);
  Expr *actOnCallExpr(SourceLocation IdentBegLoc, SourceLocation IdentEndLoc,
                      SourceLocation EndLoc, std::string_view Name,
                      std::vector<Expr *> Args);

private:
  VarDecl *findVar(std::string_view Ident);
  FunctionDecl *findFunction(std::string_view Name);

private:
  void checkScalarType(QualType T);
  void checkIntType(Expr *E);
  void checkArithmeticType(Expr *E);

  QualType getCommonArithmeticType(QualType LType, QualType RType);
  bool canCast(QualType LType, QualType RType);

  QualType checkBinaryOperatorType(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                                   unsigned Op);

  QualType checkUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
                                  unsigned Op);

private:
  QualType getTypeForDeclarator(Declarator &D);
  QualType tryDecayArrayType(QualType T);
  std::size_t getArrayLength(const Expr *E) const;

private:
  ASTContext &Ctx;
  Diagnostic &Diag;
  std::vector<VarDecl *> LocalVars;
  std::vector<ParamVarDecl *> Params;
  std::vector<FunctionDecl *> Funcs;
};

} // namespace rcc

#endif