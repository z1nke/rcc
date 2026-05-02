#include "CodeGen/CodeGen.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Type.h"
#include "Basic/Diagnostic.h"
#include "Support/Allocator.h"
#include "Support/Casting.h"
#include "Support/Unreachable.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <format>
#include <optional>

namespace rcc {

CodeGen::CodeGen(Diagnostic &Diag, FILE *Fp) : Diag(Diag), Fp(Fp) {}

namespace {

static bool shouldEmitInBss(const VarDecl *Var) { return !Var->getInit(); }

} // namespace

// Returns stack size.
static std::size_t assignLVarOffsets(const FunctionDecl *FD) {
  int Offset = 0;
  for (auto *Var : FD->getLocalVars()) {
    Offset += Var->getType()->getSize();
    Offset = alignTo(Offset, Var->getAlign());
    Var->setOffset(-Offset);
  }

  for (auto *Param : FD->getParams()) {
    Offset += Param->getType()->getSize();
    Offset = alignTo(Offset, Param->getAlign());
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

int CodeGen::simpleLog2(int Num) {
  int N = Num;
  int E = 0;
  while (N > 1) {
    if (N % 2 == 1) {
      Diag.fatal("Wrong value %d", Num);
    }
    N /= 2;
    ++E;
  }
  return E;
}

void CodeGen::emitData(const TranslationUnitDecl *TU) {
  for (const auto *D : TU->decls()) {
    if (const auto *Var = dynCast<VarDecl>(D)) {
      // Skip extern declarations.
      if (!Var->isDefinition())
        continue;

      if (Var->getLinkage() == Linkage::ExternalLinkage)
        emit("  .globl {}", getVarSymbol(Var));
      else
        emit("  .local {}", getVarSymbol(Var));
      // Align global variables.
      assert(Var->getAlign() != 0);
      emit("  .align {}", simpleLog2(Var->getAlign()));
      emit("  {}", shouldEmitInBss(Var) ? ".bss" : ".data");
      emitGlobalVarInit(Var, Var->getInit());
    }
  }

  if (!StringLiterals.empty())
    emit("  .data");
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

void CodeGen::emitGlobalVarInit(const VarDecl *Var, const Expr *Init) {
  emit("{}:", getVarSymbol(Var));
  if (!Init) {
    emit("  .zero {}", Var->getType()->getSize());
    return;
  }

  emitGlobalInit(Init, Var->getType(), 0);
}

namespace {

struct GlobalInitValue {
  std::string Label;
  std::int64_t Addend = 0;

  bool hasLabel() const { return !Label.empty(); }
};

static std::optional<GlobalInitValue> evalGlobalInitValue(const Expr *E);

static const StringLiteral *extractStringLiteral(const Expr *E) {
  if (const auto *SL = dynCast<StringLiteral>(E))
    return SL;
  if (const auto *PE = dynCast<ParenExpr>(E))
    return extractStringLiteral(PE->getSubExpr());
  if (const auto *CE = dynCast<CastExpr>(E))
    return extractStringLiteral(CE->getSubExpr());
  return nullptr;
}

static std::optional<GlobalInitValue> evalGlobalAddress(const Expr *E) {
  E = E->ignoreParens();

  switch (E->getKind()) {
  case Stmt::SK_DeclRefExpr: {
    const auto *Ref = cast<DeclRefExpr>(E);
    const auto *Var = dynCast<VarDecl>(Ref->getDecl());
    if (!Var || !Var->hasGlobalStorage())
      return std::nullopt;
    return GlobalInitValue{Var->getName(), 0};
  }
  case Stmt::SK_CompoundLiteralExpr: {
    const auto *Var = cast<CompoundLiteralExpr>(E)->getVarDecl();
    if (!Var->hasGlobalStorage())
      return std::nullopt;
    return GlobalInitValue{Var->getName(), 0};
  }
  case Stmt::SK_UnaryOperator: {
    const auto *UO = cast<UnaryOperator>(E);
    if (UO->getOpcode() == UnaryOperator::UO_Deref)
      return evalGlobalInitValue(UO->getSubExpr());
    return std::nullopt;
  }
  case Stmt::SK_CastExpr:
    return evalGlobalAddress(cast<CastExpr>(E)->getSubExpr());
  case Stmt::SK_ArraySubscriptExpr: {
    const auto *ASE = cast<ArraySubscriptExpr>(E);
    auto Base = evalGlobalInitValue(ASE->getBase());
    auto Idx = ASE->getIdx()->evaluateAsInt();
    QualType ElemTy = ASE->getBase()->getType()->getPointeeOrArrayElementType();
    if (!Base || !Idx || ElemTy.isNull())
      return std::nullopt;
    Base->Addend += *Idx * static_cast<std::int64_t>(ElemTy->getSize());
    return Base;
  }
  case Stmt::SK_MemberExpr: {
    const auto *ME = cast<MemberExpr>(E);
    auto Base = evalGlobalAddress(ME->getBase());
    if (!Base)
      return std::nullopt;
    Base->Addend += ME->getMemberDecl()->getOffset();
    return Base;
  }
  default:
    return std::nullopt;
  }
}

static std::optional<GlobalInitValue> evalGlobalInitValue(const Expr *E) {
  E = E->ignoreParens();

  if (auto Val = E->evaluateAsInt())
    return GlobalInitValue{"", *Val};

  switch (E->getKind()) {
  case Stmt::SK_DeclRefExpr:
  case Stmt::SK_CompoundLiteralExpr:
  case Stmt::SK_ArraySubscriptExpr:
  case Stmt::SK_MemberExpr:
    return evalGlobalAddress(E);
  case Stmt::SK_UnaryOperator: {
    const auto *UO = cast<UnaryOperator>(E);
    switch (UO->getOpcode()) {
    case UnaryOperator::UO_Addrof:
      return evalGlobalAddress(UO->getSubExpr());
    case UnaryOperator::UO_Deref:
      return evalGlobalInitValue(UO->getSubExpr());
    case UnaryOperator::UO_Plus:
      return evalGlobalInitValue(UO->getSubExpr());
    case UnaryOperator::UO_Minus: {
      auto Val = evalGlobalInitValue(UO->getSubExpr());
      if (!Val || Val->hasLabel())
        return std::nullopt;
      Val->Addend = -Val->Addend;
      return Val;
    }
    default:
      return std::nullopt;
    }
  }
  case Stmt::SK_CastExpr: {
    const auto *Cast = cast<CastExpr>(E);
    if (Cast->getCastKind() == CastExpr::CK_ToVoid)
      return std::nullopt;
    return evalGlobalInitValue(Cast->getSubExpr());
  }
  case Stmt::SK_BinaryOperator: {
    const auto *BO = cast<BinaryOperator>(E);
    if (BO->getOpcode() != BinaryOperator::BO_Add &&
        BO->getOpcode() != BinaryOperator::BO_Sub)
      return std::nullopt;

    auto LHS = evalGlobalInitValue(BO->getLHS());
    auto RHS = evalGlobalInitValue(BO->getRHS());
    if (!LHS || !RHS || (LHS->hasLabel() && RHS->hasLabel()))
      return std::nullopt;

    auto ScaleAddend = [](std::int64_t Addend, QualType PtrTy) {
      if (PtrTy->isPointerType())
        return Addend *
               static_cast<std::int64_t>(PtrTy->getPointeeType()->getSize());
      return Addend;
    };

    if (BO->getOpcode() == BinaryOperator::BO_Add) {
      if (LHS->hasLabel()) {
        LHS->Addend += ScaleAddend(RHS->Addend, BO->getLHS()->getType());
        return LHS;
      }
      RHS->Addend += ScaleAddend(LHS->Addend, BO->getRHS()->getType());
      return RHS;
    }

    if (!LHS->hasLabel())
      return GlobalInitValue{"", LHS->Addend - RHS->Addend};

    LHS->Addend -= ScaleAddend(RHS->Addend, BO->getLHS()->getType());
    return LHS;
  }
  default:
    return std::nullopt;
  }
}

} // namespace

void CodeGen::emitGlobalInit(const Expr *Init, QualType Ty,
                             std::size_t BaseOffset) {
  if (Ty->getAs<ConstantArrayType>()) {
    if (const auto *SL = dynCast<StringLiteral>(Init)) {
      emitGlobalStringLiteralInit(SL, Ty, BaseOffset);
      return;
    }

    const auto *ILE = dynCast<InitListExpr>(Init);
    if (!ILE)
      Diag.fatalAt(Init->getBeginLoc(), "array init requires init-list");
    std::size_t Index = 0;
    emitGlobalInitFromFlat(ILE, Ty, BaseOffset, Index);
    return;
  }

  if (const auto *RT = Ty->getAs<RecordType>()) {
    const auto *RD = RT->getDecl();
    const auto &Fields = RD->fields();
    if (RD->isUnion()) {
      if (Fields.empty()) {
        emit("  .zero {}", Ty->getSize());
        return;
      }

      if (const auto *ILE = dynCast<InitListExpr>(Init)) {
        if (ILE->getNumInits() > 0)
          emitGlobalInit(ILE->getInit(0), Fields[0]->getType(), BaseOffset);
        else
          emitGlobalZeroInit(Fields[0]->getType(), BaseOffset);
      } else {
        emitGlobalInit(Init, Fields[0]->getType(), BaseOffset);
      }

      std::size_t InitSize = Fields[0]->getType()->getSize();
      if (InitSize < Ty->getSize())
        emit("  .zero {}", Ty->getSize() - InitSize);
      return;
    }

    const auto *ILE = dynCast<InitListExpr>(Init);
    if (!ILE)
      Diag.fatalAt(Init->getBeginLoc(), "expect nested initializer list");
    std::size_t Index = 0;
    emitGlobalInitFromFlat(ILE, Ty, BaseOffset, Index);
    return;
  }

  if (const auto *SL = extractStringLiteral(Init)) {
    if (Ty->getSize() != 8)
      Diag.fatalAt(Init->getBeginLoc(), "invalid variable init type");
    emit("  .8byte {}", getStringLabel(SL));
    return;
  }

  auto Eval = Init->evaluateAsInt();
  if (!Eval) {
    auto GlobalVal = evalGlobalInitValue(Init);
    if (!GlobalVal || !GlobalVal->hasLabel() || Ty->getSize() != 8)
      Diag.fatalAt(Init->getBeginLoc(),
                   "global variable initializer is not a constant expression");

    if (GlobalVal->Addend == 0)
      emit("  .8byte {}", GlobalVal->Label);
    else if (GlobalVal->Addend > 0)
      emit("  .8byte {}+{}", GlobalVal->Label, GlobalVal->Addend);
    else
      emit("  .8byte {}{}", GlobalVal->Label, GlobalVal->Addend);
    return;
  }

  emitScalarData(BaseOffset, Ty->getSize(), *Eval);
}

void CodeGen::emitGlobalInitFromFlat(const InitListExpr *List, QualType Ty,
                                     std::size_t BaseOffset, std::size_t &Idx) {
  if (const auto *IAT = Ty->getAs<IncompleteArrayType>()) {
    QualType ElemTy = IAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    std::size_t I = 0;
    while (Idx < List->getNumInits()) {
      std::size_t Offset = BaseOffset + I * ElemSize;
      const Expr *E = List->getInit(Idx);
      if (const auto *SubList = dynCast<InitListExpr>(E)) {
        ++Idx;
        emitGlobalInit(SubList, ElemTy, Offset);
      } else if (ElemTy->isArraryType()) {
        if (const auto *SL = dynCast<StringLiteral>(E)) {
          ++Idx;
          emitGlobalStringLiteralInit(SL, ElemTy, Offset);
        } else {
          emitGlobalInitFromFlat(List, ElemTy, Offset, Idx);
        }
      } else if (ElemTy->isRecordType()) {
        emitGlobalInitFromFlat(List, ElemTy, Offset, Idx);
      } else {
        ++Idx;
        emitGlobalInit(E, ElemTy, Offset);
      }
      ++I;
    }
    return;
  }

  if (const auto *CAT = Ty->getAs<ConstantArrayType>()) {
    QualType ElemTy = CAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    for (std::size_t I = 0; I < CAT->getLength(); ++I) {
      std::size_t Offset = BaseOffset + I * ElemSize;
      if (Idx >= List->getNumInits()) {
        emitGlobalZeroInit(ElemTy, Offset);
        continue;
      }

      const Expr *E = List->getInit(Idx);
      if (const auto *SubList = dynCast<InitListExpr>(E)) {
        ++Idx;
        emitGlobalInit(SubList, ElemTy, Offset);
        continue;
      }
      if (ElemTy->isArraryType()) {
        if (const auto *SL = dynCast<StringLiteral>(E)) {
          ++Idx;
          emitGlobalStringLiteralInit(SL, ElemTy, Offset);
        } else {
          emitGlobalInitFromFlat(List, ElemTy, Offset, Idx);
        }
        continue;
      }
      if (ElemTy->isRecordType()) {
        emitGlobalInitFromFlat(List, ElemTy, Offset, Idx);
        continue;
      }
      ++Idx;
      emitGlobalInit(E, ElemTy, Offset);
    }
    return;
  }

  const auto *RT = Ty->getAs<RecordType>();
  if (!RT)
    Diag.fatalAt(List->getBeginLoc(), "expect aggregate type");
  const auto *RD = RT->getDecl();
  const auto &Fields = RD->fields();
  if (RD->isUnion()) {
    if (Fields.empty() || Idx >= List->getNumInits()) {
      emitGlobalZeroInit(Ty, BaseOffset);
      return;
    }
    const auto *Field = Fields[0];
    QualType FieldTy = Field->getType();
    std::size_t FieldOffset = BaseOffset + Field->getOffset();
    const Expr *E = List->getInit(Idx);
    if (const auto *SubList = dynCast<InitListExpr>(E)) {
      ++Idx;
      emitGlobalInit(SubList, FieldTy, FieldOffset);
    } else if (FieldTy->isArraryType()) {
      if (const auto *SL = dynCast<StringLiteral>(E)) {
        ++Idx;
        emitGlobalStringLiteralInit(SL, FieldTy, FieldOffset);
      } else {
        emitGlobalInitFromFlat(List, FieldTy, FieldOffset, Idx);
      }
    } else if (FieldTy->isRecordType()) {
      emitGlobalInitFromFlat(List, FieldTy, FieldOffset, Idx);
    } else {
      ++Idx;
      emitGlobalInit(E, FieldTy, FieldOffset);
    }

    std::size_t InitSize = FieldTy->getSize();
    if (InitSize < Ty->getSize())
      emit("  .zero {}", Ty->getSize() - InitSize);
    return;
  }

  std::size_t CurrOffset = BaseOffset;
  for (const auto *Field : Fields) {
    std::size_t FieldOffset = BaseOffset + Field->getOffset();
    if (CurrOffset < FieldOffset)
      emit("  .zero {}", FieldOffset - CurrOffset);

    if (Idx < List->getNumInits()) {
      const Expr *E = List->getInit(Idx);
      QualType FieldTy = Field->getType();
      if (const auto *SubList = dynCast<InitListExpr>(E)) {
        ++Idx;
        emitGlobalInit(SubList, FieldTy, FieldOffset);
      } else if (FieldTy->isArraryType()) {
        if (const auto *SL = dynCast<StringLiteral>(E)) {
          ++Idx;
          emitGlobalStringLiteralInit(SL, FieldTy, FieldOffset);
        } else {
          emitGlobalInitFromFlat(List, FieldTy, FieldOffset, Idx);
        }
      } else if (FieldTy->isRecordType()) {
        emitGlobalInitFromFlat(List, FieldTy, FieldOffset, Idx);
      } else {
        ++Idx;
        emitGlobalInit(E, FieldTy, FieldOffset);
      }
    } else {
      emitGlobalZeroInit(Field->getType(), FieldOffset);
    }
    CurrOffset = FieldOffset + Field->getType()->getSize();
  }

  std::size_t EndOffset = BaseOffset + Ty->getSize();
  if (CurrOffset < EndOffset)
    emit("  .zero {}", EndOffset - CurrOffset);
}

void CodeGen::emitGlobalStringLiteralInit(const StringLiteral *SL,
                                          QualType ArrTy,
                                          std::size_t BaseOffset) {
  const auto *CAT = ArrTy->getAs<ConstantArrayType>();
  if (!CAT)
    Diag.fatalAt(SL->getBeginLoc(), "expect constant array type");
  const auto *ElemBT = CAT->getElementType()->getAs<BuiltinType>();
  if (!ElemBT || ElemBT->getKind() != BuiltinType::BK_Char)
    Diag.fatalAt(SL->getBeginLoc(), "invalid variable init type");

  const std::string &Str = SL->getString();
  std::size_t Len = CAT->getLength();
  std::size_t NumInit = std::min<std::size_t>(Len, Str.size() + 1);
  for (std::size_t I = 0; I < NumInit; ++I) {
    unsigned char C = I < Str.size() ? static_cast<unsigned char>(Str[I]) : 0;
    emitScalarData(BaseOffset + I, 1, C);
  }
  if (NumInit < Len)
    emit("  .zero {}", Len - NumInit);
}

void CodeGen::emitGlobalZeroInit(QualType Ty, std::size_t BaseOffset) {
  if (const auto *CAT = Ty->getAs<ConstantArrayType>()) {
    QualType ElemTy = CAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    for (std::size_t I = 0; I < CAT->getLength(); ++I)
      emitGlobalZeroInit(ElemTy, BaseOffset + I * ElemSize);
    return;
  }

  if (const auto *RT = Ty->getAs<RecordType>()) {
    const auto *RD = RT->getDecl();
    const auto &Fields = RD->fields();
    if (RD->isUnion()) {
      emit("  .zero {}", Ty->getSize());
      return;
    }

    std::size_t CurrOffset = BaseOffset;
    for (const auto *Field : Fields) {
      std::size_t FieldOffset = BaseOffset + Field->getOffset();
      if (FieldOffset > CurrOffset)
        emit("  .zero {}", FieldOffset - CurrOffset);
      emitGlobalZeroInit(Field->getType(), FieldOffset);
      CurrOffset = FieldOffset + Field->getType()->getSize();
    }

    std::size_t EndOffset = BaseOffset + Ty->getSize();
    if (CurrOffset < EndOffset)
      emit("  .zero {}", EndOffset - CurrOffset);
    return;
  }

  emitScalarData(BaseOffset, Ty->getSize(), 0);
}

void CodeGen::emitScalarData(std::size_t Offset, std::size_t Size,
                             std::int64_t Val) {
  (void)Offset;

  std::uint64_t Mask = ~std::uint64_t(0);
  if (Size < sizeof(std::uint64_t))
    Mask = (std::uint64_t(1) << (Size * 8)) - 1;
  std::uint64_t UVal = static_cast<std::uint64_t>(Val) & Mask;

  switch (Size) {
  case 1:
    emit("  .byte {}", UVal);
    break;
  case 2:
    emit("  .2byte {}", UVal);
    break;
  case 4:
    emit("  .4byte {}", UVal);
    break;
  case 8:
    emit("  .8byte {}", UVal);
    break;
  default:
    Diag.fatalAt(SourceLocation(), "unsupported scalar size '{}'", Size);
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
  if (!FD->getBody())
    return;

  CurrFunc = FD;
  std::size_t StackSize = assignLVarOffsets(FD);
  const char *Name = FD->getName().c_str();
  if (FD->getLinkage() == Linkage::ExternalLinkage)
    emit("  .globl {}", Name);
  else
    emit("  .local {}", Name);

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
    if (const Expr *RetVal = cast<ReturnStmt>(S)->getRetValue())
      genExpr(RetVal);
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
  case Stmt::SK_DoWhileStmt:
    genDoWhileStmt(cast<DoWhileStmt>(S));
    break;
  case Stmt::SK_SwitchStmt:
    genSwitchStmt(cast<SwitchStmt>(S));
    break;
  case Stmt::SK_CaseStmt:
    genCaseStmt(cast<CaseStmt>(S));
    break;
  case Stmt::SK_DefaultStmt:
    genDefaultStmt(cast<DefaultStmt>(S));
    break;
  case Stmt::SK_BreakStmt:
    genBreakStmt(cast<BreakStmt>(S));
    break;
  case Stmt::SK_ContinueStmt:
    genContinueStmt(cast<ContinueStmt>(S));
    break;
  case Stmt::SK_GotoStmt:
    genGotoStmt(cast<GotoStmt>(S));
    break;
  case Stmt::SK_LabelStmt:
    genLabelStmt(cast<LabelStmt>(S));
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
      // Static locals are initialized in .data/.bss, not at runtime.
      if (Var->hasGlobalStorage())
        continue;
      emitLocalVarInit(Var);
    } else if (isa<TypedefDecl>(D)) {
      continue;
    } else {
      Diag.fatalAt(D->getBeginLoc(), "invalid declaration in decl-stmt");
    }
  }
}

void CodeGen::emitLocalVarInit(const VarDecl *Var) {
  const auto *Init = Var->getInit();
  if (!Init)
    return;

  if (const auto *ILE = dynCast<InitListExpr>(Init)) {
    if (!Var->getType()->isArraryType() && !Var->getType()->isRecordType())
      Diag.fatalAt(Init->getBeginLoc(),
                   "aggregate init requires array or struct type");
    genInitListExpr(Var, ILE, Var->getType(), 0);
  } else if (Var->getType()->isArraryType()) {
    if (const auto *SL = dynCast<StringLiteral>(Init)) {
      genStringLiteralInit(Var, SL, Var->getType(), 0);
    } else {
      Diag.fatalAt(Init->getBeginLoc(), "array init requires init-list");
    }
  } else {
    genAddr(Var);
    push();
    // a0 = init-expr
    genExpr(Init);
    emit("  # initialize variable '{}'", Var->getName());
    store(Var->getType().getTypePtr());
  }
}

void CodeGen::genInitListExpr(const VarDecl *Var, const InitListExpr *List,
                              QualType AggTy, std::size_t BaseOffset) {
  std::size_t Index = 0;
  genInitListExprFromFlat(Var, List, AggTy, BaseOffset, Index);
}

void CodeGen::genInitListExprFromFlat(const VarDecl *Var,
                                      const InitListExpr *List, QualType AggTy,
                                      std::size_t BaseOffset,
                                      std::size_t &Idx) {
  if (const auto *IAT = AggTy->getAs<IncompleteArrayType>()) {
    QualType ElemTy = IAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    std::size_t I = 0;
    while (Idx < List->getNumInits()) {
      std::size_t Offset = BaseOffset + I * ElemSize;
      const Expr *E = List->getInit(Idx);
      if (const auto *SubList = dynCast<InitListExpr>(E)) {
        ++Idx;
        genInitListExpr(Var, SubList, ElemTy, Offset);
      } else if (ElemTy->isArraryType()) {
        if (const auto *SL = dynCast<StringLiteral>(E)) {
          ++Idx;
          genStringLiteralInit(Var, SL, ElemTy, Offset);
        } else {
          genInitListExprFromFlat(Var, List, ElemTy, Offset, Idx);
        }
      } else if (ElemTy->isRecordType()) {
        genInitListExprFromFlat(Var, List, ElemTy, Offset, Idx);
      } else {
        ++Idx;
        genInitListElement(Var, E, ElemTy, Offset);
      }
      ++I;
    }
    return;
  }

  if (const auto *CAT = AggTy->getAs<ConstantArrayType>()) {
    QualType ElemTy = CAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    for (std::size_t I = 0; I < CAT->getLength(); ++I) {
      std::size_t Offset = BaseOffset + I * ElemSize;
      if (Idx >= List->getNumInits()) {
        genZeroInit(Var, ElemTy, Offset);
        continue;
      }

      const Expr *E = List->getInit(Idx);
      if (const auto *SubList = dynCast<InitListExpr>(E)) {
        ++Idx;
        genInitListExpr(Var, SubList, ElemTy, Offset);
        continue;
      }
      if (ElemTy->isArraryType()) {
        if (const auto *SL = dynCast<StringLiteral>(E)) {
          ++Idx;
          genStringLiteralInit(Var, SL, ElemTy, Offset);
        } else {
          genInitListExprFromFlat(Var, List, ElemTy, Offset, Idx);
        }
        continue;
      }
      if (ElemTy->isRecordType()) {
        genInitListExprFromFlat(Var, List, ElemTy, Offset, Idx);
        continue;
      }

      ++Idx;
      genInitListElement(Var, E, ElemTy, Offset);
    }
    return;
  }

  const auto *RT = AggTy->getAs<RecordType>();
  if (!RT)
    Diag.fatalAt(List->getBeginLoc(), "expect aggregate type");
  const auto *RD = RT->getDecl();
  const auto &Fields = RD->fields();
  if (RD->isUnion()) {
    if (Fields.empty() || Idx >= List->getNumInits()) {
      genZeroInit(Var, AggTy, BaseOffset);
      return;
    }

    const auto *Field = Fields[0];
    QualType FieldTy = Field->getType();
    std::size_t Offset = BaseOffset + Field->getOffset();
    const Expr *E = List->getInit(Idx);
    if (const auto *SubList = dynCast<InitListExpr>(E)) {
      ++Idx;
      genInitListExpr(Var, SubList, FieldTy, Offset);
    } else if (FieldTy->isArraryType()) {
      if (const auto *SL = dynCast<StringLiteral>(E)) {
        ++Idx;
        genStringLiteralInit(Var, SL, FieldTy, Offset);
      } else {
        genInitListExprFromFlat(Var, List, FieldTy, Offset, Idx);
      }
    } else if (FieldTy->isRecordType()) {
      genInitListExprFromFlat(Var, List, FieldTy, Offset, Idx);
    } else {
      ++Idx;
      genInitListElement(Var, E, FieldTy, Offset);
    }
    return;
  }

  for (const auto *Field : Fields) {
    std::size_t Offset = BaseOffset + Field->getOffset();
    if (Idx >= List->getNumInits()) {
      genZeroInit(Var, Field->getType(), Offset);
      continue;
    }

    const Expr *E = List->getInit(Idx);
    QualType FieldTy = Field->getType();
    if (const auto *SubList = dynCast<InitListExpr>(E)) {
      ++Idx;
      genInitListExpr(Var, SubList, FieldTy, Offset);
    } else if (FieldTy->isArraryType()) {
      if (const auto *SL = dynCast<StringLiteral>(E)) {
        ++Idx;
        genStringLiteralInit(Var, SL, FieldTy, Offset);
      } else {
        genInitListExprFromFlat(Var, List, FieldTy, Offset, Idx);
      }
    } else if (FieldTy->isRecordType()) {
      genInitListExprFromFlat(Var, List, FieldTy, Offset, Idx);
    } else {
      ++Idx;
      genInitListElement(Var, E, FieldTy, Offset);
    }
  }
}

void CodeGen::genInitListElement(const VarDecl *Var, const Expr *ElemInit,
                                 QualType ElemTy, std::size_t Offset) {
  if (const auto *SubList = dynCast<InitListExpr>(ElemInit)) {
    if (!ElemTy->isArraryType() && !ElemTy->isRecordType())
      Diag.fatalAt(SubList->getBeginLoc(), "invalid nested initializer list");
    genInitListExpr(Var, SubList, ElemTy, Offset);
    return;
  }

  if (ElemTy->isArraryType()) {
    if (const auto *SL = dynCast<StringLiteral>(ElemInit)) {
      genStringLiteralInit(Var, SL, ElemTy, Offset);
      return;
    }
    Diag.fatalAt(ElemInit->getBeginLoc(), "expect nested initializer list");
  }

  if (ElemTy->isRecordType())
    Diag.fatalAt(ElemInit->getBeginLoc(), "expect nested initializer list");

  genExpr(ElemInit);
  push();
  genAddr(Var);
  emit("  addi a1, a0, {}", Offset);
  pop("a0");
  emit("  s{} a0, 0(a1)", getWidthSuffix(ElemTy->getSize()));
}

void CodeGen::genStringLiteralInit(const VarDecl *Var, const StringLiteral *SL,
                                   QualType ArrTy, std::size_t BaseOffset) {
  const auto *CAT = ArrTy->getAs<ConstantArrayType>();
  if (!CAT)
    Diag.fatalAt(SL->getBeginLoc(), "expect constant array type");
  const auto *ElemBT = CAT->getElementType()->getAs<BuiltinType>();
  if (!ElemBT || ElemBT->getKind() != BuiltinType::BK_Char) {
    Diag.fatalAt(SL->getBeginLoc(), "invalid variable init type");
  }

  const std::string &Str = SL->getString();
  const std::size_t Len = CAT->getLength();
  std::size_t NumInit = std::min<std::size_t>(Len, Str.size() + 1);
  for (std::size_t I = 0; I < NumInit; ++I) {
    unsigned char C = I < Str.size() ? static_cast<unsigned char>(Str[I]) : 0;
    emit("  li a0, {}", static_cast<unsigned>(C));
    push();
    genAddr(Var);
    emit("  addi a1, a0, {}", BaseOffset + I);
    pop("a0");
    emit("  sb a0, 0(a1)");
  }
  for (std::size_t I = NumInit; I < Len; ++I)
    genZeroInit(Var, CAT->getElementType(), BaseOffset + I);
}

void CodeGen::genZeroInit(const VarDecl *Var, QualType Ty,
                          std::size_t BaseOffset) {
  if (const auto *CAT = Ty->getAs<ConstantArrayType>()) {
    QualType ElemTy = CAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    for (std::size_t I = 0; I < CAT->getLength(); ++I)
      genZeroInit(Var, ElemTy, BaseOffset + I * ElemSize);
    return;
  }

  if (const auto *RT = Ty->getAs<RecordType>()) {
    const auto &Fields = RT->getDecl()->fields();
    for (const auto *Field : Fields)
      genZeroInit(Var, Field->getType(), BaseOffset + Field->getOffset());
    return;
  }

  emit("  li a0, 0");
  push();
  genAddr(Var);
  emit("  addi a1, a0, {}", BaseOffset);
  pop("a0");
  emit("  s{} a0, 0(a1)", getWidthSuffix(Ty->getSize()));
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
  BreakCounts.push_back(Count);
  ContinueCounts.push_back(Count);
  emit(".L.begin.{}:", Count);
  if (const auto *Cond = For->getCond()) {
    genExpr(Cond);
    emit("  beqz a0, .L.end.{}", Count);
  }
  genStmt(For->getBody());
  emit(".L.continue.{}:", Count);
  if (const auto *Inc = For->getInc())
    genExpr(Inc);
  emit("  j .L.begin.{}", Count);
  emit(".L.end.{}:", Count);
  ContinueCounts.pop_back();
  BreakCounts.pop_back();
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
  BreakCounts.push_back(Count);
  ContinueCounts.push_back(Count);
  emit(".L.continue.{}:", Count);
  emit(".L.begin.{}:", Count);
  genExpr(While->getCond());
  emit("  beqz a0, .L.end.{}", Count);
  genStmt(While->getBody());
  emit("  j .L.begin.{}", Count);
  emit(".L.end.{}:", Count);
  ContinueCounts.pop_back();
  BreakCounts.pop_back();
}

void CodeGen::genDoWhileStmt(const DoWhileStmt *DoWhile) {
  int Count = getCount();
  // .L.begin.C:
  //   body-stmt
  // .L.continue.C:
  //   cond-expr
  //   if a0 != 0 goto .L.begin.C
  // .L.end.C:
  //   ...
  BreakCounts.push_back(Count);
  ContinueCounts.push_back(Count);
  emit(".L.begin.{}:", Count);
  genStmt(DoWhile->getBody());
  emit(".L.continue.{}:", Count);
  genExpr(DoWhile->getCond());
  emit("  bnez a0, .L.begin.{}", Count);
  emit(".L.end.{}:", Count);
  ContinueCounts.pop_back();
  BreakCounts.pop_back();
}

void CodeGen::genSwitchStmt(const SwitchStmt *Switch) {
  int Count = getCount();
  genExpr(Switch->getCond());
  const DefaultStmt *Default = nullptr;
  for (const auto *SC = Switch->getSwitchCaseList(); SC;
       SC = SC->getNextSwitchCase()) {
    if (const auto *CS = dynCast<CaseStmt>(SC)) {
      emit("  li t0, {}", CS->getCaseValue());
      emit("  beq a0, t0, .L.case.{}.{}", Count, CS->getLabelId());
      continue;
    }
    Default = cast<DefaultStmt>(SC);
  }

  if (Default)
    emit("  j .L.default.{}.{}", Count, Default->getLabelId());
  else
    emit("  j .L.end.{}", Count);

  BreakCounts.push_back(Count);
  SwitchCounts.push_back(Count);
  genStmt(Switch->getBody());
  SwitchCounts.pop_back();
  BreakCounts.pop_back();
  emit(".L.end.{}:", Count);
}

void CodeGen::genCaseStmt(const CaseStmt *Case) {
  if (SwitchCounts.empty())
    Diag.fatalAt(Case->getBeginLoc(), "case statement not in switch");
  int SwitchCount = SwitchCounts.back();
  emit(".L.case.{}.{}:", SwitchCount, Case->getLabelId());
  genStmt(Case->getSubStmt());
}

void CodeGen::genDefaultStmt(const DefaultStmt *Default) {
  if (SwitchCounts.empty())
    Diag.fatalAt(Default->getBeginLoc(), "default statement not in switch");
  int SwitchCount = SwitchCounts.back();
  emit(".L.default.{}.{}:", SwitchCount, Default->getLabelId());
  genStmt(Default->getSubStmt());
}

void CodeGen::genBreakStmt(const BreakStmt *Break) {
  if (BreakCounts.empty())
    Diag.fatalAt(Break->getBeginLoc(), "break statement not in loop");
  emit("  j .L.end.{}", BreakCounts.back());
}

void CodeGen::genContinueStmt(const ContinueStmt *Continue) {
  if (ContinueCounts.empty())
    Diag.fatalAt(Continue->getBeginLoc(), "continue statement not in loop");
  emit("  j .L.continue.{}", ContinueCounts.back());
}

void CodeGen::genGotoStmt(const GotoStmt *Goto) {
  const LabelDecl *Label = Goto->getLabel();
  std::string AsmLabel =
      std::format(".L.label.{}.{}", CurrFunc->getName(), Label->getName());
  emit("  j {}", AsmLabel);
}

void CodeGen::genLabelStmt(const LabelStmt *Label) {
  const LabelDecl *Decl = Label->getDecl();
  std::string AsmLabel =
      std::format(".L.label.{}.{}", CurrFunc->getName(), Decl->getName());
  emit("{}:", AsmLabel);
  genStmt(Label->getSubStmt());
}

void CodeGen::genExpr(const Expr *E) {
  switch (E->getKind()) {
  case Stmt::SK_UnaryOperator:
    genUnaryOperator(cast<UnaryOperator>(E));
    break;
  case Stmt::SK_BinaryOperator:
    genBinaryOperator(cast<BinaryOperator>(E));
    break;
  case Stmt::SK_ConditionalOperator:
    genConditionalOperator(cast<ConditionalOperator>(E));
    break;
  case Stmt::SK_IntegerLiteral: {
    // li a0, imm
    auto Val = cast<IntegerLiteral>(E)->getVal();
    emit("  # a0 = {}", Val);
    emit("  li a0, {}", Val);
    break;
  }
  case Stmt::SK_CharacterLiteral: {
    auto Val = cast<CharacterLiteral>(E)->getValue();
    emit("  # a0 = '{}'", static_cast<unsigned char>(Val));
    emit("  li a0, {}", Val);
    break;
  }
  case Stmt::SK_StringLiteral:
    genStringLiteral(cast<StringLiteral>(E));
    break;
  case Stmt::SK_ParenExpr:
    genExpr(cast<ParenExpr>(E)->getSubExpr());
    break;
  case Stmt::SK_DeclRefExpr:
    genDeclRefExpr(cast<DeclRefExpr>(E));
    break;
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
  case Stmt::SK_CastExpr:
    genCastExpr(cast<CastExpr>(E));
    break;
  case Stmt::SK_CompoundLiteralExpr:
    genCompoundLiteralExpr(cast<CompoundLiteralExpr>(E));
    break;
  case Stmt::SK_InitListExpr:
    Diag.fatalAt(E->getBeginLoc(), "init-list expression is not evaluatable");
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

const std::string &CodeGen::getVarSymbol(const VarDecl *Var) {
  if (!Var->isStaticLocal())
    return Var->getName();

  std::string &Name = StaticLocalNames[Var];
  if (Name.empty())
    Name = std::format(".L..{}", AnonGVarId++);
  return Name;
}

void CodeGen::genIntCast(const Type *From, const Type *To) {
  if (To->isBooleanType()) {
    emit("  snez a0, a0");
    return;
  }

  enum CastTypeID : unsigned {
    I8,
    I16,
    I32,
    I64,
  };

  auto GetTypeID = [](const Type *T) {
    if (const auto *BT = dynCast<BuiltinType>(T)) {
      switch (BT->getKind()) {
      case BuiltinType::BK_Char:
        return I8;
      case BuiltinType::BK_Short:
        return I16;
      case BuiltinType::BK_Int:
        return I32;
      default:
        return I64;
      }
    }

    return I64;
  };

#define INT_CAST(FROM, TO, SHIFT)                                              \
  static constexpr std::string_view I##FROM##To##I##TO =                       \
      "slli a0, a0, " #SHIFT "\nsrai a0, a0, " #SHIFT;

  INT_CAST(64, 8, 56);
  INT_CAST(64, 16, 48);
  INT_CAST(64, 32, 32);

  constexpr std::array<std::array<std::string_view, 10>, 10> IntCastTable = {{
      // To: i8   i16   i32  i64    | From:
      {},                            // | i8
      {I64ToI8},                     // | i16
      {I64ToI8, I64ToI16},           // | i32
      {I64ToI8, I64ToI16, I64ToI32}, // | i64
  }};

  unsigned ID1 = GetTypeID(From);
  unsigned ID2 = GetTypeID(To);
  std::string_view CastInsts = IntCastTable[ID1][ID2];
  if (!CastInsts.empty()) {
    emit("{}", CastInsts);
  }
}

void CodeGen::genStringLiteral(const StringLiteral *SL) {
  emit("  # load address of string literal");
  emit("  la a0, {}", getStringLabel(SL));
}

void CodeGen::genDeclRefExpr(const DeclRefExpr *Ref) {
  const auto *D = Ref->getDecl();
  if (const auto *ECD = dynCast<EnumConstantDecl>(D)) {
    emit("  li a0, {}", ECD->getValue());
    return;
  }

  genAddr(D);              // a0 = addr
  load(Ref->getTypePtr()); // a0 = *a0
}

void CodeGen::emitBinaryArithmeticResult(BinaryOperator::Opcode Op,
                                         QualType LType, QualType RType,
                                         const char *Suffix) {
  switch (Op) {
  case BinaryOperator::BO_Add:
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
    emit("  add{} a0, a0, a1", Suffix);
    return;
  case BinaryOperator::BO_Sub:
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
    emit("  sub{} a0, a0, a1", Suffix);
    return;
  case BinaryOperator::BO_Mul:
    emit("  mul{} a0, a0, a1", Suffix);
    return;
  case BinaryOperator::BO_Div:
    emit("  div{} a0, a0, a1", Suffix);
    return;
  case BinaryOperator::BO_Rem:
    emit("  rem{} a0, a0, a1", Suffix);
    return;
  case BinaryOperator::BO_And:
    emit("  and a0, a0, a1");
    return;
  case BinaryOperator::BO_Or:
    emit("  or a0, a0, a1");
    return;
  case BinaryOperator::BO_Xor:
    emit("  xor a0, a0, a1");
    return;
  case BinaryOperator::BO_Shl:
    emit("  sll a0, a0, a1");
    return;
  case BinaryOperator::BO_Shr:
    emit("  sra a0, a0, a1");
    return;
  default:
    RCC_UNREACHABLE("emitBinaryArithmeticResult: invalid opcode");
  }
}

void CodeGen::genBinaryOperator(const BinaryOperator *BO) {
  const auto *LHS = BO->getLHS();
  const auto *RHS = BO->getRHS();
  auto Op = BO->getOpcode();
  switch (Op) {
  case BinaryOperator::BO_Assign:
    genAddr(LHS);
    push();                   // a1 = addrof(lhs)
    genExpr(BO->getRHS());    // a0 = rhs
    store(LHS->getTypePtr()); // *(a1) = a0
    return;
  case BinaryOperator::BO_Comma:
    genExpr(LHS);
    genExpr(RHS);
    return;
  case BinaryOperator::BO_LAnd: {
    int Count = getCount();
    genExpr(LHS);
    emit("  # logical-and test left");
    emit("  beqz a0, .L.false.{}", Count);
    genExpr(RHS);
    emit("  # logical-and test right");
    emit("  beqz a0, .L.false.{}", Count);
    emit("  li a0, 1");
    emit("  j .L.end.{}", Count);
    emit(".L.false.{}:", Count);
    emit("  li a0, 0");
    emit(".L.end.{}:", Count);
    return;
  }
  case BinaryOperator::BO_LOr: {
    int Count = getCount();
    genExpr(LHS);
    emit("  # logical-or test left");
    emit("  bnez a0, .L.true.{}", Count);
    genExpr(RHS);
    emit("  # logical-or test right");
    emit("  bnez a0, .L.true.{}", Count);
    emit("  li a0, 0");
    emit("  j .L.end.{}", Count);
    emit(".L.true.{}:", Count);
    emit("  li a0, 1");
    emit(".L.end.{}:", Count);
    return;
  }
  default:
    break;
  }

  QualType LType = LHS->getType();
  QualType RType = RHS->getType();
  const char *Suffix = LType->getSize() <= 4 ? "w" : "";

  if (BO->isCompoundAssign()) {
    // A op= B
    // push &A
    genAddr(LHS);
    push();

    // a0 = LHS, a1 = RHS
    genExpr(RHS);
    push();
    genExpr(LHS);
    pop("a1");

    auto BaseOp = BO->getOpForCompoundAssign();
    // a0 = LHS op RHS
    emitBinaryArithmeticResult(BaseOp, LType, RType, Suffix);
    // *&A = a0
    store(LType.getTypePtr());
    return;
  }

  // a0 op a1
  genExpr(RHS);
  push();
  genExpr(LHS);
  pop("a1");

  switch (Op) {
  case BinaryOperator::BO_Add:
  case BinaryOperator::BO_Sub:
  case BinaryOperator::BO_Mul:
  case BinaryOperator::BO_Div:
  case BinaryOperator::BO_Rem:
  case BinaryOperator::BO_And:
  case BinaryOperator::BO_Or:
  case BinaryOperator::BO_Xor:
  case BinaryOperator::BO_Shl:
  case BinaryOperator::BO_Shr:
    emitBinaryArithmeticResult(Op, LType, RType, Suffix);
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

void CodeGen::genConditionalOperator(const ConditionalOperator *CO) {
  int Count = getCount();
  genExpr(CO->getCond());
  emit("  beqz a0, .L.else.{}", Count);
  genExpr(CO->getTrueExpr());
  emit("  j .L.end.{}", Count);
  emit(".L.else.{}:", Count);
  genExpr(CO->getFalseExpr());
  emit(".L.end.{}:", Count);
}

void CodeGen::genUnaryOperator(const UnaryOperator *UO) {
  switch (UO->getOpcode()) {
  case UnaryOperator::UO_Plus:
    emit("  # unary plus");
    genExpr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Minus:
    genExpr(UO->getSubExpr());
    emit("  neg{} a0, a0", UO->getType()->getSize() <= 4 ? "w" : "");
    break;
  case UnaryOperator::UO_LNot:
    genExpr(UO->getSubExpr());
    emit("  # unary lnot");
    emit("  seqz a0, a0");
    break;
  case UnaryOperator::UO_Not:
    genExpr(UO->getSubExpr());
    emit("  # unary not");
    emit("  not a0, a0");
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
  case UnaryOperator::UO_PreInc:
  case UnaryOperator::UO_PreDec: {
    const auto *SubExpr = UO->getSubExpr();
    QualType SubType = SubExpr->getType();
    std::size_t Step = 1;
    if (const auto *PointeeTy = SubType->getPointeeOrArrayElementTypePtr())
      Step = PointeeTy->getSize();

    emit("  # pre {} operator", UO->getOpcodeStr());
    genAddr(SubExpr);
    push();
    load(SubType.getTypePtr());
    emit("  li t0, {}", Step);
    emit("  {} a0, a0, t0", UO->isIncrement() ? "add" : "sub");
    store(SubType.getTypePtr());
    break;
  }
  case UnaryOperator::UO_PostInc:
  case UnaryOperator::UO_PostDec: {
    const auto *SubExpr = UO->getSubExpr();
    QualType SubType = SubExpr->getType();
    std::size_t Step = 1;
    if (const auto *PointeeTy = SubType->getPointeeOrArrayElementTypePtr())
      Step = PointeeTy->getSize();

    emit("  # post {} operator", UO->getOpcodeStr());
    genAddr(SubExpr);
    push();
    load(SubType.getTypePtr());
    emit("  mv t2, a0");
    emit("  li t0, {}", Step);
    emit("  {} a0, a0, t0", UO->isIncrement() ? "add" : "sub");
    store(SubType.getTypePtr());
    emit("  mv a0, t2");
    break;
  }
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
  // RISC-V ABI requires sp to be 16-byte aligned at call sites. Each push
  // adjusts sp by 8, so an odd depth means sp is only 8-byte aligned.
  if (Depth % 2 == 0) {
    emit("  # call {}", Name);
    emit("  call {}", Name);
  } else {
    emit("  # align sp to 16-byte boundary and call {}", Name);
    emit("  addi sp, sp, -8");
    emit("  call {}", Name);
    emit("  addi sp, sp, 8");
  }
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

void CodeGen::genCastExpr(const CastExpr *Cast) {
  const Expr *SubExpr = Cast->getSubExpr();
  genExpr(SubExpr);

  switch (Cast->getCastKind()) {
  case CastExpr::CK_NoOp:
  case CastExpr::CK_ToVoid:
    return;
  case CastExpr::CK_IntegralCast:
  case CastExpr::CK_IntegralToPointer:
  case CastExpr::CK_PointerToIntegral:
  case CastExpr::CK_BitCast:
    genIntCast(SubExpr->getTypePtr(), Cast->getTypePtr());
    break;
  case CastExpr::CK_FuncToPointerDecay:
  case CastExpr::CK_ArrayToPointerDecay:
    genAddr(SubExpr);
    break;
  default:
    RCC_UNREACHABLE("Unknown cast kind");
  }
}

void CodeGen::genCompoundLiteralExpr(const CompoundLiteralExpr *CLE) {
  const VarDecl *Var = CLE->getVarDecl();
  if (!Var->hasGlobalStorage())
    emitLocalVarInit(Var);
  genAddr(Var);

  if (!CLE->getType()->isArraryType() && !CLE->getType()->isRecordType())
    load(CLE->getTypePtr());
}

void CodeGen::genAddr(const Expr *E) {
  switch (E->getKind()) {
  case Stmt::SK_DeclRefExpr: {
    const auto *Ref = cast<DeclRefExpr>(E);
    genAddr(Ref->getDecl());
    return;
  }
  case Stmt::SK_CompoundLiteralExpr: {
    const auto *CLE = cast<CompoundLiteralExpr>(E);
    const VarDecl *Var = CLE->getVarDecl();
    if (!Var->hasGlobalStorage())
      emitLocalVarInit(Var);
    genAddr(Var);
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
    emit("  # genAddr gvar {}", getVarSymbol(Var));
    emit("  la a0, {}", getVarSymbol(Var));
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
  case 2:
    return 'h';
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