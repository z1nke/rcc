#ifndef RCC_SEMA_DECLSPEC_H
#define RCC_SEMA_DECLSPEC_H

#include "Basic/SourceLocation.h"

#include <string>
#include <vector>

namespace rcc {

class Expr;
class Decl;
class Diagnostic;

class DeclSpec {
public:
  // storage-class-specifier
  enum StorageClassSpec {
    SCS_Unspecified,
    SCS_Typedef,
    // SCS_Extern,
    // SCS_Static,
    // SCS_Auto,
    // SCS_Register,
  };

  // type-specifier
  enum TypeSpecType {
    TST_Unspecified,
    TST_Void,
    TST_Char,
    TST_Int,
    TST_Struct,
    TST_Union,
    TST_Typename, // Typedef, struct/union name or enum name.
  };

  enum TypeSpecWidth {
    TSW_Unspecified,
    TSW_Short,
    TSW_Long,
    TSW_LongLong,
  };

  DeclSpec(Diagnostic &Diag) : Diag(Diag) {}
  DeclSpec(const DeclSpec &) = delete;
  void operator=(const DeclSpec &) = delete;

  SourceLocation getTypeSpecLoc() const { return TSLoc; }
  StorageClassSpec getStorageClassSpec() const { return SCS; }
  TypeSpecType getTypeSpecType() const { return TST; }
  TypeSpecWidth getTypeSpecWidth() const { return TSW; }
  bool hasTypeSpecifier() const;

  void setStorageClassSpec(StorageClassSpec S, SourceLocation Loc);
  void setTypeSpecType(TypeSpecType T, SourceLocation Loc);
  void setTypeSpecWidth(TypeSpecWidth W, SourceLocation Loc);

  Decl *getRepDecl() const { return RepDecl; }
  void setRepDecl(Decl *D) { RepDecl = D; }

  static const char *getSpecifierName(StorageClassSpec S);
  static const char *getSpecifierName(TypeSpecType T);
  static const char *getSpecifierName(TypeSpecWidth T);

private:
  template <typename T>
  [[noreturn]] void reportBadSpec(SourceLocation Loc, T TNew, T TPrev);

private:
  StorageClassSpec SCS = SCS_Unspecified;
  TypeSpecType TST = TST_Unspecified;
  TypeSpecWidth TSW = TSW_Unspecified;
  SourceLocation SCSLoc;
  SourceLocation TSLoc;
  Decl *RepDecl = nullptr;
  Diagnostic &Diag;
};

struct DeclaratorChunk {
  enum DCKind {
    DCK_Pointer,
    DCK_Function,
    DCK_Array,
  } Kind;

  DeclaratorChunk(DCKind Kind) : Kind(Kind) {}

  struct ArrayTypeInfo {
    Expr *LenExpr;
  };

  union {
    ArrayTypeInfo Arr;
  };

  static DeclaratorChunk createPointer();
  static DeclaratorChunk createFunction();
  static DeclaratorChunk createArray(Expr *LenExpr);
};

class Declarator {
public:
  Declarator(const DeclSpec &DS) : DS(DS) {}

  const DeclSpec &getDeclSpec() const { return DS; }

  const std::string &getIdent() { return Ident; }
  void setIdent(std::string Name) { Ident = std::move(Name); }

  void addDeclChunk(DeclaratorChunk Chunk) { DeclChunks.push_back(Chunk); }

  const std::vector<DeclaratorChunk> &getDeclChunks() const {
    return DeclChunks;
  }

  void setLocation(SourceLocation Loc) { this->Loc = Loc; }
  SourceLocation getLocation() const { return Loc; }

  void setEndLoc(SourceLocation EndLoc) { this->EndLoc = EndLoc; }
  SourceLocation getEndLoc() const { return EndLoc; }

  SourceLocation getTypeSpecLoc() const { return DS.getTypeSpecLoc(); }

  bool isFunction() const;

private:
  const DeclSpec &DS;
  std::string Ident;
  SourceLocation Loc;
  SourceLocation EndLoc;
  std::vector<DeclaratorChunk> DeclChunks;
};

} // namespace rcc

#endif