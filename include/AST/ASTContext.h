#ifndef RCC_AST_ASTCONTEXT_H
#define RCC_AST_ASTCONTEXT_H

#include "AST/Type.h"
#include "Basic/Diagnostic.h"
#include "Support/Allocator.h"

#include <unordered_map>
namespace rcc {

class ASTContext {
public:
  ASTContext(Diagnostic &Diag) : Diag(Diag), SM(Diag.getSourceManager()) {}

  SourceManager &getSourceManager() { return SM; }
  const SourceManager &getSourceManager() const { return SM; }

  void *allocate(size_t Size, unsigned Align = 8) const {
    return BumpAlloc.allocate(Size, Align);
  }

  void deallocate(void *Ptr) const {}
  Diagnostic &getDiagnostic() const { return Diag; }

public:
  using CanQualType = QualType;
  CanQualType VoidTy;
  CanQualType BoolTy;
  CanQualType CharTy;
  CanQualType ShortTy;
  CanQualType IntTy;
  CanQualType LongTy;
  CanQualType LongLongTy;

  void initBuiltinTypes();
  void initBuiltinType(CanQualType &R, BuiltinType::Kind Kind, std::size_t Size,
                       std::size_t Align);
  QualType getPointerType(QualType PointeeType);
  QualType getFunctionType(QualType RetType, std::vector<QualType> ParamTypes,
                           bool IsVariadic = false);
  QualType getConstantArrayType(QualType ElementType, std::size_t Len);
  QualType getIncompleteArrayType(QualType ElementType);
  QualType getRecordType(RecordDecl *RD, std::size_t Size, std::size_t Align = 0);
  QualType getEnumType(EnumDecl *ED);
  QualType getTypedefType(TypedefDecl *TD, QualType Underlying);
  QualType getArrayDecayedType(QualType Ty);

  bool hasSameType(QualType T1, QualType T2) const {
    return T1.getCanonicalType() == T2.getCanonicalType();
  }
  bool hasSameType(const Type *T1, const Type *T2) const {
    return T1->getCanonicalType() == T2->getCanonicalType();
  }

  int getIntTypeOrder(QualType LHS, QualType RHS) const;
  unsigned getIntRank(const Type *T) const;
  std::size_t getIntWidth(QualType T) const;

  std::vector<Type *> Types;
  std::unordered_map<void *, PointerType *> PointerTypes;
  std::unordered_map<const TagDecl *, TagType *> TagTypes;
  std::unordered_map<const TypedefDecl *, TypedefType *> TypedefTypes;

private:
  Diagnostic &Diag;
  SourceManager &SM;
  mutable BumpPtrAllocator BumpAlloc;
};

} // namespace rcc

inline void *operator new(size_t Size, const rcc::ASTContext &C,
                          size_t Alignment) {
  return C.allocate(Size, Alignment);
}

inline void operator delete(void *Ptr, const rcc::ASTContext &C, size_t) {
  C.deallocate(Ptr);
}

#endif