#ifndef RCC_SEMA_DECLSPEC_H
#define RCC_SEMA_DECLSPEC_H

#include "Basic/SourceLocation.h"

#include <cstddef>
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
    SCS_Extern,
    SCS_Static,
    // SCS_Auto,
    // SCS_Register,
  };

  // type-specifier
  enum TypeSpecType {
    TST_Unspecified,
    TST_Void,
    TST_UnderlineBool,
    TST_Char,
    TST_Int,
    TST_Float,
    TST_Double,
    TST_Struct,
    TST_Union,
    TST_Enum,
    TST_Typename, // Typedef, struct/union name or enum name.
  };

  enum TypeSpecWidth {
    TSW_Unspecified,
    TSW_Short,
    TSW_Long,
    TSW_LongLong,
  };

  enum TypeSpecSign {
    TSS_Unspecified,
    TSS_Signed,
    TSS_Unsigned,
  };

  DeclSpec(Diagnostic &Diag) : Diag(Diag) {}
  DeclSpec(const DeclSpec &) = delete;
  void operator=(const DeclSpec &) = delete;

  SourceLocation getTypeSpecLoc() const { return TSLoc; }
  StorageClassSpec getStorageClassSpec() const { return SCS; }
  TypeSpecType getTypeSpecType() const { return TST; }
  TypeSpecWidth getTypeSpecWidth() const { return TSW; }
  TypeSpecSign getTypeSpecSign() const { return TSS; }
  bool hasTypeSpecifier() const;

  void setStorageClassSpec(StorageClassSpec S, SourceLocation Loc);
  void setTypeSpecType(TypeSpecType T, SourceLocation Loc);
  void setTypeSpecWidth(TypeSpecWidth W, SourceLocation Loc);
  void setTypeSpecSign(TypeSpecSign S, SourceLocation Loc);

  Decl *getRepDecl() const { return RepDecl; }
  void setRepDecl(Decl *D) { RepDecl = D; }

  bool isAlignasAllowed() const { return AlignasAllowed; }
  void setAlignasAllowed(bool Allowed = true) { AlignasAllowed = Allowed; }

  std::size_t getAlign() const { return Align; }
  void setAlign(std::size_t Align) { this->Align = Align; }

  static const char *getSpecifierName(StorageClassSpec S);
  static const char *getSpecifierName(TypeSpecType T);
  static const char *getSpecifierName(TypeSpecWidth T);
  static const char *getSpecifierName(TypeSpecSign T);

private:
  template <typename T>
  [[noreturn]] void reportBadSpec(SourceLocation Loc, T TNew, T TPrev);

private:
  StorageClassSpec SCS = SCS_Unspecified;
  TypeSpecType TST = TST_Unspecified;
  TypeSpecWidth TSW = TSW_Unspecified;
  TypeSpecSign TSS = TSS_Unspecified;
  SourceLocation SCSLoc;
  SourceLocation TSLoc;
  Decl *RepDecl = nullptr;
  bool AlignasAllowed = false;
  std::size_t Align = 0;
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

  struct FunctionTypeInfo {
    bool IsVariadic;
  };

  union {
    ArrayTypeInfo Arr;
    FunctionTypeInfo Fun;
  };

  static DeclaratorChunk createPointer();
  static DeclaratorChunk createFunction(bool IsVariadic = false);
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