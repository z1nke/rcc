#ifndef RCC_AST_DECL_H
#define RCC_AST_DECL_H

#include <string>

namespace rcc {

class ASTContext;

class Decl {
public:
  enum DeclKind {
    NoDeclKind = 0,
    DK_Var,
  };

  DeclKind getKind() const { return Kind; }

private:
  DeclKind Kind;
};

class VarDecl : public Decl {
public:
  static VarDecl *create(ASTContext &Ctx, std::string Name);

  static bool classof(const Decl *D) { return D->getKind() == DK_Var; }

  const std::string &getName() const { return Name; }

protected:
  VarDecl(ASTContext &Ctx, std::string Name);

private:
  ASTContext &Ctx;
  std::string Name;
};

} // namespace rcc

#endif