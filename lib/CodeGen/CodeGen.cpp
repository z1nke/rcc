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

  printf(".L.return:\n");
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

  switch (S->getKind()) {
  case Stmt::SK_CompoundStmt: {
    const Stmt *Body = cast<CompoundStmt>(S)->getBody();
    for (const Stmt *SubStmt = Body; SubStmt; SubStmt = SubStmt->getNext())
      genStmt(SubStmt);
    break;
  }
  case Stmt::SK_ReturnStmt:
    genExpr(cast<ReturnStmt>(S)->getRetValue());
    printf("  j .L.return\n");
    break;
  case Stmt::SK_NullStmt:
    break;
  case Stmt::SK_IfStmt:
    genIfStmt(cast<IfStmt>(S));
    break;
  case Stmt::SK_ForStmt:
    genForStmt(cast<ForStmt>(S));
    break;
  case Stmt::SK_WhileStmt:
    genWhileStmt(cast<WhileStmt>(S));
    break;
  case Stmt::SK_DeclStmt:
    genDeclStmt(cast<DeclStmt>(S));
    break;
  default:
    Diag.fatalAt(S->getBeginLoc(), "invalid statement: %d", S->getKind());
  }
}

void CodeGen::genDeclStmt(const DeclStmt *DS) {
  for (auto *D : DS->getDecls()) {
    if (const auto *Var = dyn_cast<VarDecl>(D)) {
      const auto *Init = Var->getInit();
      if (!Init)
        continue;

      genAddr(Var);
      push();
      // a0 = init-expr
      genExpr(Init);
      // a1 = &var
      pop("a1");
      printf("  sd a0, 0(a1)\n"); // *a1 = a0

    } else {
      Diag.fatalAt(D->getBeginLoc(), "invalid declaration in decl-stmt");
    }
  }
}

void CodeGen::genIfStmt(const IfStmt *If) {
  int Count = getCount();
  genExpr(If->getCond());
  //    if a0 == 0, goto .L.else.C
  //    then-stmt
  //    goto .L.end.C
  // .L.else.C:
  //    else-stmt
  // .L.end.C:
  //    ...
  printf("  beqz a0, .L.else.%d\n", Count);
  genStmt(If->getThen());
  printf("  j .L.end.%d\n", Count);
  printf(".L.else.%d:\n", Count);
  if (const auto *Else = If->getElse())
    genStmt(Else);
  printf(".L.end.%d:\n", Count);
}

void CodeGen::genForStmt(const ForStmt *For) {
  int Count = getCount();
  //   init-stmt
  // .L.begin.C:
  //   cond-expr
  //   if a0 == 0 goto .L.end.C
  //   body-stmt
  //   inc-expr
  //   goto .L.begin.C
  // .L.end.C:
  //   ...
  if (const auto *Init = For->getInit())
    genStmt(Init);
  printf(".L.begin.%d:\n", Count);
  if (const auto *Cond = For->getCond()) {
    genExpr(Cond);
    printf("  beqz a0, .L.end.%d\n", Count);
  }
  genStmt(For->getBody());
  if (const auto *Inc = For->getInc())
    genExpr(Inc);
  printf("  j .L.begin.%d\n", Count);
  printf(".L.end.%d:\n", Count);
}

void CodeGen::genWhileStmt(const WhileStmt *While) {
  int Count = getCount();
  // .L.begin.C:
  //   cond-expr
  //   if a0 == 0 goto .L.end.C
  //   body-stmt
  //   goto .L.begin.C
  // .L.end.C:
  //   ...
  printf(".L.begin.%d:\n", Count);
  genExpr(While->getCond());
  printf("  beqz a0, .L.end.%d\n", Count);
  genStmt(While->getBody());
  printf("  j .L.begin.%d\n", Count);
  printf(".L.end.%d:\n", Count);
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
    Diag.fatalAt(E->getBeginLoc(), "invalid expression");
  }
}

void CodeGen::genBinaryOperator(const BinaryOperator *BO) {
  const auto *LHS = BO->getLHS();
  const auto *RHS = BO->getRHS();
  if (BO->getOpcode() == BinaryOperator::BO_Assign) {
    genAddr(LHS);
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
  genExpr(RHS);
  push();
  genExpr(LHS);
  pop("a1");

  auto Op = BO->getOpcode();
  switch (Op) {
  case BinaryOperator::BO_Add: {
    QualType LType = LHS->getType();
    QualType RType = RHS->getType();
    if (LType->isPointerType()) {
      // Ptr + Int
      // FIXME: Consider the size and alignment of the element being pointed to.
      // a1 <<= 3  <=>  a1 *= 8
      printf("  slli a1, a1, 3\n");
    } else if (RType->isPointerType()) {
      // Int + Ptr
      // FIXME: Consider the size and alignment of the element being pointed to.
      printf("  slli a1, a1, 3\n");
    }

    printf("  add a0, a0, a1\n");
    return;
  }
  case BinaryOperator::BO_Sub: {
    QualType LType = LHS->getType();
    QualType RType = RHS->getType();
    if (LType->isPointerType()) {
      if (RType->isPointerType()) {
        // Ptr - Ptr
        printf("  sub a0, a0, a1\n");
        printf("  srli a0, a0, 3\n"); // a0 >>= 3  <=>  a0 /= 8
        return;
      }

      // Ptr - Int
      printf("  slli a1, a1, 3\n");
    }

    printf("  sub a0, a0, a1\n");
    return;
  }
  case BinaryOperator::BO_Mul:
    printf("  mul a0, a0, a1\n");
    return;
  case BinaryOperator::BO_Div:
    printf("  div a0, a0, a1\n");
    return;
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
    Diag.fatalAt(BO->getOpLocation(), "invalid binary opcode: %d",
                 BO->getOpcode());
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
  case UnaryOperator::UO_Addrof:
    genAddr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Deref:
    genExpr(UO->getSubExpr());
    printf("  ld a0, 0(a0)\n"); // a0 = *addr
    break;
  default:
    Diag.fatalAt(UO->getBeginLoc(), "invalid unary opcode: %d",
                 UO->getOpcode());
  }
}

void CodeGen::genAddr(const Expr *E) {
  switch (E->getKind()) {
  case Stmt::SK_DeclRefExpr: {
    const auto *Ref = cast<DeclRefExpr>(E);
    genAddr(Ref->getDecl());
    return;
  }
  case Stmt::SK_UnaryOperator: {
    const auto *UO = cast<UnaryOperator>(E);
    if (UO->getOpcode() == UnaryOperator::UO_Deref) {
      genExpr(UO->getSubExpr());
      return;
    }
    break;
  }
  default:
    break;
  }

  Diag.fatalAt(E->getBeginLoc(), "not a lvalue");
}

void CodeGen::genAddr(const Decl *D) {
  const auto *Var = dyn_cast<VarDecl>(D);
  if (!Var)
    Diag.fatalAt(D->getBeginLoc(), "expect a variable");

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

int CodeGen::getCount() const {
  static int Count = 1;
  return Count++;
}

} // namespace rcc