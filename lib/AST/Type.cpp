#include "AST/Type.h"
#include "Basic/Casting.h"

#include <cassert>

namespace rcc {

QualType::QualType(const Type *Ptr, unsigned Quals) {
  auto PtrVal = reinterpret_cast<std::uintptr_t>(Ptr);
  PtrVal |= Quals;
  Value = reinterpret_cast<void *>(PtrVal);
}

const Type *QualType::getTypePtr() const {
  auto PtrVal = reinterpret_cast<std::uintptr_t>(Value);
  PtrVal &= ~(uintptr_t)(Qualifiers::TypeAlignment - 1);
  return reinterpret_cast<const Type *>(PtrVal);
}

QualType QualType::getCanonicalType() const {
  const Type *TypePtr = getTypePtr();
  return TypePtr ? TypePtr->getCanonicalType() : *this;
}

bool QualType::isIntegerType() const {
  if (const auto *BT = dyn_cast<BuiltinType>(getCanonicalType()))
    return BT->getKind() == BuiltinType::BK_Int;

  return false;
}

QualType Type::getCanonicalType() const {
  // TODO: Handle typedef type.
  return QualType(this);
}

bool Type::isScalarType() const {
  QualType CanonicalType = getCanonicalType();
  if (const auto *BT = dyn_cast<BuiltinType>(CanonicalType)) {
    // TODO: Other builtin type.
    return BT->getKind() == BuiltinType::BK_Int;
  }

  return isa<PointerType>(CanonicalType);
}

bool Type::isArithmeticType() const {
  QualType CanonicalType = getCanonicalType();
  if (const auto *BT = dyn_cast<BuiltinType>(CanonicalType)) {
    // TODO: Other builtin type.
    return BT->getKind() == BuiltinType::BK_Int;
  }
  return false;
}

} // namespace rcc