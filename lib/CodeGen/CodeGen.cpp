#include "CodeGen/CodeGen.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Type.h"
#include "Basic/Diagnostic.h"
#include "Support/Allocator.h"
#include "Support/Casting.h"
#include "Support/Unreachable.h"

#include <cassert>
#include <cstdio>
#include <format>
#include <print>

namespace rcc {

CodeGen::CodeGen(Diagnostic &Diag, FILE *Fp) : Diag(Diag), Fp(Fp) {}

// Returns stack size.
static std::size_t assignLVarOffsets(const FunctionDecl *FD) {
  int Offset = 0;
  for (auto *Var : FD->getLocalVars()) {
    Offset += Var->getType()->getSize();
    Offset = alignTo(Offset, Var->getType()->getAlign());
    Var->setOffset(-Offset);
  }

  for (auto *Param : FD->getParams()) {
    Offset += Param->getType()->getSize();
    Offset = alignTo(Offset, Param->getType()->getAlign());
    Param->setOffset(-Offset);
  }

  return alignTo(Offset, 16);
}

static const char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};

void CodeGen::codegen(const TranslationUnitDecl *TU, const char *Input) {
  emit(".file 1 \"{}\"", Input);
  emitText(TU);
  emitData(TU);
}

void CodeGen::emitData(const TranslationUnitDecl *TU) {
  for (const auto *D : TU->decls()) {
    if (const auto *Var = dynCast<VarDecl>(D)) {
      emit("  .globl {}", Var->getName());
      emit("  .data");
      const auto *Init = Var->getInit();
      if (Init) {
        if (const auto *SL = dynCast<StringLiteral>(Init)) {
          emit("{}:", Var->getName());
          // emit("  .asciz \"{}\"", SL->getString());
          for (char C : SL->getString())
            emit("  .byte {}", static_cast<int>(C));
        } else {
          Diag.fatalAt(Var->getBeginLoc(),
                       "only string literal is supported in global var init");
        }
      } else {
        emit("{}:", Var->getName());
        emit("  .zero {}", Var->getType()->getSize());
      }
    }
  }

  for (std::size_t Idx = 0; Idx < StringLiterals.size(); ++Idx) {
    const auto *SL = StringLiterals[Idx];
    std::string Label = getStringLabel(SL);
    emit("{}:", Label);
    // emit("  .asciz \"{}\"", SL->getString());
    for (char C : SL->getString())
      emit("  .byte {}", static_cast<int>(C));
    emit("  .byte 0");
  }
}

void CodeGen::emitText(const TranslationUnitDecl *TU) {
  for (const auto *D : TU->decls()) {
    if (const auto *FD = dynCast<FunctionDecl>(D)) {
      genFunction(FD);
    }
  }
}

void CodeGen::genFunction(const FunctionDecl *FD) {
  CurrFunc = FD;
  std::size_t StackSize = assignLVarOffsets(FD);
  const char *Name = FD->getName().c_str();
  emit("  .globl {}", Name);
  emit("  .text");
  emit("{}:", Name);

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

  emit("  # create stack frame for ra, fp");
  emit("  addi sp, sp, -16");
  emit("  sd ra, 8(sp)"); // save ra
  emit("  sd fp, 0(sp)"); // save fp
  emit("  mv fp, sp");    // fp = sp
  // sp -= StackSize
  if (StackSize > 0) {
    emit("  # allocate {} bytes for local variables", StackSize);
    emit("  addi sp, sp, -{}", StackSize);
  }

  unsigned NumParams = FD->getNumParams();
  if (NumParams > 0) {
    assert(NumParams <= 6);
    emit("  # store {} parameters to stack", NumParams);
    for (unsigned I = 0; I < NumParams; ++I) {
      const auto *Param = FD->getParam(I);
      storeGenReg(I, Param->getOffset(), Param->getType()->getSize());
    }
  }

  if (const auto *CS = dynCast<CompoundStmt>(FD->getBody())) {
    for (const Stmt *S : CS->getBody()) {
      genStmt(S);
      assert(Depth == 0);
    }
  }

  emit(".L.return.{}:", Name);
  emit("  # restore sp, fp and ra");
  emit("  mv sp, fp");    // restore sp, sp = fp
  emit("  ld fp, 0(sp)"); // pop fp
  emit("  ld ra, 8(sp)"); // pop ra
  emit("  addi sp, sp, 16");
  emit("  ret");
  emit("  # end of function '{}'", Name);
}

void CodeGen::genStmt(const Stmt *S) {
  const SourceManager &SM = Diag.getSourceManager();
  emit("  .loc 1 {}", SM.getLineNumber(S->getBeginLoc()));

  if (const auto *E = dynCast<Expr>(S)) {
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
    emit("  # return stmt");
    genExpr(cast<ReturnStmt>(S)->getRetValue());
    emit("  j .L.return.{}", CurrFunc->getName());
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
  emit("  # decl-stmt");
  for (auto *D : DS->getDecls()) {
    if (const auto *Var = dynCast<VarDecl>(D)) {
      const auto *Init = Var->getInit();
      if (!Init)
        continue;

      genAddr(Var);
      push();
      // a0 = init-expr
      genExpr(Init);
      // a1 = &var
      pop("a1");
      emit("  # initialize variable '{}'", Var->getName());
      //emit("  sd a0, 0(a1)"); // *a1 = a0
      emit("  s{} a0, 0(a1)", getWidthSuffix(Var->getType()->getSize()));

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
  emit("  beqz a0, .L.else.{}", Count);
  genStmt(If->getThen());
  emit("  j .L.end.{}", Count);
  emit(".L.else.{}:", Count);
  if (const auto *Else = If->getElse())
    genStmt(Else);
  emit(".L.end.{}:", Count);
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
  emit(".L.begin.{}:", Count);
  if (const auto *Cond = For->getCond()) {
    genExpr(Cond);
    emit("  beqz a0, .L.end.{}", Count);
  }
  genStmt(For->getBody());
  if (const auto *Inc = For->getInc())
    genExpr(Inc);
  emit("  j .L.begin.{}", Count);
  emit(".L.end.{}:", Count);
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
  emit(".L.begin.{}:", Count);
  genExpr(While->getCond());
  emit("  beqz a0, .L.end.{}", Count);
  genStmt(While->getBody());
  emit("  j .L.begin.{}", Count);
  emit(".L.end.{}:", Count);
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
    emit("  # a0 = {}", Val);
    emit("  li a0, {}", Val);
    break;
  }
  case Stmt::SK_StringLiteral:
    genStringLiteral(cast<StringLiteral>(E));
    break;
  case Stmt::SK_ParenExpr:
    genExpr(cast<ParenExpr>(E)->getSubExpr());
    break;
  case Stmt::SK_DeclRefExpr: {
    const auto *Ref = cast<DeclRefExpr>(E);
    genAddr(Ref->getDecl()); // a0 = addr
    load(Ref->getTypePtr()); // a0 = *a0
    break;
  }
  case Stmt::SK_MemberExpr: {
    const auto *Member = cast<MemberExpr>(E);
    genAddr(Member);            // a0 = addr
    load(Member->getTypePtr()); // a0 = *a0
    break;
  }
  case Stmt::SK_CallExpr:
    genCallExpr(cast<CallExpr>(E));
    break;
  case Stmt::SK_ArraySubscriptExpr:
    genArraySubscriptExpr(cast<ArraySubscriptExpr>(E));
    break;
  case Stmt::SK_UnaryExprOrTypeTraitExpr:
    genUnaryExprOrTypeTraitExpr(cast<UnaryExprOrTypeTraitExpr>(E));
    break;
  case Stmt::SK_StmtExpr:
    for (const Stmt *Child : cast<StmtExpr>(E)->getSubStmt()->getBody())
      genStmt(Child);
    break;
  default:
    Diag.fatalAt(E->getBeginLoc(), "invalid expression");
  }
}

static std::string getStringLabelImpl(std::size_t Idx) {
  return std::format(".L.str.{}", Idx);
}

const std::string &CodeGen::getStringLabel(const StringLiteral *SL) {
  std::string &Label = SLCache[SL];
  if (!Label.empty())
    return Label;

  Label = getStringLabelImpl(StringLiterals.size());
  StringLiterals.push_back(SL);
  SLCache[SL] = Label;
  return Label;
}

void CodeGen::genStringLiteral(const StringLiteral *SL) {
  emit("  # load address of string literal");
  emit("  la a0, {}", getStringLabel(SL));
}

void CodeGen::genBinaryOperator(const BinaryOperator *BO) {
  const auto *LHS = BO->getLHS();
  const auto *RHS = BO->getRHS();
  if (BO->getOpcode() == BinaryOperator::BO_Assign) {
    genAddr(LHS);
    push();                   // a1 = addrof(lhs)
    genExpr(BO->getRHS());    // a0 = rhs
    store(LHS->getTypePtr()); // *(a1) = a0
    return;
  }

  if (BO->getOpcode() == BinaryOperator::BO_Comma) {
    genExpr(LHS);
    genExpr(RHS);
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
      emit("  li t0, {}", PointeeTy->getSize());
      emit("  mul a1, a1, t0");
    } else if (const auto *PointeeTy =
                   RType->getPointeeOrArrayElementTypePtr()) {
      // Int(a0) + Ptr
      emit("  li t0, {}", PointeeTy->getSize());
      emit("  mul a0, a0, t0");
    }

    emit("  add a0, a0, a1");
    return;
  }
  case BinaryOperator::BO_Sub: {
    QualType LType = LHS->getType();
    QualType RType = RHS->getType();
    if (const auto *PointeeTy = LType->getPointeeOrArrayElementTypePtr()) {
      if (RType->isPointerType()) {
        // Ptr - Ptr
        emit("  sub a0, a0, a1");
        emit("  li t0, {}", PointeeTy->getSize());
        emit("  div a0, a0, t0");
        return;
      }

      // Ptr - Int(a1)
      emit("  li t0, {}", PointeeTy->getSize());
      emit("  mul a1, a1, t0");
    }

    emit("  sub a0, a0, a1");
    return;
  }
  case BinaryOperator::BO_Mul:
    emit("  mul a0, a0, a1");
    return;
  case BinaryOperator::BO_Div:
    emit("  div a0, a0, a1");
    return;
  case BinaryOperator::BO_EQ:
    // a0 = a0 ^ a1
    // a0 = (a0 == 0) ? 1 : 0
    emit("  xor a0, a0, a1");
    emit("  seqz a0, a0");
    break;
  case BinaryOperator::BO_NE:
    // a0 = a0 ^ a1
    // a0 = (a0 != 0) ? 1 : 0
    emit("  xor a0, a0, a1");
    emit("  snez a0, a0");
    break;
  case BinaryOperator::BO_LT:
    // a0 = a0 < a1.
    // TODO: In the future, we will need to handle unsigned comparisons.
    //
    emit("  slt a0, a0, a1");
    break;
  case BinaryOperator::BO_LE:
    // a0 <= a1  <=>  !(a1 < a0)
    // a0 = a1 < a0
    // a0 = !a0
    emit("  slt a0, a1, a0");
    emit("  xori a0, a0, 1");
    break;
  case BinaryOperator::BO_GT:
    // a0 > a1  <=>  a1 < a0
    emit("  slt a0, a1, a0");
    break;
  case BinaryOperator::BO_GE:
    // a0 >= a1  <=>  !(a0 < a1)
    // a0 = a0 < a1
    // a0 = !a0
    emit("  slt a0, a0, a1");
    emit("  xori a0, a0, 1");
    break;
  default:
    Diag.fatalAt(BO->getOpLocation(), "invalid binary opcode: {}",
                 BO->getOpcodeStr());
  }
}

void CodeGen::genUnaryOperator(const UnaryOperator *UO) {
  switch (UO->getOpcode()) {
  case UnaryOperator::UO_Plus:
    emit("  # unary plus");
    genExpr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Minus:
    genExpr(UO->getSubExpr());
    emit("  neg a0, a0");
    break;
  case UnaryOperator::UO_Addrof:
    emit("  # addrof");
    genAddr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Deref:
    emit("  # deref");
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
    emit("  # set args on calling {}", Func->getName());
    for (const Expr *Arg : CE->getArgs()) {
      genExpr(Arg);
      push();
    }

    for (int I = NumArgs - 1; I >= 0; --I)
      pop(ArgReg[I]);
  }

  const std::string &Name = Func->getName();
  emit("  call {}", Name);
}

void CodeGen::genArraySubscriptExpr(const ArraySubscriptExpr *ASE) {
  // base[idx] <=> *(base + idx)
  // addr = base + idx
  genAddr(ASE);
  // a0 = *(addr)
  QualType AT = ASE->getType();
  std::size_t Size = AT->getSize();
  emit("  l{} a0, 0(a0)", getWidthSuffix(Size));
}

void CodeGen::genUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *UE) {
  std::size_t Size = UE->getSize();
  emit("  # sizeof-expr");
  emit("  li a0, {}", Size);
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
  case Stmt::SK_BinaryOperator: {
    const auto *BO = cast<BinaryOperator>(E);
    if (BO->getOpcode() == BinaryOperator::BO_Comma) {
      genExpr(BO->getLHS());
      genAddr(BO->getRHS());
      return;
    }
    break;
  }
  case Stmt::SK_ArraySubscriptExpr:
    genAddr(cast<ArraySubscriptExpr>(E));
    return;
  case Stmt::SK_StringLiteral:
    genAddr(cast<StringLiteral>(E));
    return;
  case Stmt::SK_ParenExpr:
    genAddr(cast<ParenExpr>(E)->getSubExpr());
    return;
  case Stmt::SK_MemberExpr:
    genAddr(cast<MemberExpr>(E));
    return;
  default:
    break;
  }

  Diag.fatalAt(E->getBeginLoc(), "{} not a lvalue", E->getKindStr());
}

void CodeGen::genAddr(const ArraySubscriptExpr *ASE) {
  // addr = base + idx
  const auto *Base = ASE->getBase();
  const auto *Idx = ASE->getIdx();
  QualType BaseType = Base->getType();
  QualType ElemType = BaseType->getPointeeOrArrayElementType();
  assert(ElemType);

  emit("  # array-subscript-expr");
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
  emit("  li t0, {}", ElemType->getSize());
  emit("  mul a1, a1, t0");
  emit("  add a0, a0, a1");
}

void CodeGen::genAddr(const Decl *D) {
  const auto *Var = dynCast<VarDecl>(D);
  if (!Var)
    Diag.fatalAt(D->getBeginLoc(), "expect a variable");

  if (Var->hasGlobalStorage()) {
    emit("  # genAddr gvar {}", Var->getName());
    emit("  la a0, {}", Var->getName());
  } else {
    emit("  # genAddr lvar {}, offset={}", Var->getName(), Var->getOffset());
    emit("  addi a0, fp, {}", Var->getOffset());
  }
}

void CodeGen::genAddr(const StringLiteral *SL) {
  emit("  # get address of string literal");
  emit("  la a0, {}", getStringLabel(SL));
}

void CodeGen::genAddr(const MemberExpr *ME) {
  emit("  # get address of member expr");
  const auto *Base = ME->getBase();
  genAddr(Base); // a0 = addrof base
  if (ME->isArrow()) {
    emit("  # deref base of arrow member expr");
    load(Base->getTypePtr()); // a0 = *a0
  }
  emit("  li t0, {}", ME->getMemberDecl()->getOffset()); // t0 = offset
  emit("  add a0, a0, t0"); // a0 = addrof base + offset
}

void CodeGen::push() {
  emit("  # push a0");
  emit("  addi sp, sp, -8"); // sp -= 8
  emit("  sd a0, 0(sp)");    // store a0 to stack
  ++Depth;
}

void CodeGen::pop(const char *Reg) {
  emit("  # pop {}", Reg);
  emit("  ld {}, 0(sp)", Reg); // load from stack to Reg
  emit("  addi sp, sp, 8");    // sp += 8
  --Depth;
}

// load *a0 to a0.
void CodeGen::load(const Type *Ty) {
  if (Ty->isArraryType() || Ty->isRecordType())
    return;

  emit("  # load");
  emit("  l{} a0, 0(a0)", getWidthSuffix(Ty->getSize()));
}

// store a0 to *a1.
void CodeGen::store(const Type *Ty) {
  emit("  # store");
  pop("a1");

  if (Ty->isRecordType()) {
    emit("  # store record type");
    for (std::size_t Idx = 0; Idx < Ty->getSize(); ++Idx) {
      // t1 = a0[Idx]
      emit("  li t0, {}", Idx);
      emit("  add t0, a0, t0");
      emit("  lb t1, 0(t0)");

      // *a1[Idx] = t1
      emit("  li t0, {}", Idx);
      emit("  add t0, a1, t0");
      emit("  sb t1, 0(t0)");
    }
    return;
  }

  emit("  s{} a0, 0(a1)", getWidthSuffix(Ty->getSize()));
}

void CodeGen::storeGenReg(int Reg, int Offset, int Size) {
  emit("  s{} {}, {}(fp)", getWidthSuffix(Size), ArgReg[Reg], Offset);
}

char CodeGen::getWidthSuffix(std::size_t Size) const {
  switch (Size) {
  case 1:
    return 'b';
  case 4:
    return 'w';
  case 8:
    return 'd';
  default:
    RCC_UNREACHABLE("unknown type size");
  }
}

int CodeGen::getCount() const {
  static int Count = 1;
  return Count++;
}

} // namespace rcc