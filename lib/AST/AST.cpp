#include "AST/AST.h"
#include "AST/ASTContext.h"

namespace rcc {

UnaryOperator::UnaryOperator(ASTContext &Ctx, Expr *SubExpr, Opcode Op)
    : Expr(EK_UnaryOperator), Ctx(Ctx), SubExpr(SubExpr), Kind(Op) {}

UnaryOperator *UnaryOperator::create(ASTContext &Ctx, Expr *SubExpr,
                                     Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(UnaryOperator), alignof(UnaryOperator));
  return new (Mem) UnaryOperator(Ctx, SubExpr, Op);
}

BinaryOperator::BinaryOperator(ASTContext &Ctx, Expr *LHS, Expr *RHS, Opcode Op)
    : Expr(EK_BinaryOperator), Ctx(Ctx), LHS(std::move(LHS)),
      RHS(std::move(RHS)), Kind(Op) {}

BinaryOperator *BinaryOperator::create(ASTContext &Ctx, Expr *LHS, Expr *RHS,
                                       Opcode Op) {
  void *Mem = Ctx.Allocate(sizeof(BinaryOperator), alignof(BinaryOperator));
  return new (Mem) BinaryOperator(Ctx, LHS, RHS, Op);
}

IntergerLiteral::IntergerLiteral(ASTContext &Ctx, std::int64_t Val)
    : Expr(EK_IntergerLiteral), Ctx(Ctx), Val(Val) {}

IntergerLiteral *IntergerLiteral::create(ASTContext &Ctx, std::int64_t Val) {
  void *Mem = Ctx.Allocate(sizeof(IntergerLiteral), alignof(IntergerLiteral));
  return new (Mem) IntergerLiteral(Ctx, Val);
}

} // namespace rcc