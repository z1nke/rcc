#ifndef RCC_AST_ASTCONTEXT_H
#define RCC_AST_ASTCONTEXT_H

#include "Basic/Allocator.h"
#include "Basic/Diagnostic.h"

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

private:
  Diagnostic &Diag;
  SourceManager &SM;
  mutable BumpPtrAllocator BumpAlloc;
};

} // namespace rcc

#endif