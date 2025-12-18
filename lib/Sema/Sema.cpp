#include "Sema/Sema.h"
#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/Diagnostic.h"

namespace rcc {

Stmt *Sema::actOnNullStmt(SourceLocation SemiLoc) {
  return NullStmt::create(Ctx, SemiLoc, SemiLoc);
}

Stmt *Sema::actOnReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                            Expr *RetVal) {
  // TODO: Check return value type and function return type.
  return ReturnStmt::create(Ctx, BegLoc, EndLoc, RetVal);
}

Stmt *Sema::actOnCompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
                              Stmt *Body) {
  return CompoundStmt::create(Ctx, BegLoc, EndLoc, Body);
}

Stmt *Sema::actOnIfStmt(SourceLocation BegLoc, Expr *Cond, Stmt *Then,
                        Stmt *Else) {
  checkScalarType(Cond);
  auto EndLoc = Else ? Else->getEndLoc() : Then->getEndLoc();
  return IfStmt::create(Ctx, BegLoc, EndLoc, Cond, Then, Else);
}

Stmt *Sema::actOnForStmt(SourceLocation BegLoc, Stmt *Init, Expr *Cond,
                         Expr *Inc, Stmt *Body) {
  if (Cond)
    checkScalarType(Cond);
  auto EndLoc = Body->getEndLoc();
  return ForStmt::create(Ctx, BegLoc, EndLoc, Init, Cond, Inc, Body);
}

Stmt *Sema::actOnWhileStmt(ASTContext &Ctx, SourceLocation BegLoc, Expr *Cond,
                           Stmt *Body) {
  checkScalarType(Cond);
  auto EndLoc = Body->getEndLoc();
  return WhileStmt::create(Ctx, BegLoc, EndLoc, Cond, Body);
}

Expr *Sema::actOnBinaryOperator(SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                                unsigned Op) {
  QualType ResType = checkBinaryOperatorType(OpLoc, LHS, RHS, Op);
  auto BegLoc = LHS->getBeginLoc();
  auto EndLoc = RHS->getEndLoc();
  return BinaryOperator::create(Ctx, BegLoc, EndLoc, ResType, OpLoc, LHS, RHS,
                                static_cast<BinaryOperator::Opcode>(Op));
}

Expr *Sema::actOnUnaryOperator(SourceLocation OpLoc, Expr *SubExpr,
                               unsigned Op) {
  QualType ResType = checkUnaryOperatorType(OpLoc, SubExpr, Op);
  return UnaryOperator::create(Ctx, OpLoc, SubExpr->getEndLoc(), ResType,
                               SubExpr, static_cast<UnaryOperator::Opcode>(Op));
}

Expr *Sema::actOnParenExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                           Expr *SubExpr) {
  return ParenExpr::create(Ctx, BegLoc, EndLoc, SubExpr->getType(), SubExpr);
}

void Sema::checkScalarType(Expr *E) {
  if (!E->getType()->isScalarType())
    Diag.fatalAt(E->getBeginLoc(), "expression requires scalar type");
}

void Sema::checkIntType(Expr *E) {
  if (!E->getType().isIntegerType())
    Diag.fatalAt(E->getBeginLoc(), "expression requires integer type");
}

void Sema::checkArithmeticType(Expr *E) {
  if (!E->getType()->isArithmeticType())
    Diag.fatalAt(E->getBeginLoc(), "expression requires arithmetic type");
}

QualType Sema::getCommonArithmeticType(QualType LType, QualType RType) {
  return Ctx.IntTy;
}

bool Sema::canCast(QualType LType, QualType RType) {
  bool LIsPtr = LType->isPointerType();
  bool RIsPtr = RType->isPointerType();
  if (LIsPtr && RIsPtr) {
    return canCast(cast<PointerType>(LType)->getPointeeType(),
                   cast<PointerType>(RType)->getPointeeType());
  }

  bool LIsArithmetic = LType->isArithmeticType();
  bool RIsArithmetic = LType->isArithmeticType();
  if (LIsArithmetic && RIsArithmetic)
    return true;

  return false;
}

QualType Sema::checkBinaryOperatorType(SourceLocation OpLoc, Expr *LHS,
                                       Expr *RHS, unsigned Op) {
  switch (Op) {
  case BinaryOperator::BO_Assign: {
    QualType LType = LHS->getType();
    QualType RType = RHS->getType();
    // FIXME: Check LHS type.
    if (LType.isNull()) {
      LType = RType;
      LHS->setType(LType);
    } else {
      if (!canCast(LType, RType))
        Diag.fatalAt(OpLoc, "invalid operand");
    }

    return LType;
  }
  case BinaryOperator::BO_Add: {
    checkScalarType(LHS);
    checkScalarType(RHS);
    QualType LType = LHS->getType();
    QualType RType = RHS->getType();
    bool LIsPtr = LType->isPointerType();
    bool RIsPtr = RType->isPointerType();
    if (LIsPtr && RIsPtr)
      Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");

    bool LIsArithmetic = LType->isArithmeticType();
    bool RIsArithmetic = LType->isArithmeticType();
    if (LIsArithmetic && RIsArithmetic)
      return getCommonArithmeticType(LType, RType);

    if (LIsPtr) {
      checkIntType(RHS);
      return LType;
    }

    if (RIsPtr) {
      checkIntType(LHS);
      return RType;
    }

    Diag.fatalAt(OpLoc, "invalid operand");
  }
  case BinaryOperator::BO_Sub: {
    checkScalarType(LHS);
    checkScalarType(RHS);
    QualType LType = LHS->getType();
    QualType RType = RHS->getType();
    bool LIsPtr = LType->isPointerType();
    bool RIsPtr = RType->isPointerType();
    if (LIsPtr && RIsPtr)
      return Ctx.IntTy; // FIXME: ptrdiff_t

    bool LIsArithmetic = LType->isArithmeticType();
    bool RIsArithmetic = LType->isArithmeticType();
    if (LIsArithmetic && RIsArithmetic)
      return getCommonArithmeticType(LType, RType);

    if (LIsPtr) {
      checkIntType(RHS);
      return LType;
    }

    Diag.fatalAt(OpLoc, "invalid operand");
  }
  case BinaryOperator::BO_Mul:
  case BinaryOperator::BO_Div: {
    // FIXME: Only int type.
    if (LHS->getType()->isPointerType())
      Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");
    if (RHS->getType()->isPointerType())
      Diag.fatalAt(LHS->getBeginLoc(), "invalid operand");
    return Ctx.IntTy;
  }
  case BinaryOperator::BO_EQ:
  case BinaryOperator::BO_NE:
  case BinaryOperator::BO_LT:
  case BinaryOperator::BO_GT:
  case BinaryOperator::BO_LE:
  case BinaryOperator::BO_GE:
    // TODO: Check operands and add implicit expr.
    return Ctx.IntTy;
  default:
    Diag.fatalAt(OpLoc, "unknown binary opcode");
  }
}

QualType Sema::checkUnaryOperatorType(SourceLocation OpLoc, Expr *SubExpr,
                                      unsigned Op) {
  switch (Op) {
  case UnaryOperator::UO_Plus:
  case UnaryOperator::UO_Minus:
    checkArithmeticType(SubExpr);
    return SubExpr->getType();
  case UnaryOperator::UO_Addrof:
    return Ctx.getPointerType(SubExpr->getType());
  case UnaryOperator::UO_Deref: {
    const auto *PtrType = SubExpr->getType()->getAs<PointerType>();
    if (!PtrType) {
      Diag.fatalAt(SubExpr->getBeginLoc(),
                   "dereference requires pointer operand");
    }
    return PtrType->getPointeeType();
  }
  default:
    Diag.fatalAt(OpLoc, "unknown unary opcode");
  }
}

} // namespace rcc