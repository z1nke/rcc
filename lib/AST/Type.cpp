#include "AST/Type.h"
#include "AST/Decl.h"
#include "Support/Casting.h"
#include "Support/Unreachable.h"

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

QualType QualType::getUnqualifiedType() const { return QualType(getTypePtr()); }

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
  case Type::TK_Typedef: {
    const auto *TT = cast<TypedefType>(Ty);
    return {TT->getDecl()->getName(), ""};
  }
  case Type::TK_Function: {
    const auto *FT = cast<FunctionType>(Ty);
    TypeDumper Dumper = dumpToString(FT->getReturnType());
    std::string Params;
    Params.reserve(64);
    Params += '(';
    for (unsigned I = 0; I < FT->getNumParams(); ++I) {
      if (I)
        Params += ", ";
      Params += FT->getParamType(I).getAsString();
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
  case Type::TK_Record: {
    const auto *RT = cast<RecordType>(Ty);
    const auto *Record = dynCast<RecordDecl>(RT->getDecl());
    assert(Record);

    TypeDumper Dumper;
    Dumper.Base = "struct ";
    Dumper.Postfix = Record->getName();
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
  if (const auto *BT = dynCast<BuiltinType>(getCanonicalType()))
    return BT->isIntegerType();

  return false;
}

bool QualType::isVoidType() const {
  if (const auto *BT = dynCast<BuiltinType>(getCanonicalType()))
    return BT->isVoidType();

  return false;
}

void Type::dump() const {
  QualType T(this);
  std::println(stderr, "{}", T.getAsString());
}

QualType Type::getCanonicalType() const {
  switch (Kind) {
  case TK_Typedef: {
    return cast<TypedefType>(this)->getCanonicalType();
  default:
    break;
  }
  }
  return QualType(this);
}

bool Type::isScalarType() const {
  QualType CanonicalType = getCanonicalType();
  if (const auto *BT = dynCast<BuiltinType>(CanonicalType)) {
    // TODO: Other builtin type.
    return BT->isIntegerType();
  }

  return isa<PointerType>(CanonicalType);
}

bool Type::isArithmeticType() const {
  QualType CanonicalType = getCanonicalType();
  if (const auto *BT = dynCast<BuiltinType>(CanonicalType)) {
    // TODO: Other builtin type.
    return BT->isIntegerType();
  }
  return false;
}

QualType Type::getPointeeType() const {
  if (const auto *PtrTy = dynCast<PointerType>(this))
    return PtrTy->getPointeeType();

  return QualType();
}

QualType Type::getBaseElementType() const {
  if (const auto *AT = dynCast<ArrayType>(this))
    return AT->getElementType();
  return QualType();
}

const Type *Type::getPointeeOrArrayElementTypePtr() const {
  return getPointeeOrArrayElementType().getTypePtr();
}

QualType Type::getPointeeOrArrayElementType() const {
  if (const auto *PtrTy = dynCast<PointerType>(this))
    return PtrTy->getPointeeType();
  if (const auto *AT = dynCast<ArrayType>(this))
    return AT->getElementType();
  return QualType();
}

bool Type::isUnsignedIntegerType() const {
  // TODO: Impl
  // if (const auto *BT = dynCast<BuiltinType>(getCanonicalType())) {
  // }

  return false;
}

bool Type::isSignedIntegerType() const {
  if (const auto *BT = dynCast<BuiltinType>(getCanonicalType())) {
    auto Kind = BT->getKind();
    return Kind >= BuiltinType::BK_Char && Kind <= BuiltinType::BK_LongLong;
  }
  return false;
}

bool Type::isSignedIntegerOrEnumerationType() const {
  // TODO: Enum
  return isSignedIntegerType();
}

RecordDecl *Type::getAsRecordDecl() const {
  return dynCastOrNull<RecordDecl>(getAsTagDecl());
}

TagDecl *Type::getAsTagDecl() const {
  if (const auto *TT = getAs<TagType>())
    return TT->getDecl();
  return nullptr;
}

bool BuiltinType::isIntegerType() const {
  return BK >= BK_Char && BK <= BK_LongLong;
}

bool BuiltinType::isVoidType() const { return BK == BK_Void; }

const char *BuiltinType::getKindStr() const {
  switch (BK) {
  case BK_Void:
    return "void";
  case BK_Char:
    return "char";
  case BK_Short:
    return "short";
  case BK_Int:
    return "int";
  case BK_Long:
    return "long";
  default:
    RCC_UNREACHABLE("Unknown builtin type");
  }
}

QualType TypedefType::getUnderlying() const { return D->getUnderlying(); }

QualType TypedefType::getCanonicalType() const {
  return D->getUnderlying().getCanonicalType();
}

} // namespace rcc