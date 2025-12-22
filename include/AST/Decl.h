#ifndef RCC_AST_DECL_H
#define RCC_AST_DECL_H

#include "AST/Type.h"
#include "Basic/SourceLocation.h"

#include <string>
#include <vector>

namespace rcc {

class Token;
class ASTContext;
class Stmt;
class Expr;

class Decl {
public:
  enum DeclKind {
    NoDeclKind = 0,
    DK_TranslationUnit,
    DK_Var,
    DK_Function,
  };

  Decl(DeclKind Kind, SourceLocation Loc, SourceLocation BegLoc,
       SourceLocation EndLoc)
      : Kind(Kind), Loc(Loc), BegLoc(BegLoc), EndLoc(EndLoc) {}

  DeclKind getKind() const { return Kind; }

  SourceLocation getLocation() const { return Loc; }
  SourceLocation getBeginLoc() const { return BegLoc; }
  SourceLocation getEndLoc() const { return EndLoc; }

  void setBeginLoc(SourceLocation BegLoc) { this->BegLoc = BegLoc; }
  void setEndLoc(SourceLocation EndLoc) { this->EndLoc = EndLoc; }

  bool isImplicit() const { return IsImplicit; }
  void setImplicit(bool Val) { IsImplicit = Val; }

private:
  DeclKind Kind;
  SourceLocation Loc;
  SourceLocation BegLoc;
  SourceLocation EndLoc;
  bool IsImplicit = false;
};

class TranslationUnitDecl : public Decl {
public:
  static TranslationUnitDecl *create(ASTContext &Ctx);

  static bool classof(const Decl *D) {
    return D->getKind() == DK_TranslationUnit;
  }

  const std::vector<Decl *> &decls() const { return Decls; }
  void addDecl(Decl *D) { Decls.push_back(D); }

private:
  TranslationUnitDecl()
      : Decl(DK_TranslationUnit, SourceLocation(), SourceLocation(),
             SourceLocation()) {}

  std::vector<Decl *> Decls;
};

class NamedDecl : public Decl {
public:
  NamedDecl(DeclKind Kind, SourceLocation Loc, SourceLocation BegLoc,
            SourceLocation EndLoc, std::string Name)
      : Decl(Kind, Loc, BegLoc, EndLoc), Name(std::move(Name)) {}

  const std::string &getName() const { return Name; }
  void setName(std::string Name) { this->Name = std::move(Name); }

private:
  std::string Name;
};

class ValueDecl : public NamedDecl {
public:
  ValueDecl(DeclKind Kind, SourceLocation Loc, SourceLocation BegLoc,
            SourceLocation EndLoc, QualType T, std::string Name)
      : NamedDecl(Kind, Loc, BegLoc, EndLoc, std::move(Name)), Ty(T) {}

  QualType getType() const { return Ty; }
  void setType(QualType T) { Ty = T; }

private:
  QualType Ty;
};

class VarDecl : public ValueDecl {
public:
  static VarDecl *create(ASTContext &Ctx, SourceLocation Loc,
                         SourceLocation BegLoc, SourceLocation EndLoc,
                         QualType T, std::string Name);

  static bool classof(const Decl *D) { return D->getKind() == DK_Var; }

  // Temporary handling.
  int getOffset() const { return Offset; }
  void setOffset(int Offset) { this->Offset = Offset; }

  const Expr *getInit() const { return Init; }
  Expr *getInit() { return Init; }
  void setInit(Expr *E) { Init = E; }

protected:
  VarDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
          SourceLocation EndLoc, QualType T, std::string Name);

private:
  ASTContext &Ctx;
  std::string Name;
  Expr *Init = nullptr;
  // Temporary handling.
  int Offset = 0;
};

class FunctionDecl : public ValueDecl {
public:
  static FunctionDecl *create(ASTContext &Ctx, SourceLocation Loc,
                              SourceLocation BegLoc, SourceLocation EndLoc,
                              QualType T, std::string Name, Stmt *Body);

  static bool classof(const Decl *D) { return D->getKind() == DK_Function; }

  Stmt *getBody() const { return Body; }
  void setBody(Stmt *B) { Body = B; }

  void setLocalVars(std::vector<VarDecl *> Vars);

  const std::vector<VarDecl *> &getLocalVars() const { return LocalVars; }

protected:
  FunctionDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
               SourceLocation EndLoc, QualType T, std::string Name, Stmt *Body);

private:
  ASTContext &Ctx;
  Stmt *Body = nullptr;
  std::vector<VarDecl *> LocalVars; // Temporary handling.
};

} // namespace rcc

#endif