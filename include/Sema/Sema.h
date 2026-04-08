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
class ValueDecl;
class VarDecl;
class ParamVarDecl;
class Declarator;
class FunctionDecl;
class EnumConstantDecl;
class EnumDecl;
class FieldDecl;
class DeclSpec;

class Sema {
public:
  friend class Parser;

  Sema(ASTContext &Ctx, Diagnostic &Diag) : Ctx(Ctx), Diag(Diag) {}

  ASTContext &getASTContext() const { return Ctx; }

  Decl *actOnDeclarator(Declarator &D);
  VarDecl *actOnVarDecl(Declarator &D, QualType T);
  TypedefDecl *actOnTypedefDecl(Declarator &D, QualType T);
  FieldDecl *actOnFieldDecl(Declarator &D, QualType T, RecordDecl *Parent);
  ParamVarDecl *actOnParamVarDecl(Declarator &D, unsigned Index);
  FunctionDecl *actOnFunctionDecl(ASTContext &Ctx, const DeclSpec &DS,
                                  SourceLocation Loc, SourceLocation BegLoc,
                                  SourceLocation EndLoc, std::string Name,
                                  QualType RetType, Stmt *Body);
  FunctionDecl *actOnFunctionDecl(Declarator &D, const FunctionType *FT,
                                  Stmt *Body);

  RecordDecl *actOnRecordDecl(SourceLocation Loc, SourceLocation BegLoc,
                              SourceLocation EndLoc, std::string_view Ident,
                              unsigned TagKind);

  EnumConstantDecl *actOnEnumConstantDecl(SourceLocation Loc,
                                          SourceLocation BegLoc,
                                          SourceLocation EndLoc, QualType T,
                                          std::string Name, std::int64_t Val,
                                          const Expr *Init);
  EnumDecl *actOnEnumDecl(SourceLocation Loc, SourceLocation BegLoc,
                          SourceLocation EndLoc, std::string_view Ident);

  void complete(FunctionDecl *FD);
  void complete(VarDecl *Var, Expr *Init);

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
  Expr *actOnCharacterLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                              QualType T, unsigned Val);
  Expr *actOnStringLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::string Str);
  Expr *actOnBinaryOperator(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                            unsigned Op);
  Expr *actOnUnaryOperator(SourceLocation OpLoc, Expr *SubExpr, unsigned Op);
  Expr *actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc, Expr *Ex);
  Expr *actOnUnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                      SourceLocation EndLoc, const Type *Ty);
  Expr *actOnParenExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                       Expr *SubExpr);
  Expr *actOnDeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                         std::string_view Ident);
  Expr *actOnCallExpr(SourceLocation IdentBegLoc, SourceLocation IdentEndLoc,
                      SourceLocation EndLoc, std::string_view Name,
                      std::vector<Expr *> Args);
  Expr *actOnArraySubscriptExpr(SourceLocation EndLoc, Expr *LHS, Expr *RHS);
  Expr *actOnMemberAccessExpr(SourceLocation OpLoc, SourceLocation EndLoc,
                              Expr *Base, std::string_view Ident, bool IsArrow);
  Expr *actOnCastExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                      Expr *SubExpr, bool IsImplicit);
  Expr *actOnStmtExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                      Stmt *SubStmt);

public:
  Scope *getCurrScope() const { return CurrScope; }

private:
  ValueDecl *findValueDecl(std::string_view Ident) const;
  TagDecl *findTagDecl(std::string_view Ident) const;
  FunctionDecl *findFunction(std::string_view Ident) const;
  TypedefDecl *findTypedef(std::string_view Ident) const;

private:
  enum ArithConvKind {
    ACK_Arithmetic,
    ACK_BitwiseOp,
    ACK_Comparison,
    ACK_Conditional,
    ACK_CompAssign,
  };

  void checkScalarType(QualType T);
  void checkIntType(Expr *E);
  void checkArithmeticType(Expr *E);

  QualType usualArithConv(Expr *&LHS, Expr *&RHS, ArithConvKind ACK);
  Expr *usualUnaryConv(Expr *E);
  Expr *defaultFunctionArrayLvalueConv(Expr *E);
  Expr *defaultFunctionArrayConv(Expr *E);
  Expr *defaultLvalueConv(Expr *E);
  std::optional<unsigned> getCastKind(QualType ToType, QualType FromType);

  QualType getBinaryOperatorType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                                 unsigned Op);
  QualType getAddOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS);
  QualType getSubOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS);
  QualType getMulDivOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                           bool IsCompAssign);

  QualType getUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
                                unsigned Op);

public:
  Expr *impCastExprToType(Expr *E, QualType Ty, unsigned CK);

private:
  QualType getTypeForDeclarator(Declarator &D);
  QualType convertDeclSpecToType(const DeclSpec &DS);
  QualType tryDecayArrayType(QualType T);
  std::size_t getArrayLength(const Expr *E) const;
  void addDecl(Decl *D);

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