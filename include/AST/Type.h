#ifndef RCC_AST_TYPE_H
#define RCC_AST_TYPE_H

#include "Basic/Casting.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rcc {

class QualType;
class Type;

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

  void *getOpaquePtr() const { return Value; }

  bool isNull() const { return Value == nullptr; }
  explicit operator bool() const { return !isNull(); }

  const Type *getTypePtr() const;
  unsigned getQualifiers() const;

  std::string getAsString() const;

  const Type &operator*() const { return *getTypePtr(); }
  const Type *operator->() const { return getTypePtr(); }

  QualType getCanonicalType() const;

  bool isIntegerType() const;

  QualType(const QualType &) = default;
  QualType &operator=(const QualType &) = default;

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
  };

  Type(TypeKind Kind) : Kind(Kind) {}
  Type(TypeKind Kind, std::size_t Size) : Kind(Kind), Size(Size) {}
  Type(const Type &) = delete;
  Type(Type &&) = delete;
  Type &operator=(const Type &) = delete;
  Type &operator=(Type &&) = delete;

  void dump() const;

  TypeKind getTypeKind() const { return Kind; }
  std::size_t getSize() const { return Size; }

  QualType getCanonicalType() const;

  bool isPointerType() const { return Kind == TK_Pointer; }
  bool isFunctionType() const { return Kind == TK_Function; }
  bool isArraryType() const { return Kind == TK_ConstantArray; }
  bool isScalarType() const;
  bool isArithmeticType() const;

  QualType getPointeeType() const;
  QualType getBaseElementType() const;
  const Type *getPointeeOrArrayElementTypePtr() const;
  QualType getPointeeOrArrayElementType() const;

  template <typename To> const To *getAs() const {
    if (const auto *Ty = dyn_cast<To>(this))
      return Ty;

    if (Kind != TK_Typedef)
      return nullptr;

    return dyn_cast<To>(getCanonicalType());
  }

private:
  TypeKind Kind;
  std::size_t Size = 0;
};

class BuiltinType : public Type {
public:
  enum Kind {
    BK_Int,
    BK_Char,
  };

  explicit BuiltinType(Kind BK, std::size_t Size)
      : Type(TK_Builtin, Size), BK(BK) {}

  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Builtin;
  }

  bool isIntegerType() const {
    return BK == BK_Char || BK == BK_Int;
  }

  Kind getKind() const { return BK; }
  const char *getKindStr() const;

private:
  Kind BK;
};

class PointerType : public Type {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Pointer;
  }

  explicit PointerType(QualType Pointee)
      : Type(TK_Pointer, 8), PointeeType(Pointee) {}

  QualType getPointeeType() const { return PointeeType; }

private:
  QualType PointeeType;
};

class FunctionType : public Type {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == TypeKind::TK_Function;
  }

  FunctionType(QualType RetType, std::vector<QualType> ParamTypes)
      : Type(TK_Function), RetType(RetType), ParamTypes(std::move(ParamTypes)) {
  }

  QualType getReturnType() const { return RetType; }

  unsigned getNumParams() const { return ParamTypes.size(); }
  QualType getParamType(unsigned Idx) const { return ParamTypes[Idx]; }
  const std::vector<QualType> &getParamTypes() const { return ParamTypes; }

private:
  // TODO: Function type details.
  QualType RetType;
  std::vector<QualType> ParamTypes;
};

class ArrayType : public Type {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_ConstantArray;
  }

  ArrayType(QualType ElementType, std::size_t Len)
      : Type(TK_ConstantArray, Len * ElementType->getSize()),
        ElementType(ElementType) {}

  QualType getElementType() const { return ElementType; }

private:
  QualType ElementType;
};

class ConstantArrayType : public ArrayType {
public:
  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_ConstantArray;
  }

  ConstantArrayType(QualType ElementType, std::size_t Len)
      : ArrayType(ElementType, Len), Len(Len) {}

  std::size_t getLength() const { return Len; }

private:
  std::size_t Len;
};

template <typename To> inline bool isa(QualType T) {
  return isa<To>(T.getTypePtr());
}

template <typename To> inline const To *dyn_cast(QualType T) {
  return isa<To>(T) ? static_cast<const To *>(T.getTypePtr()) : nullptr;
}

template <typename To> inline const To *cast(QualType T) {
  assert(isa<To>(T));
  return static_cast<const To *>(T.getTypePtr());
}

} // namespace rcc

#endif