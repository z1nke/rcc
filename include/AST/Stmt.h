#ifndef RCC_AST_STMT_H
#define RCC_AST_STMT_H

#include <cstdint>
#include <string_view>
#include <vector>

namespace rcc {

class ASTContext;
class Decl;
class VarDecl;

class Stmt {
public:
  enum StmtKind {
    NoStmtKind = 0,
    SK_DeclStmt,
    SK_ReturnStmt,
    SK_CompoundStmt,
    SK_NullStmt,
    SK_UnaryOperator,
    SK_BinaryOperator,
    SK_IntergerLiteral,
    SK_ParenExpr,
    SK_DeclRefExpr,
    FirstExprKind = SK_UnaryOperator,
    LastExprKind = SK_DeclRefExpr,
  };

  Stmt(StmtKind Kind) : Kind(Kind) {}

  StmtKind getKind() const { return Kind; }
  Stmt *getNext() const { return Next; }
  void setNext(Stmt *Next);
  void dump() const;

private:
  StmtKind Kind = NoStmtKind;
  Stmt *Next = nullptr;
};

class DeclStmt : public Stmt {
public:
  static DeclStmt *create(ASTContext &Ctx, std::vector<Decl *> Decls);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DeclStmt; }

  void dump() const;

protected:
  DeclStmt(std::vector<Decl *> Decls);

private:
  std::vector<Decl *> Decls;
};

class CompoundStmt : public Stmt {
public:
  static CompoundStmt *create(ASTContext &Ctx, Stmt *Body);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CompoundStmt; }

  void dump() const;

  Stmt *getBody() const { return Body; }

protected:
  CompoundStmt(Stmt *Body);

private:
  Stmt *Body;
};

class Expr : public Stmt {
public:
  Expr(StmtKind Kind) : Stmt(Kind) {}

  static bool classof(const Stmt *S) {
    return S->getKind() >= FirstExprKind && S->getKind() <= LastExprKind;
  }

  void dump() const;
};

class ReturnStmt : public Stmt {
public:
  static ReturnStmt *create(ASTContext &Ctx, Expr *RetVal);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ReturnStmt; }

  void dump() const;

  Expr *getRetValue() const { return RetVal; }

protected:
  ReturnStmt(Expr *RetVal);

private:
  Expr *RetVal;
};

class NullStmt : public Stmt {
public:
  static NullStmt *create(ASTContext &Ctx);

  static bool classof(const Stmt *S) { return S->getKind() == SK_NullStmt; }

  void dump() const;

protected:
  NullStmt();
};

class UnaryOperator : public Expr {
public:
  enum Opcode {
    UO_Plus,
    UO_Minus,
  };

  static UnaryOperator *create(ASTContext &Ctx, Expr *SubExpr, Opcode Op);

  static bool classof(const Stmt *E) {
    return E->getKind() == SK_UnaryOperator;
  }

  Expr *getSubExpr() const { return SubExpr; }

  Opcode getOpcode() const { return Kind; }
  std::string_view getOpcodeStr() const;

  void dump() const;

protected:
  UnaryOperator(Expr *SubExpr, Opcode Op);

private:
  Expr *SubExpr;
  Opcode Kind;
};

class BinaryOperator : public Expr {
public:
  enum Opcode {
    BO_Assign,
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

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_BinaryOperator;
  }

  Opcode getOpcode() const { return Kind; }
  std::string_view getOpcodeStr() const;
  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }

  void dump() const;

protected:
  BinaryOperator(Expr *LHS, Expr *RHS, Opcode Op);

private:
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
  IntergerLiteral(std::int64_t Val);

private:
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
  ParenExpr(Expr *SubExpr);

private:
  Expr *SubExpr;
};

class DeclRefExpr : public Expr {
public:
  static DeclRefExpr *create(ASTContext &Ctx, Decl *D);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DeclRefExpr; }

  void dump() const;

  Decl *getDecl() const { return D; }

protected:
  DeclRefExpr(Decl *D);

private:
  Decl *D;
};

} // namespace rcc

#endif