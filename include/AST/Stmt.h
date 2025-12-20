#ifndef RCC_AST_STMT_H
#define RCC_AST_STMT_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace rcc {

class Token;
class ASTContext;
class Decl;
class ValueDecl;
class FunctionDecl;

class Stmt {
public:
  enum StmtKind {
    NoStmtKind = 0,
    SK_DeclStmt,
    SK_ReturnStmt,
    SK_CompoundStmt,
    SK_NullStmt,
    SK_IfStmt,
    SK_ForStmt,
    SK_WhileStmt,
    SK_UnaryOperator,
    SK_BinaryOperator,
    SK_IntegerLiteral,
    SK_ParenExpr,
    SK_DeclRefExpr,
    SK_CallExpr,
    FirstExprKind = SK_UnaryOperator,
    LastExprKind = SK_CallExpr,
  };

  Stmt(StmtKind Kind, SourceLocation BegLoc = SourceLocation(),
       SourceLocation EndLoc = SourceLocation())
      : Kind(Kind), BegLoc(BegLoc), EndLoc(EndLoc) {}

  StmtKind getKind() const { return Kind; }
  SourceLocation getBeginLoc() const { return BegLoc; }
  SourceLocation getEndLoc() const { return EndLoc; }
  Stmt *getNext() const { return Next; }
  void setNext(Stmt *Next);
  void dump() const;

private:
  StmtKind Kind = NoStmtKind;
  SourceLocation BegLoc;
  SourceLocation EndLoc;
  Stmt *Next = nullptr;
};

class DeclStmt : public Stmt {
public:
  static DeclStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, std::vector<Decl *> Decls);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DeclStmt; }

  void dump() const;

  unsigned getNumDecls() const { return Decls.size(); }
  Decl *getDecl(unsigned Idx) const { return Decls[Idx]; }
  const std::vector<Decl *> &getDecls() const { return Decls; }

protected:
  DeclStmt(SourceLocation BegLoc, SourceLocation EndLoc,
           std::vector<Decl *> Decls);

private:
  std::vector<Decl *> Decls;
};

class CompoundStmt : public Stmt {
public:
  static CompoundStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                              SourceLocation EndLoc, Stmt *Body);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CompoundStmt; }

  void dump() const;

  Stmt *getBody() const { return Body; }

protected:
  CompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc, Stmt *Body);

private:
  Stmt *Body;
};

class Expr : public Stmt {
public:
  static bool classof(const Stmt *S) {
    return S->getKind() >= FirstExprKind && S->getKind() <= LastExprKind;
  }

  void dump() const;

  QualType getType() const { return Ty; }
  void setType(QualType T);

protected:
  Expr(StmtKind Kind, SourceLocation BegLoc, SourceLocation EndLoc, QualType T)
      : Stmt(Kind, BegLoc, EndLoc), Ty(T) {}

private:
  QualType Ty;
};

class ReturnStmt : public Stmt {
public:
  static ReturnStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                            SourceLocation EndLoc, Expr *RetVal);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ReturnStmt; }

  void dump() const;

  Expr *getRetValue() const { return RetVal; }

protected:
  ReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *RetVal);

private:
  Expr *RetVal;
};

class NullStmt : public Stmt {
public:
  static NullStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc);

  static bool classof(const Stmt *S) { return S->getKind() == SK_NullStmt; }

  void dump() const;

protected:
  NullStmt(SourceLocation BegLoc, SourceLocation EndLoc);
};

class IfStmt : public Stmt {
public:
  static IfStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                        SourceLocation EndLoc, Expr *Cond, Stmt *Then,
                        Stmt *Else = nullptr);

  static bool classof(const Stmt *S) { return S->getKind() == SK_IfStmt; }

  void dump() const;

  Expr *getCond() const { return Cond; }
  Stmt *getThen() const { return Then; }
  Stmt *getElse() const { return Else; }

protected:
  IfStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond, Stmt *Then,
         Stmt *Else);

private:
  Expr *Cond;
  Stmt *Then;
  Stmt *Else;
};

class ForStmt : public Stmt {
public:
  static ForStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                         SourceLocation EndLoc, Stmt *Init, Expr *Cond,
                         Expr *Inc, Stmt *Body);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ForStmt; }

  void dump() const;

  Stmt *getInit() const { return Init; }
  Expr *getCond() const { return Cond; }
  Expr *getInc() const { return Inc; }
  Stmt *getBody() const { return Body; }

protected:
  ForStmt(SourceLocation BegLoc, SourceLocation EndLoc, Stmt *Init, Expr *Cond,
          Expr *Inc, Stmt *Body);

private:
  Stmt *Init;
  Expr *Cond;
  Expr *Inc;
  Stmt *Body;
};

class WhileStmt : public Stmt {
public:
  static WhileStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, Expr *Cond, Stmt *Body);

  static bool classof(const Stmt *S) { return S->getKind() == SK_WhileStmt; }

  void dump() const;

  Expr *getCond() const { return Cond; }
  Stmt *getBody() const { return Body; }

protected:
  WhileStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
            Stmt *Body);

private:
  Expr *Cond;
  Stmt *Body;
};

class UnaryOperator : public Expr {
public:
  enum Opcode {
    UO_Plus,
    UO_Minus,
    UO_Addrof,
    UO_Deref,
  };

  static UnaryOperator *create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation EndLoc, QualType T, Expr *SubExpr,
                               Opcode Op);

  static bool classof(const Stmt *E) {
    return E->getKind() == SK_UnaryOperator;
  }

  Expr *getSubExpr() const { return SubExpr; }

  Opcode getOpcode() const { return Kind; }
  std::string_view getOpcodeStr() const;

  void dump() const;

protected:
  UnaryOperator(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                Expr *SubExpr, Opcode Op);

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

  static BinaryOperator *create(ASTContext &Ctx, SourceLocation BegLoc,
                                SourceLocation EndLoc, QualType T,
                                SourceLocation OpLoc, Expr *LHS, Expr *RHS,
                                Opcode Op);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_BinaryOperator;
  }

  Opcode getOpcode() const { return Kind; }
  SourceLocation getOpLocation() const { return OpLoc; }
  std::string_view getOpcodeStr() const;
  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }

  void dump() const;

protected:
  BinaryOperator(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                 SourceLocation OpLoc, Expr *LHS, Expr *RHS, Opcode Op);

private:
  Expr *LHS;
  Expr *RHS;
  Opcode Kind;
  SourceLocation OpLoc;
};

class IntegerLiteral : public Expr {
public:
  static IntegerLiteral *create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T,
                                 std::int64_t Val);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_IntegerLiteral;
  }

  void dump() const;

  std::int64_t getVal() const { return Val; }

protected:
  IntegerLiteral(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                  std::int64_t Val);

private:
  // FIXME: Support different integer types.
  std::int64_t Val;
};

class ParenExpr : public Expr {
public:
  static ParenExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, QualType T, Expr *SubExpr);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ParenExpr; }

  void dump() const;

  Expr *getSubExpr() const { return SubExpr; }

protected:
  ParenExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
            Expr *SubExpr);

private:
  Expr *SubExpr;
};

class DeclRefExpr : public Expr {
public:
  static DeclRefExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, QualType T, ValueDecl *D);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DeclRefExpr; }

  void dump() const;

  ValueDecl *getDecl() const { return D; }

protected:
  DeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
              ValueDecl *D);

private:
  ValueDecl *D;
};

class CallExpr : public Expr {
public:
  static CallExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, QualType T,
                          DeclRefExpr *Callee, std::vector<Expr *> Args);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CallExpr; }

  void dump() const;
  DeclRefExpr *getCallee() const { return Callee; }
  FunctionDecl *getCalleeDecl() const;
  unsigned getNumArgs() const { return Args.size(); }
  Expr *getArg(unsigned Idx) const { return Args[Idx]; }
  const std::vector<Expr *> &getArgs() const { return Args; }

private:
  CallExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
           DeclRefExpr *Callee, std::vector<Expr *> Args);

  DeclRefExpr *Callee;
  std::vector<Expr *> Args;
};

} // namespace rcc

#endif