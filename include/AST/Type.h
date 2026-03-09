#ifndef RCC_AST_TYPE_H
#define RCC_AST_TYPE_H

#include "Support/Casting.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rcc {

class QualType;
class Type;
class TagDecl;
class RecordDecl;

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

  std::string getAsString() const;

  const Type &operator*() const { return *getTypePtr(); }
  const Type *operator->() const { return getTypePtr(); }

  QualType getCanonicalType() const;
  bool isIntegerType() const;

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
    TK_Record,
    TK_Enum,
  };

  Type &operator=(const Type &) = delete;
  Type &operator=(Type &&) = delete;

  void dump() const;

  TypeKind getTypeKind() const { return Kind; }
  std::size_t getSize() const { return Size; }
  std::size_t getAlign() const { return Align; }
  void setAlign(std::size_t Align) { this->Align = Align; }

  QualType getCanonicalType() const;

  bool isScalarType() const;
  bool isArithmeticType() const;
  bool isPointerType() const { return Kind == TK_Pointer; }
  bool isFunctionType() const { return Kind == TK_Function; }
  bool isArraryType() const { return Kind == TK_ConstantArray; }
  bool isRecordType() const { return Kind == TK_Record; }

  QualType getPointeeType() const;
  QualType getBaseElementType() const;
  const Type *getPointeeOrArrayElementTypePtr() const;
  QualType getPointeeOrArrayElementType() const;

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
    BK_Int,
    BK_Char,
  };

  static bool classof(const Type *T) {
    return T->getTypeKind() == Type::TK_Builtin;
  }

  bool isIntegerType() const { return BK == BK_Char || BK == BK_Int; }

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

private:
  friend class ASTContext;

  FunctionType(QualType RetType, std::vector<QualType> ParamTypes)
      : Type(TK_Function, 0), RetType(RetType),
        ParamTypes(std::move(ParamTypes)) {}

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

  QualType getElementType() const { return ElementType; }

protected:
  friend class ASTContext;

  ArrayType(QualType ElementType, std::size_t Len)
      : Type(TK_ConstantArray, Len * ElementType->getSize(),
             ElementType->getAlign()),
        ElementType(ElementType) {}

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
      : ArrayType(ElementType, Len), Len(Len) {}

private:
  std::size_t Len;
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