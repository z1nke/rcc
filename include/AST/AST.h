#ifndef RCC_AST_AST_H
#define RCC_AST_AST_H

#include <cstdint>

namespace rcc {

class ASTContext;

class Expr {
public:
  enum ExprKind {
    EK_UnaryOperator,
    EK_BinaryOperator,
    EK_IntergerLiteral,
  };

  Expr(ExprKind Kind) : Kind(Kind) {}

  ExprKind getKind() const { return Kind; }

private:
  ExprKind Kind;
};

class UnaryOperator : public Expr {
public:
  enum Opcode {
    UO_Plus,
    UO_Minus,
  };

  static UnaryOperator *create(ASTContext &Ctx, Expr *SubExpr, Opcode Op);

  Expr *getSubExpr() const { return SubExpr; }

  Opcode getOpcode() const { return Kind; }

  static bool classof(const Expr *E) {
    return E->getKind() == EK_UnaryOperator;
  }

protected:
  UnaryOperator(ASTContext &Ctx, Expr *SubExpr, Opcode Op);

private:
  ASTContext &Ctx;
  Expr *SubExpr;
  Opcode Kind;
};

class BinaryOperator : public Expr {
public:
  enum Opcode {
    BO_Add,
    BO_Sub,
    BO_Mul,
    BO_Div,
    BO_EQ,
    BO_NE,
    BO_LT,
    BO_GT,
    BO_LE,
    BO_GE,
  };

  static BinaryOperator *create(ASTContext &Ctx, Expr *LHS, Expr *RHS,
                                Opcode Op);
  Opcode getOpcode() const { return Kind; }
  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }

  static bool classof(const Expr *E) {
    return E->getKind() == EK_BinaryOperator;
  }

protected:
  BinaryOperator(ASTContext &Ctx, Expr *LHS, Expr *RHS, Opcode Op);

private:
  ASTContext &Ctx;
  Expr *LHS;
  Expr *RHS;
  Opcode Kind;
};

class IntergerLiteral : public Expr {
public:
  static IntergerLiteral *create(ASTContext &Ctx, std::int64_t Val);

  static bool classof(const Expr *E) {
    return E->getKind() == EK_IntergerLiteral;
  }

  std::int64_t getVal() const { return Val; }

protected:
  IntergerLiteral(ASTContext &Ctx, std::int64_t Val);

private:
  ASTContext &Ctx;
  // FIXME: Support different integer types.
  std::int64_t Val;
};

} // namespace rcc

#endif