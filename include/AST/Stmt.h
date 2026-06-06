#ifndef RCC_AST_STMT_H
#define RCC_AST_STMT_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace rcc {

class Token;
class ASTContext;
class Decl;
class ValueDecl;
class VarDecl;
class FunctionDecl;
class LabelDecl;

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

  Expr *ignoreImpCasts();
  const Expr *ignoreImpCasts() const {
    return const_cast<Expr *>(this)->ignoreImpCasts();
  }

  Expr *ignoreCasts();
  const Expr *ignoreCasts() const {
    return const_cast<Expr *>(this)->ignoreCasts();
  }

  Expr *ignoreParens();
  const Expr *ignoreParens() const {
    return const_cast<Expr *>(this)->ignoreParens();
  }

  Expr *ignoreParenImpCasts();
  const Expr *ignoreParenImpCasts() const {
    return const_cast<Expr *>(this)->ignoreParenImpCasts();
  }

  Expr *ignoreParenCasts();
  const Expr *ignoreParenCasts() const {
    return const_cast<Expr *>(this)->ignoreParenCasts();
  }

public:
  using EvalResult =
      std::variant<std::int64_t, std::uint64_t, double, char, bool>;
  std::optional<EvalResult> evaluate() const;
  std::optional<std::int64_t> evaluateAsInt() const;
  std::optional<double> evaluateAsDouble() const;
  std::optional<bool> evaluateAsBool() const;

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

class DoWhileStmt final : public Stmt {
public:
  static DoWhileStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, Stmt *Body, Expr *Cond);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DoWhileStmt; }

  Stmt *getBody() const { return Body; }
  Expr *getCond() const { return Cond; }

private:
  DoWhileStmt(SourceLocation BegLoc, SourceLocation EndLoc, Stmt *Body,
              Expr *Cond);

private:
  Stmt *Body;
  Expr *Cond;
};

class CaseStmt;
class DefaultStmt;
class SwitchCaseStmt;

class SwitchStmt final : public Stmt {
public:
  static SwitchStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                            SourceLocation EndLoc, Expr *Cond, Stmt *Body,
                            SwitchCaseStmt *FirstCase);

  static bool classof(const Stmt *S) { return S->getKind() == SK_SwitchStmt; }

  Expr *getCond() const { return Cond; }
  Stmt *getBody() const { return Body; }
  SwitchCaseStmt *getSwitchCaseList() const { return FirstCase; }

private:
  SwitchStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *Cond,
             Stmt *Body, SwitchCaseStmt *FirstCase);

private:
  Expr *Cond;
  Stmt *Body;
  SwitchCaseStmt *FirstCase;
};

class SwitchCaseStmt : public Stmt {
public:
  Stmt *getSubStmt() const { return SubStmt; }
  unsigned getLabelId() const { return LabelId; }
  SwitchCaseStmt *getNextSwitchCase() const { return NextSwitchCase; }
  void setNextSwitchCase(SwitchCaseStmt *Next) { NextSwitchCase = Next; }

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_CaseStmt || S->getKind() == SK_DefaultStmt;
  }

protected:
  SwitchCaseStmt(StmtKind Kind, SourceLocation BegLoc, SourceLocation EndLoc,
                 Stmt *SubStmt, unsigned LabelId)
      : Stmt(Kind, BegLoc, EndLoc), SubStmt(SubStmt), LabelId(LabelId) {}

private:
  Stmt *SubStmt;
  unsigned LabelId;
  SwitchCaseStmt *NextSwitchCase = nullptr;
};

class CaseStmt final : public SwitchCaseStmt {
public:
  static CaseStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, Expr *LHS, Stmt *SubStmt,
                          std::int64_t CaseValue, unsigned LabelId);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CaseStmt; }

  Expr *getLHS() const { return LHS; }
  std::int64_t getCaseValue() const { return CaseValue; }

private:
  CaseStmt(SourceLocation BegLoc, SourceLocation EndLoc, Expr *LHS,
           Stmt *SubStmt, std::int64_t CaseValue, unsigned LabelId);

private:
  Expr *LHS;
  std::int64_t CaseValue;
};

class DefaultStmt final : public SwitchCaseStmt {
public:
  static DefaultStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                             SourceLocation EndLoc, Stmt *SubStmt,
                             unsigned LabelId);

  static bool classof(const Stmt *S) { return S->getKind() == SK_DefaultStmt; }

private:
  DefaultStmt(SourceLocation BegLoc, SourceLocation EndLoc, Stmt *SubStmt,
              unsigned LabelId);
};

class BreakStmt final : public Stmt {
public:
  static BreakStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc);

  static bool classof(const Stmt *S) { return S->getKind() == SK_BreakStmt; }

private:
  BreakStmt(SourceLocation BegLoc, SourceLocation EndLoc);
};

class ContinueStmt final : public Stmt {
public:
  static ContinueStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                              SourceLocation EndLoc);

  static bool classof(const Stmt *S) { return S->getKind() == SK_ContinueStmt; }

private:
  ContinueStmt(SourceLocation BegLoc, SourceLocation EndLoc);
};

class GotoStmt final : public Stmt {
public:
  static GotoStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, LabelDecl *Label);

  static bool classof(const Stmt *S) { return S->getKind() == SK_GotoStmt; }

  LabelDecl *getLabel() const { return Label; }

private:
  GotoStmt(SourceLocation BegLoc, SourceLocation EndLoc, LabelDecl *Label);

private:
  LabelDecl *Label;
};

class LabelStmt final : public Stmt {
public:
  static LabelStmt *create(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, LabelDecl *Label, Stmt *Sub);

  static bool classof(const Stmt *S) { return S->getKind() == SK_LabelStmt; }

  LabelDecl *getDecl() const { return Label; }
  Stmt *getSubStmt() const { return SubStmt; }

private:
  LabelStmt(SourceLocation BegLoc, SourceLocation EndLoc, LabelDecl *Label,
            Stmt *Sub);

private:
  LabelDecl *Label;
  Stmt *SubStmt;
};

class UnaryOperator final : public Expr {
public:
  enum Opcode {
    UO_Plus,
    UO_Minus,
    UO_LNot,
    UO_Not,
    UO_Addrof,
    UO_Deref,
    UO_PreInc,
    UO_PreDec,
    UO_PostInc,
    UO_PostDec,
  };

  static UnaryOperator *create(ASTContext &Ctx, SourceLocation BegLoc,
                               SourceLocation EndLoc, QualType T, Expr *SubExpr,
                               Opcode Op);

  static bool classof(const Stmt *E) {
    return E->getKind() == SK_UnaryOperator;
  }

  Expr *getSubExpr() const { return SubExpr; }

  Opcode getOpcode() const { return Kind; }
  const char *getOpcodeStr() const { return getOpcodeStr(getOpcode()); }
  static const char *getOpcodeStr(Opcode Op);

  bool isIncrement() const { return isIncrement(getOpcode()); }
  static bool isIncrement(Opcode Op) {
    return Op == UO_PreInc || Op == UO_PostInc;
  }

  bool isDecrement() const { return getOpcode() == UO_PreDec; }
  static bool isDecrement(Opcode Op) {
    return Op == UO_PreDec || Op == UO_PostDec;
  }

  bool isPrefix() const { return isPrefix(getOpcode()); }
  static bool isPrefix(Opcode Op) { return Op == UO_PreInc || Op == UO_PreDec; }

  bool isPostfix() const { return isPostfix(getOpcode()); }
  static bool isPostfix(Opcode Op) {
    return Op == UO_PostInc || Op == UO_PostDec;
  }

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
    BO_AddAssign,
    BO_SubAssign,
    BO_MulAssign,
    BO_DivAssign,
    BO_RemAssign,
    BO_AndAssign,
    BO_OrAssign,
    BO_XorAssign,
    BO_ShlAssign,
    BO_ShrAssign,
    BO_Add,
    BO_Sub,
    BO_Mul,
    BO_Div,
    BO_Rem,
    BO_And,
    BO_Or,
    BO_Xor,
    BO_Shl,
    BO_Shr,
    BO_LAnd,
    BO_LOr,
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
  bool isCompoundAssign() const {
    return Kind >= BO_AddAssign && Kind <= BO_ShrAssign;
  }
  Opcode getOpForCompoundAssign() const;

private:
  BinaryOperator(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                 SourceLocation OpLoc, Expr *LHS, Expr *RHS, Opcode Op);

private:
  Expr *LHS;
  Expr *RHS;
  Opcode Kind;
  SourceLocation OpLoc;
};

/// Abstract base for ConditionalOperator and BinaryConditionalOperator.
class AbstractConditionalOperator : public Expr {
public:
  Expr *getCond() const { return Cond; }
  Expr *getTrueExpr() const { return TrueExpr; }
  Expr *getFalseExpr() const { return FalseExpr; }

  SourceLocation getQuestionLoc() const { return QuestionLoc; }
  SourceLocation getColonLoc() const { return ColonLoc; }

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_ConditionalOperator ||
           S->getKind() == SK_BinaryConditionalOperator;
  }

protected:
  AbstractConditionalOperator(StmtKind Kind, SourceLocation BegLoc,
                              SourceLocation EndLoc, QualType T,
                              SourceLocation QuestionLoc,
                              SourceLocation ColonLoc, Expr *Cond,
                              Expr *TrueExpr, Expr *FalseExpr);

private:
  SourceLocation QuestionLoc;
  SourceLocation ColonLoc;
  Expr *Cond;
  Expr *TrueExpr;
  Expr *FalseExpr;
};

class ConditionalOperator final : public AbstractConditionalOperator {
public:
  static ConditionalOperator *create(ASTContext &Ctx, SourceLocation BegLoc,
                                     SourceLocation EndLoc, QualType T,
                                     SourceLocation QuestionLoc,
                                     SourceLocation ColonLoc, Expr *Cond,
                                     Expr *TrueExpr, Expr *FalseExpr);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_ConditionalOperator;
  }

private:
  ConditionalOperator(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                      SourceLocation QuestionLoc, SourceLocation ColonLoc,
                      Expr *Cond, Expr *TrueExpr, Expr *FalseExpr);
};

/// GNU extension: `x ?: y`. The common operand is evaluated once and used as
/// both the condition and the true-expression value.
class BinaryConditionalOperator final : public AbstractConditionalOperator {
public:
  static BinaryConditionalOperator *
  create(ASTContext &Ctx, SourceLocation BegLoc, SourceLocation EndLoc,
         QualType T, SourceLocation QuestionLoc, SourceLocation ColonLoc,
         Expr *Common, Expr *FalseExpr);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_BinaryConditionalOperator;
  }

  Expr *getCommon() const { return getCond(); }

private:
  BinaryConditionalOperator(SourceLocation BegLoc, SourceLocation EndLoc,
                            QualType T, SourceLocation QuestionLoc,
                            SourceLocation ColonLoc, Expr *Common,
                            Expr *FalseExpr);
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

class FloatingLiteral final : public Expr {
public:
  static FloatingLiteral *create(ASTContext &Ctx, SourceLocation BegLoc,
                                 SourceLocation EndLoc, QualType T, double Val);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_FloatingLiteral;
  }

  double getVal() const { return Val; }

private:
  FloatingLiteral(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                  double Val);

private:
  double Val;
};

class CharacterLiteral final : public Expr {
public:
  static CharacterLiteral *create(ASTContext &Ctx, SourceLocation BegLoc,
                                  SourceLocation EndLoc, QualType T,
                                  unsigned Val);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_CharacterLiteral;
  }

  unsigned getValue() const { return Val; }

private:
  CharacterLiteral(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                   unsigned Val);

private:
  unsigned Val;
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
                          SourceLocation EndLoc, QualType T, Expr *Callee,
                          const FunctionType *FuncType,
                          std::vector<Expr *> Args);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CallExpr; }

  Expr *getCallee() const { return Callee; }
  FunctionDecl *getCalleeDecl() const;
  const FunctionType *getCalleeFunctionType() const { return FuncType; }
  unsigned getNumArgs() const { return Args.size(); }
  Expr *getArg(unsigned Idx) const { return Args[Idx]; }
  const std::vector<Expr *> &getArgs() const { return Args; }

  /// Temporary local holding a struct/union return value (caller-allocated).
  VarDecl *getRetBuffer() const { return RetBuffer; }
  void setRetBuffer(VarDecl *Buf) { RetBuffer = Buf; }

private:
  CallExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
           Expr *Callee, const FunctionType *FuncType,
           std::vector<Expr *> Args);

private:
  Expr *Callee;
  const FunctionType *FuncType;
  std::vector<Expr *> Args;
  VarDecl *RetBuffer = nullptr;
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
                                          const Type *Ty);

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
                           QualType T, const Type *Ty);

private:
  bool IsType;
  union {
    const Type *Ty;
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

class CastExpr final : public Expr {
public:
  enum CastKind {
    CK_NoOp,
    CK_ToVoid,
    CK_BitCast,
    CK_IntegralCast,
    CK_FloatingCast,
    CK_FloatingToIntegral,
    CK_IntegralToFloating,
    CK_FloatingToBoolean,
    CK_PointerToIntegral,
    CK_IntegralToPointer,
    CK_FuncToPointerDecay,
    CK_ArrayToPointerDecay,
  };

  static CastExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                          SourceLocation EndLoc, QualType T, Expr *SubExpr,
                          CastKind CK, bool IsImplicit);

  static bool classof(const Stmt *S) { return S->getKind() == SK_CastExpr; }

  Expr *getSubExpr() { return SubExpr; }
  const Expr *getSubExpr() const { return SubExpr; }
  CastKind getCastKind() const { return CK; }
  const char *getCastKindStr() const;
  bool isImplicit() const { return IsImplicit; }
  bool isExplicit() const { return !IsImplicit; }

  CastExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
           Expr *SubExpr, CastKind CK, bool IsImplicit);

private:
  Expr *SubExpr;
  CastKind CK;
  bool IsImplicit;
};

class InitListExpr final : public Expr {
public:
  static InitListExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                              SourceLocation EndLoc, QualType T,
                              std::vector<Expr *> Inits);

  static bool classof(const Stmt *S) { return S->getKind() == SK_InitListExpr; }

  const std::vector<Expr *> &getInits() const { return Inits; }
  unsigned getNumInits() const { return Inits.size(); }
  const Expr *getInit(unsigned Idx) const { return Inits[Idx]; }

  /// For union initializers: which member the (single) init initializes.
  unsigned getUnionFieldIndex() const { return UnionFieldIndex; }
  void setUnionFieldIndex(unsigned Idx) { UnionFieldIndex = Idx; }

private:
  InitListExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
               std::vector<Expr *> Inits);

private:
  std::vector<Expr *> Inits;
  unsigned UnionFieldIndex = 0;
};

/// C99/GNU designator in a designated initializer: [index] or .field
class Designator {
public:
  enum DesignatorKind { DK_ArrayIndex, DK_Field };

  static Designator createArrayIndex(std::uint64_t Index) {
    Designator D;
    D.Kind = DK_ArrayIndex;
    D.ArrayIndex = Index;
    return D;
  }

  static Designator createField(std::string Name) {
    Designator D;
    D.Kind = DK_Field;
    D.FieldName = std::move(Name);
    return D;
  }

  bool isArrayIndex() const { return Kind == DK_ArrayIndex; }
  bool isField() const { return Kind == DK_Field; }

  std::uint64_t getArrayIndex() const { return ArrayIndex; }
  const std::string &getFieldName() const { return FieldName; }

private:
  DesignatorKind Kind = DK_ArrayIndex;
  std::uint64_t ArrayIndex = 0;
  std::string FieldName;
};

/// C99 designated initializer: [i][j]= init / .a.b= init / mixed
class DesignatedInitExpr final : public Expr {
public:
  static DesignatedInitExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                                    SourceLocation EndLoc,
                                    std::vector<Designator> Designators,
                                    Expr *Init);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_DesignatedInitExpr;
  }

  const std::vector<Designator> &getDesignators() const { return Designators; }
  Expr *getInit() const { return Init; }

private:
  DesignatedInitExpr(SourceLocation BegLoc, SourceLocation EndLoc,
                     std::vector<Designator> Designators, Expr *Init);

private:
  std::vector<Designator> Designators;
  Expr *Init;
};

class CompoundLiteralExpr final : public Expr {
public:
  static CompoundLiteralExpr *create(ASTContext &Ctx, SourceLocation BegLoc,
                                     SourceLocation EndLoc, QualType T,
                                     VarDecl *Var);

  static bool classof(const Stmt *S) {
    return S->getKind() == SK_CompoundLiteralExpr;
  }

  VarDecl *getVarDecl() const { return Var; }

private:
  CompoundLiteralExpr(SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
                      VarDecl *Var);

private:
  VarDecl *Var;
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