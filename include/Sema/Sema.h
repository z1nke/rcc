#ifndef RCC_SEMA_SEMA_H
#define RCC_SEMA_SEMA_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"
#include "Sema/Scope.h"

#include <string>
#include <string_view>
#include <unordered_map>
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
class LabelDecl;
class CaseStmt;
class DefaultStmt;
class SwitchCaseStmt;
class InitListExpr;

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

  TagDecl *actOnTagDecl(SourceLocation Loc, SourceLocation BegLoc,
                        SourceLocation EndLoc, std::string_view Ident,
                        unsigned TagKind);
  void actOnTagStartDefinition(SourceLocation Loc, TagDecl *Tag);
  void actOnTagFinishDefinition(TagDecl *Tag, SourceLocation EndLoc);

  EnumConstantDecl *actOnEnumConstantDecl(SourceLocation Loc,
                                          SourceLocation BegLoc,
                                          SourceLocation EndLoc, QualType T,
                                          std::string Name, std::int64_t Val,
                                          const Expr *Init);
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
  void actOnSwitchStmtStart();
  Stmt *actOnSwitchStmt(SourceLocation BegLoc, Expr *Cond, Stmt *Body);
  Stmt *actOnCaseStmt(SourceLocation BegLoc, Expr *LHS, Stmt *SubStmt);
  Stmt *actOnDefaultStmt(SourceLocation BegLoc, Stmt *SubStmt);
  Stmt *actOnBreakStmt(SourceLocation BegLoc, SourceLocation EndLoc);
  Stmt *actOnContinueStmt(SourceLocation BegLoc, SourceLocation EndLoc);
  Stmt *actOnGotoStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                      std::string_view LabelName);
  Stmt *actOnLabelStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                       std::string_view LabelName, Stmt *SubStmt);
  Expr *actOnCharacterLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                              QualType T, unsigned Val);
  Expr *actOnStringLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::string Str);
  Expr *actOnBinaryOperator(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                            unsigned Op);
  Expr *actOnConditionalOperator(SourceLocation QLoc, SourceLocation ColonLoc,
                                 Expr *Cond, Expr *TrueExpr, Expr *FalseExpr);
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

  void checkScalarType(Expr *E) const;
  void checkIntType(Expr *E) const;
  void checkArithmeticType(Expr *E) const;
  void checkSizeofType(SourceLocation BegLoc, QualType T) const;

  QualType usualArithConv(Expr *&LHS, Expr *&RHS, ArithConvKind ACK) const;
  Expr *usualUnaryConv(Expr *E) const;
  Expr *defaultFunctionArrayLvalueConv(Expr *E) const;
  Expr *defaultFunctionArrayConv(Expr *E) const;
  Expr *defaultLvalueConv(Expr *E) const;
  std::optional<unsigned> getCastKind(QualType ToType, QualType FromType) const;

  QualType getBinaryOperatorType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                                 unsigned Op) const;
  QualType getAddOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                        bool IsCompAssign = false) const;
  QualType getSubOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                        bool IsCompAssign = false) const;
  QualType getMulDivOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                           bool IsCompAssign = false) const;
  QualType getBitwiseOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                            bool IsCompAssign = false) const;
  QualType getShiftOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                          bool IsCompAssign = false) const;
  QualType getCompoundAssignOpType(SourceLocation OpLoc, Expr *&LHS, Expr *&RHS,
                                   unsigned Op) const;
  QualType getConditionalOperatorType(SourceLocation OpLoc, Expr *&TrueExpr,
                                      Expr *&FalseExpr) const;

  QualType getUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
                                unsigned Op) const;
  void checkInitList(const InitListExpr *List, QualType ArrTy) const;

public:
  Expr *impCastExprToType(Expr *E, QualType Ty, unsigned CK) const;

private:
  QualType getTypeForDeclarator(Declarator &D) const;
  QualType convertDeclSpecToType(const DeclSpec &DS) const;
  QualType tryDecayArrayType(QualType T) const;
  std::size_t getArrayLength(const Expr *E) const;
  void addDecl(Decl *D);
  [[noreturn]] void actOnDuplicateDefinition(SourceLocation Loc,
                                             std::string_view Name,
                                             unsigned TagKind) const;

private:
  ASTContext &Ctx;
  Diagnostic &Diag;
  Scope *CurrScope = nullptr;
  Decl *CurrScopeDecl = nullptr;

  std::vector<VarDecl *> LocalVars;
  std::vector<ParamVarDecl *> Params;
  std::vector<FunctionDecl *> Funcs;
  std::unordered_map<std::string, LabelDecl *> Labels;

  struct SwitchInfo {
    SwitchCaseStmt *FirstCase = nullptr;
    bool HasDefault = false;
    unsigned NextLabelId = 0;
  };
  std::vector<SwitchInfo> SwitchStack;
};

} // namespace rcc

#endif