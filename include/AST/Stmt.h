#ifndef RCC_AST_STMT_H
#define RCC_AST_STMT_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"

#include <cstdint>
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
#define STMT(KIND) SK_##KIND,
#define STMT_RANGE(BASE, FIRST, LAST)                                          \
  First##BASE##Kind = SK_##FIRST, Last##BASE##Kind = SK_##LAST,
#include "AST/Stmt.def"
  };

  StmtKind getKind() const { return Kind; }
  const char *getKindStr() const;
  SourceLocation getBeginLoc() const { return BegLoc; }
  SourceLocation getEndLoc() const { return EndLoc; }
  void dump() const;

protected:
  Stmt(StmtKind Kind, SourceLocation BegLoc = SourceLocation(),
       SourceLocation EndLoc = SourceLocation())
      : Kind(Kind), BegLoc(BegLoc), EndLoc(EndLoc) {}

private:
  StmtKind Kind = NoStmtKind;
  SourceLocation BegLoc;
  SourceLocation EndLoc;
};

class DeclStmt final : public Stmt {
public:
  static DeclStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, std::vector<Decl *> Decls);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DeclStmt; }

  unsigned getNumDecls() const { return Decls.size(); }
  Decl *getDecl(unsigned Idx) const { return Decls[Idx]; }
  const std::vector<Decl *> &getDecls() const { return Decls; }

private:
  DeclStmt(SourceLocation BegLoc, SourceLocation EndLoc,
           std::vector<Decl *> Decls);

private:
  std::vector<Decl *> Decls;
};

class CompoundStmt final : public Stmt {
public:
  static CompoundStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                              SourceLocation EndLoc, std::vector<Stmt *> Body);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CompoundStmt; }

  const std::vector<Stmt *> &getBody() const { return Body; }
  std::vector<Stmt *> &getBody() { return Body; }

private:
  CompoundStmt(SourceLocation BegLoc, SourceLocation EndLoc,
               std::vector<Stmt *> Body);

private:
  std::vector<Stmt *> Body;
};

class Expr : public Stmt {
public:
  static bool classof(const Stmt *S) {
    return S->getKind() >= FirstExprKind && S->getKind() <= LastExprKind;
  }

  QualType getType() const { return Ty; }
  const Type *getTypePtr() const { return Ty.getTypePtr(); }
  void setType(QualType T);

protected:
  Expr(StmtKind Kind, SourceLocation BegLoc, SourceLocation EndLoc, QualType T)
      : Stmt(Kind, BegLoc, EndLoc), Ty(T) {}

private:
  QualType Ty;
};

class ReturnStmt final : public Stmt {
public:
  static ReturnStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                            SourceLocation EndLoc, Expr *RetVal);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ReturnStmt; }

  Expr *getRetValue() const { return RetVal; }

private:
  ReturnStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *RetVal);

private:
  Expr *RetVal;
};

class NullStmt final : public Stmt {
public:
  static NullStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc);

  static bool classof(const Stmt *S) { return S->getKind() == SK_NullStmt; }

private:
  NullStmt(SourceLocation BegLoc, SourceLocation EndLoc);
};

class IfStmt final : public Stmt {
public:
  static IfStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                        SourceLocation EndLoc, Expr *Cond, Stmt *Then,
                        Stmt *Else = nullptr);

  static bool classof(const Stmt *S) { return S->getKind() == SK_IfStmt; }

  Expr *getCond() const { return Cond; }
  Stmt *getThen() const { return Then; }
  Stmt *getElse() const { return Else; }

private:
  IfStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond, Stmt *Then,
         Stmt *Else);

private:
  Expr *Cond;
  Stmt *Then;
  Stmt *Else;
};

class ForStmt final : public Stmt {
public:
  static ForStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                         SourceLocation EndLoc, Stmt *Init, Expr *Cond,
                         Expr *Inc, Stmt *Body);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ForStmt; }

  Stmt *getInit() const { return Init; }
  Expr *getCond() const { return Cond; }
  Expr *getInc() const { return Inc; }
  Stmt *getBody() const { return Body; }

private:
  ForStmt(SourceLocation BegLoc, SourceLocation EndLoc, Stmt *Init, Expr *Cond,
          Expr *Inc, Stmt *Body);

private:
  Stmt *Init;
  Expr *Cond;
  Expr *Inc;
  Stmt *Body;
};

class WhileStmt final : public Stmt {
public:
  static WhileStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, Expr *Cond, Stmt *Body);

  static bool classof(const Stmt *S) { return S->getKind() == SK_WhileStmt; }

  Expr *getCond() const { return Cond; }
  Stmt *getBody() const { return Body; }

private:
  WhileStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
            Stmt *Body);

private:
  Expr *Cond;
  Stmt *Body;
};

class UnaryOperator final : public Expr {
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
  const char *getOpcodeStr() const;

private:
  UnaryOperator(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                Expr *SubExpr, Opcode Op);

private:
  Expr *SubExpr;
  Opcode Kind;
};

class BinaryOperator final : public Expr {
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
    BO_Comma,
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
  const char *getOpcodeStr() const;
  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }

private:
  BinaryOperator(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                 SourceLocation OpLoc, Expr *LHS, Expr *RHS, Opcode Op);

private:
  Expr *LHS;
  Expr *RHS;
  Opcode Kind;
  SourceLocation OpLoc;
};

class IntegerLiteral final : public Expr {
public:
  static IntegerLiteral *create(ASTContext &Ctx, SourceLocation BegLoc,
                                SourceLocation EndLoc, QualType T,
                                std::int64_t Val);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_IntegerLiteral;
  }

  std::int64_t getVal() const { return Val; }

private:
  IntegerLiteral(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                 std::int64_t Val);

private:
  // FIXME: Support different integer types.
  std::int64_t Val;
};

class StringLiteral final : public Expr {
public:
  static StringLiteral *create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation EndLoc, QualType T,
                               std::string Str);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_StringLiteral;
  }

  const std::string &getString() const { return Str; }

private:
  StringLiteral(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                std::string Str);

private:
  std::string Str;
};

class ParenExpr : public Expr {
public:
  static ParenExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, QualType T, Expr *SubExpr);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ParenExpr; }

  Expr *getSubExpr() const { return SubExpr; }

protected:
  ParenExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
            Expr *SubExpr);

private:
  Expr *SubExpr;
};

class DeclRefExpr final : public Expr {
public:
  static DeclRefExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, QualType T, ValueDecl *D);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DeclRefExpr; }

  ValueDecl *getDecl() const { return D; }

private:
  DeclRefExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
              ValueDecl *D);

private:
  ValueDecl *D;
};

class ArraySubscriptExpr final : public Expr {
public:
  static ArraySubscriptExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                                    SourceLocation EndLoc, QualType T,
                                    Expr *LHS, Expr *RHS);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_ArraySubscriptExpr;
  }

  Expr *getBase() { return isLHSBase() ? LHS : RHS; }
  const Expr *getBase() const { return isLHSBase() ? LHS : RHS; }
  Expr *getIdx() { return isLHSBase() ? RHS : LHS; }
  const Expr *getIdx() const { return isLHSBase() ? RHS : LHS; }

  const Expr *getLHS() const { return LHS; }
  Expr *getLHS() { return LHS; }
  const Expr *getRHS() const { return RHS; }
  Expr *getRHS() { return RHS; }

private:
  ArraySubscriptExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                     Expr *LHS, Expr *RHS);

private:
  bool isLHSBase() const { return RHS->getType().isIntegerType(); }

private:
  Expr *LHS;
  Expr *RHS;
};

class CallExpr final : public Expr {
public:
  static CallExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, QualType T,
                          DeclRefExpr *Callee, std::vector<Expr *> Args);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CallExpr; }

  DeclRefExpr *getCallee() const { return Callee; }
  FunctionDecl *getCalleeDecl() const;
  unsigned getNumArgs() const { return Args.size(); }
  Expr *getArg(unsigned Idx) const { return Args[Idx]; }
  const std::vector<Expr *> &getArgs() const { return Args; }

private:
  CallExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
           DeclRefExpr *Callee, std::vector<Expr *> Args);

private:
  DeclRefExpr *Callee;
  std::vector<Expr *> Args;
};

class UnaryExprOrTypeTraitExpr final : public Expr {
public:
  static UnaryExprOrTypeTraitExpr *create(ASTContext &Ctx,
                                          SourceLocation BegLoc,
                                          SourceLocation EndLoc, QualType T,
                                          Expr *Ex);

  static UnaryExprOrTypeTraitExpr *create(ASTContext &Ctx,
                                          SourceLocation BegLoc,
                                          SourceLocation EndLoc, QualType T,
                                          Type *Ty);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_UnaryExprOrTypeTraitExpr;
  }

  bool isArgumentType() const { return IsType; }
  std::size_t getSize() const;
  QualType getArgumentType() const {
    assert(isArgumentType());
    return QualType(Argument.Ty);
  }

  Expr *getArgumentExpr() {
    assert(!isArgumentType());
    return static_cast<Expr *>(Argument.Ex);
  }
  const Expr *getArgumentExpr() const {
    return const_cast<UnaryExprOrTypeTraitExpr *>(this)->getArgumentExpr();
  }

private:
  UnaryExprOrTypeTraitExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, Expr *Ex);

  UnaryExprOrTypeTraitExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, Type *Ty);

private:
  bool IsType;
  union {
    Type *Ty;
    Expr *Ex;
  } Argument;
};

class FieldDecl;

class MemberExpr final : public Expr {
public:
  static MemberExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                            SourceLocation OpLoc, SourceLocation EndLoc,
                            QualType T, Expr *Base, FieldDecl *Field,
                            bool IsArrow);

  static bool classof(const Stmt *S) { return S->getKind() == SK_MemberExpr; }

  const Expr *getBase() const { return Base; }
  bool isArrow() const { return IsArrow; }
  FieldDecl *getMemberDecl() const { return Member; }

private:
  MemberExpr(SourceLocation BegLoc, SourceLocation OpLoc, SourceLocation EndLoc,
             QualType T, Expr *Base, FieldDecl *Field, bool IsArrow);

private:
  SourceLocation OpLoc;
  const Expr *Base;
  FieldDecl *Member;
  bool IsArrow;
};

class StmtExpr final : public Expr {
public:
  static StmtExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, QualType T,
                          CompoundStmt *SubStmt);

  static bool classof(const Stmt *S) { return S->getKind() == SK_StmtExpr; }

  CompoundStmt *getSubStmt() { return SubStmt; }
  const CompoundStmt *getSubStmt() const { return SubStmt; }

private:
  StmtExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
           CompoundStmt *SubStmt);

private:
  CompoundStmt *SubStmt;
};

} // namespace rcc

#endif