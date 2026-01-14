#include "CodeGen/CodeGen.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "Basic/Allocator.h"
#include "Basic/Casting.h"
#include "Basic/Diagnostic.h"

#include <cassert>
#include <cstdio>
#include <print>

namespace rcc {

CodeGen::CodeGen(Diagnostic &Diag) : Diag(Diag) {}

// Returns stack size.
static std::size_t assignLVarOffsets(const FunctionDecl *FD) {
  int Offset = 0;
  for (auto *Var : FD->getLocalVars()) {
    Offset += Var->getType()->getSize();
    Var->setOffset(-Offset);
  }

  for (auto *Param : FD->getParams()) {
    Offset += Param->getType()->getSize();
    Param->setOffset(-Offset);
  }

  return alignTo(Offset, 16);
}

static const char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};

void CodeGen::codegen(const TranslationUnitDecl *TU) {
  for (const auto *D : TU->decls()) {
    if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
      genFunction(FD);
    } else {
      Diag.fatalAt(D->getBeginLoc(), "only function declaration is allowed");
    }
  }
}

void CodeGen::genFunction(const FunctionDecl *FD) {
  CurrFunc = FD;
  std::size_t StackSize = assignLVarOffsets(FD);
  const char *Name = FD->getName().c_str();
  std::println("  .globl {}", Name);
  std::println("{}:", Name);

  // stack frame
  //-------------------------------// sp
  //              ra
  //-------------------------------// ra = sp-8
  //              fp
  //-------------------------------// fp = sp-16
  //                                       |
  //          local vars               StackSize
  //                                       |
  //-------------------------------// sp = sp-16-StackSize
  //        eval-expression
  //-------------------------------//

  std::println("  # create stack frame for ra, fp");
  std::println("  addi sp, sp, -16");
  std::println("  sd ra, 8(sp)"); // save ra
  std::println("  sd fp, 0(sp)"); // save fp
  std::println("  mv fp, sp");    // fp = sp
  // sp -= StackSize
  if (StackSize > 0) {
    std::println("  # allocate {} bytes for local variables", StackSize);
    std::println("  addi sp, sp, -{}", StackSize);
  }

  unsigned NumParams = FD->getNumParams();
  if (NumParams > 0) {
    assert(NumParams <= 6);
    std::println("  # store {} parameters to stack", NumParams);
    for (unsigned I = 0; I < NumParams; ++I) {
      const auto *Param = FD->getParam(I);
      std::println("  sd {}, {}(fp)", ArgReg[I], Param->getOffset());
    }
  }

  if (const auto *CS = dyn_cast<CompoundStmt>(FD->getBody())) {
    for (const Stmt *S : CS->getBody()) {
      genStmt(S);
      assert(Depth == 0);
    }
  }

  std::println(".L.return.{}:", Name);
  std::println("  # restore sp, fp and ra");
  std::println("  mv sp, fp");    // restore sp, sp = fp
  std::println("  ld fp, 0(sp)"); // pop fp
  std::println("  ld ra, 8(sp)"); // pop ra
  std::println("  addi sp, sp, 16");
  std::println("  ret");
  std::println("  # end of function '{}'", Name);
}

void CodeGen::genStmt(const Stmt *S) {
  if (const auto *E = dyn_cast<Expr>(S)) {
    genExpr(E);
    return;
  }

  switch (S->getKind()) {
  case Stmt::SK_CompoundStmt: {
    const auto &Body = cast<CompoundStmt>(S)->getBody();
    for (const Stmt *SubStmt : Body)
      genStmt(SubStmt);
    break;
  }
  case Stmt::SK_ReturnStmt:
    std::println("  # return stmt");
    genExpr(cast<ReturnStmt>(S)->getRetValue());
    std::println("  j .L.return.{}", CurrFunc->getName());
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
    Diag.fatalAt(S->getBeginLoc(), "invalid statement: {}", S->getKindStr());
  }
}

void CodeGen::genDeclStmt(const DeclStmt *DS) {
  std::println("  # decl-stmt");
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
      std::println("  # initialize variable '{}'", Var->getName());
      std::println("  sd a0, 0(a1)"); // *a1 = a0

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
  std::println("  beqz a0, .L.else.{}", Count);
  genStmt(If->getThen());
  std::println("  j .L.end.{}", Count);
  std::println(".L.else.{}:", Count);
  if (const auto *Else = If->getElse())
    genStmt(Else);
  std::println(".L.end.{}:", Count);
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
  std::println(".L.begin.{}:", Count);
  if (const auto *Cond = For->getCond()) {
    genExpr(Cond);
    std::println("  beqz a0, .L.end.{}", Count);
  }
  genStmt(For->getBody());
  if (const auto *Inc = For->getInc())
    genExpr(Inc);
  std::println("  j .L.begin.{}", Count);
  std::println(".L.end.{}:", Count);
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
  std::println(".L.begin.{}:", Count);
  genExpr(While->getCond());
  std::println("  beqz a0, .L.end.{}", Count);
  genStmt(While->getBody());
  std::println("  j .L.begin.{}", Count);
  std::println(".L.end.{}:", Count);
}

void CodeGen::genExpr(const Expr *E) {
  switch (E->getKind()) {
  case Stmt::SK_UnaryOperator:
    genUnaryOperator(cast<UnaryOperator>(E));
    break;
  case Stmt::SK_BinaryOperator:
    genBinaryOperator(cast<BinaryOperator>(E));
    break;
  case Stmt::SK_IntegerLiteral: {
    // li a0, imm
    auto Val = cast<IntegerLiteral>(E)->getVal();
    std::println("  # a0 = {}", Val);
    std::println("  li a0, {}", Val);
    break;
  }
  case Stmt::SK_ParenExpr:
    genExpr(cast<ParenExpr>(E)->getSubExpr());
    break;
  case Stmt::SK_DeclRefExpr: {
    const auto *Ref = cast<DeclRefExpr>(E);
    genAddr(Ref->getDecl()); // a0 = addr
    load(Ref->getTypePtr()); // a0 = *a0
    break;
  }
  case Stmt::SK_CallExpr:
    genCallExpr(cast<CallExpr>(E));
    break;
  case Stmt::SK_ArraySubscriptExpr:
    genArraySubscriptExpr(cast<ArraySubscriptExpr>(E));
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
    push();                // a1 = addrof(lhs)
    genExpr(BO->getRHS()); // a0 = rhs
    store();               // *(a1) = a0
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
    if (const auto *PointeeTy = LType->getPointeeOrArrayElementTypePtr()) {
      // Ptr + Int(a1)
      std::println("  li t0, {}", PointeeTy->getSize());
      std::println("  mul a1, a1, t0");
    } else if (const auto *PointeeTy =
                   RType->getPointeeOrArrayElementTypePtr()) {
      // Int(a0) + Ptr
      std::println("  li t0, {}", PointeeTy->getSize());
      std::println("  mul a0, a0, t0");
    }

    std::println("  add a0, a0, a1");
    return;
  }
  case BinaryOperator::BO_Sub: {
    QualType LType = LHS->getType();
    QualType RType = RHS->getType();
    if (const auto *PointeeTy = LType->getPointeeOrArrayElementTypePtr()) {
      if (RType->isPointerType()) {
        // Ptr - Ptr
        std::println("  sub a0, a0, a1");
        std::println("  li t0, {}", PointeeTy->getSize());
        std::println("  div a0, a0, t0");
        return;
      }

      // Ptr - Int(a1)
      std::println("  li t0, {}", PointeeTy->getSize());
      std::println("  mul a1, a1, t0");
    }

    std::println("  sub a0, a0, a1");
    return;
  }
  case BinaryOperator::BO_Mul:
    std::println("  mul a0, a0, a1");
    return;
  case BinaryOperator::BO_Div:
    std::println("  div a0, a0, a1");
    return;
  case BinaryOperator::BO_EQ:
    // a0 = a0 ^ a1
    // a0 = (a0 == 0) ? 1 : 0
    std::println("  xor a0, a0, a1");
    std::println("  seqz a0, a0");
    break;
  case BinaryOperator::BO_NE:
    // a0 = a0 ^ a1
    // a0 = (a0 != 0) ? 1 : 0
    std::println("  xor a0, a0, a1");
    std::println("  snez a0, a0");
    break;
  case BinaryOperator::BO_LT:
    // a0 = a0 < a1.
    // TODO: In the future, we will need to handle unsigned comparisons.
    //
    std::println("  slt a0, a0, a1");
    break;
  case BinaryOperator::BO_LE:
    // a0 <= a1  <=>  !(a1 < a0)
    // a0 = a1 < a0
    // a0 = !a0
    std::println("  slt a0, a1, a0");
    std::println("  xori a0, a0, 1");
    break;
  case BinaryOperator::BO_GT:
    // a0 > a1  <=>  a1 < a0
    std::println("  slt a0, a1, a0");
    break;
  case BinaryOperator::BO_GE:
    // a0 >= a1  <=>  !(a0 < a1)
    // a0 = a0 < a1
    // a0 = !a0
    std::println("  slt a0, a0, a1");
    std::println("  xori a0, a0, 1");
    break;
  default:
    Diag.fatalAt(BO->getOpLocation(), "invalid binary opcode: {}",
                 BO->getOpcodeStr());
  }
}

void CodeGen::genUnaryOperator(const UnaryOperator *UO) {
  switch (UO->getOpcode()) {
  case UnaryOperator::UO_Plus:
    std::println("  # unary plus");
    genExpr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Minus:
    genExpr(UO->getSubExpr());
    std::println("  neg a0, a0");
    break;
  case UnaryOperator::UO_Addrof:
    std::println("  # addrof");
    genAddr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Deref:
    std::println("  # deref");
    genExpr(UO->getSubExpr());
    load(UO->getTypePtr());
    break;
  default:
    Diag.fatalAt(UO->getBeginLoc(), "invalid unary opcode: {}",
                 UO->getOpcodeStr());
  }
}

void CodeGen::genCallExpr(const CallExpr *CE) {
  const auto *Func = CE->getCalleeDecl();
  if (!Func)
    Diag.fatalAt(CE->getCallee()->getBeginLoc(), "undeclared function");

  int NumArgs = static_cast<int>(CE->getNumArgs());
  if (NumArgs != 0) {
    std::println("  # set args on calling {}", Func->getName());
    for (const Expr *Arg : CE->getArgs()) {
      genExpr(Arg);
      push();
    }

    for (int I = NumArgs - 1; I >= 0; --I)
      pop(ArgReg[I]);
  }

  const std::string &Name = Func->getName();
  std::println("  call {}", Name);
}

void CodeGen::genArraySubscriptExpr(const ArraySubscriptExpr *ASE) {
  genAddr(ASE);
  std::println("  ld a0, 0(a0)");
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
  case Stmt::SK_ArraySubscriptExpr:
    genAddr(cast<ArraySubscriptExpr>(E));
    return;
  default:
    break;
  }

  Diag.fatalAt(E->getBeginLoc(), "not a lvalue");
}

void CodeGen::genAddr(const ArraySubscriptExpr *ASE) {
  // base[idx] <=> *(base + idx)
  const auto *Base = ASE->getBase();
  const auto *Idx = ASE->getIdx();
  QualType BaseType = Base->getType();
  QualType ElemType = BaseType->getPointeeOrArrayElementType();
  assert(ElemType);

  std::println("  # array-subscript-expr");
  // a0[a1]
  genExpr(Idx);
  push();
  if (BaseType->isPointerType())
    genExpr(Base);
  else if (BaseType->isArraryType())
    genAddr(Base);
  else
    Diag.fatalAt(Base->getBeginLoc(), "expect pointer or array type");
  pop("a1");
  std::println("  li t0, {}", ElemType->getSize());
  std::println("  mul a1, a1, t0");
  std::println("  add a0, a0, a1");
}

void CodeGen::genAddr(const Decl *D) {
  const auto *Var = dyn_cast<VarDecl>(D);
  if (!Var)
    Diag.fatalAt(D->getBeginLoc(), "expect a variable");

  std::println("  # get address of variable {}, offset={}", Var->getName(),
               Var->getOffset());
  std::println("  addi a0, fp, {}", Var->getOffset());
}

void CodeGen::push() {
  std::println("  # push a0");
  std::println("  addi sp, sp, -8"); // sp -= 8
  std::println("  sd a0, 0(sp)");    // store a0 to stack
  ++Depth;
}

void CodeGen::pop(const char *Reg) {
  std::println("  # pop {}", Reg);
  std::println("  ld {}, 0(sp)", Reg); // load from stack to Reg
  std::println("  addi sp, sp, 8");    // sp += 8
  --Depth;
}

void CodeGen::load(const Type *Ty) {
  if (Ty->isArraryType())
    return;
  std::println("  # load");
  std::println("  ld a0, 0(a0)");
}

void CodeGen::store(void) {
  std::println("  # store");
  pop("a1");
  std::println("  sd a0, 0(a1)");
}

int CodeGen::getCount() const {
  static int Count = 1;
  return Count++;
}

} // namespace rcc