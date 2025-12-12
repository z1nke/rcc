#include "CodeGen/CodeGen.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/Allocator.h"
#include "Basic/Casting.h"
#include "Basic/Diagnostic.h"
#include "Basic/Unreachable.h"

#include <cassert>
#include <cstdio>

namespace rcc {

CodeGen::CodeGen(Diagnostic &Diag) : Diag(Diag) {}

// Returns stack size.
static std::size_t assignLVarOffsets(const FunctionDecl *FD) {
  int Offset = 0;
  for (auto *Var : FD->getLocalVars()) {
    Offset += 8;
    Var->setOffset(-Offset);
  }

  return alignTo(Offset, 16);
}

void CodeGen::codegen(const FunctionDecl *FD) {
  std::size_t StackSize = assignLVarOffsets(FD);
  printf("  .globl main\n");
  printf("main:\n");

  // stack frame
  //-------------------------------// sp
  //              fp
  //-------------------------------// fp = sp-8
  //                                       |
  //          local vars              stack-size
  //                                       |
  //-------------------------------// sp = sp-8-StackSize
  //        eval-expression
  //-------------------------------//

  // push sp
  printf("  addi sp, sp, -8\n");
  printf("  sd fp, 0(sp)\n");
  // fp = sp
  printf("  mv fp, sp\n");
  // sp -= StackSize
  printf("  addi sp, sp, -%ld\n", StackSize);

  for (const Stmt *S = FD->getBody(); S; S = S->getNext()) {
    genStmt(S);
    assert(Depth == 0);
  }

  // sp = fp
  printf("  mv sp, fp\n");
  // pop fp
  printf("  ld fp, 0(sp)\n");
  printf("  addi sp, sp, 8\n");
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
  switch (E->getKind()) {
  case Stmt::SK_UnaryOperator:
    genUnaryOperator(cast<UnaryOperator>(E));
    break;
  case Stmt::SK_BinaryOperator:
    genBinaryOperator(cast<BinaryOperator>(E));
    break;
  case Stmt::SK_IntergerLiteral:
    // li a0, imm
    printf("  li a0, %ld\n", cast<IntergerLiteral>(E)->getVal());
    break;
  case Stmt::SK_ParenExpr:
    genExpr(cast<ParenExpr>(E)->getSubExpr());
    break;
  case Stmt::SK_DeclRefExpr:
    // a0 = addr
    genAddr(cast<DeclRefExpr>(E));
    // a0 = *a0
    printf("  ld a0, 0(a0)\n");
    break;
  default:
    RCC_UNREACHABLE("[CodeGen] Unknown expression kind");
  }
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
  if (BO->getOpcode() == BinaryOperator::BO_Assign) {
    const auto *DRE = dyn_cast<DeclRefExpr>(BO->getLHS());
    if (!DRE)
      Diag.fatal("not a lvalue");

    genAddr(DRE);
    push();
    // a0 = rhs
    genExpr(BO->getRHS());
    // a1 = addrof(lhs)
    pop("a1");
    // *(a1) = a0
    printf("  sd a0, 0(a1)\n");
    return;
  }

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

void CodeGen::genAddr(const DeclRefExpr *DRE) {
  const auto *Var = dyn_cast<VarDecl>(DRE->getDecl());
  if (!Var)
    Diag.fatal("expect a variable");

  printf("  addi a0, fp, %d\n", -Var->getOffset());
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