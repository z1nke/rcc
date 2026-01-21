#include "AST/Stmt.h"
#include "AST/ASTContext.h"
#include "AST/ASTDumper.h"
#include "AST/Decl.h"
#include "Basic/Casting.h"
#include "Basic/Unreachable.h"

namespace rcc {

const char *Stmt::getKindStr() const {
  switch (Kind) {
#define STMT(KIND)                                                             \
  case SK_##KIND:                                                              \
    return #KIND;
#include "AST/Stmt.def"
  default:
    RCC_UNREACHABLE("Unknown statement");
  }
}

DeclStmt::DeclStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                   std::vector<Decl *> Decls)
    : Stmt(SK_DeclStmt, BegLoc, EndLoc), Decls(std::move(Decls)) {}

DeclStmt *DeclStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, std::vector<Decl *> Decls) {
  void *Mem = Ctx.Allocate(sizeof(DeclStmt), alignof(DeclStmt));
  return new (Mem) DeclStmt(BegLoc, EndLoc, std::move(Decls));
}

CompoundStmt::CompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                           std::vector<Stmt *> Body)
    : Stmt(SK_CompoundStmt, BegLoc, EndLoc), Body(std::move(Body)) {}

CompoundStmt *CompoundStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                                   SourceLocation EndLoc,
                                   std::vector<Stmt *> Body) {
  void *Mem = Ctx.Allocate(sizeof(CompoundStmt), alignof(CompoundStmt));
  return new (Mem) CompoundStmt(BegLoc, EndLoc, std::move(Body));
}

ReturnStmt::ReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                       Expr *RetVal)
    : Stmt(SK_ReturnStmt, BegLoc, EndLoc), RetVal(RetVal) {}

ReturnStmt *ReturnStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation EndLoc, Expr *RetVal) {
  void *Mem = Ctx.Allocate(sizeof(ReturnStmt), alignof(ReturnStmt));
  return new (Mem) ReturnStmt(BegLoc, EndLoc, RetVal);
}

NullStmt::NullStmt(SourceLocation BegLoc, SourceLocation EndLoc)
    : Stmt(SK_NullStmt, BegLoc, EndLoc) {}

NullStmt *NullStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc) {
  void *Mem = Ctx.Allocate(sizeof(NullStmt), alignof(NullStmt));
  return new (Mem) NullStmt(BegLoc, EndLoc);
}

IfStmt::IfStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
               Stmt *Then, Stmt *Else)
    : Stmt(SK_IfStmt, BegLoc, EndLoc), Cond(Cond), Then(Then), Else(Else) {}

IfStmt *IfStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                       SourceLocation EndLoc, Expr *Cond, Stmt *Then,
                       Stmt *Else) {
  void *Mem = Ctx.Allocate(sizeof(IfStmt), alignof(IfStmt));
  return new (Mem) IfStmt(BegLoc, EndLoc, Cond, Then, Else);
}

ForStmt::ForStmt(SourceLocation BegLoc, SourceLocation EndLoc, Stmt *Init,
                 Expr *Cond, Expr *Inc, Stmt *Body)
    : Stmt(SK_ForStmt, BegLoc, EndLoc), Init(Init), Cond(Cond), Inc(Inc),
      Body(Body) {}

ForStmt *ForStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                         SourceLocation EndLoc, Stmt *Init, Expr *Cond,
                         Expr *Inc, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(ForStmt), alignof(ForStmt));
  return new (Mem) ForStmt(BegLoc, EndLoc, Init, Cond, Inc, Body);
}

WhileStmt::WhileStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
                     Stmt *Body)
    : Stmt(SK_WhileStmt, BegLoc, EndLoc), Cond(Cond), Body(Body) {}

WhileStmt *WhileStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, Expr *Cond, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(WhileStmt), alignof(WhileStmt));
  return new (Mem) WhileStmt(BegLoc, EndLoc, Cond, Body);
}

UnaryOperator::UnaryOperator(SourceLocation BegLoc, SourceLocation EndLoc,
                             QualType T, Expr *SubExpr, Opcode Op)
    : Expr(SK_UnaryOperator, BegLoc, EndLoc, T), SubExpr(SubExpr), Kind(Op) {}

UnaryOperator *UnaryOperator::create(ASTContext &Ctx, SourceLocation BegLoc,
                                     SourceLocation EndLoc, QualType T,
                                     Expr *SubExpr, Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(UnaryOperator), alignof(UnaryOperator));
  return new (Mem) UnaryOperator(BegLoc, EndLoc, T, SubExpr, Op);
}

const char *UnaryOperator::getOpcodeStr() const {
  switch (Kind) {
  case UO_Plus:
    return "+";
  case UO_Minus:
    return "-";
  case UO_Addrof:
    return "&";
  case UO_Deref:
    return "*";
  default:
    RCC_UNREACHABLE("[AST] Unknown unary opcode");
  }
}

void Stmt::dump() const {
  ASTDumper Dumper;
  Dumper.visit(this);
}

void Expr::setType(QualType T) {
  Ty = T;
  // FIXME: Temporary handling.
  if (auto *Ref = dyn_cast<DeclRefExpr>(this))
    Ref->getDecl()->setType(T);
}

BinaryOperator::BinaryOperator(SourceLocation BegLoc, SourceLocation EndLoc,
                               QualType T, SourceLocation OpLoc, Expr *LHS,
                               Expr *RHS, Opcode Op)
    : Expr(SK_BinaryOperator, BegLoc, EndLoc, T), LHS(std::move(LHS)),
      RHS(std::move(RHS)), Kind(Op) {}

BinaryOperator *BinaryOperator::create(ASTContext &Ctx, SourceLocation BegLoc,
                                       SourceLocation EndLoc, QualType T,
                                       SourceLocation OpLoc, Expr *LHS,
                                       Expr *RHS, Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(BinaryOperator), alignof(BinaryOperator));
  return new (Mem) BinaryOperator(BegLoc, EndLoc, T, OpLoc, LHS, RHS, Op);
}

const char *BinaryOperator::getOpcodeStr() const {
  switch (Kind) {
  case BO_Add:
    return "+";
  case BO_Sub:
    return "-";
  case BO_Mul:
    return "*";
  case BO_Div:
    return "/";
  case BO_Assign:
    return "=";
  case BO_EQ:
    return "==";
  case BO_NE:
    return "!=";
  case BO_LT:
    return "<";
  case BO_GT:
    return ">";
  case BO_LE:
    return "<=";
  case BO_GE:
    return ">=";
  default:
    RCC_UNREACHABLE("[AST] Unknown binary opcode");
  }
}

IntegerLiteral::IntegerLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                               QualType T, std::int64_t Val)
    : Expr(SK_IntegerLiteral, BegLoc, EndLoc, T), Val(Val) {}

IntegerLiteral *IntegerLiteral::create(ASTContext &Ctx, SourceLocation BegLoc,
                                       SourceLocation EndLoc, QualType T,
                                       std::int64_t Val) {
  void *Mem = Ctx.Allocate(sizeof(IntegerLiteral), alignof(IntegerLiteral));
  return new (Mem) IntegerLiteral(BegLoc, EndLoc, T, Val);
}

StringLiteral *StringLiteral::create(ASTContext &Ctx, SourceLocation BegLoc,
                                     SourceLocation EndLoc, QualType T,
                                     std::string Str) {
  void *Mem = Ctx.Allocate(sizeof(StringLiteral), alignof(StringLiteral));
  return new (Mem) StringLiteral(BegLoc, EndLoc, T, std::move(Str));
}

StringLiteral::StringLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                             QualType T, std::string Str)
    : Expr(SK_StringLiteral, BegLoc, EndLoc, T), Str(std::move(Str)) {}

ParenExpr::ParenExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                     Expr *SubExpr)
    : Expr(SK_ParenExpr, BegLoc, EndLoc, T), SubExpr(SubExpr) {}

ParenExpr *ParenExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, QualType T, Expr *SubExpr) {
  void *Mem = Ctx.Allocate(sizeof(ParenExpr), alignof(ParenExpr));
  return new (Mem) ParenExpr(BegLoc, EndLoc, T, SubExpr);
}

DeclRefExpr::DeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                         QualType T, ValueDecl *D)
    : Expr(SK_DeclRefExpr, BegLoc, EndLoc, T), D(D) {}

DeclRefExpr *DeclRefExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T,
                                 ValueDecl *D) {
  void *Mem = Ctx.Allocate(sizeof(DeclRefExpr), alignof(DeclRefExpr));
  return new (Mem) DeclRefExpr(BegLoc, EndLoc, T, D);
}

ArraySubscriptExpr::ArraySubscriptExpr(SourceLocation BegLoc,
                                       SourceLocation EndLoc, QualType T,
                                       Expr *LHS, Expr *RHS)
    : Expr(SK_ArraySubscriptExpr, BegLoc, EndLoc, T), LHS(LHS), RHS(RHS) {}

ArraySubscriptExpr *ArraySubscriptExpr::create(ASTContext &Ctx,
                                               SourceLocation BegLoc,
                                               SourceLocation EndLoc,
                                               QualType T, Expr *LHS,
                                               Expr *RHS) {
  static constexpr std::size_t Size = sizeof(ArraySubscriptExpr);
  void *Mem = Ctx.Allocate(Size, alignof(ArraySubscriptExpr));
  return new (Mem) ArraySubscriptExpr(BegLoc, EndLoc, T, LHS, RHS);
}

CallExpr::CallExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                   DeclRefExpr *Callee, std::vector<Expr *> Args)
    : Expr(SK_CallExpr, BegLoc, EndLoc, T), Callee(Callee), Args(Args) {}

CallExpr *CallExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, QualType T,
                           DeclRefExpr *Callee, std::vector<Expr *> Args) {
  void *Mem = Ctx.Allocate(sizeof(CallExpr), alignof(CallExpr));
  return new (Mem) CallExpr(BegLoc, EndLoc, T, Callee, Args);
}

FunctionDecl *CallExpr::getCalleeDecl() const {
  if (!Callee)
    return nullptr;
  return dyn_cast<FunctionDecl>(Callee->getDecl());
}

UnaryExprOrTypeTraitExpr *
UnaryExprOrTypeTraitExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T, Expr *Ex) {
  static constexpr std::size_t Size = sizeof(UnaryExprOrTypeTraitExpr);
  void *Mem = Ctx.Allocate(Size, alignof(UnaryExprOrTypeTraitExpr));
  return new (Mem) UnaryExprOrTypeTraitExpr(BegLoc, EndLoc, T, Ex);
}

UnaryExprOrTypeTraitExpr *
UnaryExprOrTypeTraitExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T, Type *Ty) {
  static constexpr std::size_t Size = sizeof(UnaryExprOrTypeTraitExpr);
  void *Mem = Ctx.Allocate(Size, alignof(UnaryExprOrTypeTraitExpr));
  return new (Mem) UnaryExprOrTypeTraitExpr(BegLoc, EndLoc, T, Ty);
}

UnaryExprOrTypeTraitExpr::UnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                                   SourceLocation EndLoc,
                                                   QualType T, Expr *Ex)
    : Expr(SK_UnaryExprOrTypeTraitExpr, BegLoc, EndLoc, T), IsType(false) {
  Argument.Ex = Ex;
}

UnaryExprOrTypeTraitExpr::UnaryExprOrTypeTraitExpr(SourceLocation BegLoc,
                                                   SourceLocation EndLoc,
                                                   QualType T, Type *Ty)
    : Expr(SK_UnaryExprOrTypeTraitExpr, BegLoc, EndLoc, T), IsType(true) {
  Argument.Ty = Ty;
}

std::size_t UnaryExprOrTypeTraitExpr::getSize() const {
  if (isArgumentType())
    return Argument.Ty->getSize();

  QualType Ty = Argument.Ex->getType();
  return Ty->getSize();
}

} // namespace rcc