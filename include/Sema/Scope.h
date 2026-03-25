#ifndef RCC_SEMA_SCOPE_H
#define RCC_SEMA_SCOPE_H

#include <vector>

namespace rcc {

class Decl;
class TagDecl;

class Scope {
public:
  enum ScopeFlags {
    NoScope = 0x0,
    // Function scope.
    FnScope = 0x1,

    // Decl scope: can contain a declaration.
    DeclScope = 0x2,

    // Break scope: while, do, switch, for, etc that can have break statements
    // embedded into it.
    BreakScope = 0x4,

    // Continue scope: while, do, for, which can have continue statements
    // embedded into it.
    ContinueScope = 0x8,

    // Control scope: in a if/switch/while/for statement.
    ControlScope = 0x10,

    // Switch scope: in a switch statement.
    SwitchScope = 0x20,

    // Block scope: in a stmtexpr.
    BlockScope = 0x40,

    // Compound scope: in a compound statement.
    CompoundScope = 0x80,

    // Struct scope: in a struct declaration.
    StructScope = 0x100,
  };

  Scope(Scope *Parent, unsigned Flags, Decl *DeclCtx = nullptr);

  void addDecl(Decl *D);
  void removeDecl(Decl *D);
  void addTag(TagDecl *D);
  void removeTag(TagDecl *D);

  unsigned getDepth() const { return Depth; }
  Scope *getParent() const { return Parent; }
  unsigned getFlags() const { return Flags; }
  const auto &decls() const { return Decls; }
  const auto &tags() const { return TagDecls; }

  bool isStructScope() const { return Flags & StructScope; }
  bool isFunctionScope() const { return Flags & FnScope; }

  const Decl *getDeclContext() const { return DeclCtx; }
  Decl *getDeclContext() { return DeclCtx; }
  void setDeclContext(Decl *DeclCtx);

private:
  std::vector<Decl *> Decls;
  std::vector<TagDecl *> TagDecls;
  Scope *Parent = nullptr;
  // The depth of translation unit is 0.
  unsigned Depth;
  unsigned Flags;
  Decl *DeclCtx = nullptr;
};

} // namespace rcc

#endif