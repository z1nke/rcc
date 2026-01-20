#ifndef RCC_SEMA_DECLSPEC_H
#define RCC_SEMA_DECLSPEC_H

#include "Basic/SourceLocation.h"

#include <string>
#include <vector>

namespace rcc {

class Expr;

class DeclSpec {
public:
  enum TypeSpecType {
    TST_Unspecified,
    TST_Int,
    TST_Char,
  };

  DeclSpec() = default;
  DeclSpec(const DeclSpec &) = delete;
  void operator=(const DeclSpec &) = delete;

  void setTypeSpecType(TypeSpecType TST, SourceLocation TSTLoc);
  TypeSpecType getTypeSpecType() const { return TST; }
  SourceLocation getTypeSpecLoc() const { return TSTLoc; }

private:
  TypeSpecType TST = TST_Unspecified;
  SourceLocation TSTLoc;
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