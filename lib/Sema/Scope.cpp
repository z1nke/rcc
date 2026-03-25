#include "Sema/Scope.h"

#include <cassert>

namespace rcc {

Scope::Scope(Scope *Parent, unsigned Flags, Decl *DeclCtx)
    : Parent(Parent), Depth(Parent ? Parent->Depth + 1 : 0), Flags(Flags),
      DeclCtx(DeclCtx) {}

void Scope::addDecl(Decl *D) { Decls.push_back(D); }
void Scope::addTag(TagDecl *D) { TagDecls.push_back(D); }

void Scope::setDeclContext(Decl *DeclCtx) {
  assert(!this->DeclCtx);
  this->DeclCtx = DeclCtx;
}

} // namespace rcc