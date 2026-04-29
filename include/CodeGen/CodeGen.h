#ifndef RCC_CODEGEN_CODEGEN_H
#define RCC_CODEGEN_CODEGEN_H

#include "AST/Stmt.h"

#include <print>
#include <unordered_map>

namespace rcc {

class Decl;
class TranslationUnitDecl;
class FunctionDecl;
class VarDecl;
class Type;
class Diagnostic;

class CodeGen {
public:
  CodeGen(Diagnostic &Diag,  FILE *Fp);

  void codegen(const TranslationUnitDecl *TU, const char *Input);

private:
  void emitData(const TranslationUnitDecl *TU);
  void emitText(const TranslationUnitDecl *TU);

private:
  void genFunction(const FunctionDecl *FD);

private:
  void genStmt(const Stmt *S);
  void genDeclStmt(const DeclStmt *DS);
  void genIfStmt(const IfStmt *If);
  void genForStmt(const ForStmt *For);
  void genWhileStmt(const WhileStmt *While);
  void genSwitchStmt(const SwitchStmt *Switch);
  void genCaseStmt(const CaseStmt *Case);
  void genDefaultStmt(const DefaultStmt *Default);
  void genBreakStmt(const BreakStmt *Break);
  void genContinueStmt(const ContinueStmt *Continue);
  void genGotoStmt(const GotoStmt *Goto);
  void genLabelStmt(const LabelStmt *Label);
  void genExpr(const Expr *E);
  void genStringLiteral(const StringLiteral *SL);
  void genDeclRefExpr(const DeclRefExpr *Ref);
  void genBinaryOperator(const BinaryOperator *BO);
  void genConditionalOperator(const ConditionalOperator *CO);

  /// Emits a0 = (lhs op rhs) given a0 = lhs and a1 = rhs.
  void emitBinaryArithmeticResult(BinaryOperator::Opcode Op, QualType LType,
                          QualType RType, const char *Suffix);
  void genUnaryOperator(const UnaryOperator *UO);
  void genCallExpr(const CallExpr *CE);
  void genArraySubscriptExpr(const ArraySubscriptExpr *ASE);
  void genUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *UE);
  void genCastExpr(const CastExpr *Cast);
  void genInitListExpr(const VarDecl *Var, const InitListExpr *List,
                       QualType AggTy, std::size_t BaseOffset);
  void genInitListElement(const VarDecl *Var, const Expr *ElemInit,
                          QualType ElemTy, std::size_t Offset);
  void genStringLiteralInit(const VarDecl *Var, const StringLiteral *SL,
                            QualType ArrTy, std::size_t BaseOffset);
  void genZeroInit(const VarDecl *Var, QualType Ty, std::size_t BaseOffset);
  void emitGlobalVarInit(const VarDecl *Var, const Expr *Init);
  void emitGlobalInit(const Expr *Init, QualType Ty, std::size_t BaseOffset);
  void emitGlobalStringLiteralInit(const StringLiteral *SL, QualType ArrTy,
                                   std::size_t BaseOffset);
  void emitGlobalZeroInit(QualType Ty, std::size_t BaseOffset);
  void emitScalarData(std::size_t Offset, std::size_t Size, std::int64_t Val);
  void genAddr(const Expr *E);
  void genAddr(const Decl *D);
  void genAddr(const ArraySubscriptExpr *ASE);
  void genAddr(const StringLiteral *SL);
  void genAddr(const MemberExpr *ME);

private:
  void push();
  void pop(const char *Reg);
  void load(const Type *Ty);
  void store(const Type *Ty);

  void storeGenReg(int Reg, int Offset, int Size);

  char getWidthSuffix(std::size_t Size) const;

  int getCount() const;
  const std::string &getStringLabel(const StringLiteral *SL);

  void genIntCast(const Type *From, const Type *To);

private:
  template <typename... ARGS>
  void emit(std::format_string<ARGS...> Fmt, ARGS &&...Args) const {
    std::println(Fp, Fmt, std::forward<ARGS &&>(Args)...);
  }

private:
  Diagnostic &Diag;
  const FunctionDecl *CurrFunc = nullptr;
  int Depth = 0;
  std::vector<int> BreakCounts;
  std::vector<int> ContinueCounts;
  std::vector<int> SwitchCounts;
  std::vector<const StringLiteral *> StringLiterals;
  std::unordered_map<const StringLiteral *, std::string> SLCache;
  FILE *Fp;
};

} // namespace rcc

#endif