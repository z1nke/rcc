#ifndef RCC_CODEGEN_CODEGEN_H
#define RCC_CODEGEN_CODEGEN_H

namespace rcc {

class Expr;
class BinaryOperator;
class UnaryOperator;

class Diagnostic;

class CodeGen {
public:
  CodeGen(Diagnostic &Diag);

  void genExpr(const Expr *E);

private:
  void genBinaryOperator(const BinaryOperator *BO);

  void genUnaryOperator(const UnaryOperator *UO);

private:
  void push();

  void pop(const char *Reg);

private:
  Diagnostic &Diag;
  int Depth = 0;
};

} // namespace rcc

#endif