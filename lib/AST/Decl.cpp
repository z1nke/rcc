#include "AST/Decl.h"
#include "AST/ASTContext.h"

namespace rcc {

VarDecl *VarDecl::create(ASTContext &Ctx, SourceLocation Loc,
                         SourceLocation BegLoc, SourceLocation EndLoc,
                         QualType T, std::string Name) {
  void *Mem = Ctx.Allocate(sizeof(VarDecl), alignof(VarDecl));
  return new (Mem) VarDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name));
}

VarDecl::VarDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
                 SourceLocation EndLoc, QualType T, std::string Name)
    : ValueDecl(DK_Var, Loc, BegLoc, EndLoc, T), Ctx(Ctx),
      Name(std::move(Name)) {}

FunctionDecl *FunctionDecl::create(ASTContext &Ctx, SourceLocation Loc,
                                   SourceLocation BegLoc, SourceLocation EndLoc,
                                   QualType T, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(FunctionDecl), alignof(FunctionDecl));
  return new (Mem) FunctionDecl(Ctx, Loc, BegLoc, EndLoc, T, Body);
}

FunctionDecl::FunctionDecl(ASTContext &Ctx, SourceLocation Loc,
                           SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, Stmt *Body)
    : ValueDecl(DK_Function, Loc, BegLoc, EndLoc, T), Ctx(Ctx), Body(Body) {}

void FunctionDecl::addLocalVar(VarDecl *Var) { LocalVars.push_back(Var); }

void FunctionDecl::setLocalVars(std::vector<VarDecl *> Vars) {
  LocalVars = std::move(Vars);
}

} // namespace rcc