#include "AST/Decl.h"
#include "AST/ASTContext.h"

namespace rcc {

VarDecl *VarDecl::create(ASTContext &Ctx, SourceLocation BegLoc,
                         SourceLocation EndLoc, std::string Name) {
  void *Mem = Ctx.Allocate(sizeof(VarDecl), alignof(VarDecl));
  return new (Mem) VarDecl(Ctx, BegLoc, EndLoc, std::move(Name));
}

VarDecl::VarDecl(ASTContext &Ctx, SourceLocation BegLoc, SourceLocation EndLoc,
                 std::string Name)
    : Decl(DK_Var, BegLoc, EndLoc), Ctx(Ctx), Name(std::move(Name)) {}

FunctionDecl *FunctionDecl::create(ASTContext &Ctx, SourceLocation BegLoc,
                                   SourceLocation EndLoc, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(FunctionDecl), alignof(FunctionDecl));
  return new (Mem) FunctionDecl(Ctx, BegLoc, EndLoc, Body);
}

FunctionDecl::FunctionDecl(ASTContext &Ctx, SourceLocation BegLoc,
                           SourceLocation EndLoc, Stmt *Body)
    : Decl(DK_Function, BegLoc, EndLoc), Ctx(Ctx), Body(Body) {}

void FunctionDecl::addLocalVar(VarDecl *Var) { LocalVars.push_back(Var); }

void FunctionDecl::setLocalVars(std::vector<VarDecl *> Vars) {
  LocalVars = std::move(Vars);
}

} // namespace rcc