#ifndef RCC_CODEGEN_CODEGEN_H
#define RCC_CODEGEN_CODEGEN_H

namespace rcc {

class FunctionDecl;
class Stmt;
class Expr;
class IfStmt;
class ForStmt;
class WhileStmt;
class BinaryOperator;
class UnaryOperator;
class Diagnostic;

class CodeGen {
public:
  CodeGen(Diagnostic &Diag);

  void codegen(const FunctionDecl *FD);

private:
  void genStmt(const Stmt *S);
  void genIfStmt(const IfStmt *If);
  void genForStmt(const ForStmt *For);
  void genWhileStmt(const WhileStmt *While);
  void genExpr(const Expr *E);
  void genBinaryOperator(const BinaryOperator *BO);
  void genUnaryOperator(const UnaryOperator *UO);
  void genAddr(const Expr *E);

private:
  void push();
  void pop(const char *Reg);
  int getCount() const;

private:
  Diagnostic &Diag;
  int Depth = 0;
};

} // namespace rcc

#endif