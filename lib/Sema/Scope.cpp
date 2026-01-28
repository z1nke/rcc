#include "Sema/Scope.h"

namespace rcc {

Scope::Scope(Scope *Parent, unsigned Flags)
    : Parent(Parent), Depth(Parent ? Parent->Depth + 1 : 0), Flags(Flags) {}

void Scope::addDecl(Decl *D) { Decls.insert(D); }
void Scope::removeDecl(Decl *D) { Decls.erase(D); }

} // namespace rcc