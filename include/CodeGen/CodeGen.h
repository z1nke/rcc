#ifndef RCC_CODEGEN_CODEGEN_H
#define RCC_CODEGEN_CODEGEN_H

#include "AST/Stmt.h"

#include <print>
#include <unordered_map>

namespace rcc {

class Decl;
class TranslationUnitDecl;
class FunctionDecl;
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
  void genExpr(const Expr *E);
  void genStringLiteral(const StringLiteral *SL);
  void genBinaryOperator(const BinaryOperator *BO);
  void genUnaryOperator(const UnaryOperator *UO);
  void genCallExpr(const CallExpr *CE);
  void genArraySubscriptExpr(const ArraySubscriptExpr *ASE);
  void genUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *UE);
  void genCastExpr(const CastExpr *Cast);
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
  std::vector<const StringLiteral *> StringLiterals;
  std::unordered_map<const StringLiteral *, std::string> SLCache;
  FILE *Fp;
};

} // namespace rcc

#endif