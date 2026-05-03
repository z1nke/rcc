#include "AST/ASTContext.h"
#include "AST/Decl.h"
#include "Support/Unreachable.h"

namespace rcc {

void ASTContext::initBuiltinTypes() {
  initBuiltinType(VoidTy, BuiltinType::BK_Void, 1, 1);
  initBuiltinType(BoolTy, BuiltinType::BK_Bool, 1, 1);
  initBuiltinType(CharTy, BuiltinType::BK_Char, 1, 1);
  initBuiltinType(SignedCharTy, BuiltinType::BK_SignedChar, 1, 1);
  initBuiltinType(UnsignedCharTy, BuiltinType::BK_UnsignedChar, 1, 1);
  initBuiltinType(ShortTy, BuiltinType::BK_Short, 2, 2);
  initBuiltinType(UnsignedShortTy, BuiltinType::BK_UnsignedShort, 2, 2);
  initBuiltinType(IntTy, BuiltinType::BK_Int, 4, 4);
  initBuiltinType(UnsignedIntTy, BuiltinType::BK_UnsignedInt, 4, 4);
  initBuiltinType(LongTy, BuiltinType::BK_Long, 8, 8);
  initBuiltinType(UnsignedLongTy, BuiltinType::BK_UnsignedLong, 8, 8);
  initBuiltinType(LongLongTy, BuiltinType::BK_LongLong, 8, 8);
  initBuiltinType(UnsignedLongLongTy, BuiltinType::BK_UnsignedLongLong, 8, 8);
  initBuiltinType(FloatTy, BuiltinType::BK_Float, 4, 4);
  initBuiltinType(DoubleTy, BuiltinType::BK_Double, 8, 8);
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
                                     std::vector<QualType> ParamTypes,
                                     bool IsVariadic) {
  auto *Ty = new (*this, alignof(FunctionType))
      FunctionType(RetType, std::move(ParamTypes), IsVariadic);
  return QualType(Ty);
}

QualType ASTContext::getConstantArrayType(QualType ElementType,
                                          std::size_t Len) {
  auto *Ty = new (*this, alignof(ConstantArrayType))
      ConstantArrayType(ElementType, Len);
  return QualType(Ty);
}

QualType ASTContext::getIncompleteArrayType(QualType ElementType) {
  auto *Ty = new (*this, alignof(IncompleteArrayType))
      IncompleteArrayType(ElementType);
  return QualType(Ty);
}

QualType ASTContext::getRecordType(RecordDecl *RD, std::size_t Size,
                                   std::size_t Align) {
  const auto *Canonical = RD->getCanonicalDecl();
  auto *&Ty = TagTypes[Canonical];
  if (Ty)
    return QualType(Ty);

  Ty = new (*this, alignof(RecordType)) RecordType(RD, Size, Align);
  return QualType(Ty);
}

QualType ASTContext::getEnumType(EnumDecl *ED) {
  const auto *Canonical = ED->getCanonicalDecl();
  auto *&Ty = TagTypes[Canonical];
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
  case BuiltinType::BK_SignedChar:
  case BuiltinType::BK_UnsignedChar:
    return 2 + (getIntWidth(CharTy) << 3);
  case BuiltinType::BK_Short:
  case BuiltinType::BK_UnsignedShort:
    return 3 + (getIntWidth(ShortTy) << 3);
  case BuiltinType::BK_Int:
  case BuiltinType::BK_UnsignedInt:
    return 4 + (getIntWidth(IntTy) << 3);
  case BuiltinType::BK_Long:
  case BuiltinType::BK_UnsignedLong:
    return 5 + (getIntWidth(LongTy) << 3);
  case BuiltinType::BK_LongLong:
  case BuiltinType::BK_UnsignedLongLong:
    return 6 + (getIntWidth(LongLongTy) << 3);
  default:
    RCC_UNREACHABLE("Unknown int type kind");
  }
}

std::size_t ASTContext::getIntWidth(QualType T) const {
  // TODO: Bool -> 1
  return 8 * T->getSize();
}

QualType ASTContext::getCorrespondingUnsignedType(QualType T) const {
  const auto *BT = dynCast<BuiltinType>(T.getCanonicalType().getTypePtr());
  assert(BT);
  switch (BT->getKind()) {
  case BuiltinType::BK_Char:
  case BuiltinType::BK_SignedChar:
    return UnsignedCharTy;
  case BuiltinType::BK_Short:
    return UnsignedShortTy;
  case BuiltinType::BK_Int:
    return UnsignedIntTy;
  case BuiltinType::BK_Long:
    return UnsignedLongTy;
  case BuiltinType::BK_LongLong:
    return UnsignedLongLongTy;
  case BuiltinType::BK_UnsignedChar:
  case BuiltinType::BK_UnsignedShort:
  case BuiltinType::BK_UnsignedInt:
  case BuiltinType::BK_UnsignedLong:
  case BuiltinType::BK_UnsignedLongLong:
  case BuiltinType::BK_Bool:
    return T;
  default:
    RCC_UNREACHABLE("getCorrespondingUnsignedType: not an integer type");
  }
}

} // namespace rcc