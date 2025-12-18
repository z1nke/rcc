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

class Decl {
public:
  enum DeclKind {
    NoDeclKind = 0,
    DK_Var,
    DK_Function,
  };

  Decl(DeclKind Kind, SourceLocation BegLoc, SourceLocation EndLoc)
      : Kind(Kind), BegLoc(BegLoc), EndLoc(EndLoc) {}

  DeclKind getKind() const { return Kind; }

private:
  DeclKind Kind;
  SourceLocation BegLoc;
  SourceLocation EndLoc;
};

class ValueDecl : public Decl {
public:
  ValueDecl(DeclKind Kind, SourceLocation BegLoc, SourceLocation EndLoc,
            QualType T)
      : Decl(Kind, BegLoc, EndLoc), Ty(T) {}

  QualType getType() const { return Ty; }
  void setType(QualType T) { Ty = T; }

private:
  QualType Ty;
};

class VarDecl : public ValueDecl {
public:
  static VarDecl *create(ASTContext &Ctx, SourceLocation BegLoc,
                         SourceLocation EndLoc, QualType T, std::string Name);

  static bool classof(const Decl *D) { return D->getKind() == DK_Var; }

  const std::string &getName() const { return Name; }

  // Temporary handling.
  int getOffset() const { return Offset; }
  void setOffset(int Offset) { this->Offset = Offset; }

protected:
  VarDecl(ASTContext &Ctx, SourceLocation BegLoc, SourceLocation EndLoc,
          QualType T, std::string Name);

private:
  ASTContext &Ctx;
  std::string Name;
  // Temporary handling.
  int Offset = 0;
};

class FunctionDecl : public ValueDecl {
public:
  static FunctionDecl *create(ASTContext &Ctx, SourceLocation BegLoc,
                              SourceLocation EndLoc, QualType T, Stmt *Body);

  Stmt *getBody() const { return Body; }

  void addLocalVar(VarDecl *Var);

  void setLocalVars(std::vector<VarDecl *> Vars);

  const std::vector<VarDecl *> &getLocalVars() const { return LocalVars; }

protected:
  FunctionDecl(ASTContext &Ctx, SourceLocation BegLoc, SourceLocation EndLoc,
               QualType T, Stmt *Body);

private:
  ASTContext &Ctx;
  Stmt *Body;
  std::vector<VarDecl *> LocalVars; // Temporary handling.
};

} // namespace rcc

#endif