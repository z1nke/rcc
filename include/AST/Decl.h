#ifndef RCC_AST_DECL_H
#define RCC_AST_DECL_H

#include "AST/Type.h"
#include "Basic/Linkage.h"
#include "Basic/SourceLocation.h"

#include <string>
#include <vector>

namespace rcc {

class Token;
class ASTContext;
class Stmt;
class Expr;
class LabelStmt;

class Decl {
public:
  enum DeclKind {
    NoDeclKind = 0,
    DK_TranslationUnit,
    DK_Var,
    DK_Function,
    DK_EnumConstant,
    DK_Field,
    DK_Label,
    DK_Record,
    DK_Enum,
    DK_Typedef,
    FirstTypeDeclKind = DK_Record,
    LastTypeDeclKind = DK_Typedef,
    FirstTagDeclKind = DK_Record,
    LastTagDeclKind = DK_Enum,
    FirstNamedDeclKind = DK_Var,
    LastNamedDeclKind = DK_Typedef,
    FirstValueDeclKind = DK_Var,
    LastValueDeclKind = DK_Field,
  };

  Decl(ASTContext &Ctx, DeclKind Kind, SourceLocation Loc,
       SourceLocation BegLoc, SourceLocation EndLoc)
      : Ctx(Ctx), Kind(Kind), Loc(Loc), BegLoc(BegLoc), EndLoc(EndLoc) {}

  ASTContext &getContext() const { return Ctx; }

  DeclKind getKind() const { return Kind; }
  const char *getKindStr() const;

  void dump() const;

  SourceLocation getLocation() const { return Loc; }
  SourceLocation getBeginLoc() const { return BegLoc; }
  SourceLocation getEndLoc() const { return EndLoc; }

  void setBeginLoc(SourceLocation BegLoc) { this->BegLoc = BegLoc; }
  void setEndLoc(SourceLocation EndLoc) { this->EndLoc = EndLoc; }

  bool isImplicit() const { return IsImplicit; }
  void setImplicit(bool Val) { IsImplicit = Val; }

  Decl *getCanonicalDecl() { return this; }
  const Decl *getCanonicalDecl() const { return this; }

private:
  ASTContext &Ctx;
  DeclKind Kind;
  SourceLocation Loc;
  SourceLocation BegLoc;
  SourceLocation EndLoc;
  bool IsImplicit = false;
};

class TranslationUnitDecl final : public Decl {
public:
  static TranslationUnitDecl *create(ASTContext &Ctx);

  static bool classof(const Decl *D) {
    return D->getKind() == DK_TranslationUnit;
  }

  const std::vector<Decl *> &decls() const { return Decls; }
  void addDecl(Decl *D) { Decls.push_back(D); }

private:
  TranslationUnitDecl(ASTContext &Ctx)
      : Decl(Ctx, DK_TranslationUnit, SourceLocation(), SourceLocation(),
             SourceLocation()) {}

  std::vector<Decl *> Decls;
};

class NamedDecl : public Decl {
public:
  NamedDecl(ASTContext &Ctx, DeclKind Kind, SourceLocation Loc,
            SourceLocation BegLoc, SourceLocation EndLoc, std::string Name)
      : Decl(Ctx, Kind, Loc, BegLoc, EndLoc), Name(std::move(Name)) {}

  static bool classof(const Decl *D) { return classofKind(D->getKind()); }
  static bool classofKind(DeclKind DK) {
    return DK >= FirstNamedDeclKind && DK <= LastNamedDeclKind;
  }

  const std::string &getName() const { return Name; }
  void setName(std::string Name) { this->Name = std::move(Name); }

  Linkage getLinkage() const { return Lnk; }
  void setLinkage(Linkage L) { Lnk = L; }

private:
  std::string Name;
  Linkage Lnk = Linkage::NoLinkage;
};

class ValueDecl : public NamedDecl {
public:
  ValueDecl(ASTContext &Ctx, DeclKind Kind, SourceLocation Loc,
            SourceLocation BegLoc, SourceLocation EndLoc, QualType T,
            std::string Name)
      : NamedDecl(Ctx, Kind, Loc, BegLoc, EndLoc, std::move(Name)), Ty(T) {}

  static bool classof(const Decl *D) { return classofKind(D->getKind()); }
  static bool classofKind(DeclKind DK) {
    return DK >= FirstValueDeclKind && DK <= LastValueDeclKind;
  }

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

  /// True when a 9–16 byte struct is split: first 8 bytes in a GP register,
  /// the rest on the caller's stack (RISC-V LP64 ABI).
  bool isHalfByStack() const { return IsHalfByStack; }
  void setHalfByStack(bool V = true) { IsHalfByStack = V; }

  const Expr *getInit() const { return Init; }
  Expr *getInit() { return Init; }
  void setInit(Expr *E) { Init = E; }
  void setGlobalStorage(bool IsGlobalStorage) {
    this->IsGlobalStorage = IsGlobalStorage;
  }
  bool hasGlobalStorage() const { return IsGlobalStorage; }
  bool isStaticLocal() const { return IsStaticLocal; }
  void setStaticLocal(bool IsStaticLocal = true) {
    this->IsStaticLocal = IsStaticLocal;
  }

  bool isDefinition() const { return IsDefinition; }
  void setIsDefinition(bool IsDefinition = true) {
    this->IsDefinition = IsDefinition;
  }

  std::size_t getAlign() const { return Align; }
  void setAlign(std::size_t Align) { this->Align = Align; }

protected:
  VarDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
          SourceLocation EndLoc, QualType T, std::string Name);

private:
  Expr *Init = nullptr;
  // Temporary handling.
  int Offset = 0;
  bool IsHalfByStack = false;
  bool IsGlobalStorage = false;
  bool IsStaticLocal = false;
  bool IsDefinition = true;
  std::size_t Align = 0;
};

class ParamVarDecl final : public VarDecl {
public:
  static ParamVarDecl *create(ASTContext &Ctx, SourceLocation Loc,
                              SourceLocation BegLoc, SourceLocation EndLoc,
                              QualType T, std::string Name, unsigned Index);

private:
  ParamVarDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
               SourceLocation EndLoc, QualType T, std::string Name,
               unsigned Index);

  unsigned getIndex() const { return Index; }

private:
  unsigned Index = 0;
};

class FunctionDecl final : public ValueDecl {
public:
  static FunctionDecl *create(ASTContext &Ctx, SourceLocation Loc,
                              SourceLocation BegLoc, SourceLocation EndLoc,
                              QualType T, std::string Name, Stmt *Body);

  static bool classof(const Decl *D) { return D->getKind() == DK_Function; }

  Stmt *getBody() const { return Body; }
  void setBody(Stmt *B) { Body = B; }

  void setLocalVars(std::vector<VarDecl *> Vars);
  const std::vector<VarDecl *> &getLocalVars() const { return LocalVars; }

  void setParams(std::vector<ParamVarDecl *> Params) {
    this->Params = std::move(Params);
  }
  const std::vector<ParamVarDecl *> &getParams() const { return Params; }
  unsigned getNumParams() const { return Params.size(); }
  const ParamVarDecl *getParam(unsigned Index) const { return Params[Index]; }

protected:
  FunctionDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
               SourceLocation EndLoc, QualType T, std::string Name, Stmt *Body);

private:
  Stmt *Body = nullptr;
  std::vector<ParamVarDecl *> Params;
  std::vector<VarDecl *> LocalVars; // Temporary handling.
};

class RecordDecl;

class FieldDecl final : public ValueDecl {
public:
  static FieldDecl *create(ASTContext &Ctx, SourceLocation Loc,
                           SourceLocation BegLoc, SourceLocation EndLoc,
                           QualType T, std::string Name, RecordDecl *Parent);

  static bool classof(const Decl *D) { return D->getKind() == DK_Field; }

  int getOffset() const { return Offset; }
  void setOffset(int Offset) { this->Offset = Offset; }

  std::size_t getAlign() const { return Align; }
  void setAlign(std::size_t Align) { this->Align = Align; }

  RecordDecl *getParent() const { return Parent; }

private:
  FieldDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
            SourceLocation EndLoc, QualType T, std::string Name,
            RecordDecl *Parent);

private:
  int Offset = 0;
  std::size_t Align = 0;
  RecordDecl *Parent = nullptr; // Temporary handling.
};

class LabelDecl final : public NamedDecl {
public:
  static LabelDecl *create(ASTContext &Ctx, SourceLocation Loc,
                           SourceLocation BegLoc, SourceLocation EndLoc,
                           std::string Name, LabelStmt *Stmt = nullptr);

  static bool classof(const Decl *D) { return D->getKind() == DK_Label; }

  LabelStmt *getStmt() const { return StmtNode; }
  void setStmt(LabelStmt *S) { StmtNode = S; }

private:
  LabelDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
            SourceLocation EndLoc, std::string Name, LabelStmt *Stmt);

private:
  LabelStmt *StmtNode = nullptr;
};

class TypeDecl : public NamedDecl {
public:
  const Type *getTypeForDecl() const { return Ty; }
  void setTypeForDecl(const Type *Ty) { this->Ty = Ty; }

  static bool classof(const Decl *D) { return classofKind(D->getKind()); }

  static bool classofKind(DeclKind DK) {
    return DK >= FirstTypeDeclKind && DK <= LastTypeDeclKind;
  }

  QualType getType() const { return QualType(Ty, 0); }

protected:
  TypeDecl(ASTContext &Ctx, DeclKind Kind, SourceLocation Loc,
           SourceLocation BegLoc, SourceLocation EndLoc, std::string Name)
      : NamedDecl(Ctx, Kind, Loc, BegLoc, EndLoc, std::move(Name)) {}

private:
  const Type *Ty = nullptr;
};

class TagDecl : public TypeDecl {
public:
  enum TagKind : std::uint8_t {
    TK_Struct,
    TK_Union,
    TK_Enum,
  };

  static bool classof(const Decl *D) { return classofKind(D->getKind()); }
  static bool classofKind(DeclKind DK) {
    return DK >= FirstTagDeclKind && DK <= LastTagDeclKind;
  }

  TagKind getTagKind() const { return TK; }
  void setTagKind(TagKind TK) { this->TK = TK; }

  bool isStruct() const { return TK == TK_Struct; }
  bool isUnion() const { return TK == TK_Union; }
  bool isEnum() const { return TK == TK_Enum; }

  TagDecl *getCanonicalDecl() { return CanonicalDecl; }
  const TagDecl *getCanonicalDecl() const { return CanonicalDecl; }
  void setCanonicalDecl(TagDecl *D) { CanonicalDecl = D; }

  bool isCompleteDefinition() const { return Definition == this; }
  TagDecl *getDefinition() { return Definition; }
  const TagDecl *getDefinition() const { return Definition; }
  bool hasDefinition() const { return Definition; }
  void setDefinition(TagDecl *D) { Definition = D; }

protected:
  TagDecl(ASTContext &Ctx, DeclKind DK, TagKind TK, SourceLocation Loc,
          SourceLocation BegLoc, SourceLocation EndLoc, std::string Name)
      : TypeDecl(Ctx, DK, Loc, BegLoc, EndLoc, std::move(Name)), TK(TK) {}

private:
  TagKind TK;
  TagDecl *CanonicalDecl = this;
  TagDecl *Definition = nullptr;
};

class RecordDecl final : public TagDecl {
public:
  static RecordDecl *create(ASTContext &Ctx, SourceLocation Loc,
                            SourceLocation BegLoc, SourceLocation EndLoc,
                            std::string Name, TagKind TK);

  static bool classof(const Decl *D) { return D->getKind() == DK_Record; }

  const std::vector<FieldDecl *> &fields() const;
  void addField(FieldDecl *D) { Fields.push_back(D); }
  void setFields(std::vector<FieldDecl *> Fields) {
    this->Fields = std::move(Fields);
  }

private:
  RecordDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
             SourceLocation EndLoc, std::string Name, TagKind TK);

private:
  std::vector<FieldDecl *> Fields;
};

class EnumConstantDecl final : public ValueDecl {
public:
  static EnumConstantDecl *create(ASTContext &Ctx, SourceLocation Loc,
                                  SourceLocation BegLoc, SourceLocation EndLoc,
                                  QualType T, std::string Name,
                                  std::int64_t Val, const Expr *Init);

  static bool classof(const Decl *D) { return D->getKind() == DK_EnumConstant; }

  int getValue() const { return Val; }
  const Expr *getInit() const { return Init; }
  Expr *getInit() { return Init; }

private:
  EnumConstantDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
                   SourceLocation EndLoc, QualType T, std::string Name,
                   std::int64_t Val, const Expr *Init);

private:
  std::int64_t Val;
  Expr *Init;
};

class EnumDecl final : public TagDecl {
public:
  static EnumDecl *create(ASTContext &Ctx, SourceLocation Loc,
                          SourceLocation BegLoc, SourceLocation EndLoc,
                          std::string Name);

  static bool classof(const Decl *D) { return D->getKind() == DK_Enum; }

  const std::vector<EnumConstantDecl *> &enumerators() const {
    return EnumConstants;
  }
  void addEnumerator(EnumConstantDecl *D) { EnumConstants.push_back(D); }
  void setEnumerators(std::vector<EnumConstantDecl *> EnumConstants) {
    this->EnumConstants = std::move(EnumConstants);
  }

private:
  EnumDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
           SourceLocation EndLoc, std::string Name);

  std::vector<EnumConstantDecl *> EnumConstants;
};

class TypedefDecl final : public TypeDecl {
public:
  static TypedefDecl *create(ASTContext &Ctx, SourceLocation Loc,
                             SourceLocation BegLoc, SourceLocation EndLoc,
                             std::string Name, QualType Underlying);

  static bool classof(const Decl *D) { return D->getKind() == DK_Typedef; }

  QualType getUnderlying() const { return Underlying; }

private:
  TypedefDecl(ASTContext &Ctx, SourceLocation Loc, SourceLocation BegLoc,
              SourceLocation EndLoc, std::string Name, QualType Underlying);

private:
  QualType Underlying;
};

} // namespace rcc

#endif