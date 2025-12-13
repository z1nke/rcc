#include "AST/Stmt.h"
#include "AST/ASTContext.h"
#include "Basic/Casting.h"
#include "Basic/Unreachable.h"

#include <cstdio>

namespace rcc {

void Stmt::setNext(Stmt *Next) { this->Next = Next; }

DeclStmt::DeclStmt(std::vector<Decl *> Decls)
    : Stmt(SK_DeclStmt), Decls(std::move(Decls)) {}

DeclStmt *DeclStmt::create(ASTContext &Ctx, std::vector<Decl *> Decls) {
  void *Mem = Ctx.Allocate(sizeof(DeclStmt), alignof(DeclStmt));
  return new (Mem) DeclStmt(std::move(Decls));
}

void DeclStmt::dump() const {
  // TODO: Impl.
}

CompoundStmt::CompoundStmt(Stmt *Body) : Stmt(SK_CompoundStmt), Body(Body) {}

CompoundStmt *CompoundStmt::create(ASTContext &Ctx, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(CompoundStmt), alignof(CompoundStmt));
  return new (Mem) CompoundStmt(Body);
}

void CompoundStmt::dump() const {
  // TODO: Impl.
}

ReturnStmt::ReturnStmt(Expr *RetVal) : Stmt(SK_ReturnStmt), RetVal(RetVal) {}

ReturnStmt *ReturnStmt::create(ASTContext &Ctx, Expr *RetVal) {
  void *Mem = Ctx.Allocate(sizeof(ReturnStmt), alignof(ReturnStmt));
  return new (Mem) ReturnStmt(RetVal);
}

void ReturnStmt::dump() const {
  // TODO: Impl.
}

NullStmt *NullStmt::create(ASTContext &Ctx) {
  void *Mem = Ctx.Allocate(sizeof(NullStmt), alignof(NullStmt));
  return new (Mem) NullStmt();
}

void NullStmt::dump() const {}

NullStmt::NullStmt() : Stmt(SK_NullStmt) {}

IfStmt *IfStmt::create(ASTContext &Ctx, Expr *Cond, Stmt *Then, Stmt *Else) {
  void *Mem = Ctx.Allocate(sizeof(IfStmt), alignof(IfStmt));
  return new (Mem) IfStmt(Cond, Then, Else);
}

void IfStmt::dump() const {
  // TODO: Impl.
}

IfStmt::IfStmt(Expr *Cond, Stmt *Then, Stmt *Else)
    : Stmt(SK_IfStmt), Cond(Cond), Then(Then), Else(Else) {}

UnaryOperator::UnaryOperator(Expr *SubExpr, Opcode Op)
    : Expr(SK_UnaryOperator), SubExpr(SubExpr), Kind(Op) {}

UnaryOperator *UnaryOperator::create(ASTContext &Ctx, Expr *SubExpr,
                                     Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(UnaryOperator), alignof(UnaryOperator));
  return new (Mem) UnaryOperator(SubExpr, Op);
}

std::string_view UnaryOperator::getOpcodeStr() const {
  switch (Kind) {
  case UO_Plus:
    return "+";
  case UO_Minus:
    return "-";
  default:
    RCC_UNREACHABLE("[AST] Unknown unary opcode");
  }
}

void Stmt::dump() const {
  // FIXME: Use AST visitor dump ast node.
  if (const auto *E = dyn_cast<Expr>(this)) {
    E->dump();
    return;
  }

  switch (getKind()) {
  case SK_DeclStmt:
    cast<DeclStmt>(this)->dump();
    break;
  case SK_CompoundStmt:
    cast<CompoundStmt>(this)->dump();
    break;
  case SK_ReturnStmt:
    cast<ReturnStmt>(this)->dump();
    break;
  case SK_NullStmt:
    cast<NullStmt>(this)->dump();
    break;
  case SK_IfStmt:
    cast<IfStmt>(this)->dump();
    break;
  default:
    RCC_UNREACHABLE("Unknown stmt kind");
    break;
  }
}

void Expr::dump() const {
  // FIXME: Use AST visitor dump ast node.
  switch (getKind()) {
  case Stmt::SK_UnaryOperator:
    cast<UnaryOperator>(this)->dump();
    break;
  case Stmt::SK_BinaryOperator:
    cast<BinaryOperator>(this)->dump();
    break;
  case Stmt::SK_IntergerLiteral:
    cast<IntergerLiteral>(this)->dump();
    break;
  case Stmt::SK_ParenExpr:
    cast<ParenExpr>(this)->dump();
    break;
  default:
    RCC_UNREACHABLE("Unknown expr kind");
    break;
  }
}

void UnaryOperator::dump() const {
  // FIXME: Use AST visitor dump ast node.
  printf("UnaryOperator prefix '%s'\n", getOpcodeStr().data());
  printf("`- ");
  SubExpr->dump();
}

BinaryOperator::BinaryOperator(Expr *LHS, Expr *RHS, Opcode Op)
    : Expr(SK_BinaryOperator), LHS(std::move(LHS)), RHS(std::move(RHS)),
      Kind(Op) {}

BinaryOperator *BinaryOperator::create(ASTContext &Ctx, Expr *LHS, Expr *RHS,
                                       Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(BinaryOperator), alignof(BinaryOperator));
  return new (Mem) BinaryOperator(LHS, RHS, Op);
}

std::string_view BinaryOperator::getOpcodeStr() const {
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

void BinaryOperator::dump() const {
  // FIXME: Use AST visitor dump ast node.
  printf("BinaryOperator '%s'\n", getOpcodeStr().data());
  printf("|-");
  LHS->dump();
  printf("`-");
  RHS->dump();
}

IntergerLiteral::IntergerLiteral(std::int64_t Val)
    : Expr(SK_IntergerLiteral), Val(Val) {}

IntergerLiteral *IntergerLiteral::create(ASTContext &Ctx, std::int64_t Val) {
  void *Mem = Ctx.Allocate(sizeof(IntergerLiteral), alignof(IntergerLiteral));
  return new (Mem) IntergerLiteral(Val);
}

void IntergerLiteral::dump() const { printf("IntegerLiteral %ld\n", Val); }

ParenExpr::ParenExpr(Expr *SubExpr) : Expr(SK_ParenExpr), SubExpr(SubExpr) {}

ParenExpr *ParenExpr::create(ASTContext &Ctx, Expr *SubExpr) {
  void *Mem = Ctx.Allocate(sizeof(ParenExpr), alignof(ParenExpr));
  return new (Mem) ParenExpr(SubExpr);
}

void ParenExpr::dump() const {
  printf("ParenExpr\n");
  printf("`-");
  SubExpr->dump();
}

DeclRefExpr::DeclRefExpr(Decl *D) : Expr(SK_DeclRefExpr), D(D) {}

DeclRefExpr *DeclRefExpr::create(ASTContext &Ctx, Decl *D) {
  void *Mem = Ctx.Allocate(sizeof(DeclRefExpr), alignof(DeclRefExpr));
  return new (Mem) DeclRefExpr(D);
}

void DeclRefExpr::dump() const {
  // TODO: Impl.
}

} // namespace rcc