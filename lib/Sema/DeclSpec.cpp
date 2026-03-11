#include "Sema/DeclSpec.h"
#include "Basic/Diagnostic.h"
#include "Support/Unreachable.h"

namespace rcc {

DeclaratorChunk DeclaratorChunk::createPointer() {
  return DeclaratorChunk(DCK_Pointer);
}

DeclaratorChunk DeclaratorChunk::createFunction() {
  return DeclaratorChunk(DCK_Function);
}

DeclaratorChunk DeclaratorChunk::createArray(Expr *LenExpr) {
  DeclaratorChunk Chunk(DCK_Array);
  Chunk.Arr.LenExpr = LenExpr;
  return Chunk;
}

void DeclSpec::setTypeSpecType(TypeSpecType T, SourceLocation Loc) {
  if (TST != TST_Unspecified) {
    Diag.fatalAt(TSTLoc,
                 "cannot combine with previous '{}' declaration specifier",
                 getSpecifierName(TST));
  }

  TST = T;
  TSTLoc = Loc;
}

void DeclSpec::setTypeSpecWidth(TypeSpecWidth W, SourceLocation Loc) {
  if (TSW == TSW_Unspecified) {
    if (TSTLoc.isInvalid())
      TSTLoc = Loc;
  } else {
    Diag.fatalAt(Loc, "cannot combine with previous '{}' declaration specifier",
                 getSpecifierName(TSW));
  }

  TSW = W;
}

const char *DeclSpec::getSpecifierName(TypeSpecType T) {
  switch (T) {
  case TST_Void:
    return "void";
  case TST_Char:
    return "char";
  case TST_Int:
    return "int";
  case TST_Struct:
    return "struct";
  case TST_Union:
    return "union";
  default:
    RCC_UNREACHABLE("Unknown type specifier type");
  }
}
const char *DeclSpec::getSpecifierName(TypeSpecWidth T) {
  switch (T) {
  case TSW_Short:
    return "short";
  case TSW_Long:
    return "long";
  default:
    RCC_UNREACHABLE("Unknown type specifier width");
  }
}

} // namespace rcc