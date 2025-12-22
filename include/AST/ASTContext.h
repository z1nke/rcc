#ifndef RCC_AST_ASTCONTEXT_H
#define RCC_AST_ASTCONTEXT_H

#include "AST/Type.h"
#include "Basic/Allocator.h"
#include "Basic/Diagnostic.h"

#include <unordered_map>
namespace rcc {

class ASTContext {
public:
  ASTContext(Diagnostic &Diag) : Diag(Diag), SM(Diag.getSourceManager()) {}

  SourceManager &getSourceManager() { return SM; }
  const SourceManager &getSourceManager() const { return SM; }

  void *Allocate(size_t Size, unsigned Align = 8) const {
    return BumpAlloc.Allocate(Size, Align);
  }

  void Deallocate(void *Ptr) const {}

  Diagnostic &getDiagnostic() const { return Diag; }

public:
  using CanQualType = QualType;
  CanQualType IntTy;

  void initBuiltinTypes();
  void initBuiltinType(CanQualType &R, BuiltinType::Kind Kind);
  QualType getPointerType(QualType PointeeType);
  QualType getFunctionType(QualType RetType);

  std::vector<Type *> Types;
  std::unordered_map<void *, PointerType *> PointerTypes;

private:
  Diagnostic &Diag;
  SourceManager &SM;
  mutable BumpPtrAllocator BumpAlloc;
};

} // namespace rcc

inline void *operator new(size_t Size, const rcc::ASTContext &C,
                          size_t Alignment) {
  return C.Allocate(Size, Alignment);
}

inline void operator delete(void *Ptr, const rcc::ASTContext &C, size_t) {
  C.Deallocate(Ptr);
}

#endif