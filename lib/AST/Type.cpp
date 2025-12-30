#include "AST/Type.h"
#include "Basic/Casting.h"
#include "Basic/Unreachable.h"

#include <cassert>
#include <print>

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

unsigned QualType::getQualifiers() const {
  auto PtrVal = reinterpret_cast<std::uintptr_t>(Value);
  PtrVal |= (uintptr_t)(Qualifiers::TypeAlignment - 1);
  return static_cast<unsigned>(PtrVal);
}

struct TypeDumper {
  std::string asString() const { return Base + Postfix; }
  std::string Base;
  std::string Postfix;
};

static TypeDumper dumpToString(QualType T) {
  // TODO: Get qualifiers.
  const auto *Ty = T.getTypePtr();
  switch (Ty->getTypeKind()) {
  case Type::TK_Builtin: {
    const auto *BT = cast<BuiltinType>(Ty);
    return {BT->getKindStr(), ""};
  }
  case Type::TK_Pointer: {
    const auto *PT = cast<PointerType>(Ty);
    QualType PointeeType = PT->getPointeeType();
    TypeDumper Dumper = dumpToString(PointeeType);
    bool NeedParen = PointeeType->isArraryType();
    if (NeedParen) {
      Dumper.Base += "(*";
      Dumper.Postfix = ")" + Dumper.Postfix;
    } else {
      Dumper.Base += " *";
    }
    return Dumper;
  }
  case Type::TK_Typedef:
    // TODO: Impl
    return TypeDumper();
  case Type::TK_Function: {
    const auto *FT = cast<FunctionType>(Ty);
    TypeDumper Dumper = dumpToString(FT->getReturnType());
    std::string Params;
    Params.reserve(64);
    Params += '(';
    for (unsigned i = 0; i < FT->getNumParams(); ++i) {
      if (i)
        Params += ", ";
      Params += FT->getParamType(i).getAsString();
    }
    Params += ')';
    Dumper.Postfix += Params;
    return Dumper;
  }
  case Type::TK_ConstantArray: {
    const auto *CAT = cast<ConstantArrayType>(Ty);
    std::size_t Len = CAT->getLength();
    TypeDumper Dumper = dumpToString(CAT->getBaseElementType());
    if (Len > 0) {
      std::string ArrStr;
      ArrStr.reserve(32);
      ArrStr += '[';
      ArrStr += std::to_string(Len);
      ArrStr += ']';
      Dumper.Postfix = ArrStr + Dumper.Postfix;
    } else {
      Dumper.Postfix = "[]" + Dumper.Postfix;
    }
    return Dumper;
  }
  default:
    RCC_UNREACHABLE("Unknown type kind");
  }
}

std::string QualType::getAsString() const {
  TypeDumper Dumper = dumpToString(*this);
  return Dumper.asString();
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

void Type::dump() const {
  QualType T(this);
  std::println(stderr, "{}", T.getAsString());
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

QualType Type::getPointeeType() const {
  if (const auto *PtrTy = dyn_cast<PointerType>(this))
    return PtrTy->getPointeeType();

  return QualType();
}

QualType Type::getBaseElementType() const {
  if (const auto *AT = dyn_cast<ArrayType>(this))
    return AT->getElementType();
  return QualType();
}

const Type *Type::getPointeeOrArrayElementType() const {
  if (isPointerType())
    return getPointeeType().getTypePtr();
  if (isArraryType())
    return getBaseElementType().getTypePtr();
  return nullptr;
}

const char *BuiltinType::getKindStr() const {
  switch (BK) {
  case BK_Int:
    return "int";
  default:
    RCC_UNREACHABLE("Unknown builtin type");
  }
}

} // namespace rcc