#ifndef RCC_CODEGEN_CODEGEN_H
#define RCC_CODEGEN_CODEGEN_H

namespace rcc {

class Expr;

class Diagnostic;

class CodeGen {
public:
  CodeGen(Diagnostic &Diag);

  void genExpr(Expr *E);

private:
  void push();

  void pop(const char *Reg);

private:
  Diagnostic &Diag;
  int Depth = 0;
};

} // namespace rcc

#endif