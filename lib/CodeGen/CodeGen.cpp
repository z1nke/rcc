#include "CodeGen/CodeGen.h"
#include "AST/AST.h"
#include "Basic/Casting.h"
#include "Basic/Diagnostic.h"
#include "Basic/Unreachable.h"

#include <cstdio>

namespace rcc {

CodeGen::CodeGen(Diagnostic &Diag) : Diag(Diag) {}

static const char *getBinaryOpcodeInstName(BinaryOperator::Opcode Op) {
  switch (Op) {
  case BinaryOperator::BO_Add:
    return "add";
  case BinaryOperator::BO_Sub:
    return "sub";
  case BinaryOperator::BO_Mul:
    return "mul";
  case BinaryOperator::BO_Div:
    return "div";
  default:
    RCC_UNREACHABLE("Unknown binary opcode");
  }
}

void CodeGen::genExpr(Expr *E) {
  if (const auto *IL = dyn_cast<IntergerLiteral>(E)) {
    // li a0, imm
    printf("  li a0, %d\n", static_cast<int>(IL->getVal()));
    return;
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    genExpr(BO->getRHS());
    push();
    genExpr(BO->getLHS()); // a0 = lhs
    pop("a1");             // a1 = rhs
    const char *Op = getBinaryOpcodeInstName(BO->getOpcode());
    printf("  %s a0, a0, a1\n", Op);
    return;
  }
}

void CodeGen::push() {
  printf("  addi sp, sp, -8\n"); // sp -= 8
  printf("  sd a0, 0(sp)\n");    // store a0 to stack
  ++Depth;
}

void CodeGen::pop(const char *Reg) {
  printf("  ld %s, 0(sp)\n", Reg); // load from stack to Reg
  printf("  addi sp, sp, 8\n");    // sp += 8
  --Depth;
}

} // namespace rcc