#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "Support/Unreachable.h"

namespace rcc {

void ASTContext::initBuiltinTypes() {
  initBuiltinType(VoidTy, BuiltinType::BK_Void, 1, 1);
  initBuiltinType(BoolTy, BuiltinType::BK_Bool, 1, 1);
  initBuiltinType(CharTy, BuiltinType::BK_Char, 1, 1);
  initBuiltinType(ShortTy, BuiltinType::BK_Short, 2, 2);
  initBuiltinType(IntTy, BuiltinType::BK_Int, 4, 4);
  initBuiltinType(LongTy, BuiltinType::BK_Long, 8, 8);
  initBuiltinType(LongLongTy, BuiltinType::BK_LongLong, 8, 8);
}

void ASTContext::initBuiltinType(CanQualType &R, BuiltinType::Kind Kind,
                                 std::size_t Size, std::size_t Align) {
  auto *Ty = new (*this, alignof(BuiltinType)) BuiltinType(Kind, Size, Align);
  R = CanQualType(Ty);
  Types.push_back(Ty);
}

QualType ASTContext::getPointerType(QualType PointeeType) {
  assert(!PointeeType.isNull());
  void *Opaque = PointeeType.getOpaquePtr();
  auto Iter = PointerTypes.find(Opaque);
  if (Iter != PointerTypes.end())
    return Iter->second;

  auto *Ty = new (*this, alignof(PointerType)) PointerType(PointeeType);
  PointerTypes[Opaque] = Ty;
  return QualType(Ty);
}

QualType ASTContext::getFunctionType(QualType RetType,
                                     std::vector<QualType> ParamTypes) {
  auto *Ty = new (*this, alignof(FunctionType))
      FunctionType(RetType, std::move(ParamTypes));
  return QualType(Ty);
}

QualType ASTContext::getConstantArrayType(QualType ElementType,
                                          std::size_t Len) {
  auto *Ty = new (*this, alignof(ConstantArrayType))
      ConstantArrayType(ElementType, Len);
  return QualType(Ty);
}

QualType ASTContext::getRecordType(RecordDecl *RD, std::size_t Size,
                                   std::size_t Align) {
  const auto *Canonical = RD->getCanonicalDecl();
  RecordType *&Ty = RecordTypes[Canonical];
  if (Ty)
    return QualType(Ty);

  Ty = new (*this, alignof(RecordType)) RecordType(RD, Size, Align);
  return QualType(Ty);
}

QualType ASTContext::getEnumType(EnumDecl *ED) {
  const auto *Canonical = ED->getCanonicalDecl();
  EnumType *&Ty = EnumTypes[Canonical];
  if (Ty)
    return QualType(Ty);

  Ty = new (*this, alignof(EnumType))
      EnumType(ED, IntTy->getSize(), IntTy->getAlign());
  return QualType(Ty);
}

QualType ASTContext::getTypedefType(TypedefDecl *TD, QualType Underlying) {
  TypedefType *&Ty = TypedefTypes[TD];
  if (Ty)
    return QualType(Ty);

  Ty = new (*this, alignof(TypedefType)) TypedefType(TD, Underlying);
  return QualType(Ty);
}

QualType ASTContext::getArrayDecayedType(QualType Ty) {
  const auto *AT = Ty->getAs<ArrayType>();
  assert(AT);

  return getPointerType(AT->getElementType());
}

int ASTContext::getIntTypeOrder(QualType LHS, QualType RHS) const {
  const auto *LTy = LHS.getCanonicalType().getTypePtr();
  const auto *RTy = RHS.getCanonicalType().getTypePtr();

  // TODO: Enum type.
  if (LTy == RTy)
    return 0;

  bool IsLU = LTy->isUnsignedIntegerType();
  bool IsRU = RTy->isUnsignedIntegerType();
  unsigned LRank = getIntRank(LTy);
  unsigned RRank = getIntRank(RTy);
  if (IsLU == IsRU) {
    if (LRank == RRank)
      return 0;
    return LRank > RRank ? 1 : -1;
  }

  if (IsLU)
    return LRank >= RRank ? 1 : -1;

  return RRank >= LRank ? -1 : 1;
}

unsigned ASTContext::getIntRank(const Type *T) const {
  const auto *BT = dynCast<BuiltinType>(T);
  assert(BT);
  switch (BT->getKind()) {
  case BuiltinType::BK_Bool:
    return 1 + (getIntWidth(BoolTy) << 3);
  case BuiltinType::BK_Char:
    return 2 + (getIntWidth(CharTy) << 3);
  case BuiltinType::BK_Short:
    return 3 + (getIntWidth(ShortTy) << 3);
  case BuiltinType::BK_Int:
    return 4 + (getIntWidth(IntTy) << 3);
  case BuiltinType::BK_Long:
    return 5 + (getIntWidth(LongTy) << 3);
  case BuiltinType::BK_LongLong:
    return 6 + (getIntWidth(LongLongTy) << 3);
  default:
    RCC_UNREACHABLE("Unknown int type kind");
  }
}

std::size_t ASTContext::getIntWidth(QualType T) const {
  // TODO: Bool -> 1
  return 8 * T->getSize();
}

} // namespace rcc