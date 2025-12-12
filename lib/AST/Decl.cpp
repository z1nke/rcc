#include "AST/Decl.h"
#include "AST/ASTContext.h"

namespace rcc {

VarDecl *VarDecl::create(ASTContext &Ctx, std::string Name) {
  void *Mem = Ctx.Allocate(sizeof(VarDecl), alignof(VarDecl));
  return new (Mem) VarDecl(Ctx, std::move(Name));
}

VarDecl::VarDecl(ASTContext &Ctx, std::string Name)
    : Decl(DK_Var), Ctx(Ctx), Name(std::move(Name)) {}

FunctionDecl *FunctionDecl::create(ASTContext &Ctx, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(FunctionDecl), alignof(FunctionDecl));
  return new (Mem) FunctionDecl(Ctx, Body);
}

FunctionDecl::FunctionDecl(ASTContext &Ctx, Stmt *Body)
    : Decl(DK_Function), Ctx(Ctx), Body(Body) {}

void FunctionDecl::addLocalVar(VarDecl *Var) { LocalVars.push_back(Var); }

void FunctionDecl::setLocalVars(std::vector<VarDecl *> Vars) {
  LocalVars = std::move(Vars);
}

} // namespace rcc