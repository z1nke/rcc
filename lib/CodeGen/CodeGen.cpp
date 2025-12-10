#include "CodeGen/CodeGen.h"
#include "AST/AST.h"
#include "Basic/Casting.h"
#include "Basic/Diagnostic.h"
#include "Basic/Unreachable.h"

#include <cassert>
#include <cstdio>

namespace rcc {

CodeGen::CodeGen(Diagnostic &Diag) : Diag(Diag) {}

void CodeGen::codegen(const Stmt *Stmts) {
  printf("  .globl main\n");
  printf("main:\n");

  for (const Stmt *S = Stmts; S; S = S->getNext()) {
    //S->dump();
    genStmt(S);
    assert(Depth == 0);
  }

  printf("  ret\n");
}

void CodeGen::genStmt(const Stmt *S) {
  if (const auto *E = dyn_cast<Expr>(S)) {
    genExpr(E);
    return;
  }

  Diag.fatal("invalid statement");
}

void CodeGen::genExpr(const Expr *E) {
  if (const auto *IL = dyn_cast<IntergerLiteral>(E)) {
    // li a0, imm
    printf("  li a0, %d\n", static_cast<int>(IL->getVal()));
    return;
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    genBinaryOperator(BO);
    return;
  }

  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    genUnaryOperator(UO);
    return;
  }

  if (const auto *Paren = dyn_cast<ParenExpr>(E)) {
    genExpr(Paren->getSubExpr());
    return;
  }

  RCC_UNREACHABLE("[CodeGen] Unknown expression kind");
}

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

void CodeGen::genBinaryOperator(const BinaryOperator *BO) {
  // a0 op a1
  genExpr(BO->getRHS());
  push();
  genExpr(BO->getLHS());
  pop("a1");

  auto Op = BO->getOpcode();
  switch (Op) {
  case BinaryOperator::BO_Add:
  case BinaryOperator::BO_Sub:
  case BinaryOperator::BO_Mul:
  case BinaryOperator::BO_Div:
    printf("  %s a0, a0, a1\n", getBinaryOpcodeInstName(Op));
    break;
  case BinaryOperator::BO_EQ:
    // a0 = a0 ^ a1
    // a0 = (a0 == 0) ? 1 : 0
    printf("  xor a0, a0, a1\n");
    printf("  seqz a0, a0\n");
    break;
  case BinaryOperator::BO_NE:
    // a0 = a0 ^ a1
    // a0 = (a0 != 0) ? 1 : 0
    printf("  xor a0, a0, a1\n");
    printf("  snez a0, a0\n");
    break;
  case BinaryOperator::BO_LT:
    // a0 = a0 < a1.
    // TODO: In the future, we will need to handle unsigned comparisons.
    //
    printf("  slt a0, a0, a1\n");
    break;
  case BinaryOperator::BO_LE:
    // a0 <= a1  <=>  !(a1 < a0)
    // a0 = a1 < a0
    // a0 = !a0
    printf("  slt a0, a1, a0\n");
    printf("  xori a0, a0, 1\n");
    break;
  case BinaryOperator::BO_GT:
    // a0 > a1  <=>  a1 < a0
    printf("  slt a0, a1, a0\n");
    break;
  case BinaryOperator::BO_GE:
    // a0 >= a1  <=>  !(a0 < a1)
    // a0 = a0 < a1
    // a0 = !a0
    printf("  slt a0, a0, a1\n");
    printf("  xori a0, a0, 1\n");
    break;
  default:
    RCC_UNREACHABLE("[CodeGen] Unknown binary opcode");
    break;
  }
}

void CodeGen::genUnaryOperator(const UnaryOperator *UO) {
  switch (UO->getOpcode()) {
  case UnaryOperator::UO_Plus:
    genExpr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Minus:
    genExpr(UO->getSubExpr());
    printf("  neg a0, a0\n");
    break;
  default:
    RCC_UNREACHABLE("[CodeGen] Unknown unary opcode");
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