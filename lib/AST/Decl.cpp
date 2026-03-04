#include "AST/Decl.h"
#include "AST/ASTContext.h"
#include "AST/ASTDumper.h"
#include "Support/Unreachable.h"

namespace rcc {

void Decl::dump() const {
  ASTDumper Dumper;
  Dumper.visit(this);
}

const char *Decl::getKindStr() const {
  switch (Kind) {
  case DK_TranslationUnit:
    return "TranslationUnitDecl";
  case DK_Var:
    return "VarDecl";
  case DK_Function:
    return "FunctionDecl";
  default:
    RCC_UNREACHABLE("Unknown decl kind");
  }
}

TranslationUnitDecl *TranslationUnitDecl::create(ASTContext &Ctx) {
  void *Mem =
      Ctx.allocate(sizeof(TranslationUnitDecl), alignof(TranslationUnitDecl));
  return new (Mem) TranslationUnitDecl(Ctx);
}

VarDecl *VarDecl::create(ASTContext &Ctx, SourceLocation Loc,
                         SourceLocation BegLoc, SourceLocation EndLoc,
                         QualType T, std::string Name) {
  void *Mem = Ctx.allocate(sizeof(VarDecl), alignof(VarDecl));
  return new (Mem) VarDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name));
}

VarDecl::VarDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
                 SourceLocation EndLoc, QualType T, std::string Name)
    : ValueDecl(Ctx, DK_Var, Loc, BegLoc, EndLoc, T, std::move(Name)) {}

ParamVarDecl::ParamVarDecl(ASTContext &Ctx, SourceLocation Loc,
                           SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::string Name, unsigned Index)
    : VarDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name)), Index(Index) {}

ParamVarDecl *ParamVarDecl::create(ASTContext &Ctx, SourceLocation Loc,
                                   SourceLocation BegLoc, SourceLocation EndLoc,
                                   QualType T, std::string Name,
                                   unsigned Index) {
  void *Mem = Ctx.allocate(sizeof(ParamVarDecl), alignof(ParamVarDecl));
  return new (Mem)
      ParamVarDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name), Index);
}

FunctionDecl *FunctionDecl::create(ASTContext &Ctx, SourceLocation Loc,
                                   SourceLocation BegLoc, SourceLocation EndLoc,
                                   QualType T, std::string Name, Stmt *Body) {
  void *Mem = Ctx.allocate(sizeof(FunctionDecl), alignof(FunctionDecl));
  return new (Mem)
      FunctionDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name), Body);
}

FunctionDecl::FunctionDecl(ASTContext &Ctx, SourceLocation Loc,
                           SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::string Name, Stmt *Body)
    : ValueDecl(Ctx, DK_Function, Loc, BegLoc, EndLoc, T, std::move(Name)),
      Body(Body) {}

void FunctionDecl::setLocalVars(std::vector<VarDecl *> Vars) {
  LocalVars = std::move(Vars);
}

FieldDecl::FieldDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
                     SourceLocation EndLoc, QualType T, std::string Name,
                     RecordDecl *Parent)
    : ValueDecl(Ctx, DK_Field, Loc, BegLoc, EndLoc, T, std::move(Name)),
      Parent(Parent) {}

FieldDecl *FieldDecl::create(ASTContext &Ctx, SourceLocation Loc,
                             SourceLocation BegLoc, SourceLocation EndLoc,
                             QualType T, std::string Name, RecordDecl *Parent) {
  void *Mem = Ctx.allocate(sizeof(FieldDecl), alignof(FieldDecl));
  return new (Mem)
      FieldDecl(Ctx, Loc, BegLoc, EndLoc, T, std::move(Name), Parent);
}

RecordDecl *RecordDecl::create(ASTContext &Ctx, SourceLocation Loc,
                               SourceLocation BegLoc, SourceLocation EndLoc,
                               std::string Name, TagDecl::TagKind TK) {
  void *Mem = Ctx.allocate(sizeof(RecordDecl), alignof(RecordDecl));
  return new (Mem) RecordDecl(Ctx, Loc, BegLoc, EndLoc, std::move(Name), TK);
}

RecordDecl::RecordDecl(ASTContext &Ctx, SourceLocation Loc,
                       SourceLocation BegLoc, SourceLocation EndLoc,
                       std::string Name, TagDecl::TagKind TK)
    : TagDecl(Ctx, DK_Record, TK, Loc, BegLoc, EndLoc, std::move(Name)) {}

} // namespace rcc