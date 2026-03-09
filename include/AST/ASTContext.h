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
  CanQualType IntTy;
  CanQualType CharTy;
  CanQualType LongTy;

  void initBuiltinTypes();
  void initBuiltinType(CanQualType &R, BuiltinType::Kind Kind,
                       std::size_t Size, std::size_t Align);
  QualType getPointerType(QualType PointeeType);
  QualType getFunctionType(QualType RetType, std::vector<QualType> ParamTypes);
  QualType getConstantArrayType(QualType ElementType, std::size_t Len);
  QualType getRecordType(RecordDecl *RD, std::size_t Size,
                         std::size_t Align = 0);

  std::vector<Type *> Types;
  std::unordered_map<void *, PointerType *> PointerTypes;
  std::unordered_map<const RecordDecl *, RecordType *> RecordTypes;

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