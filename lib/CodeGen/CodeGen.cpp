#include "CodeGen/CodeGen.h"
#include "AST/Decl.h"
#include "AST/Stmt.h"
#include "AST/Type.h"
#include "Basic/Diagnostic.h"
#include "Basic/Linkage.h"
#include "Support/Allocator.h"
#include "Support/Casting.h"
#include "Support/Unreachable.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <optional>
#include <utility>
#include <vector>

namespace rcc {

CodeGen::CodeGen(Diagnostic &Diag, FILE *Fp) : Diag(Diag), Fp(Fp) {}

namespace {

static bool shouldEmitInBss(const VarDecl *Var) { return !Var->getInit(); }

// Arrays of at least 16 bytes must be aligned to at least 16 bytes
// (SysV ABI / GCC convention for large local/global arrays).
static std::size_t getVarEmitAlign(const VarDecl *Var) {
  std::size_t Align = Var->getAlign();
  QualType Ty = Var->getType();
  if (Ty->getAs<ArrayType>() && Ty->getSize() >= 16)
    return std::max(Align, std::size_t{16});
  return Align;
}

static constexpr int GPMAX = 8;
static constexpr int FPMAX = 8;

struct FloatStructPassInfo {
  const Type *Reg1Ty = nullptr;
  const Type *Reg2Ty = nullptr;
  bool IsFloatStruct = false;
};

static void collectFloatStructMembers(const Type *Ty, const Type *RegsTy[2],
                                      int &Idx) {
  if (const auto *RT = Ty->getAs<RecordType>()) {
    if (RT->getDecl()->isUnion()) {
      Idx += 2;
      return;
    }
    for (const FieldDecl *Field : RT->getDecl()->fields())
      collectFloatStructMembers(Field->getType().getTypePtr(), RegsTy, Idx);
    return;
  }
  if (const auto *CAT = Ty->getAs<ConstantArrayType>()) {
    for (std::size_t I = 0; I < CAT->getLength(); ++I)
      collectFloatStructMembers(CAT->getElementType().getTypePtr(), RegsTy,
                                Idx);
    return;
  }
  if (Idx < 2)
    RegsTy[Idx] = Ty;
  ++Idx;
}

static FloatStructPassInfo getFloatStructPassInfo(const Type *Ty, int GP,
                                                  int FP) {
  FloatStructPassInfo Info;
  if (const auto *RT = Ty->getAs<RecordType>()) {
    if (RT->getDecl()->isUnion())
      return Info;
  }

  const Type *RegsTy[2] = {nullptr, nullptr};
  int Idx = 0;
  collectFloatStructMembers(Ty, RegsTy, Idx);
  if (Idx > 2)
    return Info;

  if ((RegsTy[0] && RegsTy[0]->isFloatingType() && !RegsTy[1] && FP < FPMAX) ||
      (RegsTy[0] && RegsTy[0]->isFloatingType() && RegsTy[1] &&
       RegsTy[1]->isIntegerType() && FP < FPMAX && GP < GPMAX) ||
      (RegsTy[0] && RegsTy[0]->isIntegerType() && RegsTy[1] &&
       RegsTy[1]->isFloatingType() && FP < FPMAX && GP < GPMAX) ||
      (RegsTy[0] && RegsTy[0]->isFloatingType() && RegsTy[1] &&
       RegsTy[1]->isFloatingType() && FP + 1 < FPMAX)) {
    Info.Reg1Ty = RegsTy[0];
    Info.Reg2Ty = RegsTy[1];
    Info.IsFloatStruct = true;
  }
  return Info;
}

static bool isLargeStructByPointer(const Type *Ty) {
  if (Ty->getSize() <= 16)
    return false;
  const auto *RT = Ty->getAs<RecordType>();
  return RT && !RT->getDecl()->isUnion();
}

static void countStructArgRegs(const Type *Ty, int &GP, int &FP,
                               bool &PassByStack) {
  PassByStack = false;
  if (isLargeStructByPointer(Ty)) {
    if (GP < GPMAX)
      ++GP;
    else
      PassByStack = true;
    return;
  }

  FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, GP, FP);
  if (PassInfo.IsFloatStruct) {
    const Type *Regs[2] = {PassInfo.Reg1Ty, PassInfo.Reg2Ty};
    int GPNeeded = 0;
    int FPNeeded = 0;
    for (int I = 0; I < 2; ++I) {
      if (!Regs[I])
        break;
      if (Regs[I]->isFloatingType())
        ++FPNeeded;
      else
        ++GPNeeded;
    }
    if (GPNeeded <= GPMAX - GP && FPNeeded <= FPMAX - FP) {
      GP += GPNeeded;
      FP += FPNeeded;
    } else {
      PassByStack = true;
    }
    return;
  }

  int Regs = (Ty->getSize() > 8 && Ty->getSize() <= 16) ? 2 : 1;
  for (int I = 0; I < Regs; ++I) {
    if (GP < GPMAX)
      ++GP;
    else
      PassByStack = true;
  }
}

static int countStructArgStackSlots(const Type *Ty) {
  if (isLargeStructByPointer(Ty))
    return 1;
  return static_cast<int>(alignTo(Ty->getSize(), 8) / 8);
}

static bool useFloatStructStackPass(const FloatStructPassInfo &PassInfo) {
  if (!PassInfo.IsFloatStruct)
    return false;
  if (PassInfo.Reg2Ty && PassInfo.Reg2Ty->isFloatingType())
    return true;
  return PassInfo.Reg1Ty && PassInfo.Reg1Ty->isFloatingType() &&
         PassInfo.Reg2Ty;
}

} // namespace

static const VarDecl *findVaAreaVar(const FunctionDecl *FD) {
  for (const VarDecl *Var : FD->getLocalVars()) {
    if (Var->getName() == "__va_area__")
      return Var;
  }
  return nullptr;
}

static const VarDecl *findSretVar(const FunctionDecl *FD) {
  for (const VarDecl *Var : FD->getLocalVars()) {
    if (Var->getName() == "__sret__")
      return Var;
  }
  return nullptr;
}

/// Count GP registers that named parameters (and sret) would consume.
static int countNamedParamGPs(const FunctionDecl *FD) {
  int GP = 0, FP = 0;
  if (findSretVar(FD))
    ++GP;
  for (const auto *Param : FD->getParams()) {
    const Type *Ty = Param->getType().getTypePtr();
    if (Ty->isFloatingType()) {
      if (FP < FPMAX)
        ++FP;
      else
        ++GP;
      continue;
    }
    if (Ty->isRecordType()) {
      FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, GP, FP);
      if (PassInfo.IsFloatStruct) {
        const Type *Regs[2] = {PassInfo.Reg1Ty, PassInfo.Reg2Ty};
        for (int I = 0; I < 2; ++I) {
          if (!Regs[I])
            break;
          if (Regs[I]->isFloatingType())
            ++FP;
          if (Regs[I]->isIntegerType())
            ++GP;
        }
        continue;
      }
      if (isLargeStructByPointer(Ty) || Ty->getSize() <= 8 ||
          Param->isHalfByStack())
        ++GP;
      else
        GP += 2;
      continue;
    }
    ++GP;
  }
  return GP;
}

// Returns stack size for locals / register-passed params (below fp).
// Stack-passed params keep positive offsets at fp+16 and above (caller area).
static std::size_t assignLVarOffsets(const FunctionDecl *FD) {
  // After the prologue saves ra/fp (16 bytes), the caller's stack arguments
  // begin at fp+16.
  int ReOffset = 16;
  int GP = 0, FP = 0;
  // Hidden sret pointer occupies a0 when returning a large struct/union.
  if (findSretVar(FD))
    ++GP;
  for (auto *Param : FD->getParams()) {
    const Type *Ty = Param->getType().getTypePtr();
    bool PassByStack = false;
    if (Ty->isFloatingType()) {
      if (FP < FPMAX) {
        ++FP;
        continue;
      }
      if (GP < GPMAX) {
        ++GP;
        continue;
      }
      PassByStack = true;
    } else if (Ty->isRecordType()) {
      FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, GP, FP);
      if (PassInfo.IsFloatStruct) {
        const Type *Regs[2] = {PassInfo.Reg1Ty, PassInfo.Reg2Ty};
        for (int I = 0; I < 2; ++I) {
          if (!Regs[I])
            break;
          if (Regs[I]->isFloatingType())
            ++FP;
          if (Regs[I]->isIntegerType())
            ++GP;
        }
        continue;
      }

      // 9–16 byte integer structs use two GP registers; if only one remains,
      // the upper half is passed on the stack.
      if (Ty->getSize() > 8 && Ty->getSize() <= 16) {
        if (GP == GPMAX - 1)
          Param->setHalfByStack(true);
        if (GP < GPMAX)
          ++GP;
      }
      if (GP < GPMAX) {
        ++GP;
        continue;
      }
      PassByStack = true;
    } else if (GP < GPMAX) {
      ++GP;
      continue;
    } else {
      PassByStack = true;
    }

    if (PassByStack) {
      ReOffset = alignTo(ReOffset, 8);
      Param->setOffset(ReOffset);
      // Half-by-stack: only the upper half lives in the caller arg area.
      ReOffset += Param->isHalfByStack() ? static_cast<int>(Ty->getSize()) - 8
                                         : static_cast<int>(Ty->getSize());
    }
  }

  // Place __va_area__ at a positive offset so saved register args are
  // contiguous with stack-passed variadic arguments (caller area).
  if (VarDecl *VaArea = const_cast<VarDecl *>(findVaAreaVar(FD))) {
    ReOffset = alignTo(ReOffset, 8);
    VaArea->setOffset(ReOffset);
  }

  int Offset = 0;
  for (auto *Var : FD->getLocalVars()) {
    // __va_area__ already has a positive caller-area offset.
    if (Var->getOffset() > 0)
      continue;
    Offset += Var->getType()->getSize();
    Offset = alignTo(Offset, getVarEmitAlign(Var));
    Var->setOffset(-Offset);
  }

  for (auto *Param : FD->getParams()) {
    // Fully stack-passed params keep their positive caller-area offset.
    // Half-by-stack params are copied into a local slot in the prologue.
    if (Param->getOffset() > 0 && !Param->isHalfByStack())
      continue;
    Offset += Param->getType()->getSize();
    Offset = alignTo(Offset, getVarEmitAlign(Param));
    Param->setOffset(-Offset);
  }

  return alignTo(Offset, 16);
}

static const char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"};
static const char *FaArgReg[] = {"fa0", "fa1", "fa2", "fa3",
                                 "fa4", "fa5", "fa6", "fa7"};

void CodeGen::codegen(const TranslationUnitDecl *TU, const char *Input) {
  emit(".file 1 \"{}\"", Input);
  emitText(TU);
  emitData(TU);
  // Global initializers may reference deferred functions.
  emitDeferred();
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
      emit("  .align {}", simpleLog2(static_cast<int>(getVarEmitAlign(Var))));
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
    for (unsigned char C : SL->getString())
      emit("  .byte {}", static_cast<int>(C));
    // Emit a null terminator sized to the string element type.
    std::size_t NullBytes = 1;
    if (const auto *CAT = SL->getType()->getAs<ConstantArrayType>())
      NullBytes = CAT->getElementType()->getSize();
    for (std::size_t I = 0; I < NullBytes; ++I)
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
  const FunctionDecl *Func = nullptr;

  bool hasLabel() const { return !Label.empty(); }
};

static std::optional<GlobalInitValue> evalGlobalInitValue(const Expr *E);

static bool recordHasBitField(const RecordDecl *RD) {
  for (const auto *Field : RD->fields()) {
    if (Field->isBitField())
      return true;
    if (const auto *FRT = Field->getType()->getAs<RecordType>()) {
      if (recordHasBitField(FRT->getDecl()))
        return true;
    }
  }
  return false;
}

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
    const auto *VD = Ref->getDecl();
    if (const auto *Var = dynCast<VarDecl>(VD)) {
      if (!Var->hasGlobalStorage())
        return std::nullopt;
      return GlobalInitValue{Var->getName(), 0};
    }
    if (const auto *Func = dynCast<FunctionDecl>(VD))
      return GlobalInitValue{Func->getName(), 0, Func};
    return std::nullopt;
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

      unsigned FI = 0;
      if (const auto *ILE = dynCast<InitListExpr>(Init))
        FI = ILE->getUnionFieldIndex();
      if (FI >= Fields.size())
        FI = 0;
      const auto *Field = Fields[FI];
      QualType FieldTy = Field->getType();

      if (recordHasBitField(RD)) {
        std::vector<std::uint8_t> Buf(Ty->getSize(), 0);
        if (const auto *ILE = dynCast<InitListExpr>(Init)) {
          if (ILE->getNumInits() > 0)
            writeGlobalInitToBuf(Buf, Field->getOffset(), ILE->getInit(0),
                                 FieldTy);
        } else {
          writeGlobalInitToBuf(Buf, Field->getOffset(), Init, FieldTy);
        }
        emitDataBuf(Buf);
        return;
      }

      if (const auto *ILE = dynCast<InitListExpr>(Init)) {
        if (ILE->getNumInits() > 0)
          emitGlobalInit(ILE->getInit(0), FieldTy,
                         BaseOffset + Field->getOffset());
        else
          emitGlobalZeroInit(FieldTy, BaseOffset + Field->getOffset());
      } else {
        emitGlobalInit(Init, FieldTy, BaseOffset + Field->getOffset());
      }

      std::size_t InitSize = FieldTy->getSize();
      if (InitSize < Ty->getSize())
        emit("  .zero {}", Ty->getSize() - InitSize);
      return;
    }

    const auto *ILE = dynCast<InitListExpr>(Init);
    if (!ILE)
      Diag.fatalAt(Init->getBeginLoc(), "expect nested initializer list");

    if (recordHasBitField(RD)) {
      std::vector<std::uint8_t> Buf(Ty->getSize(), 0);
      std::size_t Index = 0;
      writeGlobalInitToBufFromFlat(Buf, 0, ILE, Ty, Index);
      emitDataBuf(Buf);
      return;
    }

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

  // Floating-point constant initializers.
  if (Ty->isFloatingType()) {
    auto DblVal = Init->evaluateAsDouble();
    if (!DblVal)
      Diag.fatalAt(Init->getBeginLoc(),
                   "global variable initializer is not a constant expression");

    if (Ty->getSize() == 4) {
      float FVal = static_cast<float>(*DblVal);
      std::uint32_t Bits = 0;
      static_assert(sizeof(FVal) == sizeof(Bits));
      memcpy(&Bits, &FVal, sizeof(Bits));
      emit("  .4byte {}", Bits);
    } else {
      assert(Ty->getSize() == 8);
      double DVal = *DblVal;
      std::uint64_t Bits = 0;
      static_assert(sizeof(DVal) == sizeof(Bits));
      memcpy(&Bits, &DVal, sizeof(Bits));
      emit("  .8byte {}", Bits);
    }
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
    if (GlobalVal->Func)
      noteDeferredUse(GlobalVal->Func);
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

  if (recordHasBitField(RD)) {
    std::vector<std::uint8_t> Buf(Ty->getSize(), 0);
    writeGlobalInitToBufFromFlat(Buf, 0, List, Ty, Idx);
    emitDataBuf(Buf);
    return;
  }

  if (RD->isUnion()) {
    if (Fields.empty() || Idx >= List->getNumInits()) {
      emitGlobalZeroInit(Ty, BaseOffset);
      return;
    }
    unsigned FI = List->getUnionFieldIndex();
    if (FI >= Fields.size())
      FI = 0;
    const auto *Field = Fields[FI];
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

  std::size_t ElemSize = CAT->getElementType()->getSize();
  const std::string &Str = SL->getString();
  assert(ElemSize != 0 && Str.size() % ElemSize == 0);
  std::size_t NumUnits = Str.size() / ElemSize;
  std::size_t Len = CAT->getLength();
  std::size_t NumInit = std::min(Len, NumUnits + 1);
  for (std::size_t I = 0; I < NumInit; ++I) {
    std::uint64_t Val = 0;
    if (I < NumUnits)
      std::memcpy(&Val, Str.data() + I * ElemSize, ElemSize);
    emitScalarData(BaseOffset + I * ElemSize, ElemSize,
                   static_cast<std::int64_t>(Val));
  }
  if (NumInit < Len)
    emit("  .zero {}", (Len - NumInit) * ElemSize);
}

void CodeGen::emitGlobalZeroInit(QualType Ty, std::size_t BaseOffset) {
  if (const auto *CAT = Ty->getAs<ConstantArrayType>()) {
    QualType ElemTy = CAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    for (std::size_t I = 0; I < CAT->getLength(); ++I)
      emitGlobalZeroInit(ElemTy, BaseOffset + I * ElemSize);
    return;
  }

  if (Ty->getAs<RecordType>()) {
    emit("  .zero {}", Ty->getSize());
    return;
  }

  emitScalarData(BaseOffset, Ty->getSize(), 0);
}

namespace {

std::uint64_t readInitBuf(const std::uint8_t *P, std::size_t Sz) {
  std::uint64_t Val = 0;
  std::memcpy(&Val, P, Sz);
  return Val;
}

void writeInitBuf(std::uint8_t *P, std::uint64_t Val, std::size_t Sz) {
  std::memcpy(P, &Val, Sz);
}

} // namespace

void CodeGen::emitDataBuf(const std::vector<std::uint8_t> &Buf) {
  std::size_t I = 0;
  while (I < Buf.size()) {
    if (Buf[I] == 0) {
      std::size_t J = I + 1;
      while (J < Buf.size() && Buf[J] == 0)
        ++J;
      emit("  .zero {}", J - I);
      I = J;
      continue;
    }
    emit("  .byte {}", Buf[I]);
    ++I;
  }
}

void CodeGen::writeGlobalInitToBuf(std::vector<std::uint8_t> &Buf,
                                   std::size_t Offset, const Expr *Init,
                                   QualType Ty) {
  if (Ty->getAs<ConstantArrayType>()) {
    if (const auto *SL = dynCast<StringLiteral>(Init)) {
      const auto *CAT = Ty->getAs<ConstantArrayType>();
      std::size_t ElemSize = CAT->getElementType()->getSize();
      const std::string &Str = SL->getString();
      assert(ElemSize != 0 && Str.size() % ElemSize == 0);
      std::size_t NumUnits = Str.size() / ElemSize;
      std::size_t Len = CAT->getLength();
      std::size_t NumInit = std::min(Len, NumUnits + 1);
      for (std::size_t I = 0; I < NumInit; ++I) {
        if (I < NumUnits)
          std::memcpy(Buf.data() + Offset + I * ElemSize,
                      Str.data() + I * ElemSize, ElemSize);
        else
          std::memset(Buf.data() + Offset + I * ElemSize, 0, ElemSize);
      }
      return;
    }

    const auto *ILE = dynCast<InitListExpr>(Init);
    if (!ILE)
      Diag.fatalAt(Init->getBeginLoc(), "array init requires init-list");
    std::size_t Index = 0;
    writeGlobalInitToBufFromFlat(Buf, Offset, ILE, Ty, Index);
    return;
  }

  if (const auto *RT = Ty->getAs<RecordType>()) {
    const auto *ILE = dynCast<InitListExpr>(Init);
    if (!ILE && !RT->getDecl()->isUnion())
      Diag.fatalAt(Init->getBeginLoc(), "expect nested initializer list");
    if (ILE) {
      std::size_t Index = 0;
      writeGlobalInitToBufFromFlat(Buf, Offset, ILE, Ty, Index);
    } else {
      // Union initialized from a scalar expression.
      writeGlobalInitToBuf(Buf, Offset, Init,
                           RT->getDecl()->fields()[0]->getType());
    }
    return;
  }

  if (Ty->isFloatingType()) {
    auto DblVal = Init->evaluateAsDouble();
    if (!DblVal)
      Diag.fatalAt(Init->getBeginLoc(),
                   "global variable initializer is not a constant expression");
    if (Ty->getSize() == 4) {
      float FVal = static_cast<float>(*DblVal);
      std::memcpy(Buf.data() + Offset, &FVal, 4);
    } else {
      double DVal = *DblVal;
      std::memcpy(Buf.data() + Offset, &DVal, 8);
    }
    return;
  }

  // Pointer initializers that refer to other globals cannot be packed into
  // a raw byte buffer; fall back is not needed for bit-field tests.
  auto Eval = Init->evaluateAsInt();
  if (!Eval) {
    auto GlobalVal = evalGlobalInitValue(Init);
    if (GlobalVal && GlobalVal->hasLabel())
      Diag.fatalAt(Init->getBeginLoc(),
                   "address constant in bit-field/struct buffer init "
                   "is not supported");
    Diag.fatalAt(Init->getBeginLoc(),
                 "global variable initializer is not a constant expression");
  }

  writeInitBuf(Buf.data() + Offset, static_cast<std::uint64_t>(*Eval),
               Ty->getSize());
}

void CodeGen::writeGlobalInitToBufFromFlat(std::vector<std::uint8_t> &Buf,
                                           std::size_t Offset,
                                           const InitListExpr *List,
                                           QualType Ty, std::size_t &Idx) {
  if (const auto *IAT = Ty->getAs<IncompleteArrayType>()) {
    QualType ElemTy = IAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    std::size_t I = 0;
    while (Idx < List->getNumInits()) {
      std::size_t ElemOff = Offset + I * ElemSize;
      const Expr *E = List->getInit(Idx);
      if (const auto *SubList = dynCast<InitListExpr>(E)) {
        ++Idx;
        writeGlobalInitToBuf(Buf, ElemOff, SubList, ElemTy);
      } else if (ElemTy->isArraryType() || ElemTy->isRecordType()) {
        writeGlobalInitToBufFromFlat(Buf, ElemOff, List, ElemTy, Idx);
      } else {
        ++Idx;
        writeGlobalInitToBuf(Buf, ElemOff, E, ElemTy);
      }
      ++I;
    }
    return;
  }

  if (const auto *CAT = Ty->getAs<ConstantArrayType>()) {
    QualType ElemTy = CAT->getElementType();
    std::size_t ElemSize = ElemTy->getSize();
    for (std::size_t I = 0; I < CAT->getLength(); ++I) {
      std::size_t ElemOff = Offset + I * ElemSize;
      if (Idx >= List->getNumInits())
        break;
      const Expr *E = List->getInit(Idx);
      if (const auto *SubList = dynCast<InitListExpr>(E)) {
        ++Idx;
        writeGlobalInitToBuf(Buf, ElemOff, SubList, ElemTy);
      } else if (ElemTy->isArraryType()) {
        if (const auto *SL = dynCast<StringLiteral>(E)) {
          ++Idx;
          writeGlobalInitToBuf(Buf, ElemOff, SL, ElemTy);
        } else {
          writeGlobalInitToBufFromFlat(Buf, ElemOff, List, ElemTy, Idx);
        }
      } else if (ElemTy->isRecordType()) {
        writeGlobalInitToBufFromFlat(Buf, ElemOff, List, ElemTy, Idx);
      } else {
        ++Idx;
        writeGlobalInitToBuf(Buf, ElemOff, E, ElemTy);
      }
    }
    return;
  }

  const auto *RT = Ty->getAs<RecordType>();
  if (!RT)
    Diag.fatalAt(List->getBeginLoc(), "expect aggregate type");
  const auto *RD = RT->getDecl();
  const auto &Fields = RD->fields();

  if (RD->isUnion()) {
    if (Fields.empty() || Idx >= List->getNumInits())
      return;
    unsigned FI = List->getUnionFieldIndex();
    if (FI >= Fields.size())
      FI = 0;
    const auto *Field = Fields[FI];
    QualType FieldTy = Field->getType();
    std::size_t FieldOff = Offset + Field->getOffset();
    const Expr *E = List->getInit(Idx);
    if (const auto *SubList = dynCast<InitListExpr>(E)) {
      ++Idx;
      writeGlobalInitToBuf(Buf, FieldOff, SubList, FieldTy);
    } else if (FieldTy->isArraryType() || FieldTy->isRecordType()) {
      if (FieldTy->isArraryType() && dynCast<StringLiteral>(E)) {
        ++Idx;
        writeGlobalInitToBuf(Buf, FieldOff, E, FieldTy);
      } else {
        writeGlobalInitToBufFromFlat(Buf, FieldOff, List, FieldTy, Idx);
      }
    } else {
      ++Idx;
      writeGlobalInitToBuf(Buf, FieldOff, E, FieldTy);
    }
    return;
  }

  for (const auto *Field : Fields) {
    std::size_t FieldOff = Offset + Field->getOffset();
    if (Idx >= List->getNumInits())
      break;

    const Expr *E = List->getInit(Idx);
    QualType FieldTy = Field->getType();

    if (Field->isBitField()) {
      auto Eval = E->evaluateAsInt();
      if (!Eval)
        Diag.fatalAt(E->getBeginLoc(),
                     "bit-field initializer is not a constant expression");
      ++Idx;
      std::uint8_t *Loc = Buf.data() + FieldOff;
      std::size_t Sz = FieldTy->getSize();
      std::uint64_t OldVal = readInitBuf(Loc, Sz);
      std::uint64_t Mask = (1ULL << Field->getBitWidth()) - 1;
      std::uint64_t Combined =
          OldVal |
          ((static_cast<std::uint64_t>(*Eval) & Mask) << Field->getBitOffset());
      writeInitBuf(Loc, Combined, Sz);
      continue;
    }

    if (const auto *SubList = dynCast<InitListExpr>(E)) {
      ++Idx;
      writeGlobalInitToBuf(Buf, FieldOff, SubList, FieldTy);
    } else if (FieldTy->isArraryType()) {
      if (const auto *SL = dynCast<StringLiteral>(E)) {
        ++Idx;
        writeGlobalInitToBuf(Buf, FieldOff, SL, FieldTy);
      } else {
        writeGlobalInitToBufFromFlat(Buf, FieldOff, List, FieldTy, Idx);
      }
    } else if (FieldTy->isRecordType()) {
      writeGlobalInitToBufFromFlat(Buf, FieldOff, List, FieldTy, Idx);
    } else {
      ++Idx;
      writeGlobalInitToBuf(Buf, FieldOff, E, FieldTy);
    }
  }
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

  for (const Decl *D : TU->decls()) {
    const auto *FD = dynCast<FunctionDecl>(D);
    if (!FD || !FD->getBody())
      continue;
    if (mustBeEmitted(FD))
      addDeferredDeclToEmit(FD);
    else
      DeferredDecls[FD->getName()] = FD;
  }
  emitDeferred();
}

bool CodeGen::mustBeEmitted(const FunctionDecl *FD) {
  return FD->getLinkage() != Linkage::InternalLinkage ||
         !FD->isInlineSpecified();
}

void CodeGen::addDeferredDeclToEmit(const FunctionDecl *FD) {
  DeferredDeclsToEmit.push_back(FD);
}

void CodeGen::noteDeferredUse(const FunctionDecl *FD) {
  if (!FD)
    return;
  auto It = DeferredDecls.find(FD->getName());
  if (It == DeferredDecls.end())
    return;
  addDeferredDeclToEmit(It->second);
  DeferredDecls.erase(It);
}

void CodeGen::emitDeferred() {
  for (std::size_t I = 0; I < DeferredDeclsToEmit.size(); ++I) {
    const FunctionDecl *FD = DeferredDeclsToEmit[I];
    if (!EmittedDecls.insert(FD).second)
      continue;
    genFunction(FD);
  }
  DeferredDeclsToEmit.clear();
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

  // stack frame (variadic):
  //-------------------------------// sp (on entry) = caller stack args
  //        VaArea (remaining GPs)
  //-------------------------------//
  //              ra
  //-------------------------------//
  //              fp
  //-------------------------------//      fp
  //                                       |
  //          local vars               StackSize
  //                                       |
  //-------------------------------//      sp
  //        eval-expression
  //-------------------------------//

  // Reserve space for unused GP argument registers so the save area sits
  // immediately below the caller's stack arguments (contiguous for va_arg).
  int VaSize = 0;
  if (findVaAreaVar(FD)) {
    int NamedGPs = countNamedParamGPs(FD);
    if (NamedGPs < GPMAX) {
      VaSize = (GPMAX - NamedGPs) * 8;
      emit("  # reserve {} bytes for variadic register save area", VaSize);
      emit("  addi sp, sp, -{}", VaSize);
    }
  }

  emit("  # create stack frame for ra, fp");
  emit("  addi sp, sp, -16");
  emit("  sd ra, 8(sp)"); // save ra
  emit("  sd fp, 0(sp)"); // save fp
  emit("  mv fp, sp");    // fp = sp
  // sp -= StackSize
  if (StackSize > 0) {
    emit("  # allocate {} bytes for local variables", StackSize);
    emit("  li t0, -{}", StackSize);
    emit("  add sp, sp, t0");
  }

  // Save register-passed arguments from a*/fa* into the stack frame.
  // Fully stack-passed params (Offset > 0) stay in the caller's argument area.
  // Half-by-stack structs are reconstructed into a local slot.
  unsigned NumParams = FD->getNumParams();
  int GP = 0, FP = 0;
  if (const VarDecl *Sret = findSretVar(FD)) {
    emit("  # save struct return buffer pointer from a0");
    storeGenReg(GP++, Sret->getOffset(), 8);
  }
  if (NumParams > 0) {
    emit("  # store register parameters to local frame");
    for (unsigned I = 0; I < NumParams; ++I) {
      const auto *Param = FD->getParam(I);
      int Offset = Param->getOffset();
      if (Offset > 0 && !Param->isHalfByStack())
        continue;

      const Type *Ty = Param->getType().getTypePtr();
      if (Ty->isRecordType()) {
        storeStructParam(Ty, Offset, GP, FP, Param->isHalfByStack());
        continue;
      }

      int Size = static_cast<int>(Ty->getSize());
      if (Ty->isFloatingType()) {
        if (FP < 8) {
          emit("  # store float param '{}' from fa{}", Param->getName(), FP);
          storeFloatReg(FP++, Offset, Size);
        } else {
          assert(GP < 8 && "too many floating-point parameters");
          emit("  # store float param '{}' from a{}", Param->getName(), GP);
          storeGenReg(GP++, Offset, Size);
        }
      } else {
        assert(GP < 8 && "too many integer parameters");
        emit("  # store int param '{}' from a{}", Param->getName(), GP);
        storeGenReg(GP++, Offset, Size);
      }
    }
  }

  if (const VarDecl *VaArea = findVaAreaVar(FD)) {
    int Offset = VaArea->getOffset();
    emit("  # store remaining GP args to {} (offset {})", VaArea->getName(),
         Offset);
    while (GP < GPMAX) {
      storeGenReg(GP++, Offset, 8);
      Offset += 8;
    }
  }

  if (const auto *CS = dynCast<CompoundStmt>(FD->getBody())) {
    for (const Stmt *S : CS->getBody()) {
      genStmt(S);
      assert(Depth == 0);
    }
  }

  // The C spec defines a special rule for the main function. Reaching the end
  // of main is equivalent to returning 0, even though the behavior is undefined
  // for other functions.
  if (FD->getName() == "main")
    emit("  li a0, 0");

  emit(".L.return.{}:", Name);
  emit("  # restore sp, fp and ra");
  emit("  mv sp, fp");    // restore sp, sp = fp
  emit("  ld fp, 0(sp)"); // pop fp
  emit("  ld ra, 8(sp)"); // pop ra
  emit("  addi sp, sp, 16");
  if (VaSize > 0) {
    emit("  # release variadic register save area");
    emit("  addi sp, sp, {}", VaSize);
  }
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
    if (const Expr *RetVal = cast<ReturnStmt>(S)->getRetValue()) {
      genExpr(RetVal);
      const Type *Ty = RetVal->getTypePtr();
      if (Ty->isRecordType()) {
        if (Ty->getSize() <= 16)
          copyStructReg();
        else
          copyStructMem();
      }
    }
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
  case Stmt::SK_AsmStmt:
    genAsmStmt(cast<AsmStmt>(S));
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
        if (E->getType()->isRecordType()) {
          ++Idx;
          genInitListElement(Var, E, ElemTy, Offset);
        } else {
          genInitListExprFromFlat(Var, List, ElemTy, Offset, Idx);
        }
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
        if (E->getType()->isRecordType()) {
          ++Idx;
          genInitListElement(Var, E, ElemTy, Offset);
        } else {
          genInitListExprFromFlat(Var, List, ElemTy, Offset, Idx);
        }
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

    unsigned FI = List->getUnionFieldIndex();
    if (FI >= Fields.size())
      FI = 0;
    const auto *Field = Fields[FI];
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
      if (E->getType()->isRecordType()) {
        ++Idx;
        genInitListElement(Var, E, FieldTy, Offset, Field);
      } else {
        genInitListExprFromFlat(Var, List, FieldTy, Offset, Idx);
      }
    } else {
      ++Idx;
      genInitListElement(Var, E, FieldTy, Offset, Field);
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
      if (E->getType()->isRecordType()) {
        ++Idx;
        genInitListElement(Var, E, FieldTy, Offset, Field);
      } else {
        genInitListExprFromFlat(Var, List, FieldTy, Offset, Idx);
      }
    } else {
      ++Idx;
      genInitListElement(Var, E, FieldTy, Offset, Field);
    }
  }
}

void CodeGen::genInitListElement(const VarDecl *Var, const Expr *ElemInit,
                                 QualType ElemTy, std::size_t Offset,
                                 const FieldDecl *Field) {
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

  if (ElemTy->isRecordType()) {
    genAddr(Var);
    emit("  addi a0, a0, {}", Offset);
    push();
    genExpr(ElemInit);
    store(ElemTy.getTypePtr());
    return;
  }

  if (Field && Field->isBitField()) {
    genAddr(Var);
    emit("  addi a0, a0, {}", Offset);
    push();
    genExpr(ElemInit);
    if (ElemInit->getType().getTypePtr() != ElemTy.getTypePtr())
      genScalarCast(ElemInit->getTypePtr(), ElemTy.getTypePtr());
    storeBitField(Field);
    store(ElemTy.getTypePtr());
    return;
  }

  genExpr(ElemInit);
  if (ElemTy->isFloatingType()) {
    if (!ElemInit->getType().isFloatingType())
      genScalarCast(ElemInit->getTypePtr(), ElemTy.getTypePtr());
    genAddr(Var);
    emit("  addi a1, a0, {}", Offset);
    if (ElemTy->getSize() == 4) {
      if (ElemInit->getType().isFloatingType() &&
          ElemInit->getTypePtr()->getSize() == 8)
        emit("  fcvt.s.d fa0, fa0");
      emit("  fsw fa0, 0(a1)");
    } else {
      emit("  fsd fa0, 0(a1)");
    }
    return;
  }
  if (ElemInit->getType().getTypePtr() != ElemTy.getTypePtr())
    genScalarCast(ElemInit->getTypePtr(), ElemTy.getTypePtr());
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

  QualType ElemTy = CAT->getElementType();
  std::size_t ElemSize = ElemTy->getSize();
  const std::string &Str = SL->getString();
  assert(ElemSize != 0 && Str.size() % ElemSize == 0);
  std::size_t NumUnits = Str.size() / ElemSize;
  const std::size_t Len = CAT->getLength();
  std::size_t NumInit = std::min(Len, NumUnits + 1);
  for (std::size_t I = 0; I < NumInit; ++I) {
    std::uint64_t Val = 0;
    if (I < NumUnits)
      std::memcpy(&Val, Str.data() + I * ElemSize, ElemSize);
    emit("  li a0, {}", static_cast<std::int64_t>(Val));
    push();
    genAddr(Var);
    emit("  addi a1, a0, {}", BaseOffset + I * ElemSize);
    pop("a0");
    emit("  s{} a0, 0(a1)", getWidthSuffix(ElemSize));
  }
  for (std::size_t I = NumInit; I < Len; ++I)
    genZeroInit(Var, ElemTy, BaseOffset + I * ElemSize);
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
  emitIsNotZero(If->getCond()->getTypePtr());
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
    emitIsNotZero(Cond->getTypePtr());
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
  emitIsNotZero(While->getCond()->getTypePtr());
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
  emitIsNotZero(DoWhile->getCond()->getTypePtr());
  emit("  bnez a0, .L.begin.{}", Count);
  emit(".L.end.{}:", Count);
  ContinueCounts.pop_back();
  BreakCounts.pop_back();
}

void CodeGen::genSwitchStmt(const SwitchStmt *Switch) {
  int Count = getCount();
  genExpr(Switch->getCond());

  // Compare at the width of the controlling expression: convert case
  // constants as if cast to that type (C99 6.8.4.2).
  QualType CondTy = Switch->getCond()->getType();
  bool Is32 = CondTy->getSize() == 4;
  bool IsUnsigned = CondTy->isUnsignedIntegerType();
  if (Is32) {
    if (IsUnsigned) {
      emit("  slli a0, a0, 32");
      emit("  srli a0, a0, 32");
    } else {
      emit("  sext.w a0, a0");
    }
  }

  const DefaultStmt *Default = nullptr;
  for (const auto *SC = Switch->getSwitchCaseList(); SC;
       SC = SC->getNextSwitchCase()) {
    if (const auto *CS = dynCast<CaseStmt>(SC)) {
      std::int64_t CaseVal = CS->getCaseValue();
      if (Is32) {
        auto U32 = static_cast<std::uint32_t>(CaseVal);
        CaseVal =
            IsUnsigned
                ? static_cast<std::int64_t>(U32)
                : static_cast<std::int64_t>(static_cast<std::int32_t>(U32));
      }
      emit("  li t0, {}", CaseVal);
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

void CodeGen::genAsmStmt(const AsmStmt *AS) {
  emit("  # asm");
  emit("  {}", AS->getAsmString());
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
  case Stmt::SK_BinaryConditionalOperator:
    genBinaryConditionalOperator(cast<BinaryConditionalOperator>(E));
    break;
  case Stmt::SK_IntegerLiteral: {
    // li a0, imm
    auto Val = cast<IntegerLiteral>(E)->getVal();
    emit("  # a0 = {}", Val);
    emit("  li a0, {}", Val);
    break;
  }
  case Stmt::SK_FloatingLiteral: {
    const auto *FL = cast<FloatingLiteral>(E);
    double FVal = FL->getVal();
    union {
      float F32;
      double F64;
      std::uint32_t U32;
      std::uint64_t U64;
    } U;
    if (FL->getType().isFloatingType() && FL->getTypePtr()->getSize() == 4) {
      U.F32 = static_cast<float>(FVal);
      emit("  # fa0 = {}f", FVal);
      emit("  li a0, {}", U.U32);
      emit("  fmv.w.x fa0, a0");
    } else {
      U.F64 = FVal;
      emit("  # fa0 = {}", FVal);
      emit("  li a0, {}", U.U64);
      emit("  fmv.d.x fa0, a0");
    }
    break;
  }
  case Stmt::SK_CharacterLiteral: {
    auto Val = cast<CharacterLiteral>(E)->getValue();
    // Emit as signed 32-bit so values like L'\xffffffff' become -1.
    emit("  # a0 = '{}'", static_cast<unsigned char>(Val));
    emit("  li a0, {}", static_cast<std::int32_t>(Val));
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
    if (Member->getMemberDecl()->isBitField())
      loadBitField(Member->getMemberDecl());
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
  genScalarCast(From, To);
}

void CodeGen::genScalarCast(const Type *From, const Type *To) {
  if (const auto *BT = dynCast<BuiltinType>(To)) {
    if (BT->isVoidType())
      return;
  }

  if (To->isBooleanType()) {
    emitIsNotZero(From);
    emit("  snez a0, a0");
    return;
  }

  enum CastTypeID : unsigned {
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
  };

  auto GetTypeID = [](const Type *T) -> unsigned {
    if (const auto *BT = dynCast<BuiltinType>(T)) {
      switch (BT->getKind()) {
      case BuiltinType::BK_SignedChar:
        return I8;
      case BuiltinType::BK_Char:
      case BuiltinType::BK_UnsignedChar:
      case BuiltinType::BK_Bool:
        return U8;
      case BuiltinType::BK_Short:
        return I16;
      case BuiltinType::BK_UnsignedShort:
        return U16;
      case BuiltinType::BK_Int:
        return I32;
      case BuiltinType::BK_UnsignedInt:
        return U32;
      case BuiltinType::BK_Long:
      case BuiltinType::BK_LongLong:
        return I64;
      case BuiltinType::BK_UnsignedLong:
      case BuiltinType::BK_UnsignedLongLong:
        return U64;
      case BuiltinType::BK_Float:
        return F32;
      case BuiltinType::BK_Double:
        return F64;
      default:
        return I64;
      }
    }
    return I64;
  };

  static constexpr std::string_view I64ToI8 =
      "  slli a0, a0, 56\n  srai a0, a0, 56";
  static constexpr std::string_view I64ToI16 =
      "  slli a0, a0, 48\n  srai a0, a0, 48";
  static constexpr std::string_view I64ToI32 =
      "  slli a0, a0, 32\n  srai a0, a0, 32";
  static constexpr std::string_view I64ToU8 =
      "  slli a0, a0, 56\n  srli a0, a0, 56";
  static constexpr std::string_view I64ToU16 =
      "  slli a0, a0, 48\n  srli a0, a0, 48";
  static constexpr std::string_view I64ToU32 =
      "  slli a0, a0, 32\n  srli a0, a0, 32";
  static constexpr std::string_view U32ToI64 =
      "  slli a0, a0, 32\n  srli a0, a0, 32";

  static constexpr std::string_view I32ToF32 = "  fcvt.s.w fa0, a0";
  static constexpr std::string_view I32ToF64 = "  fcvt.d.w fa0, a0";
  static constexpr std::string_view I64ToF32 = "  fcvt.s.l fa0, a0";
  static constexpr std::string_view I64ToF64 = "  fcvt.d.l fa0, a0";
  static constexpr std::string_view U32ToF32 = "  fcvt.s.wu fa0, a0";
  static constexpr std::string_view U32ToF64 = "  fcvt.d.wu fa0, a0";
  static constexpr std::string_view U64ToF32 = "  fcvt.s.lu fa0, a0";
  static constexpr std::string_view U64ToF64 = "  fcvt.d.lu fa0, a0";

  static constexpr std::string_view F32ToI8 =
      "  fcvt.w.s a0, fa0, rtz\n  slli a0, a0, 56\n  srai a0, a0, 56";
  static constexpr std::string_view F32ToI16 =
      "  fcvt.w.s a0, fa0, rtz\n  slli a0, a0, 48\n  srai a0, a0, 48";
  static constexpr std::string_view F32ToI32 =
      "  fcvt.w.s a0, fa0, rtz\n  slli a0, a0, 32\n  srai a0, a0, 32";
  static constexpr std::string_view F32ToI64 = "  fcvt.l.s a0, fa0, rtz";
  static constexpr std::string_view F32ToU8 =
      "  fcvt.wu.s a0, fa0, rtz\n  slli a0, a0, 56\n  srli a0, a0, 56";
  static constexpr std::string_view F32ToU16 =
      "  fcvt.wu.s a0, fa0, rtz\n  slli a0, a0, 48\n  srli a0, a0, 48";
  static constexpr std::string_view F32ToU32 =
      "  fcvt.wu.s a0, fa0, rtz\n  slli a0, a0, 32\n  srli a0, a0, 32";
  static constexpr std::string_view F32ToU64 = "  fcvt.lu.s a0, fa0, rtz";
  static constexpr std::string_view F32ToF64 = "  fcvt.d.s fa0, fa0";

  static constexpr std::string_view F64ToI8 =
      "  fcvt.w.d a0, fa0, rtz\n  slli a0, a0, 56\n  srai a0, a0, 56";
  static constexpr std::string_view F64ToI16 =
      "  fcvt.w.d a0, fa0, rtz\n  slli a0, a0, 48\n  srai a0, a0, 48";
  static constexpr std::string_view F64ToI32 =
      "  fcvt.w.d a0, fa0, rtz\n  slli a0, a0, 32\n  srai a0, a0, 32";
  static constexpr std::string_view F64ToI64 = "  fcvt.l.d a0, fa0, rtz";
  static constexpr std::string_view F64ToU8 =
      "  fcvt.wu.d a0, fa0, rtz\n  slli a0, a0, 56\n  srli a0, a0, 56";
  static constexpr std::string_view F64ToU16 =
      "  fcvt.wu.d a0, fa0, rtz\n  slli a0, a0, 48\n  srli a0, a0, 48";
  static constexpr std::string_view F64ToU32 =
      "  fcvt.wu.d a0, fa0, rtz\n  slli a0, a0, 32\n  srli a0, a0, 32";
  static constexpr std::string_view F64ToU64 = "  fcvt.lu.d a0, fa0, rtz";
  static constexpr std::string_view F64ToF32 = "  fcvt.s.d fa0, fa0";

  // clang-format off
  // To:   i8       i16      i32      i64      u8       u16      u32      u64      f32      f64
  constexpr std::array<std::array<std::string_view, 10>, 10> CastTable = {{
      {{ {},       {},       {},       {},       I64ToU8,  I64ToU16, I64ToU32, {},       I32ToF32, I32ToF64 }}, // i8
      {{ I64ToI8,  {},       {},       {},       I64ToU8,  I64ToU16, I64ToU32, {},       I32ToF32, I32ToF64 }}, // i16
      {{ I64ToI8,  I64ToI16, {},       {},       I64ToU8,  I64ToU16, I64ToU32, {},       I32ToF32, I32ToF64 }}, // i32
      {{ I64ToI8,  I64ToI16, I64ToI32, {},       I64ToU8,  I64ToU16, I64ToU32, {},       I64ToF32, I64ToF64 }}, // i64
      {{ I64ToI8,  {},       {},       {},       {},       {},       {},       {},       U32ToF32, U32ToF64 }}, // u8
      {{ I64ToI8,  I64ToI16, {},       {},       I64ToU8,  {},       {},       {},       U32ToF32, U32ToF64 }}, // u16
      {{ I64ToI8,  I64ToI16, I64ToI32, U32ToI64, I64ToU8,  I64ToU16, {},       U32ToI64, U32ToF32, U32ToF64 }}, // u32
      {{ I64ToI8,  I64ToI16, I64ToI32, {},       I64ToU8,  I64ToU16, I64ToU32, {},       U64ToF32, U64ToF64 }}, // u64
      {{ F32ToI8,  F32ToI16, F32ToI32, F32ToI64, F32ToU8,  F32ToU16, F32ToU32, F32ToU64, {},       F32ToF64 }}, // f32
      {{ F64ToI8,  F64ToI16, F64ToI32, F64ToI64, F64ToU8,  F64ToU16, F64ToU32, F64ToU64, F64ToF32, {}       }}, // f64
  }};
  // clang-format on

  unsigned ID1 = GetTypeID(From);
  unsigned ID2 = GetTypeID(To);
  std::string_view CastInsts = CastTable[ID1][ID2];
  if (!CastInsts.empty()) {
    emit("  # cast");
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
  bool IsUnsigned = LType->isUnsignedIntegerType();
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
    emit("  div{}{} a0, a0, a1", IsUnsigned ? "u" : "", Suffix);
    return;
  case BinaryOperator::BO_Rem:
    emit("  rem{}{} a0, a0, a1", IsUnsigned ? "u" : "", Suffix);
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
    emit("  sll{} a0, a0, a1", Suffix);
    return;
  case BinaryOperator::BO_Shr:
    emit("  sr{}{} a0, a0, a1", IsUnsigned ? "l" : "a", Suffix);
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
    push();                // a1 = addrof(lhs)
    genExpr(BO->getRHS()); // a0 = rhs
    if (const auto *Member = dynCast<MemberExpr>(LHS);
        Member && Member->getMemberDecl()->isBitField())
      storeBitField(Member->getMemberDecl());
    store(LHS->getTypePtr()); // *(a1) = a0
    return;
  case BinaryOperator::BO_Comma:
    genExpr(LHS);
    genExpr(RHS);
    return;
  case BinaryOperator::BO_LAnd: {
    int Count = getCount();
    genExpr(LHS);
    emitIsNotZero(LHS->getTypePtr());
    emit("  # logical-and test left");
    emit("  beqz a0, .L.false.{}", Count);
    genExpr(RHS);
    emitIsNotZero(RHS->getTypePtr());
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
    emitIsNotZero(LHS->getTypePtr());
    emit("  # logical-or test left");
    emit("  bnez a0, .L.true.{}", Count);
    genExpr(RHS);
    emitIsNotZero(RHS->getTypePtr());
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
    // A op= B  =>  A = (typeof A)((common)A op (common)B)
    genAddr(LHS);
    push();

    auto BaseOp = BO->getOpForCompoundAssign();

    if (LType.isFloatingType() || RType.isFloatingType()) {
      QualType CompTy = LType;
      if (RType.isFloatingType() &&
          (!LType.isFloatingType() || RType->getSize() > LType->getSize()))
        CompTy = RType;

      genExpr(RHS);
      pushF();
      genExpr(LHS);
      if (!LType.isFloatingType() || LType->getSize() != CompTy->getSize())
        genScalarCast(LType.getTypePtr(), CompTy.getTypePtr());
      popF("fa1");

      const char *FSuffix = CompTy->getSize() == 4 ? "s" : "d";
      switch (BaseOp) {
      case BinaryOperator::BO_Add:
        emit("  # fa0 + fa1");
        emit("  fadd.{} fa0, fa0, fa1", FSuffix);
        break;
      case BinaryOperator::BO_Sub:
        emit("  # fa0 - fa1");
        emit("  fsub.{} fa0, fa0, fa1", FSuffix);
        break;
      case BinaryOperator::BO_Mul:
        emit("  # fa0 * fa1");
        emit("  fmul.{} fa0, fa0, fa1", FSuffix);
        break;
      case BinaryOperator::BO_Div:
        emit("  # fa0 / fa1");
        emit("  fdiv.{} fa0, fa0, fa1", FSuffix);
        break;
      default:
        Diag.fatalAt(BO->getOpLocation(),
                     "invalid floating compound assignment: {}",
                     BO->getOpcodeStr());
      }

      if (!LType.isFloatingType() || LType->getSize() != CompTy->getSize())
        genScalarCast(CompTy.getTypePtr(), LType.getTypePtr());
      if (const auto *Member = dynCast<MemberExpr>(LHS);
          Member && Member->getMemberDecl()->isBitField()) {
        emit("  mv t2, a0");
        storeBitField(Member->getMemberDecl());
        store(LType.getTypePtr());
        emit("  mv a0, t2");
      } else {
        store(LType.getTypePtr());
      }
      return;
    }

    // a0 = LHS, a1 = RHS
    genExpr(RHS);
    push();
    genExpr(LHS);
    pop("a1");

    // a0 = LHS op RHS
    emitBinaryArithmeticResult(BaseOp, LType, RType, Suffix);
    // *&A = a0
    if (const auto *Member = dynCast<MemberExpr>(LHS);
        Member && Member->getMemberDecl()->isBitField()) {
      emit("  mv t2, a0");
      storeBitField(Member->getMemberDecl());
      store(LType.getTypePtr());
      emit("  mv a0, t2");
    } else {
      store(LType.getTypePtr());
    }
    return;
  }

  // a0 op a1 (or fa0 op fa1 for floating types)
  if (LType.isFloatingType()) {
    genExpr(RHS);
    pushF();
    genExpr(LHS);
    popF("fa1");

    const char *FSuffix = LType->getSize() == 4 ? "s" : "d";
    switch (Op) {
    case BinaryOperator::BO_Add:
      emit("  # fa0 + fa1");
      emit("  fadd.{} fa0, fa0, fa1", FSuffix);
      return;
    case BinaryOperator::BO_Sub:
      emit("  # fa0 - fa1");
      emit("  fsub.{} fa0, fa0, fa1", FSuffix);
      return;
    case BinaryOperator::BO_Mul:
      emit("  # fa0 * fa1");
      emit("  fmul.{} fa0, fa0, fa1", FSuffix);
      return;
    case BinaryOperator::BO_Div:
      emit("  # fa0 / fa1");
      emit("  fdiv.{} fa0, fa0, fa1", FSuffix);
      return;
    case BinaryOperator::BO_EQ:
      emit("  # fa0 == fa1");
      emit("  feq.{} a0, fa0, fa1", FSuffix);
      return;
    case BinaryOperator::BO_NE:
      emit("  # fa0 != fa1");
      emit("  feq.{} a0, fa0, fa1", FSuffix);
      emit("  seqz a0, a0");
      return;
    case BinaryOperator::BO_LT:
      emit("  # fa0 < fa1");
      emit("  flt.{} a0, fa0, fa1", FSuffix);
      return;
    case BinaryOperator::BO_LE:
      emit("  # fa0 <= fa1");
      emit("  fle.{} a0, fa0, fa1", FSuffix);
      return;
    case BinaryOperator::BO_GT:
      // a0 > a1  <=>  a1 < a0
      emit("  # fa0 > fa1");
      emit("  flt.{} a0, fa1, fa0", FSuffix);
      return;
    case BinaryOperator::BO_GE:
      // a0 >= a1  <=>  a1 <= a0
      emit("  # fa0 >= fa1");
      emit("  fle.{} a0, fa1, fa0", FSuffix);
      return;
    default:
      Diag.fatalAt(BO->getOpLocation(), "invalid floating binary opcode: {}",
                   BO->getOpcodeStr());
    }
  }

  genExpr(RHS);
  push();
  genExpr(LHS);
  pop("a1");

  bool IsLHSUnsignedOrPointer =
      LType->isUnsignedIntegerType() || LType->isPointerType();

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
  case BinaryOperator::BO_NE: {
    // Clear high bits before comparing U32.
    auto TruncU32 = [this](const char *Reg, QualType Ty) {
      if (Ty->isUnsignedIntegerType() && Ty->getSize() == 4) {
        emit("  slli {}, {}, 32", Reg, Reg);
        emit("  srli {}, {}, 32", Reg, Reg);
      }
    };
    TruncU32("a0", LType);
    TruncU32("a1", RType);
    // a0 = a0 ^ a1
    // a0 = (a0 == 0) ? 1 : 0
    emit("  xor a0, a0, a1");
    emit("  {} a0, a0", Op == BinaryOperator::BO_EQ ? "seqz" : "snez");
    break;
  }
  case BinaryOperator::BO_LT:
    // a0 = a0 < a1
    emit("  slt{} a0, a0, a1", IsLHSUnsignedOrPointer ? "u" : "");
    break;
  case BinaryOperator::BO_LE:
    // a0 <= a1  <=>  !(a1 < a0)
    emit("  slt{} a0, a1, a0", IsLHSUnsignedOrPointer ? "u" : "");
    emit("  xori a0, a0, 1");
    break;
  case BinaryOperator::BO_GT:
    // a0 > a1  <=>  a1 < a0
    emit("  slt{} a0, a1, a0", IsLHSUnsignedOrPointer ? "u" : "");
    break;
  case BinaryOperator::BO_GE:
    // a0 >= a1  <=>  !(a0 < a1)
    emit("  slt{} a0, a0, a1", IsLHSUnsignedOrPointer ? "u" : "");
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
  emitIsNotZero(CO->getCond()->getTypePtr());
  emit("  beqz a0, .L.else.{}", Count);
  genExpr(CO->getTrueExpr());
  emit("  j .L.end.{}", Count);
  emit(".L.else.{}:", Count);
  genExpr(CO->getFalseExpr());
  emit(".L.end.{}:", Count);
}

void CodeGen::genBinaryConditionalOperator(
    const BinaryConditionalOperator *BCO) {
  int Count = getCount();
  const Expr *Common = BCO->getCommon();
  QualType CommonTy = Common->getType();
  genExpr(Common);
  // Common's value stays in a0/fa0; emitIsNotZero only writes the boolean to
  // a0.
  emitIsNotZero(CommonTy.getTypePtr());
  emit("  beqz a0, .L.else.{}", Count);
  if (CommonTy != BCO->getType())
    genScalarCast(CommonTy.getTypePtr(), BCO->getTypePtr());
  emit("  j .L.end.{}", Count);
  emit(".L.else.{}:", Count);
  genExpr(BCO->getFalseExpr());
  emit(".L.end.{}:", Count);
}

void CodeGen::genUnaryOperator(const UnaryOperator *UO) {
  switch (UO->getOpcode()) {
  case UnaryOperator::UO_Plus:
    emit("  # unary plus");
    genExpr(UO->getSubExpr());
    break;
  case UnaryOperator::UO_Minus: {
    genExpr(UO->getSubExpr());
    QualType Ty = UO->getType();
    if (Ty.isFloatingType())
      emit("  fneg.{} fa0, fa0", Ty->getSize() == 4 ? "s" : "d");
    else
      emit("  neg{} a0, a0", Ty->getSize() <= 4 ? "w" : "");
    break;
  }
  case UnaryOperator::UO_LNot:
    genExpr(UO->getSubExpr());
    emitIsNotZero(UO->getSubExpr()->getTypePtr());
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
    const FieldDecl *BitField = nullptr;
    if (const auto *Member = dynCast<MemberExpr>(SubExpr);
        Member && Member->getMemberDecl()->isBitField())
      BitField = Member->getMemberDecl();

    emit("  # pre {} operator", UO->getOpcodeStr());
    genAddr(SubExpr);
    push();
    load(SubType.getTypePtr());
    if (BitField)
      loadBitField(BitField);
    if (SubType.isFloatingType()) {
      const char *FSuffix = SubType->getSize() == 4 ? "s" : "d";
      // fa1 = 1.0
      if (SubType->getSize() == 4) {
        emit("  li t0, {}", 0x3f800000u);
        emit("  fmv.w.x fa1, t0");
      } else {
        emit("  li t0, {}", 0x3ff0000000000000ull);
        emit("  fmv.d.x fa1, t0");
      }
      emit("  f{}.{} fa0, fa0, fa1", UO->isIncrement() ? "add" : "sub",
           FSuffix);
    } else {
      std::size_t Step = 1;
      if (const auto *PointeeTy = SubType->getPointeeOrArrayElementTypePtr())
        Step = PointeeTy->getSize();
      emit("  li t0, {}", Step);
      emit("  {} a0, a0, t0", UO->isIncrement() ? "add" : "sub");
    }
    if (BitField) {
      emit("  mv t2, a0");
      storeBitField(BitField);
      store(SubType.getTypePtr());
      emit("  mv a0, t2");
    } else {
      store(SubType.getTypePtr());
    }
    break;
  }
  case UnaryOperator::UO_PostInc:
  case UnaryOperator::UO_PostDec: {
    const auto *SubExpr = UO->getSubExpr();
    QualType SubType = SubExpr->getType();
    const FieldDecl *BitField = nullptr;
    if (const auto *Member = dynCast<MemberExpr>(SubExpr);
        Member && Member->getMemberDecl()->isBitField())
      BitField = Member->getMemberDecl();

    emit("  # post {} operator", UO->getOpcodeStr());
    genAddr(SubExpr);
    push();
    load(SubType.getTypePtr());
    if (BitField)
      loadBitField(BitField);
    if (SubType.isFloatingType()) {
      const char *FSuffix = SubType->getSize() == 4 ? "s" : "d";
      emit("  fmv.{} ft0, fa0", FSuffix);
      if (SubType->getSize() == 4) {
        emit("  li t0, {}", 0x3f800000u);
        emit("  fmv.w.x fa1, t0");
      } else {
        emit("  li t0, {}", 0x3ff0000000000000ull);
        emit("  fmv.d.x fa1, t0");
      }
      emit("  f{}.{} fa0, fa0, fa1", UO->isIncrement() ? "add" : "sub",
           FSuffix);
      if (BitField)
        storeBitField(BitField);
      store(SubType.getTypePtr());
      emit("  fmv.{} fa0, ft0", FSuffix);
    } else {
      std::size_t Step = 1;
      if (const auto *PointeeTy = SubType->getPointeeOrArrayElementTypePtr())
        Step = PointeeTy->getSize();
      emit("  mv t2, a0");
      emit("  li t0, {}", Step);
      emit("  {} a0, a0, t0", UO->isIncrement() ? "add" : "sub");
      if (BitField)
        storeBitField(BitField);
      store(SubType.getTypePtr());
      emit("  mv a0, t2");
    }
    break;
  }
  default:
    Diag.fatalAt(UO->getBeginLoc(), "invalid unary opcode: {}",
                 UO->getOpcodeStr());
  }
}

void CodeGen::genCallExpr(const CallExpr *CE) {
  const auto *FT = CE->getCalleeFunctionType();
  assert(FT);
  const auto *Func = CE->getCalleeDecl();
  const unsigned NumParams = Func ? Func->getNumParams() : FT->getNumParams();
  const bool IsVariadic = FT->isVariadic();
  const VarDecl *RetBuf = CE->getRetBuffer();
  const bool LargeRet = RetBuf && CE->getTypePtr()->getSize() > 16;

  int NumArgs = static_cast<int>(CE->getNumArgs());
  enum class ArgKind { Scalar, Struct };
  std::vector<ArgKind> ArgKinds(NumArgs, ArgKind::Scalar);
  std::vector<std::pair<bool, const char *>> ArgDest(NumArgs);
  std::vector<bool> PassByStack(NumArgs, false);
  int GP = 0, FP = 0, Stack = 0;

  // Hidden first argument: pointer to the caller-allocated return buffer.
  if (LargeRet)
    ++GP;

  for (int I = 0; I < NumArgs; ++I) {
    const Type *Ty = CE->getArg(I)->getType().getTypePtr();
    const bool VariadicTail =
        IsVariadic && static_cast<unsigned>(I) >= NumParams;
    if (VariadicTail) {
      if (GP < GPMAX)
        ArgDest[I] = {false, ArgReg[GP++]};
      else {
        PassByStack[I] = true;
        ++Stack;
      }
      continue;
    }

    if (Ty->isRecordType()) {
      ArgKinds[I] = ArgKind::Struct;
      bool OnStack = false;
      int GPBefore = GP;
      int FPBefore = FP;
      countStructArgRegs(Ty, GP, FP, OnStack);
      PassByStack[I] = OnStack;
      if (OnStack) {
        int Used = (GP - GPBefore) + (FP - FPBefore);
        Stack += countStructArgStackSlots(Ty) - Used;
      }
      continue;
    }

    if (Ty->isFloatingType()) {
      if (FP < FPMAX)
        ArgDest[I] = {true, FaArgReg[FP++]};
      else if (GP < GPMAX)
        ArgDest[I] = {false, ArgReg[GP++]};
      else {
        PassByStack[I] = true;
        ++Stack;
      }
    } else if (GP < GPMAX) {
      ArgDest[I] = {false, ArgReg[GP++]};
    } else {
      PassByStack[I] = true;
      ++Stack;
    }
  }

  createBigStructCallSpace(CE);
  int BSStack = BigStructDepth;

  if ((Depth + Stack) % 2 == 1) {
    emit("  # align stack args to 16-byte boundary");
    emit("  addi sp, sp, -8");
    ++Depth;
    ++Stack;
  }

  auto PushArg = [this, &PassByStack](const Expr *Arg, int I) {
    genExpr(Arg);
    const Type *Ty = Arg->getType().getTypePtr();
    if (Ty->isRecordType())
      pushStructArg(Ty, PassByStack[I]);
    else if (Arg->getType().isFloatingType())
      pushF();
    else
      push();
  };

  if (NumArgs != 0)
    emit("  # set call args");

  for (int I = NumArgs - 1; I >= 0; --I) {
    if (PassByStack[I])
      PushArg(CE->getArg(I), I);
  }
  for (int I = NumArgs - 1; I >= 0; --I) {
    if (!PassByStack[I])
      PushArg(CE->getArg(I), I);
  }

  if (LargeRet) {
    emit("  # push pointer to struct return buffer");
    emit("  li t0, {}", RetBuf->getOffset());
    emit("  add a0, fp, t0");
    push();
  }

  if (!Func) {
    genExpr(CE->getCallee());
    emit("  mv t5, a0");
  }

  GP = 0;
  FP = 0;
  if (LargeRet) {
    emit("  # large struct return: a0 points to return buffer");
    pop(ArgReg[GP++]);
  }

  for (int I = 0; I < NumArgs; ++I) {
    if (ArgKinds[I] == ArgKind::Struct) {
      popStructArgToRegs(CE->getArg(I)->getType().getTypePtr(), GP, FP,
                         PassByStack[I]);
      continue;
    }
    if (PassByStack[I])
      continue;
    if (ArgDest[I].first) {
      popF(ArgDest[I].second);
      ++FP;
    } else {
      pop(ArgDest[I].second);
      ++GP;
    }
  }

  if (Func) {
    noteDeferredUse(Func);
    const std::string &Name = Func->getName();
    emit("  # call {}", Name);
    emit("  call {}", Name);
  } else {
    emit("  # indirect call");
    emit("  jalr t5");
  }

  if (Stack > 0 || BSStack > 0) {
    emit("  # reclaim {} stack argument slots", Stack + BSStack);
    emit("  addi sp, sp, {}", (Stack + BSStack) * 8);
    Depth -= Stack + BSStack;
  }
  BigStructDepth = 0;

  const Type *RetTy = CE->getTypePtr();
  if (const auto *BT = dynCast<BuiltinType>(RetTy)) {
    switch (BT->getKind()) {
    case BuiltinType::BK_Bool:
      emit("  # clear high bits for bool return");
      emit("  slli a0, a0, 63");
      emit("  srli a0, a0, 63");
      break;
    case BuiltinType::BK_SignedChar:
      emit("  # sign-extend signed char return");
      emit("  slli a0, a0, 56");
      emit("  srai a0, a0, 56");
      break;
    case BuiltinType::BK_Char:
    case BuiltinType::BK_UnsignedChar:
      emit("  # zero-extend unsigned char return");
      emit("  slli a0, a0, 56");
      emit("  srli a0, a0, 56");
      break;
    case BuiltinType::BK_Short:
      emit("  # sign-extend short return");
      emit("  slli a0, a0, 48");
      emit("  srai a0, a0, 48");
      break;
    case BuiltinType::BK_UnsignedShort:
      emit("  # zero-extend unsigned short return");
      emit("  slli a0, a0, 48");
      emit("  srli a0, a0, 48");
      break;
    default:
      break;
    }
  }

  // Small struct/union returns arrive in registers; copy into the buffer and
  // leave a0 pointing at it (large returns already wrote through a0/sret).
  if (RetBuf && RetTy->getSize() <= 16) {
    copyRetBuffer(RetBuf);
    emit("  li t0, {}", RetBuf->getOffset());
    emit("  add a0, fp, t0");
  }
}

void CodeGen::genArraySubscriptExpr(const ArraySubscriptExpr *ASE) {
  // base[idx] <=> *(base + idx)
  // addr = base + idx
  genAddr(ASE);
  // a0 = *(addr)
  load(ASE->getTypePtr());
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
  case CastExpr::CK_FloatingCast:
  case CastExpr::CK_FloatingToIntegral:
  case CastExpr::CK_IntegralToFloating:
  case CastExpr::CK_FloatingToBoolean:
    genScalarCast(SubExpr->getTypePtr(), Cast->getTypePtr());
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
  case Stmt::SK_CallExpr: {
    const auto *CE = cast<CallExpr>(E);
    if (CE->getRetBuffer()) {
      genExpr(CE);
      return;
    }
    break;
  }
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
  if (const auto *FD = dynCast<FunctionDecl>(D)) {
    noteDeferredUse(FD);
    emit("  # genAddr func {}", FD->getName());
    emit("  la a0, {}", FD->getName());
    return;
  }

  const auto *Var = dynCast<VarDecl>(D);
  if (!Var)
    Diag.fatalAt(D->getBeginLoc(), "expect a variable");

  if (Var->hasGlobalStorage()) {
    emit("  # genAddr gvar {}", getVarSymbol(Var));
    emit("  la a0, {}", getVarSymbol(Var));
  } else {
    emit("  # genAddr lvar {}, offset={}", Var->getName(), Var->getOffset());
    emit("  li t0, {}", Var->getOffset());
    emit("  add a0, fp, t0");
  }
}

void CodeGen::genAddr(const StringLiteral *SL) {
  emit("  # get address of string literal");
  emit("  la a0, {}", getStringLabel(SL));
}

void CodeGen::genAddr(const MemberExpr *ME) {
  emit("  # get address of member expr");
  const auto *Base = ME->getBase();
  // Base is a pointer rvalue: address = base + offset.
  if (ME->isArrow())
    genExpr(Base); // a0 = base
  else
    genAddr(Base); // a0 = addrof base

  emit("  li t0, {}", ME->getMemberDecl()->getOffset()); // t0 = offset
  emit("  add a0, a0, t0");                              // a0 = a0 + offset
}

void CodeGen::pushStructArg(const Type *Ty, bool OnStack) {
  std::size_t Sz = Ty->getSize();
  if (isLargeStructByPointer(Ty)) {
    int Aligned = static_cast<int>(alignTo(Sz, 8));
    BigStructDepth -= Aligned / 8;
    int BSOffset = BigStructDepth * 8;
    emit("  # copy large struct argument to {}(t6)", BSOffset);
    for (int I = 0; I < Aligned; ++I) {
      emit("  lb t0, {}(a0)", I);
      emit("  sb t0, {}(t6)", BSOffset + I);
    }
    emit("  addi a0, t6, {}", BSOffset);
    push();
    return;
  }

  FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, 0, 0);
  if (!OnStack && useFloatStructStackPass(PassInfo)) {
    emit("  # push float struct argument");
    emit("  addi sp, sp, -16");
    Depth += 2;
    emit("  ld t0, 0(a0)");
    emit("  sd t0, 0(sp)");
    int Off = static_cast<int>(
        std::max(PassInfo.Reg1Ty ? PassInfo.Reg1Ty->getSize() : 0,
                 PassInfo.Reg2Ty ? PassInfo.Reg2Ty->getSize() : 0));
    emit("  ld t0, {}(a0)", Off);
    emit("  sd t0, 8(sp)");
    return;
  }

  int Aligned = static_cast<int>(alignTo(Sz, 8));
  emit("  # push struct argument ({} bytes)", Aligned);
  emit("  addi sp, sp, -{}", Aligned);
  Depth += Aligned / 8;
  for (std::size_t I = 0; I < Sz; ++I) {
    emit("  lb t0, {}(a0)", I);
    emit("  sb t0, {}(sp)", I);
  }
}

void CodeGen::popStructArgToRegs(const Type *Ty, int &GP, int &FP,
                                 bool OnStack) {
  std::size_t Sz = Ty->getSize();
  if (isLargeStructByPointer(Ty)) {
    if (GP < GPMAX)
      pop(ArgReg[GP++]);
    return;
  }

  FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, GP, FP);
  if (!OnStack && PassInfo.IsFloatStruct) {
    const Type *Regs[2] = {PassInfo.Reg1Ty, PassInfo.Reg2Ty};
    for (int I = 0; I < 2; ++I) {
      if (!Regs[I])
        break;
      if (Regs[I]->isFloatingType()) {
        if (FP >= FPMAX)
          continue;
        if (Regs[I]->getSize() == 4) {
          emit("  # pop float struct part to fa{}", FP);
          emit("  flw fa{}, 0(sp)", FP);
          emit("  addi sp, sp, 8");
          --Depth;
          ++FP;
        } else {
          popF(FaArgReg[FP++]);
        }
      } else if (Regs[I]->isIntegerType()) {
        if (GP < GPMAX)
          pop(ArgReg[GP++]);
      }
    }
    return;
  }

  int Regs = (Sz > 8 && Sz <= 16) ? 2 : 1;
  for (int I = 0; I < Regs; ++I) {
    if (GP < GPMAX)
      pop(ArgReg[GP++]);
  }
}

void CodeGen::storeStructParam(const Type *Ty, int Offset, int &GP, int &FP,
                               bool HalfByStack) {
  std::size_t Sz = Ty->getSize();
  if (isLargeStructByPointer(Ty)) {
    emit("  # copy large struct parameter from a{}", GP);
    for (std::size_t I = 0; I < Sz; ++I) {
      emit("  lb t0, {}(a{})", I, GP);
      emit("  li t1, {}", Offset + static_cast<int>(I));
      emit("  add t1, fp, t1");
      emit("  sb t0, 0(t1)");
    }
    ++GP;
    return;
  }

  FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, GP, FP);
  if (PassInfo.IsFloatStruct) {
    const Type *Regs[2] = {PassInfo.Reg1Ty, PassInfo.Reg2Ty};
    int PartOff = 0;
    for (int I = 0; I < 2; ++I) {
      if (!Regs[I])
        break;
      if (Regs[I]->isFloatingType())
        storeFloatReg(FP++, Offset + PartOff,
                      static_cast<int>(Regs[I]->getSize()));
      else
        storeGenReg(GP++, Offset + PartOff,
                    static_cast<int>(Regs[I]->getSize()));
      if (I == 0 && Regs[1])
        PartOff =
            static_cast<int>(std::max(Regs[0]->getSize(), Regs[1]->getSize()));
    }
    return;
  }

  // First 8 bytes in a GP register; remainder copied from the caller area
  // at fp+16 (this half-struct is the first stack-resident argument).
  if (HalfByStack) {
    emit("  # store half-register / half-stack struct parameter");
    storeGenReg(GP++, Offset, 8);
    for (std::size_t I = 0; I < Sz - 8; ++I) {
      emit("  lb t0, {}(fp)", 16 + static_cast<int>(I));
      emit("  li t1, {}", Offset + 8 + static_cast<int>(I));
      emit("  add t1, fp, t1");
      emit("  sb t0, 0(t1)");
    }
    return;
  }

  if (Sz > 8 && Sz <= 16) {
    storeGenReg(GP++, Offset, 8);
    storeGenReg(GP++, Offset + 8, static_cast<int>(Sz - 8));
    return;
  }
  storeGenReg(GP++, Offset, static_cast<int>(Sz));
}

void CodeGen::copyRetBuffer(const VarDecl *Buf) {
  const Type *Ty = Buf->getType().getTypePtr();
  int Offset = Buf->getOffset();
  int GP = 0, FP = 0;

  emit("  # copy struct return value into buffer");
  FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, GP, FP);
  if (PassInfo.IsFloatStruct) {
    const Type *Regs[2] = {PassInfo.Reg1Ty, PassInfo.Reg2Ty};
    int PartOff = 0;
    for (int I = 0; I < 2; ++I) {
      if (!Regs[I])
        break;
      if (Regs[I]->isFloatingType())
        storeFloatReg(FP++, Offset + PartOff,
                      static_cast<int>(Regs[I]->getSize()));
      else
        storeGenReg(GP++, Offset + PartOff,
                    static_cast<int>(Regs[I]->getSize()));
      if (I == 0 && Regs[1])
        PartOff =
            static_cast<int>(std::max(Regs[0]->getSize(), Regs[1]->getSize()));
    }
    return;
  }

  int Sz = static_cast<int>(Ty->getSize());
  for (int Off = 0; Off < Sz; Off += 8) {
    int Rem = Sz - Off;
    int StoreSize;
    if (Rem == 1)
      StoreSize = 1;
    else if (Rem == 2)
      StoreSize = 2;
    else if (Rem == 3 || Rem == 4)
      StoreSize = 4;
    else
      StoreSize = 8;
    storeGenReg(GP++, Offset + Off, StoreSize);
  }
}

void CodeGen::copyStructReg() {
  const auto *FT = CurrFunc->getType()->getAs<FunctionType>();
  assert(FT);
  const Type *Ty = FT->getReturnType().getTypePtr();
  int GP = 0, FP = 0;

  emit("  # copy small struct return into registers");
  emit("  mv t1, a0");

  FloatStructPassInfo PassInfo = getFloatStructPassInfo(Ty, GP, FP);
  if (PassInfo.IsFloatStruct) {
    const Type *Regs[2] = {PassInfo.Reg1Ty, PassInfo.Reg2Ty};
    int PartOff = 0;
    for (int I = 0; I < 2; ++I) {
      if (!Regs[I])
        break;
      if (Regs[I]->isFloatingType())
        loadFloatRegFromT1(FP++, PartOff, static_cast<int>(Regs[I]->getSize()));
      else
        loadGenRegFromT1(GP++, PartOff, static_cast<int>(Regs[I]->getSize()));
      if (I == 0 && Regs[1])
        PartOff =
            static_cast<int>(std::max(Regs[0]->getSize(), Regs[1]->getSize()));
    }
    return;
  }

  int Sz = static_cast<int>(Ty->getSize());
  for (int Off = 0; Off < Sz; Off += 8) {
    int Rem = Sz - Off;
    int LoadSize;
    if (Rem == 1)
      LoadSize = 1;
    else if (Rem == 2)
      LoadSize = 2;
    else if (Rem == 3 || Rem == 4)
      LoadSize = 4;
    else
      LoadSize = 8;
    loadGenRegFromT1(GP++, Off, LoadSize);
  }
}

void CodeGen::copyStructMem() {
  const auto *FT = CurrFunc->getType()->getAs<FunctionType>();
  assert(FT);
  const Type *Ty = FT->getReturnType().getTypePtr();
  const VarDecl *Sret = findSretVar(CurrFunc);
  assert(Sret && "large struct return requires __sret__");

  emit("  # copy large struct return into caller buffer");
  emit("  li t0, {}", Sret->getOffset());
  emit("  add t0, fp, t0");
  emit("  ld t1, 0(t0)");
  for (std::size_t I = 0; I < Ty->getSize(); ++I) {
    emit("  lb t0, {}(a0)", I);
    emit("  sb t0, {}(t1)", I);
  }
  // ABI: return the address of the buffer in a0.
  emit("  mv a0, t1");
}

int CodeGen::createBigStructCallSpace(const CallExpr *CE) {
  int BSStack = 0;
  for (const Expr *Arg : CE->getArgs()) {
    const Type *Ty = Arg->getType().getTypePtr();
    if (!isLargeStructByPointer(Ty))
      continue;
    int Aligned = static_cast<int>(alignTo(Ty->getSize(), 8));
    emit("  # reserve stack space for large struct argument");
    emit("  addi sp, sp, -{}", Aligned);
    emit("  mv t6, sp");
    Depth += Aligned / 8;
    BigStructDepth += Aligned / 8;
    BSStack += Aligned / 8;
  }
  return BSStack;
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

void CodeGen::pushF() {
  emit("  # push fa0");
  emit("  addi sp, sp, -8");
  emit("  fsd fa0, 0(sp)");
  ++Depth;
}

void CodeGen::popF(const char *Reg) {
  emit("  # pop {}", Reg);
  emit("  fld {}, 0(sp)", Reg);
  emit("  addi sp, sp, 8");
  --Depth;
}

void CodeGen::emitIsNotZero(const Type *Ty) {
  if (!Ty->isFloatingType())
    return;

  // Compare fa0 against +0.0; result is 1 if non-zero (incl. NaN), else 0.
  if (Ty->getSize() == 4) {
    emit("  fmv.w.x fa1, zero");
    emit("  feq.s a0, fa0, fa1");
  } else {
    emit("  fmv.d.x fa1, zero");
    emit("  feq.d a0, fa0, fa1");
  }
  emit("  xori a0, a0, 1");
}

// load *a0 to a0 (or fa0 for floating types).
void CodeGen::load(const Type *Ty) {
  if (Ty->isArraryType() || Ty->isRecordType() || Ty->isFunctionType())
    return;

  if (Ty->isFloatingType()) {
    emit("  # load float");
    if (Ty->getSize() == 4)
      emit("  flw fa0, 0(a0)");
    else
      emit("  fld fa0, 0(a0)");
    return;
  }

  bool IsUnsigned = Ty->isUnsignedIntegerType();
  emit("  # load");
  emit("  l{} a0, 0(a0)", getWidthSuffix(Ty->getSize(), IsUnsigned));
}

void CodeGen::loadBitField(const FieldDecl *Field) {
  emit("  # extract bit-field ({} bits @ bit {})", Field->getBitWidth(),
       Field->getBitOffset());
  emit("  slli a0, a0, {}", 64 - Field->getBitWidth() - Field->getBitOffset());
  if (Field->getType()->isUnsignedIntegerType())
    emit("  srli a0, a0, {}", 64 - Field->getBitWidth());
  else
    emit("  srai a0, a0, {}", 64 - Field->getBitWidth());
}

void CodeGen::storeBitField(const FieldDecl *Field) {
  emit("  # merge bit-field value");
  emit("  mv t1, a0");
  emit("  li t0, {}", (1L << Field->getBitWidth()) - 1);
  emit("  and t1, t1, t0");
  emit("  slli t1, t1, {}", Field->getBitOffset());

  emit("  ld a0, 0(sp)");
  load(Field->getType().getTypePtr());

  long Mask = ((1L << Field->getBitWidth()) - 1) << Field->getBitOffset();
  emit("  li t0, {}", ~Mask);
  emit("  and a0, a0, t0");
  emit("  or a0, a0, t1");
}

// store a0 (or fa0) to *a1.
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

  if (Ty->isFloatingType()) {
    if (Ty->getSize() == 4)
      emit("  fsw fa0, 0(a1)");
    else
      emit("  fsd fa0, 0(a1)");
    return;
  }

  emit("  s{} a0, 0(a1)", getWidthSuffix(Ty->getSize()));
}

void CodeGen::storeGenReg(int Reg, int Offset, int Size) {
  emit("  li t0, {}", Offset);
  emit("  add t0, fp, t0");
  emit("  s{} {}, 0(t0)", getWidthSuffix(Size), ArgReg[Reg]);
}

void CodeGen::storeFloatReg(int Reg, int Offset, int Size) {
  emit("  li t0, {}", Offset);
  emit("  add t0, fp, t0");
  if (Size == 4)
    emit("  fsw {}, 0(t0)", FaArgReg[Reg]);
  else {
    assert(Size == 8);
    emit("  fsd {}, 0(t0)", FaArgReg[Reg]);
  }
}

void CodeGen::loadGenRegFromT1(int Reg, int Offset, int Size) {
  emit("  l{} {}, {}(t1)", getWidthSuffix(Size), ArgReg[Reg], Offset);
}

void CodeGen::loadFloatRegFromT1(int Reg, int Offset, int Size) {
  if (Size == 4)
    emit("  flw {}, {}(t1)", FaArgReg[Reg], Offset);
  else {
    assert(Size == 8);
    emit("  fld {}, {}(t1)", FaArgReg[Reg], Offset);
  }
}

const char *CodeGen::getWidthSuffix(std::size_t Size, bool IsUnsigned) const {
  switch (Size) {
  case 1:
    return IsUnsigned ? "bu" : "b";
  case 2:
    return IsUnsigned ? "hu" : "h";
  case 4:
    return IsUnsigned ? "wu" : "w";
  case 8:
    return "d";
  default:
    RCC_UNREACHABLE("unknown type size");
  }
}

int CodeGen::getCount() const {
  static int Count = 1;
  return Count++;
}

} // namespace rcc