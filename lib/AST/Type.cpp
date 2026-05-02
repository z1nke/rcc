#include "AST/Type.h"
#include "AST/ASTContext.h"
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
    if (FT->isVariadic()) {
      if (FT->getNumParams() != 0)
        Params += ", ";
      Params += "...";
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
  case Type::TK_IncompleteArray: {
    const auto *IAT = cast<IncompleteArrayType>(Ty);
    TypeDumper Dumper = dumpToString(IAT->getBaseElementType());
    Dumper.Postfix = "[]" + Dumper.Postfix;
    return Dumper;
  }
  case Type::TK_Record: {
    const auto *RT = cast<RecordType>(Ty);
    const auto *Record = dynCast<RecordDecl>(RT->getDecl());
    assert(Record);

    TypeDumper Dumper;
    Dumper.Base = Record->isUnion() ? "union " : "struct ";
    Dumper.Postfix = Record->getName();
    return Dumper;
  }
  case Type::TK_Enum: {
    const auto *ET = cast<EnumType>(Ty);
    const auto *Enum = dynCast<EnumDecl>(ET->getDecl());
    assert(Enum);

    TypeDumper Dumper;
    Dumper.Base = "enum ";
    Dumper.Postfix = Enum->getName();
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

bool QualType::isVoidType() const {
  if (const auto *BT = dynCast<BuiltinType>(getCanonicalType()))
    return BT->isVoidType();

  return false;
}

bool QualType::isIntegerType() const {
  QualType CanType = getCanonicalType();
  if (const auto *BT = dynCast<BuiltinType>(CanType))
    return BT->isIntegerType();

  if (isa<EnumType>(CanType))
    return true;

  return false;
}

void Type::dump() const {
  QualType T(this);
  std::println(stderr, "{}", T.getAsString());
}

std::size_t Type::getSize() const {
  if (Size != 0)
    return Size;

  QualType Canonical = getCanonicalType();
  if (Canonical->Size != 0)
    return Canonical->Size;

  if (const auto *TT = getAs<RecordType>()) {
    const auto *D = TT->getDecl();
    assert(D);
    const auto *Def = D->getDefinition();
    if (!Def)
      return 0;
    QualType DefType = Def->getType();
    const auto *DefTypePtr = DefType.getTypePtr();
    if (!DefTypePtr || DefTypePtr == this)
      return Size;
    if (DefTypePtr->Size != 0)
      return DefTypePtr->Size;
    return 0;
  }

  return 0;
}

QualType Type::getCanonicalType() const {
  switch (Kind) {
  case TK_Typedef:
    return cast<TypedefType>(this)->getCanonicalType();
  default:
    break;
  }
  return QualType(this);
}

// Scalar type: arithmetic type or pointer type.
bool Type::isScalarType() const {
  return isArithmeticType() || isPointerType();
}

// Arithmetic type: integer type or floating point type.
bool Type::isArithmeticType() const { return isIntegerType(); }

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

bool Type::isIntegerType() const {
  QualType CanonicalType = getCanonicalType();
  if (const auto *BT = dynCast<BuiltinType>(CanonicalType))
    return BT->isIntegerType();

  if (CanonicalType->isEnumType())
    return true;

  return false;
}

bool Type::isBooleanType() const {
  const auto *BT = dynCast<BuiltinType>(getCanonicalType());
  return BT && BT->getKind() == BuiltinType::BK_Bool;
}

bool Type::isUnsignedIntegerType() const {
  if (const auto *BT = dynCast<BuiltinType>(getCanonicalType())) {
    switch (BT->getKind()) {
    case BuiltinType::BK_Bool:
    case BuiltinType::BK_Char: // Plain char is unsigned on RISC-V.
    case BuiltinType::BK_UnsignedChar:
    case BuiltinType::BK_UnsignedShort:
    case BuiltinType::BK_UnsignedInt:
    case BuiltinType::BK_UnsignedLong:
    case BuiltinType::BK_UnsignedLongLong:
      return true;
    default:
      return false;
    }
  }

  return false;
}

bool Type::isSignedIntegerType() const {
  if (const auto *BT = dynCast<BuiltinType>(getCanonicalType())) {
    switch (BT->getKind()) {
    case BuiltinType::BK_SignedChar:
    case BuiltinType::BK_Short:
    case BuiltinType::BK_Int:
    case BuiltinType::BK_Long:
    case BuiltinType::BK_LongLong:
      return true;
    default:
      return false;
    }
  }
  return false;
}

bool Type::isSignedIntegerOrEnumerationType() const {
  if (isSignedIntegerType())
    return true;

  if (isa<EnumType>(getCanonicalType()))
    return true;

  return false;
}
bool Type::isPointerType() const {
  QualType CanType = getCanonicalType();
  return CanType->getTypeKind() == TK_Pointer;
}

bool Type::isFunctionType() const {
  QualType CanType = getCanonicalType();
  return CanType->getTypeKind() == TK_Function;
}

bool Type::isArraryType() const {
  QualType CanType = getCanonicalType();
  return CanType->getAs<ArrayType>();
}

bool Type::isRecordType() const {
  QualType CanType = getCanonicalType();
  return CanType->getAs<RecordType>();
}

bool Type::isEnumType() const {
  QualType CanType = getCanonicalType();
  return CanType->getAs<EnumType>();
}

bool Type::isIncompleteType() const {
  QualType CanType = getCanonicalType();
  switch (CanType->getTypeKind()) {
  case TK_Builtin:
    return cast<BuiltinType>(CanType)->isVoidType();
  case TK_IncompleteArray:
    return true;
  case TK_Record: {
    const auto *RT = cast<RecordType>(CanType);
    return RT->getDecl()->getDefinition() == nullptr;
  }
  case TK_Enum: {
    const auto *ET = cast<EnumType>(CanType);
    return ET->getDecl()->getDefinition() == nullptr;
  }
  default:
    return false;
  }
}

RecordDecl *Type::getAsRecordDecl() const {
  return dynCastOrNull<RecordDecl>(getAsTagDecl());
}

TagDecl *Type::getAsTagDecl() const {
  if (const auto *TT = getAs<TagType>())
    return TT->getDecl();
  return nullptr;
}

bool BuiltinType::isBooleanType() const { return BK == BK_Bool; }

bool BuiltinType::isIntegerType() const {
  return BK >= BK_Bool && BK <= BK_UnsignedLongLong;
}

bool BuiltinType::isVoidType() const { return BK == BK_Void; }

const char *BuiltinType::getKindStr() const {
  switch (BK) {
  case BK_Void:
    return "void";
  case BK_Bool:
    return "_Bool";
  case BK_Char:
    return "char";
  case BK_SignedChar:
    return "signed char";
  case BK_UnsignedChar:
    return "unsigned char";
  case BK_Short:
    return "short";
  case BK_UnsignedShort:
    return "unsigned short";
  case BK_Int:
    return "int";
  case BK_UnsignedInt:
    return "unsigned int";
  case BK_Long:
    return "long";
  case BK_UnsignedLong:
    return "unsigned long";
  case BK_LongLong:
    return "long long";
  case BK_UnsignedLongLong:
    return "unsigned long long";
  default:
    RCC_UNREACHABLE("Unknown builtin type");
  }
}

QualType TypedefType::getUnderlying() const { return D->getUnderlying(); }

QualType TypedefType::getCanonicalType() const {
  return D->getUnderlying().getCanonicalType();
}

} // namespace rcc