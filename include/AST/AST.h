#ifndef RCC_AST_AST_H
#define RCC_AST_AST_H

#include <cstdint>
#include <string_view>

namespace rcc {

class ASTContext;

class Stmt {
public:
  enum StmtKind {
    NoStmtKind = 0,
    SK_UnaryOperator,
    SK_BinaryOperator,
    SK_IntergerLiteral,
    SK_ParenExpr,
    FirstExprKind = SK_UnaryOperator,
    LastExprKind = SK_ParenExpr,
  };

  Stmt(StmtKind Kind) : Kind(Kind) {}

  constexpr Stmt() = default;

  StmtKind getKind() const { return Kind; }
  Stmt *getNext() const { return Next; }
  void setNext(Stmt *Next);
  void dump() const;

private:
  StmtKind Kind = NoStmtKind;
  Stmt *Next = nullptr;
};

class Expr : public Stmt {
public:
  Expr(StmtKind Kind) : Stmt(Kind) {}

  static bool classof(const Stmt *S) {
    return S->getKind() >= FirstExprKind && S->getKind() <= LastExprKind;
  }

  void dump() const;
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
  std::string_view getOpcodeStr() const;

  static bool classof(const Stmt *E) {
    return E->getKind() == SK_UnaryOperator;
  }

  void dump() const;

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
  std::string_view getOpcodeStr() const;
  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_BinaryOperator;
  }

  void dump() const;

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

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_IntergerLiteral;
  }

  void dump() const;

  std::int64_t getVal() const { return Val; }

protected:
  IntergerLiteral(ASTContext &Ctx, std::int64_t Val);

private:
  ASTContext &Ctx;
  // FIXME: Support different integer types.
  std::int64_t Val;
};

class ParenExpr : public Expr {
public:
  static ParenExpr *create(ASTContext &Ctx, Expr *SubExpr);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ParenExpr; }

  void dump() const;

  Expr *getSubExpr() const { return SubExpr; }

protected:
  ParenExpr(ASTContext &Ctx, Expr *SubExpr);

private:
  ASTContext &Ctx;
  Expr *SubExpr;
};

} // namespace rcc

#endif