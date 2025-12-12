#ifndef RCC_AST_DECL_H
#define RCC_AST_DECL_H

#include <string>
#include <vector>

namespace rcc {

class ASTContext;
class Stmt;

class Decl {
public:
  enum DeclKind {
    NoDeclKind = 0,
    DK_Var,
    DK_Function,
  };

  Decl(DeclKind Kind) : Kind(Kind) {}

  DeclKind getKind() const { return Kind; }

private:
  DeclKind Kind;
};

class VarDecl : public Decl {
public:
  static VarDecl *create(ASTContext &Ctx, std::string Name);

  static bool classof(const Decl *D) { return D->getKind() == DK_Var; }

  const std::string &getName() const { return Name; }

  // Temporary handling.
  int getOffset() const { return Offset; }
  void setOffset(int Offset) { this->Offset = Offset; }

protected:
  VarDecl(ASTContext &Ctx, std::string Name);

private:
  ASTContext &Ctx;
  std::string Name;
  // Temporary handling.
  int Offset = 0;
};

class FunctionDecl : public Decl {
public:
  static FunctionDecl *create(ASTContext &Ctx, Stmt *Body);

  Stmt *getBody() const { return Body; }

  void addLocalVar(VarDecl *Var);

  void setLocalVars(std::vector<VarDecl *> Vars);

  const std::vector<VarDecl *> &getLocalVars() const { return LocalVars; }

protected:
  FunctionDecl(ASTContext &Ctx, Stmt *Body);

private:
  ASTContext &Ctx;
  Stmt *Body;
  std::vector<VarDecl *> LocalVars; // Temporary handling.
};

} // namespace rcc

#endif