#include "AST/Decl.h"
#include "AST/ASTContext.h"

namespace rcc {

TranslationUnitDecl *TranslationUnitDecl::create(ASTContext &Ctx) {
  void *Mem =
      Ctx.Allocate(sizeof(TranslationUnitDecl), alignof(TranslationUnitDecl));
  return new (Mem) TranslationUnitDecl();
}

VarDecl *VarDecl::create(ASTContext &Ctx, SourceLocation Loc,
                         SourceLocation BegLoc, SourceLocation EndLoc,
                         QualType T, std::string Name) {
  void *Mem = Ctx.Allocate(sizeof(VarDecl), alignof(VarDecl));
  return new (Mem) VarDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name));
}

VarDecl::VarDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
                 SourceLocation EndLoc, QualType T, std::string Name)
    : ValueDecl(DK_Var, Loc, BegLoc, EndLoc, T, std::move(Name)), Ctx(Ctx) {}

FunctionDecl *FunctionDecl::create(ASTContext &Ctx, SourceLocation Loc,
                                   SourceLocation BegLoc, SourceLocation EndLoc,
                                   QualType T, std::string Name, Stmt *Body) {
  void *Mem = Ctx.Allocate(sizeof(FunctionDecl), alignof(FunctionDecl));
  return new (Mem)
      FunctionDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name), Body);
}

FunctionDecl::FunctionDecl(ASTContext &Ctx, SourceLocation Loc,
                           SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::string Name, Stmt *Body)
    : ValueDecl(DK_Function, Loc, BegLoc, EndLoc, T, std::move(Name)), Ctx(Ctx),
      Body(Body) {}

void FunctionDecl::setLocalVars(std::vector<VarDecl *> Vars) {
  LocalVars = std::move(Vars);
}

} // namespace rcc