#ifndef RCC_AST_TYPE_H
#define RCC_AST_TYPE_H

#include "Support/Casting.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rcc {

class ASTContext;
class QualType;
class Type;
class TagDecl;
class RecordDecl;
class EnumDecl;
class TypedefDecl;

class Qualifiers {
public:
  enum : std::uint32_t {
    Const = 0x1,
    Restrict = 0x2,
    Volatile = 0x4,
    CVRMask = Const | Restrict | Volatile,
  };

  static constexpr std::uint64_t FastWidth = 3;
  static constexpr std::uint64_t TypeAlignment = 1 << FastWidth;

  bool hasConst() const { return Mask & Const; }
  void removeConst() { Mask &= ~Const; }
  void addConst() { Mask |= Const; }

  bool hasVolatile() const { return Mask & Volatile; }
  void removeVolatile() { Mask &= ~Volatile; }
  void addVolatile() { Mask |= Volatile; }

  bool hasRestrict() const { return Mask & Restrict; }
  void removeRestrict() { Mask &= ~Restrict; }
  void addRestrict() { Mask |= Restrict; }

private:
  std::uint32_t Mask = 0;
};

class QualType {
public:
  QualType() = default;
  QualType(const Type *Ptr, unsigned Quals = 0);
  QualType(const QualType &) = default;
  QualType &operator=(const QualType &) = default;

  void *getOpaquePtr() const { return Value; }

  bool isNull() const { return Value == nullptr; }
  explicit operator bool() const { return !isNull(); }

  const Type *getTypePtr() const;
  unsigned getQualifiers() const;
  QualType getUnqualifiedType() const;

  std::string getAsString() const;

  const Type &operator*() const { return *getTypePtr(); }
  const Type *operator->() const { return getTypePtr(); }

  QualType getCanonicalType() const;
  bool isVoidType() const;
  bool isIntegerType() const;
  bool isFloatingType() const;

  friend bool operator==(const QualType &LHS, const QualType &RHS) {
    return LHS.Value == RHS.Value;
  }

  friend auto operator<(const QualType &LHS, const QualType &RHS) {
    return LHS.Value < RHS.Value;
  }

private:
  void *Value = nullptr;
};

class alignas(Qualifiers::TypeAlignment) Type {
public:
  enum TypeKind {
    TK_Builtin,
    TK_Pointer,
    TK_Typedef,
    TK_Function,
    TK_ConstantArray,
    TK_IncompleteArray,
    TK_Record,
    TK_Enum,
  };

  Type &operator=(const Type &) = delete;
  Type &operator=(Type &&) = delete;

  void dump() const;

  TypeKind getTypeKind() const { return Kind; }
  std::size_t getSize() const;
  void setSize(std::size_t Size) { this->Size = Size; }
  std::size_t getAlign() const { return Align; }
  void setAlign(std::size_t Align) { this->Align = Align; }

  QualType getCanonicalType() const;

  QualType getPointeeType() const;
  QualType getBaseElementType() const;
  const Type *getPointeeOrArrayElementTypePtr() const;
  QualType getPointeeOrArrayElementType() const;

  bool isScalarType() const;
  bool isArithmeticType() const;
  bool isIntegerType() const;
  bool isFloatingType() const;
  bool isBooleanType() const;
  bool isUnsignedIntegerType() const;
  bool isSignedIntegerType() const;
  bool isSignedIntegerOrEnumerationType() const;
  bool isPointerType() const;
  bool isFunctionType() const;
  bool isArraryType() const;
  bool isRecordType() const;
  bool isEnumType() const;
  bool isIncompleteType() const;

  template <typename To> const To *getAs() const {
    if (const auto *Ty = dynCast<To>(this))
      return Ty;

    if (Kind != TK_Typedef)
      return nullptr;

    return dynCast<To>(getCanonicalType());
  }

  RecordDecl *getAsRecordDecl() const;
  TagDecl *getAsTagDecl() const;

protected:
  Type(TypeKind Kind, std::size_t Size, std::size_t Align = 0)
      : Kind(Kind), Size(Size), Align(Align) {}
  Type(const Type &) = delete;
  Type(Type &&) = delete;

private:
  TypeKind Kind;
  std::size_t Size = 0;
  std::size_t Align = 0;
};

class BuiltinType final : public Type {
public:
  enum Kind {
    BK_Void,
    BK_Bool,
    BK_Char, // Plain char (unsigned on RISC-V)
    BK_SignedChar,
    BK_UnsignedChar,
    BK_Short,
    BK_UnsignedShort,
    BK_Int,
    BK_UnsignedInt,
    BK_Long,
    BK_UnsignedLong,
    BK_LongLong,
    BK_UnsignedLongLong,
    BK_Float,
    BK_Double,
  };

  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Builtin;
  }

  bool isBooleanType() const;
  bool isIntegerType() const;
  bool isFloatingType() const;
  bool isVoidType() const;

  Kind getKind() const { return BK; }
  const char *getKindStr() const;

private:
  friend class ASTContext;

  explicit BuiltinType(Kind BK, std::size_t Size, std::size_t Align)
      : Type(TK_Builtin, Size, Align), BK(BK) {}

private:
  Kind BK;
};

class PointerType final : public Type {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Pointer;
  }

  QualType getPointeeType() const { return PointeeType; }

private:
  friend class ASTContext;

  explicit PointerType(QualType Pointee)
      : Type(TK_Pointer, 8, 8), PointeeType(Pointee) {}

private:
  QualType PointeeType;
};

class FunctionType final : public Type {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == TypeKind::TK_Function;
  }

  QualType getReturnType() const { return RetType; }

  unsigned getNumParams() const { return ParamTypes.size(); }
  QualType getParamType(unsigned Idx) const { return ParamTypes[Idx]; }
  const std::vector<QualType> &getParamTypes() const { return ParamTypes; }
  bool isVariadic() const { return IsVariadic; }

private:
  friend class ASTContext;

  FunctionType(QualType RetType, std::vector<QualType> ParamTypes,
               bool IsVariadic)
      // [GNU] C forbids sizeof(function type); GCC allows it and yields 1.
      : Type(TK_Function, 1, 1), RetType(RetType),
        ParamTypes(std::move(ParamTypes)), IsVariadic(IsVariadic) {}

private:
  QualType RetType;
  std::vector<QualType> ParamTypes;
  bool IsVariadic;
};

class ArrayType : public Type {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() >= Type::TK_ConstantArray &&
           T->getTypeKind() <= Type::TK_IncompleteArray;
  }

  QualType getElementType() const { return ElementType; }

protected:
  friend class ASTContext;

  ArrayType(TypeKind Kind, QualType ElementType, std::size_t Size)
      : Type(Kind, Size, ElementType->getAlign()), ElementType(ElementType) {}

private:
  QualType ElementType;
};

class ConstantArrayType final : public ArrayType {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_ConstantArray;
  }

  std::size_t getLength() const { return Len; }

private:
  friend class ASTContext;

  ConstantArrayType(QualType ElementType, std::size_t Len)
      : ArrayType(TK_ConstantArray, ElementType, ElementType->getSize() * Len),
        Len(Len) {}

private:
  std::size_t Len;
};

class IncompleteArrayType final : public ArrayType {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_IncompleteArray;
  }

private:
  friend class ASTContext;

  IncompleteArrayType(QualType ElementType)
      : ArrayType(TK_IncompleteArray, ElementType, 0) {}
};

class TagType : public Type {
public:
  TagDecl *getDecl() const { return TD; }

  static bool classof(const Type *T) {
    auto Kind = T->getTypeKind();
    return Kind == Type::TK_Record || Kind == Type::TK_Enum;
  }

protected:
  friend class ASTContext;

  TagType(TypeKind TK, std::size_t Size, std::size_t Align, TagDecl *TD)
      : Type(TK, Size, Align), TD(TD) {}

private:
  TagDecl *TD;
};

class RecordType final : public TagType {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Record;
  }

  RecordDecl *getDecl() const {
    return reinterpret_cast<RecordDecl *>(TagType::getDecl());
  }

private:
  friend class ASTContext;

  RecordType(RecordDecl *RD, std::size_t Size, std::size_t Align = 0)
      : TagType(TK_Record, Size, Align, reinterpret_cast<TagDecl *>(RD)) {}
};

class EnumType final : public TagType {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Enum;
  }

  EnumDecl *getDecl() const {
    return reinterpret_cast<EnumDecl *>(TagType::getDecl());
  }

private:
  friend class ASTContext;

  EnumType(EnumDecl *ED, std::size_t Size, std::size_t Align = 0)
      : TagType(TK_Enum, Size, Align, reinterpret_cast<TagDecl *>(ED)) {}
};

class TypedefType final : public Type {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Typedef;
  }

  TypedefDecl *getDecl() const { return D; }
  QualType getUnderlying() const;
  QualType getCanonicalType() const;

private:
  friend class ASTContext;

  TypedefType(TypedefDecl *D, QualType Underlying)
      : Type(TK_Typedef, Underlying->getSize(), Underlying->getAlign()), D(D) {}

private:
  TypedefDecl *D;
};

template <typename To> inline bool isa(QualType T) {
  return isa<To>(T.getTypePtr());
}

template <typename To> inline const To *dynCast(QualType T) {
  return isa<To>(T) ? static_cast<const To *>(T.getTypePtr()) : nullptr;
}

template <typename To> inline const To *cast(QualType T) {
  assert(isa<To>(T));
  return static_cast<const To *>(T.getTypePtr());
}

} // namespace rcc

#endif