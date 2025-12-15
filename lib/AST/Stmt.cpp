#include "AST/Stmt.h"
#include "AST/ASTContext.h"
#include "Basic/Casting.h"
#include "Basic/Unreachable.h"

#include <cstdio>

namespace rcc {

void Stmt::setNext(Stmt *Next) { this->Next = Next; }

DeclStmt::DeclStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                   std::vector<Decl *> Decls)
    : Stmt(SK_DeclStmt, BegLoc, EndLoc), Decls(std::move(Decls)) {}

DeclStmt *DeclStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, std::vector<Decl *> Decls) {
  void *Mem = Ctx.Allocate(sizeof(DeclStmt), alignof(DeclStmt));
  return new (Mem) DeclStmt(BegLoc, EndLoc, std::move(Decls));
}

void DeclStmt::dump() const {
  // TODO: Impl.
}

CompoundStmt::CompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                           Stmt *Body)
    : Stmt(SK_CompoundStmt, BegLoc, EndLoc), Body(Body) {}

CompoundStmt *CompoundStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                                   SourceLocation EndLoc, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(CompoundStmt), alignof(CompoundStmt));
  return new (Mem) CompoundStmt(BegLoc, EndLoc, Body);
}

void CompoundStmt::dump() const {
  // TODO: Impl.
}

ReturnStmt::ReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                       Expr *RetVal)
    : Stmt(SK_ReturnStmt, BegLoc, EndLoc), RetVal(RetVal) {}

ReturnStmt *ReturnStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation EndLoc, Expr *RetVal) {
  void *Mem = Ctx.Allocate(sizeof(ReturnStmt), alignof(ReturnStmt));
  return new (Mem) ReturnStmt(BegLoc, EndLoc, RetVal);
}

void ReturnStmt::dump() const {
  // TODO: Impl.
}

NullStmt::NullStmt(SourceLocation BegLoc, SourceLocation EndLoc)
    : Stmt(SK_NullStmt, BegLoc, EndLoc) {}

NullStmt *NullStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc) {
  void *Mem = Ctx.Allocate(sizeof(NullStmt), alignof(NullStmt));
  return new (Mem) NullStmt(BegLoc, EndLoc);
}

void NullStmt::dump() const {}

IfStmt::IfStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
               Stmt *Then, Stmt *Else)
    : Stmt(SK_IfStmt, BegLoc, EndLoc), Cond(Cond), Then(Then), Else(Else) {}

IfStmt *IfStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                       SourceLocation EndLoc, Expr *Cond, Stmt *Then,
                       Stmt *Else) {
  void *Mem = Ctx.Allocate(sizeof(IfStmt), alignof(IfStmt));
  return new (Mem) IfStmt(BegLoc, EndLoc, Cond, Then, Else);
}

void IfStmt::dump() const {
  // TODO: Impl.
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

void ForStmt::dump() const {
  // TODO: Impl.
}

WhileStmt::WhileStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
                     Stmt *Body)
    : Stmt(SK_WhileStmt, BegLoc, EndLoc), Cond(Cond), Body(Body) {}

WhileStmt *WhileStmt::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, Expr *Cond, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(WhileStmt), alignof(WhileStmt));
  return new (Mem) WhileStmt(BegLoc, EndLoc, Cond, Body);
}

void WhileStmt::dump() const {
  // TODO: Impl
}

UnaryOperator::UnaryOperator(SourceLocation BegLoc, SourceLocation EndLoc,
                             Expr *SubExpr, Opcode Op)
    : Expr(SK_UnaryOperator, BegLoc, EndLoc), SubExpr(SubExpr), Kind(Op) {}

UnaryOperator *UnaryOperator::create(ASTContext &Ctx, SourceLocation BegLoc,
                                     SourceLocation EndLoc, Expr *SubExpr,
                                     Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(UnaryOperator), alignof(UnaryOperator));
  return new (Mem) UnaryOperator(BegLoc, EndLoc, SubExpr, Op);
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
  case SK_ForStmt:
    cast<ForStmt>(this)->dump();
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

BinaryOperator::BinaryOperator(SourceLocation BegLoc, SourceLocation EndLoc,
                               SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                               Opcode Op)
    : Expr(SK_BinaryOperator, BegLoc, EndLoc), LHS(std::move(LHS)),
      RHS(std::move(RHS)), Kind(Op) {}

BinaryOperator *BinaryOperator::create(ASTContext &Ctx, SourceLocation BegLoc,
                                       SourceLocation EndLoc,
                                       SourceLocation OpLoc, Expr *LHS,
                                       Expr *RHS, Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(BinaryOperator), alignof(BinaryOperator));
  return new (Mem) BinaryOperator(BegLoc, EndLoc, OpLoc, LHS, RHS, Op);
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

IntergerLiteral::IntergerLiteral(SourceLocation BegLoc, SourceLocation EndLoc,
                                 std::int64_t Val)
    : Expr(SK_IntergerLiteral, BegLoc, EndLoc), Val(Val) {}

IntergerLiteral *IntergerLiteral::create(ASTContext &Ctx, SourceLocation BegLoc,
                                         SourceLocation EndLoc,
                                         std::int64_t Val) {
  void *Mem = Ctx.Allocate(sizeof(IntergerLiteral), alignof(IntergerLiteral));
  return new (Mem) IntergerLiteral(BegLoc, EndLoc, Val);
}

void IntergerLiteral::dump() const { printf("IntegerLiteral %ld\n", Val); }

ParenExpr::ParenExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                     Expr *SubExpr)
    : Expr(SK_ParenExpr, BegLoc, EndLoc), SubExpr(SubExpr) {}

ParenExpr *ParenExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, Expr *SubExpr) {
  void *Mem = Ctx.Allocate(sizeof(ParenExpr), alignof(ParenExpr));
  return new (Mem) ParenExpr(BegLoc, EndLoc, SubExpr);
}

void ParenExpr::dump() const {
  printf("ParenExpr\n");
  printf("`-");
  SubExpr->dump();
}

DeclRefExpr::DeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc, Decl *D)
    : Expr(SK_DeclRefExpr, BegLoc, EndLoc), D(D) {}

DeclRefExpr *DeclRefExpr::create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, Decl *D) {
  void *Mem = Ctx.Allocate(sizeof(DeclRefExpr), alignof(DeclRefExpr));
  return new (Mem) DeclRefExpr(BegLoc, EndLoc, D);
}

void DeclRefExpr::dump() const {
  // TODO: Impl.
}

} // namespace rcc