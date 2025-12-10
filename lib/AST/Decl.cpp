#include "AST/Decl.h"
#include "AST/ASTContext.h"

namespace rcc {

VarDecl *VarDecl::create(ASTContext &Ctx, std::string Name) {
  void *Mem = Ctx.Allocate(sizeof(VarDecl), alignof(VarDecl));
  return new (Mem) VarDecl(Ctx, std::move(Name));
}

VarDecl::VarDecl(ASTContext &Ctx, std::string Name)
    : Ctx(Ctx), Name(std::move(Name)) {}

} // namespace rcc