#ifndef RCC_CODEGEN_CODEGEN_H
#define RCC_CODEGEN_CODEGEN_H

namespace rcc {

class Decl;
class TranslationUnitDecl;
class FunctionDecl;
class Stmt;
class Expr;
class DeclStmt;
class IfStmt;
class ForStmt;
class WhileStmt;
class BinaryOperator;
class UnaryOperator;
class CallExpr;
class Type;
class Diagnostic;

class CodeGen {
public:
  CodeGen(Diagnostic &Diag);

  void codegen(const TranslationUnitDecl *FD);

private:
  void genFunction(const FunctionDecl *FD);

private:
  void genStmt(const Stmt *S);
  void genDeclStmt(const DeclStmt *DS);
  void genIfStmt(const IfStmt *If);
  void genForStmt(const ForStmt *For);
  void genWhileStmt(const WhileStmt *While);
  void genExpr(const Expr *E);
  void genBinaryOperator(const BinaryOperator *BO);
  void genUnaryOperator(const UnaryOperator *UO);
  void genCallExpr(const CallExpr *CE);
  void genAddr(const Expr *E);
  void genAddr(const Decl *D);

private:
  void push();
  void pop(const char *Reg);
  void load(const Type *Ty);
  void store(void);

  int getCount() const;

private:
  Diagnostic &Diag;
  const FunctionDecl *CurrFunc = nullptr;
  int Depth = 0;
};

} // namespace rcc

#endif